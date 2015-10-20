/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTexture.h"

#include <algorithm>
#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Scoped.h"
#include "mozilla/unused.h"
#include "ScopedGLHelpers.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLTexelConversions.h"

namespace mozilla {

/*static*/ const WebGLTexture::ImageInfo WebGLTexture::ImageInfo::kUndefined;

////////////////////////////////////////

template <typename T>
static inline T&
Mutable(const T& x)
{
    return const_cast<T&>(x);
}

void
WebGLTexture::ImageInfo::Clear()
{
    if (!IsDefined())
        return;

    OnRespecify();

    Mutable(mFormat) = LOCAL_GL_NONE;
    Mutable(mWidth) = 0;
    Mutable(mHeight) = 0;
    Mutable(mDepth) = 0;

    MOZ_ASSERT(!IsDefined());
}

WebGLTexture::ImageInfo&
WebGLTexture::ImageInfo::operator =(const ImageInfo& a)
{
    MOZ_ASSERT(a.IsDefined());

    Mutable(mFormat) = a.mFormat;
    Mutable(mWidth) = a.mWidth;
    Mutable(mHeight) = a.mHeight;
    Mutable(mDepth) = a.mDepth;

    mIsDataInitialized = a.mIsDataInitialized;

    // But *don't* transfer mAttachPoints!
    MOZ_ASSERT(a.mAttachPoints.empty());
    OnRespecify();

    return *this;
}

bool
WebGLTexture::ImageInfo::IsPowerOfTwo() const
{
    return mozilla::IsPowerOfTwo(mWidth) &&
           mozilla::IsPowerOfTwo(mHeight) &&
           mozilla::IsPowerOfTwo(mDepth);
}

void
WebGLTexture::ImageInfo::AddAttachPoint(WebGLFBAttachPoint* attachPoint)
{
    const auto pair = mAttachPoints.insert(attachPoint);
    DebugOnly<bool> didInsert = pair.second;
    MOZ_ASSERT(didInsert);
}

void
WebGLTexture::ImageInfo::RemoveAttachPoint(WebGLFBAttachPoint* attachPoint)
{
    DebugOnly<size_t> numElemsErased = mAttachPoints.erase(attachPoint);
    MOZ_ASSERT_IF(IsDefined(), numElemsErased == 1);
}

void
WebGLTexture::ImageInfo::OnRespecify() const
{
    for (auto cur : mAttachPoints) {
        cur->OnBackingStoreRespecified();
    }
}

size_t
WebGLTexture::ImageInfo::MemoryUsage() const
{
    if (!IsDefined())
        return 0;

    const auto bytesPerTexel = mFormat->formatInfo->bytesPerPixel;
    return size_t(mWidth) * size_t(mHeight) * size_t(mDepth) * bytesPerTexel;
}

void
WebGLTexture::ImageInfo::SetIsDataInitialized(bool isDataInitialized, WebGLTexture* tex)
{
    MOZ_ASSERT(tex);
    MOZ_ASSERT(this >= &tex->mImageInfoArr[0]);
    MOZ_ASSERT(this < &tex->mImageInfoArr[kMaxLevelCount * kMaxFaceCount]);

    mIsDataInitialized = isDataInitialized;
    tex->InvalidateFakeBlackCache();
}

////////////////////////////////////////

JSObject*
WebGLTexture::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) {
    return dom::WebGLTextureBinding::Wrap(cx, this, givenProto);
}

WebGLTexture::WebGLTexture(WebGLContext* webgl, GLuint tex)
    : WebGLContextBoundObject(webgl)
    , mGLName(tex)
    , mTarget(LOCAL_GL_NONE)
    , mMinFilter(LOCAL_GL_NEAREST_MIPMAP_LINEAR)
    , mMagFilter(LOCAL_GL_LINEAR)
    , mWrapS(LOCAL_GL_REPEAT)
    , mWrapT(LOCAL_GL_REPEAT)
    , mFaceCount(0)
    , mImmutable(false)
    , mImmutableLevelCount(0)
    , mBaseMipmapLevel(0)
    , mMaxMipmapLevel(1000)
    , mFakeBlackStatus(WebGLTextureFakeBlackStatus::IncompleteTexture)
    , mTexCompareMode(LOCAL_GL_NONE)
{
    mContext->mTextures.insertBack(this);
}

void
WebGLTexture::Delete()
{
    for (auto& cur : mImageInfoArr) {
        cur.Clear();
    }

    mContext->MakeContextCurrent();
    mContext->gl->fDeleteTextures(1, &mGLName);

    LinkedListElement<WebGLTexture>::removeFrom(mContext->mTextures);
}

size_t
WebGLTexture::MemoryUsage() const
{
    if (IsDeleted())
        return 0;

    size_t result = 0;
    MOZ_CRASH("todo");
    return result;
}

void
WebGLTexture::SetImageInfo(ImageInfo* target, const ImageInfo& newInfo)
{
    *target = newInfo;

    InvalidateFakeBlackCache();
}

void
WebGLTexture::SetImageInfosAtLevel(uint32_t level, const ImageInfo& newInfo)
{
    for (uint8_t i = 0; i < mFaceCount; i++) {
        ImageInfoAtFace(i, level) = newInfo;
    }

    InvalidateFakeBlackCache();
}

static inline uint32_t
MaxMipmapLevelsForSize(const WebGLTexture::ImageInfo& info)
{
    auto size = std::max(std::max(info.mWidth, info.mHeight), info.mDepth);

    // Find floor(log2(maxsize)) + 1. (ES 3.0.4, 3.8 - Mipmapping).
    return mozilla::FloorLog2(size) + 1;
}

bool
WebGLTexture::IsMipmapComplete() const
{
    MOZ_ASSERT(DoesMinFilterRequireMipmap());
    // GLES 3.0.4, p161

    // "* `level_base <= level_max`"
    if (mBaseMipmapLevel > mMaxMipmapLevel)
        return false;

    // Make a copy so we can modify it.
    const ImageInfo& baseImageInfo = BaseImageInfo();
    if (!baseImageInfo.IsDefined())
        return false;

    // Reference dimensions based on the current level.
    uint32_t refWidth = baseImageInfo.mWidth;
    uint32_t refHeight = baseImageInfo.mHeight;
    uint32_t refDepth = baseImageInfo.mDepth;
    MOZ_ASSERT(refWidth && refHeight && refDepth);

    for (uint32_t level = mBaseMipmapLevel; level < mMaxMipmapLevel; level++) {
        // "A cube map texture is mipmap complete if each of the six texture images,
        //  considered individually, is mipmap complete."

        for (uint8_t face = 0; face < mFaceCount; face++) {
            const ImageInfo& cur = ImageInfoAtFace(face, level);

            // "* The set of mipmap arrays `level_base` through `q` (where `q` is defined
            //    the "Mipmapping" discussion of section 3.8.10) were each specified with
            //    the same effective internal format."

            // "* The dimensions of the arrays follow the sequence described in the
            //    "Mipmapping" discussion of section 3.8.10."

            if (cur.mWidth != refWidth ||
                cur.mHeight != refHeight ||
                cur.mDepth != refDepth ||
                cur.mFormat != baseImageInfo.mFormat)
            {
                return false;
            }
        }

        // GLES 3.0.4, p158:
        // "[...] until the last array is reached with dimension 1 x 1 x 1."
        if (refWidth == 1 &&
            refHeight == 1 &&
            refDepth == 1)
        {
            break;
        }

        refWidth  = std::max(uint32_t(1), refWidth  / 2);
        refHeight = std::max(uint32_t(1), refHeight / 2);
        refDepth  = std::max(uint32_t(1), refDepth  / 2);
    }

    return true;
}

bool
WebGLTexture::IsCubeComplete() const
{
    // GLES 3.0.4, p161
    // "[...] a cube map texture is cube complete if the following conditions all hold
    //  true:
    //  * The `level_base` arrays of each of the six texture images making up the cube map
    //    have identical, positive, and square dimensions.
    //  * The `level_base` arrays were each specified with the same effective internal
    //    format."

    // Note that "cube complete" does not imply "mipmap complete".

    const ImageInfo& reference = BaseImageInfo();
    if (!reference.IsDefined())
        return false;

    auto refWidth = reference.mWidth;
    auto refFormat = reference.mFormat;

    for (uint8_t face = 0; face < mFaceCount; face++) {
        const ImageInfo& cur = ImageInfoAtFace(face, mBaseMipmapLevel);
        if (!cur.IsDefined())
            return false;

        MOZ_ASSERT(cur.mDepth == 1);
        if (cur.mFormat != refFormat || // Check effective formats.
            cur.mWidth != refWidth ||   // Check both width and height against refWidth to
            cur.mHeight != refWidth)    // to enforce positive and square dimensions.
        {
            return false;
        }
    }

    return true;
}

bool
WebGLTexture::IsComplete(const char** const out_reason) const
{
    // Texture completeness is established at GLES 3.0.4, p160-161.
    // "[A] texture is complete unless any of the following conditions hold true:"

    // "* Any dimension of the `level_base` array is not positive."
    const ImageInfo& baseImageInfo = BaseImageInfo();
    if (!baseImageInfo.IsDefined()) {
        // In case of undefined texture image, we don't print any message because this is
        // a very common and often legitimate case (asynchronous texture loading).
        *out_reason = nullptr;
        return false;
    }

    if (!baseImageInfo.mWidth || !baseImageInfo.mHeight || !baseImageInfo.mDepth) {
        *out_reason = "The dimensions of `level_base` are not all positive.";
        return false;
    }

    // "* The texture is a cube map texture, and is not cube complete."
    if (IsCubeMap() && !IsCubeComplete()) {
        *out_reason = "Cubemaps must be \"cube complete\".";
        return false;
    }

    // "* The minification filter requires a mipmap (is neither NEAREST nor LINEAR) and
    //    the texture is not mipmap complete."
    const bool requiresMipmap = (mMinFilter != LOCAL_GL_NEAREST &&
                                 mMinFilter != LOCAL_GL_LINEAR);
    if (requiresMipmap && !IsMipmapComplete()) {
        *out_reason = "Because the minification filter requires mipmapping, the texture"
                      " must be \"mipmap complete\".";
        return false;
    }

    const bool isMinFilteringNearest = (mMinFilter == LOCAL_GL_NEAREST ||
                                        mMinFilter == LOCAL_GL_NEAREST_MIPMAP_NEAREST);
    const bool isMagFilteringNearest = (mMagFilter == LOCAL_GL_NEAREST);
    const bool isFilteringNearestOnly = (isMinFilteringNearest && isMagFilteringNearest);
    if (!isFilteringNearestOnly) {
        auto formatUsage = baseImageInfo.mFormat;
        auto format = formatUsage->formatInfo;

        // "* The effective internal format specified for the texture arrays is a sized
        //    internal color format that is not texture-filterable, and either the
        //    magnification filter is not NEAREST or the minification filter is neither
        //    NEAREST nor NEAREST_MIPMAP_NEAREST."
        // Since all (GLES3) unsized color formats are filterable just like their sized
        // equivalents, we don't have to care whether its sized or not.
        if (format->isColorFormat && !formatUsage->isFilterable) {
            *out_reason = "Because minification or magnification filtering is not NEAREST"
                          " or NEAREST_MIPMAP_NEAREST, and the texture's format is a"
                          " color format, its format must be \"texture-filterable\".";
            return false;
        }

        // "* The effective internal format specified for the texture arrays is a sized
        //    internal depth or depth and stencil format, the value of
        //    TEXTURE_COMPARE_MODE is NONE[1], and either the magnification filter is not
        //    NEAREST, or the minification filter is neither NEAREST nor
        //    NEAREST_MIPMAP_NEAREST."
        // [1]: This sounds suspect, but is explicitly noted in the change log for GLES
        //      3.0.1:
        //      "* Clarify that a texture is incomplete if it has a depth component, no
        //         shadow comparison, and linear filtering (also Bug 9481)."
        // As of OES_packed_depth_stencil rev #3, the sample code explicitly samples from
        // a DEPTH_STENCIL_OES texture with a min-filter of LINEAR. Therefore we relax
        // this restriction if WEBGL_depth_texture is enabled.
        if (!mContext->IsExtensionEnabled(WebGLExtensionID::WEBGL_depth_texture)) {
            if (format->hasDepth && mTexCompareMode != LOCAL_GL_NONE) {
                *out_reason = "A depth or depth-stencil format with TEXTURE_COMPARE_MODE"
                              " of NONE must have minification or magnification filtering"
                              " of NEAREST or NEAREST_MIPMAP_NEAREST.";
                return false;
            }
        }
    }

    // Texture completeness is effectively (though not explicitly) amended for GLES2 by
    // the "Texture Access" section under $3.8 "Fragment Shaders". This also applies to
    // vertex shaders, as noted on GLES 2.0.25, p41.
    if (!mContext->IsWebGL2()) {
        // GLES 2.0.25, p87-88:
        // "Calling a sampler from a fragment shader will return (R,G,B,A)=(0,0,0,1) if
        //  any of the following conditions are true:"

        // "* A two-dimensional sampler is called, the minification filter is one that
        //    requires a mipmap[...], and the sampler's associated texture object is not
        //    complete[.]"
        // (already covered)

        // "* A two-dimensional sampler is called, the minification filter is not one that
        //    requires a mipmap (either NEAREST nor[sic] LINEAR), and either dimension of
        //    the level zero array of the associated texture object is not positive."
        // (already covered)

        // "* A two-dimensional sampler is called, the corresponding texture image is a
        //    non-power-of-two image[...], and either the texture wrap mode is not
        //    CLAMP_TO_EDGE, or the minification filter is neither NEAREST nor LINEAR."

        // "* A cube map sampler is called, any of the corresponding texture images are
        //    non-power-of-two images, and either the texture wrap mode is not
        //    CLAMP_TO_EDGE, or the minification filter is neither NEAREST nor LINEAR."
        if (!baseImageInfo.IsPowerOfTwo()) {
            // "either the texture wrap mode is not CLAMP_TO_EDGE"
            if (mWrapS != LOCAL_GL_CLAMP_TO_EDGE ||
                mWrapS != LOCAL_GL_CLAMP_TO_EDGE)
            {
                *out_reason = "Non-power-of-two textures must have a wrap mode of"
                              " CLAMP_TO_EDGE.";
                return false;
            }

            // "or the minification filter is neither NEAREST nor LINEAR"
            if (requiresMipmap) {
                *out_reason = "Mipmapping requires power-of-two textures.";
                return false;
            }
        }

        // "* A cube map sampler is called, and either the corresponding cube map texture
        //    image is not cube complete, or TEXTURE_MIN_FILTER is one that requires a
        //    mipmap and the texture is not mipmap cube complete."
        // (already covered)
    }

    return true;
}


uint32_t
WebGLTexture::MaxEffectiveMipmapLevel() const
{
    const bool requiresMipmap = (mMinFilter != LOCAL_GL_NEAREST &&
                                 mMinFilter != LOCAL_GL_LINEAR);
    if (!requiresMipmap)
        return mBaseMipmapLevel;

    const auto& imageInfo = BaseImageInfo();
    MOZ_ASSERT(imageInfo.IsDefined());

    uint32_t maxLevelBySize = mBaseMipmapLevel + imageInfo.MaxMipmapLevels() - 1;
    return std::min<uint32_t>(maxLevelBySize, mMaxMipmapLevel);
}

bool
WebGLTexture::ResolveFakeBlackStatus(WebGLTextureFakeBlackStatus* const out)
{
    if (!ResolveFakeBlackStatus())
        return false;

    *out = mFakeBlackStatus;
    return true;
}

bool
WebGLTexture::ResolveFakeBlackStatus()
{
    if (MOZ_LIKELY(mFakeBlackStatus != WebGLTextureFakeBlackStatus::Unknown))
        return true;

    const char* incompleteReason;
    if (!IsComplete(&incompleteReason)) {
        if (incompleteReason) {
            mContext->GenerateWarning("An active texture is going to be rendered as if it"
                                      " were black, as per the GLES 2.0.24 $3.8.2: %s",
                                      incompleteReason);
        }
        mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
        return true;
    }

    // We have exhausted all cases of incomplete textures, where we would need opaque black.
    // We may still need transparent black in case of uninitialized image data.
    bool hasUninitializedData = false;
    bool hasInitializedData = false;

    const auto maxLevel = MaxEffectiveMipmapLevel();
    MOZ_ASSERT(mBaseMipmapLevel <= maxLevel);
    for (uint32_t level = mBaseMipmapLevel; level <= maxLevel; level++) {
        for (uint8_t face = 0; face < mFaceCount; face++) {
            const auto& cur = ImageInfoAtFace(face, level);
            if (cur.IsDataInitialized())
                hasInitializedData = true;
            else
                hasUninitializedData = true;
        }
    }
    MOZ_ASSERT(hasUninitializedData || hasInitializedData);

    if (!hasUninitializedData) {
        mFakeBlackStatus = WebGLTextureFakeBlackStatus::NotNeeded;
        return true;
    }

    if (!hasInitializedData) {
        mFakeBlackStatus = WebGLTextureFakeBlackStatus::UninitializedImageData;
        return true;
    }

    // Alright, we have both initialized and uninitialized data, so we have to initialize
    // the uninitialized images. Feel free to be slow.
    mContext->GenerateWarning("An active texture contains TexImages with uninitialized"
                              " data along with TexImages with initialized data, forcing"
                              " the implementation to (slowly) initialize the"
                              " uninitialized TexImages.");

    for (uint32_t level = mBaseMipmapLevel; level <= maxLevel; level++) {
        for (uint8_t face = 0; face < mFaceCount; face++) {
            if (!EnsureInitializedImageData(face, level))
                return false; // The world just exploded.
        }
    }

    mFakeBlackStatus = WebGLTextureFakeBlackStatus::NotNeeded;
    return true;
}

static bool
ClearByMask(WebGLContext* webgl, GLbitfield mask)
{
    gl::GLContext* gl = webgl->GL();
    MOZ_ASSERT(gl->IsCurrent());

    GLenum status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return false;

    bool colorAttachmentsMask[WebGLContext::kMaxColorAttachments] = {false};
    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        colorAttachmentsMask[0] = true;
    }

    webgl->ForceClearFramebufferWithDefaultValues(false, mask, colorAttachmentsMask);
    return true;
}

// `mask` from glClear.
static bool
ClearWithTempFB(WebGLContext* webgl, GLuint tex, TexImageTarget target, GLint level,
                const ImageInfo& imageInfo)
{
    if (mTarget == LOCAL_GL_TEXTURE_2D)
        return false;

    gl::GLContext* gl = webgl->gl;
    MOZ_ASSERT(gl->IsCurrent());

    gl::ScopedFramebuffer fb(gl);
    gl::ScopedBindFramebuffer autoFB(gl, fb.FB());

    auto formatInfo = target->mFormat->formatInfo;
    GLbitfield mask = 0;

    if (formatInfo->isColorFormat) {
        mask |= LOCAL_GL_COLOR_BUFFER_BIT;
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                  GLenum(target), tex, level);
    } else {
        if (formatInfo->hasDepth) {
            mask |= LOCAL_GL_DEPTH_BUFFER_BIT;
            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                      GLenum(target), tex, level);
        }
        if (formatInfo->hasStencil) {
            mask |= LOCAL_GL_STENCIL_BUFFER_BIT;
            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                      GLenum(target), tex, level);
        }
    }

    if (!mask)
        return false;

    if (ClearByMask(webgl, mask))
        return true;

    // Failed to simply build an FB from the tex, but maybe it needs a
    // color buffer to be complete.
    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        // Nope, it already had one.
        return false;
    }

    gl::ScopedRenderbuffer rb(gl);
    {
        gl::ScopedBindRenderbuffer rbBinding(gl, rb.RB());

        // Only GLES guarantees RGBA4.
        GLenum format = gl->IsGLES() ? LOCAL_GL_RGBA4 : LOCAL_GL_RGBA8;
        gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, format, width, height);
    }

    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                 LOCAL_GL_RENDERBUFFER, rb.RB());
    mask |= LOCAL_GL_COLOR_BUFFER_BIT;

    // Last chance!
    return ClearByMask(webgl, mask);
}

static TexImageTarget
TexImageTargetFromFace(TexTarget target, uint8_t face)
{
    if (target != LOCAL_GL_TEXTURE_CUBE_MAP)
        return target.get();

    return LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
}

static CheckedUint32
NumWholeBlocksForLength(CheckedUint32 length, uint8_t blockLength)
{
    return (length + blockLength - 1) / blockLength;
}

bool
WebGLTexture::InitializeImageData(TexImageTarget target, uint32_t level)
{
    ImageInfo& imageInfo = ImageInfoAt(target, level);
    MOZ_ASSERT(imageInfo.IsDefined());
    MOZ_ASSERT(!imageInfo.IsDataInitialized());

    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    // Try to clear with glClear.
    if (ClearWithTempFB(mContext, mGLName, target, level, imageInfo)) {
        SetIsDataInitialized(true, this);
        return true;
    }
    // That didn't work. Try uploading zeros then.

    const webgl::FormatUsageInfo* format = imageInfo.mFormat;
    auto& width = imageInfo.mWidth;
    auto& height = imageInfo.mHeight;
    auto& depth = imageInfo.mDepth;

    ScopedUnpackReset scopedReset(mContext, 0);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 1); // Don't bother with striding it well.

    auto compression = format->formatInfo->compression;
    if (compression) {
        CheckedUint32 checkedByteCount = compression->bytesPerBlock;
        checkedByteCount *= NumWholeBlocksForLength(width, compression->blockWidth);
        checkedByteCount *= NumWholeBlocksForLength(height, compression->blockHeight);
        checkedByteCount *= depth;

        if (!checkedByteCount.isValid())
            return false;

        size_t byteCount = checkedByteCount.value();

        UniqueBuffer zeros = calloc(1, byteCount);
        if (!zeros)
            return false;

        GLenum error = CompressedTexSubImage(gl, target, level, 0, 0, 0, width, height,
                                             depth, format, zeros.get());
        if (error)
            return false;
    } else {
        auto texImageInfo = format->GetAnyUnpack();

        CheckedUint32 checkedByteCount = texImageInfo->bytesPerPixel;
        checkedByteCount *= width;
        checkedByteCount *= height;
        checkedByteCount *= depth;

        if (!checkedByteCount.isValid())
            return false;

        size_t byteCount = checkedByteCount.value();

        UniqueBuffer zeros = calloc(1, byteCount);
        if (!zeros)
            return false;

        GLenum error = TexSubImage(gl, target, level, 0, 0, 0, width, height, depth,
                                   texImageInfo, zeros.get());
        if (error)
            return false;
    }

    SetIsDataInitialized(true, this);
    return true;
}




    gl::ScopedBindTexture autoBindTex(gl, mGLName, mTarget.get());

    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
    GLenum driverUnpackType = LOCAL_GL_NONE;
    DriverFormatsForTextures(gl, formatUsage, &driverInternalFormat, &driverUnpackFormat,
                             &driverUnpackType);

    mContext->GetAndFlushUnderlyingGLErrors();
    if (texImageTarget == LOCAL_GL_TEXTURE_3D) {
        gl->fTexSubImage3D(texImageTarget.get(), level, 0, 0, 0, imageInfo.mWidth,
                           imageInfo.mHeight, imageInfo.mDepth, driverUnpackFormat,
                           driverUnpackType, zeros.get());
    } else {
        MOZ_ASSERT(imageInfo.mDepth == 1);
        gl->fTexSubImage2D(texImageTarget.get(), level, 0, 0, imageInfo.mWidth,
                           imageInfo.mHeight, driverUnpackFormat, driverUnpackType,
                           zeros.get());
    }

    GLenum error = mContext->GetAndFlushUnderlyingGLErrors();
    if (error) {
        // Should only be OUT_OF_MEMORY. Anyway, there's no good way to recover
        // from this here.
        if (error != LOCAL_GL_OUT_OF_MEMORY) {
            printf_stderr("Error: 0x%4x\n", error);
            gfxCriticalError() << "GL context GetAndFlushUnderlyingGLErrors " << gfx::hexa(error);
            // Errors on texture upload have been related to video
            // memory exposure in the past, which is a security issue.
            // Force loss of context.
            mContext->ForceLoseContext(true);
            return false;
        }

        // Out-of-memory uploading pixels to GL. Lose context and report OOM.
        mContext->ForceLoseContext(true);
        mContext->ErrorOutOfMemory("EnsureNoUninitializedImageData: Failed to "
                                   "upload texture of width: %u, height: %u, "
                                   "depth: %u to target %s level %d.",
                                   imageInfo.mWidth, imageInfo.mHeight, imageInfo.mDepth,
                                   mContext->EnumName(texImageTarget.get()), level);
        return false;
    }

    imageInfo.SetIsDataInitialized(true, this);
    return true;
}

void
WebGLTexture::ClampLevelBaseAndMax()
{
    if (!mImmutable)
        return;

    // GLES 3.0.4, p158:
    // "For immutable-format textures, `level_base` is clamped to the range
    //  `[0, levels-1]`, `level_max` is then clamped to the range `
    //  `[level_base, levels-1]`, where `levels` is the parameter passed to
    //   TexStorage* for the texture object."
    mBaseMipmapLevel = Clamp<uint32_t>(mBaseMipmapLevel, 0, mImmutableLevelCount - 1);
    mMaxMipmapLevel = Clamp<uint32_t>(mMaxMipmapLevel, mBaseMipmapLevel,
                                      mImmutableLevelCount - 1);
}

void
WebGLTexture::PopulateMipChain(uint32_t firstLevel, uint32_t lastLevel)
{
    const ImageInfo& baseImageInfo = ImageInfoAtFace(0, firstLevel);
    MOZ_ASSERT(baseImageInfo.IsDefined());

    uint32_t refWidth = baseImageInfo.mWidth;
    uint32_t refHeight = baseImageInfo.mHeight;
    uint32_t refDepth = baseImageInfo.mDepth;
    MOZ_ASSERT(refWidth && refHeight && refDepth);

    for (uint32_t level = firstLevel; level <= lastLevel; level++) {
        const ImageInfo cur(baseImageInfo.mFormat, refWidth, refHeight, refDepth,
                            baseImageInfo.IsDataInitialized());

        SetImageInfosAtLevel(level, cur);

        bool isMinimal = (refWidth == 1 &&
                          refHeight == 1);
        if (mTarget == LOCAL_GL_TEXTURE_3D) {
            isMinimal &= (refDepth == 1);
        }

        // Higher levels are unaffected.
        if (isMinimal)
            break;

        refWidth = std::max(uint32_t(1), refWidth / 2);
        refHeight = std::max(uint32_t(1), refHeight / 2);
        if (mTarget == LOCAL_GL_TEXTURE_3D) { // But not TEXTURE_2D_ARRAY!
            refDepth = std::max(uint32_t(1), refDepth / 2);
        }
    }
}

void
WebGLTexture::InvalidateFakeBlackCache()
{
    mContext->InvalidateFakeBlackCache();
    mFakeBlackStatus = WebGLTextureFakeBlackStatus::Unknown;
}

//////////////////////////////////////////////////////////////////////////////////////////
// GL calls

bool
WebGLTexture::BindTexture(TexTarget texTarget)
{
    // silently ignore a deleted texture
    if (IsDeleted())
        return false;

    const bool isFirstBinding = !HasEverBeenBound();
    if (!isFirstBinding && mTarget != texTarget) {
        mContext->ErrorInvalidOperation("bindTexture: This texture has already been bound"
                                        " to a different target.");
        return false;
    }

    mTarget = texTarget;

    mContext->gl->fBindTexture(mTarget.get(), mGLName);

    if (isFirstBinding) {
        mFaceCount = IsCubeMap() ? 6 : 1;

        // Thanks to the WebKit people for finding this out: GL_TEXTURE_WRAP_R
        // is not present in GLES 2, but is present in GL and it seems as if for
        // cube maps we need to set it to GL_CLAMP_TO_EDGE to get the expected
        // GLES behavior.
        if (IsCubeMap() && !mContext->gl->IsGLES()) {
            mContext->gl->fTexParameteri(texTarget.get(),
                                         LOCAL_GL_TEXTURE_WRAP_R,
                                         LOCAL_GL_CLAMP_TO_EDGE);
        }
    }

    if (mFakeBlackStatus != WebGLTextureFakeBlackStatus::NotNeeded) {
        mContext->InvalidateFakeBlackCache();
    }

    return true;
}


void
WebGLTexture::GenerateMipmap(TexTarget texTarget)
{
    if (mBaseMipmapLevel > mMaxMipmapLevel) {
        mContext->ErrorInvalidOperation("generateMipmap: Texture does not have a valid mipmap"
                              " range.");
        return;
    }

    if (IsCubeMap() && !IsCubeComplete()) {
        mContext->ErrorInvalidOperation("generateMipmap: Cube maps must be \"cube complete\".");
        return;
    }

    const ImageInfo& baseImageInfo = BaseImageInfo();
    if (!baseImageInfo.IsDefined()) {
        mContext->ErrorInvalidOperation("generateMipmap: The base level of the texture is not"
                              " defined.");
        return;
    }

    if (!mContext->IsWebGL2() && !baseImageInfo.IsPowerOfTwo()) {
        mContext->ErrorInvalidOperation("generateMipmap: The base level of the texture does not"
                              " have power-of-two dimensions.");
        return;
    }

    auto format = baseImageInfo.mFormat->formatInfo;
    if (format->compression) {
        mContext->ErrorInvalidOperation("generateMipmap: Texture data at base level is"
                              " compressed.");
        return;
    }

    if (format->hasDepth)
        return mContext->ErrorInvalidOperation("generateMipmap: Depth textures are not supported");

    // Done with validation. Do the operation.

    mContext->MakeContextCurrent();
    gl::GLContext* gl = mContext->gl;

    if (gl->WorkAroundDriverBugs()) {
        // bug 696495 - to work around failures in the texture-mips.html test on various drivers, we
        // set the minification filter before calling glGenerateMipmap. This should not carry a significant performance
        // overhead so we do it unconditionally.
        //
        // note that the choice of GL_NEAREST_MIPMAP_NEAREST really matters. See Chromium bug 101105.
        gl->fTexParameteri(texTarget.get(), LOCAL_GL_TEXTURE_MIN_FILTER,
                           LOCAL_GL_NEAREST_MIPMAP_NEAREST);
        gl->fGenerateMipmap(texTarget.get());
        gl->fTexParameteri(texTarget.get(), LOCAL_GL_TEXTURE_MIN_FILTER,
                           mMinFilter.get());
    } else {
        gl->fGenerateMipmap(texTarget.get());
    }

    // Record the results.
    PopulateMipChain(mBaseMipmapLevel, mMaxMipmapLevel);
}

JS::Value
WebGLTexture::GetTexParameter(TexTarget texTarget, GLenum pname)
{
    mContext->MakeContextCurrent();

    GLint i = 0;
    GLfloat f = 0.0f;

    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
    case LOCAL_GL_TEXTURE_MAG_FILTER:
    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_BASE_LEVEL:
    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
    case LOCAL_GL_TEXTURE_COMPARE_MODE:
    case LOCAL_GL_TEXTURE_IMMUTABLE_FORMAT:
    case LOCAL_GL_TEXTURE_IMMUTABLE_LEVELS:
    case LOCAL_GL_TEXTURE_MAX_LEVEL:
    case LOCAL_GL_TEXTURE_SWIZZLE_A:
    case LOCAL_GL_TEXTURE_SWIZZLE_B:
    case LOCAL_GL_TEXTURE_SWIZZLE_G:
    case LOCAL_GL_TEXTURE_SWIZZLE_R:
    case LOCAL_GL_TEXTURE_WRAP_R:
        mContext->gl->fGetTexParameteriv(texTarget.get(), pname, &i);
        return JS::NumberValue(uint32_t(i));

    case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
    case LOCAL_GL_TEXTURE_MAX_LOD:
    case LOCAL_GL_TEXTURE_MIN_LOD:
        mContext->gl->fGetTexParameterfv(texTarget.get(), pname, &f);
        return JS::NumberValue(float(f));

    default:
        MOZ_CRASH("Unhandled pname.");
    }
}

bool
WebGLTexture::IsTexture() const
{
    return HasEverBeenBound() && !IsDeleted();
}

// Here we have to support all pnames with both int and float params.
// See this discussion:
//   https://www.khronos.org/webgl/public-mailing-list/archives/1008/msg00014.html
void
WebGLTexture::TexParameter(TexTarget texTarget, GLenum pname, GLint* maybeIntParam,
                           GLfloat* maybeFloatParam)
{
    MOZ_ASSERT(maybeIntParam || maybeFloatParam);

    GLint   intParam   = maybeIntParam   ? *maybeIntParam   : GLint(*maybeFloatParam);
    GLfloat floatParam = maybeFloatParam ? *maybeFloatParam : GLfloat(*maybeIntParam);

    bool paramBadEnum = false;
    bool paramBadValue = false;

    switch (pname) {
    case LOCAL_GL_TEXTURE_BASE_LEVEL:
    case LOCAL_GL_TEXTURE_MAX_LEVEL:
        if (!mContext->IsWebGL2())
            return mContext->ErrorInvalidEnumInfo("texParameter: pname", pname);

        if (intParam < 0) {
            paramBadValue = true;
            break;
        }

        InvalidateFakeBlackCache();

        if (pname == LOCAL_GL_TEXTURE_BASE_LEVEL)
            mBaseMipmapLevel = intParam;
        else
            mMaxMipmapLevel = intParam;

        ClampLevelBaseAndMax();

        break;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
        if (!mContext->IsWebGL2())
            return mContext->ErrorInvalidEnumInfo("texParameter: pname", pname);

        InvalidateFakeBlackCache();

        paramBadValue = (intParam != LOCAL_GL_NONE &&
                         intParam != LOCAL_GL_COMPARE_REF_TO_TEXTURE);
        break;

    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        if (!mContext->IsWebGL2())
            return mContext->ErrorInvalidEnumInfo("texParameter: pname", pname);

        InvalidateFakeBlackCache();

        switch (intParam) {
        case LOCAL_GL_LEQUAL:
        case LOCAL_GL_GEQUAL:
        case LOCAL_GL_LESS:
        case LOCAL_GL_GREATER:
        case LOCAL_GL_EQUAL:
        case LOCAL_GL_NOTEQUAL:
        case LOCAL_GL_ALWAYS:
        case LOCAL_GL_NEVER:
            break;

        default:
            paramBadValue = true;
        }
        break;

    case LOCAL_GL_TEXTURE_MIN_FILTER:
        switch (intParam) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
        case LOCAL_GL_NEAREST_MIPMAP_NEAREST:
        case LOCAL_GL_LINEAR_MIPMAP_NEAREST:
        case LOCAL_GL_NEAREST_MIPMAP_LINEAR:
        case LOCAL_GL_LINEAR_MIPMAP_LINEAR:
            InvalidateFakeBlackCache();
            mMinFilter = intParam;
            break;

        default:
            paramBadEnum = true;
        }
        break;

    case LOCAL_GL_TEXTURE_MAG_FILTER:
        switch (intParam) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
            InvalidateFakeBlackCache();
            mMagFilter = intParam;
            break;

        default:
            paramBadEnum = true;
        }
        break;

    case LOCAL_GL_TEXTURE_WRAP_S:
        switch (intParam) {
        case LOCAL_GL_CLAMP_TO_EDGE:
        case LOCAL_GL_MIRRORED_REPEAT:
        case LOCAL_GL_REPEAT:
            InvalidateFakeBlackCache();
            mWrapS = intParam;
            break;

        default:
            paramBadEnum = true;
        }
        break;

    case LOCAL_GL_TEXTURE_WRAP_T:
        switch (intParam) {
        case LOCAL_GL_CLAMP_TO_EDGE:
        case LOCAL_GL_MIRRORED_REPEAT:
        case LOCAL_GL_REPEAT:
            InvalidateFakeBlackCache();
            mWrapT = intParam;
            break;

        default:
            paramBadEnum = true;
        }
        break;

    case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
        if (!mContext->IsExtensionEnabled(WebGLExtensionID::EXT_texture_filter_anisotropic))
            return mContext->ErrorInvalidEnumInfo("texParameter: pname", pname);

        if (maybeFloatParam && floatParam < 1.0f)
            paramBadValue = true;
        else if (maybeIntParam && intParam < 1)
            paramBadValue = true;

        break;

    default:
        return mContext->ErrorInvalidEnumInfo("texParameter: pname", pname);
    }

    if (paramBadEnum) {
        if (maybeIntParam) {
            mContext->ErrorInvalidEnum("texParameteri: pname 0x%04x: Invalid param"
                                       " 0x%04x.",
                                       pname, intParam);
        } else {
            mContext->ErrorInvalidEnum("texParameterf: pname 0x%04x: Invalid param %g.",
                                       pname, floatParam);
        }
        return;
    }

    if (paramBadValue) {
        if (maybeIntParam) {
            mContext->ErrorInvalidValue("texParameteri: pname 0x%04x: Invalid param %i"
                                        " (0x%x).",
                                        pname, intParam, intParam);
        } else {
            mContext->ErrorInvalidValue("texParameterf: pname 0x%04x: Invalid param %g.",
                                        pname, floatParam);
        }
        return;
    }

    mContext->MakeContextCurrent();
    if (maybeIntParam)
        mContext->gl->fTexParameteri(texTarget.get(), pname, intParam);
    else
        mContext->gl->fTexParameterf(texTarget.get(), pname, floatParam);
}

////////////////////////////////////////////////////////////////////////////////

GLenum
TexImage(gl::GLContext* gl, TexImageTarget target, GLint level,
         const webgl::TexImageInfo* texImageInfo, GLsizei width, GLsizei height,
         GLsizei depth, const void* data)
{
    const GLenum& internalFormat = texImageInfo->internalFormat;
    const GLint border = 0;
    const GLenum& unpackFormat = texImageInfo->unpackFormat;
    const GLenum& unpackType = texImageInfo->unpackType;

    gl::GLContext::LocalErrorScope errorScope(*gl);

    switch (GLenum(target)) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        MOZ_ASSERT(depth == 1);
        gl->fTexImage2D(GLenum(target), level, internalFormat, width, height, border,
                        unpackFormat, unpackType, data);
        break;

    case LOCAL_GL_TEXTURE_3D:
    case LOCAL_GL_TEXTURE_2D_ARRAY:
        gl->fTexImage3D(GLenum(target), level, internalFormat, width, height, depth,
                        border, unpackFormat, unpackType, data);
        break;

    default:
        MOZ_CRASH('bad `target`');
    }

    return errorScope.GetError();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLTexture)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTexture, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTexture, Release)

} // namespace mozilla
