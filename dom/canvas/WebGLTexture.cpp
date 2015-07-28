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
#include "ScopedGLHelpers.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLTexelConversions.h"

namespace mozilla {

/*static*/ WebGLTexture::ImageInfo WebGLTexture::sUndefinedImageInfo;

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
    mImageInfoMap.clear();

    mContext->MakeContextCurrent();
    mContext->gl->fDeleteTextures(1, &mGLName);

    LinkedListElement<WebGLTexture>::removeFrom(mContext->mTextures);
}

// TODO: De-interleave WebGLTexture and WebGLTexture::ImageInfo func definitions.
size_t
WebGLTexture::ImageInfo::MemoryUsage() const
{
    if (!IsDefined())
        return 0;

    size_t bitsPerTexel = GetBitsPerTexel(mFormat);
    return size_t(mWidth) * size_t(mHeight) * size_t(mDepth) * bitsPerTexel / 8;
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

static inline size_t
MaxMipmapLevelsForSize(const WebGLTexture::ImageInfo& info)
{
    GLsizei size = std::max(std::max(info.mWidth, info.mHeight), info.mDepth);

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
    const ImageInfo& baseImageInfo = ImageInfoAtFace(0, mBaseMipmapLevel);

    // Reference dimensions based on the current level.
    size_t refWidth = baseImageInfo.mWidth;
    size_t refHeight = baseImageInfo.mHeight;
    size_t refDepth = baseImageInfo.mDepth;

    for (size_t level = mBaseMipmapLevel; level < mMaxMipmapLevel; level++) {
        // "A cube map texture is mipmap complete if each of the six texture images,
        //  considered individually, is mipmap complete."

        for (size_t face = 0; face < mFaceCount; face++) {
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

        refWidth  = std::max(size_t(1), refWidth  / 2);
        refHeight = std::max(size_t(1), refHeight / 2);
        refDepth  = std::max(size_t(1), refDepth  / 2);
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

    const ImageInfo& reference = ImageInfoAtFace(0, mBaseMipmapLevel);
    auto refWidth = reference.mWidth;
    auto refFormat = reference.mFormat;

    if (!refWidth || !refFormat.get())
        return false;

    for (size_t face = 0; face < mFaceCount; face++) {
        const ImageInfo& cur = ImageInfoAtFace(face, mBaseMipmapLevel);

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
/*
void
WebGLTexture::SetImageInfo(TexImageTarget texImageTarget, GLint level,
                           GLsizei width, GLsizei height, GLsizei depth,
                           TexInternalFormat effectiveInternalFormat,
                           WebGLImageDataStatus status)
{
    MOZ_ASSERT(depth == 1 || texImageTarget == LOCAL_GL_TEXTURE_3D);
    MOZ_ASSERT(TexImageTargetToTexTarget(texImageTarget) == mTarget);

    InvalidateStatusOfAttachedFBs();

    EnsureMaxLevelWithCustomImagesAtLeast(level);

    ImageInfoAt(texImageTarget, level) = ImageInfo(width, height, depth,
                                                   effectiveInternalFormat,
                                                   status);

    if (level > 0)
        SetCustomMipmap();

    SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);
}
*/

static bool
FormatSupportsFiltering(WebGLContext* webgl, TexInternalFormat format)
{
    TexType type = TypeFromInternalFormat(format);

    if (type == LOCAL_GL_FLOAT)
        return webgl->IsExtensionEnabled(WebGLExtensionID::OES_texture_float_linear);

    MOZ_ASSERT(type != LOCAL_GL_HALF_FLOAT_OES);
    if (type == LOCAL_GL_HALF_FLOAT)
        return webgl->IsExtensionEnabled(WebGLExtensionID::OES_texture_half_float_linear);

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
        auto format = baseImageInfo.mFormat;

        // "* The effective internal format specified for the texture arrays is a sized
        //    internal color format that is not texture-filterable, and either the
        //    magnification filter is not NEAREST or the minification filter is neither
        //    NEAREST nor NEAREST_MIPMAP_NEAREST."
        // Since all (GLES3) unsized color formats are filterable just like their sized
        // equivalents, we don't have to care whether its sized or not.
        if (FormatHasColor(format) && !FormatSupportsFiltering(mContext, format)) {
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
        // [1]: This sounds suspect to [jgilbert]. It seems like this should instead be
        //      "is not NONE".
        if (FormatHasDepth(format) && mTexCompareMode == LOCAL_GL_NONE) {
            *out_reason = "Because minification or magnification filtering is not NEAREST"
                          " or NEAREST_MIPMAP_NEAREST, and the texture's format is a"
                          " sized depth or depth-stencil format, its TEXTURE_COMPARE_MODE"
                          " must be NONE.";
            return false;
        }
    }

    return true;
}


size_t
WebGLTexture::MaxEffectiveMipmapLevel() const
{
    const bool requiresMipmap = (mMinFilter != LOCAL_GL_NEAREST &&
                                 mMinFilter != LOCAL_GL_LINEAR);
    if (!requiresMipmap)
        return mBaseMipmapLevel;

    const auto& imageInfo = BaseImageInfo();
    MOZ_ASSERT(imageInfo.IsDefined());

    size_t maxLevelBySize = mBaseMipmapLevel + imageInfo.MaxMipmapLevels() - 1;
    return std::min(maxLevelBySize, mMaxMipmapLevel);
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

    const size_t maxLevel = MaxEffectiveMipmapLevel();
    MOZ_ASSERT(mBaseMipmapLevel <= maxLevel);
    for (size_t level = mBaseMipmapLevel; level <= maxLevel; level++) {
        for (size_t face = 0; face < mFaceCount; face++) {
            const auto& cur = ImageInfoAtFace(face, level);
            if (cur.mHasUninitData)
                hasUninitializedData = true;
            else
                hasInitializedData = true;
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

    for (size_t level = mBaseMipmapLevel; level <= maxLevel; level++) {
        for (size_t face = 0; face < mFaceCount; face++) {
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
ClearWithTempFB(WebGLContext* webgl, GLuint tex,
                TexImageTarget texImageTarget, GLint level,
                TexInternalFormat baseInternalFormat,
                GLsizei width, GLsizei height)
{
    MOZ_ASSERT(texImageTarget == LOCAL_GL_TEXTURE_2D);

    gl::GLContext* gl = webgl->GL();
    MOZ_ASSERT(gl->IsCurrent());

    gl::ScopedFramebuffer fb(gl);
    gl::ScopedBindFramebuffer autoFB(gl, fb.FB());
    GLbitfield mask = 0;

    switch (baseInternalFormat.get()) {
    case LOCAL_GL_LUMINANCE:
    case LOCAL_GL_LUMINANCE_ALPHA:
    case LOCAL_GL_ALPHA:
    case LOCAL_GL_RGB:
    case LOCAL_GL_RGBA:
    case LOCAL_GL_BGR:
    case LOCAL_GL_BGRA:
        mask = LOCAL_GL_COLOR_BUFFER_BIT;
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                  texImageTarget.get(), tex, level);
        break;
    case LOCAL_GL_DEPTH_COMPONENT32_OES:
    case LOCAL_GL_DEPTH_COMPONENT24_OES:
    case LOCAL_GL_DEPTH_COMPONENT16:
    case LOCAL_GL_DEPTH_COMPONENT:
        mask = LOCAL_GL_DEPTH_BUFFER_BIT;
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                  texImageTarget.get(), tex, level);
        break;

    case LOCAL_GL_DEPTH24_STENCIL8:
    case LOCAL_GL_DEPTH_STENCIL:
        mask = LOCAL_GL_DEPTH_BUFFER_BIT |
               LOCAL_GL_STENCIL_BUFFER_BIT;
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                  texImageTarget.get(), tex, level);
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                  texImageTarget.get(), tex, level);
        break;

    default:
        return false;
    }
    MOZ_ASSERT(mask);

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
        // Only GLES guarantees RGBA4.
        GLenum format = gl->IsGLES() ? LOCAL_GL_RGBA4 : LOCAL_GL_RGBA8;
        gl::ScopedBindRenderbuffer rbBinding(gl, rb.RB());
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

bool
WebGLTexture::EnsureInitializedImageData(uint8_t face, size_t level)
{
    ImageInfo& imageInfo = ImageInfoAtFace(face, level);
    MOZ_ASSERT(imageInfo.IsDefined());

    if (!imageInfo.mHasUninitData)
        return true;

    mContext->MakeContextCurrent();

    const TexImageTarget texImageTarget = TexImageTargetFromFace(mTarget, face);

    // Try to clear with glClear.
    if (mTarget == LOCAL_GL_TEXTURE_2D) {
        bool cleared = ClearWithTempFB(mContext, mGLName, texImageTarget, level,
                                       imageInfo.mFormat, imageInfo.mHeight,
                                       imageInfo.mWidth);
        if (cleared) {
            imageInfo.mHasUninitData = false;
            return true;
        }
    }

    auto imageFormat = imageInfo.mFormat;

    // That didn't work. Try uploading zeros then.
    size_t bitsPerTexel = GetBitsPerTexel(imageFormat);
    MOZ_ASSERT((bitsPerTexel % 8) == 0, "No compressed formats allowed. (yet)");
    size_t bytesPerTexel = bitsPerTexel / 8;

    const auto unpackAlignment = mContext->mPixelStoreUnpackAlignment;
    CheckedUint32 checked_byteLength = WebGLContext::GetImageSize(imageInfo.mHeight,
                                                                  imageInfo.mWidth,
                                                                  imageInfo.mDepth,
                                                                  bytesPerTexel,
                                                                  unpackAlignment);
    MOZ_ASSERT(checked_byteLength.isValid()); // Should have been checked earlier.

    size_t byteCount = checked_byteLength.value();

    UniquePtr<uint8_t> zeros((uint8_t*)calloc(1, byteCount));
    if (zeros == nullptr) {
        // Failed to allocate memory. Lose the context. Return OOM error.
        mContext->ForceLoseContext(true);
        mContext->ErrorOutOfMemory("EnsureInitializedImageData: Failed to alloc %u "
                                   "bytes to clear image target `%s` level `%d`.",
                                   byteCount, mContext->EnumName(texImageTarget.get()),
                                   level);
         return false;
    }

    gl::GLContext* gl = mContext->gl;
    gl::ScopedBindTexture autoBindTex(gl, mGLName, mTarget.get());

    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
    GLenum driverUnpackType = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl, imageFormat, &driverInternalFormat,
                                             &driverUnpackFormat, &driverUnpackType);

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
        gfxCriticalError() << "GL context GetAndFlushUnderlyingGLErrors " << gfx::hexa(error);
        printf_stderr("Error: 0x%4x\n", error);
        if (error != LOCAL_GL_OUT_OF_MEMORY) {
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

    imageInfo.mHasUninitData = false;
    return true;
}

template<typename T>
static T
Clamp(T val, T min, T max) {
    return std::max(min, std::min(val, max));
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
    mBaseMipmapLevel = Clamp(mBaseMipmapLevel, size_t(0), mImmutableLevelCount - 1);
    mMaxMipmapLevel = Clamp(mMaxMipmapLevel, mBaseMipmapLevel,
                            mImmutableLevelCount - 1);
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

    auto format = baseImageInfo.mFormat;
    if (IsTextureFormatCompressed(format)) {
        mContext->ErrorInvalidOperation("generateMipmap: Texture data at base level is"
                              " compressed.");
        return;
    }

    if ((IsGLDepthFormat(format) || IsGLDepthStencilFormat(format)))
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

void
WebGLTexture::PopulateMipChain(size_t baseLevel, size_t maxLevel)
{
    const ImageInfo& baseImageInfo = ImageInfoAtFace(0, baseLevel);
    MOZ_ASSERT(baseImageInfo.IsDefined());

    uint32_t refWidth = baseImageInfo.mWidth;
    uint32_t refHeight = baseImageInfo.mHeight;
    uint32_t refDepth = baseImageInfo.mDepth;

    for (size_t level = baseLevel; level <= maxLevel; level++) {
        const bool hasUninitData = false;
        const ImageInfo cur(baseImageInfo.mFormat, refWidth, refHeight, refDepth,
                            baseImageInfo.mHasUninitData);

        SetImageInfosAtLevel(level, cur);

        // Higher levels are unaffected.
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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLTexture)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTexture, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTexture, Release)

} // namespace mozilla
