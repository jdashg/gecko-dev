/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTexture.h"

#include <algorithm>
#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Scoped.h"
#include "ScopedGLHelpers.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLFormats.h"
#include "WebGLTexelConversions.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {

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
    , mFacesCount(0)
    , mMaxLevelWithCustomImages(0)
    , mHaveGeneratedMipmap(false)
    , mImmutable(false)
    , mBaseMipmapLevel(0)
    , mMaxMipmapLevel(1000)
    , mFakeBlackStatus(WebGLTextureFakeBlackStatus::IncompleteTexture)
{
    mContext->mTextures.insertBack(this);
}

void
WebGLTexture::Delete()
{
    mImageInfos.Clear();
    mContext->MakeContextCurrent();
    mContext->gl->fDeleteTextures(1, &mGLName);
    LinkedListElement<WebGLTexture>::removeFrom(mContext->mTextures);
}

size_t
WebGLTexture::ImageInfo::MemoryUsage() const
{
    if (mImageDataStatus == WebGLImageDataStatus::NoImageData)
        return 0;

    auto bytesPerPixel = mFormat->formatInfo->bytesPerPixel;
    return size_t(mWidth) * size_t(mHeight) * size_t(mDepth) * bytesPerPixel;
}

size_t
WebGLTexture::MemoryUsage() const
{
    if (IsDeleted())
        return 0;

    size_t ret = 0;
    for (size_t i = 0; i < mImageInfos.Length(); i++) {
        ret += mImageInfos[i].MemoryUsage();
    }
    return ret;
}

bool
WebGLTexture::IsMipmapComplete() const
{
    // GLES 3.0.4, p161

    // "* `level_base <= level_max`"
    if (mBaseMipmapLevel > mMaxMipmapLevel)
        return false;

    // Make a copy so we can modify it.
    ImageInfo reference = ImageInfoAtFace(0, mBaseMipmapLevel);

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

            if (cur.mWidth != reference.mWidth ||
                cur.mHeight != reference.mHeight ||
                cur.mDepth != reference.mDepth ||
                cur.mFormat != reference.mFormat)
            {
                return false;
            }
        }

        // GLES 3.0.4, p158:
        // "[...] until the last array is reached with dimension 1 x 1 x 1."
        if (reference.mWidth == 1 &&
            reference.mHeight == 1 &&
            reference.mDepth == 1)
        {
            break;
        }

        reference.mWidth  = std::max(1, reference.mWidth  / 2);
        reference.mHeight = std::max(1, reference.mHeight / 2);
        reference.mDepth  = std::max(1, reference.mDepth  / 2);
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

    const ImageInfo& reference = BaseImageInfoAtFace(0);
    auto refWidth = reference.mWidth;
    auto refFormat = reference.mFormat;

    if (!refWidth || !refFormat)
        return false;

    for (size_t face = 0; face < mFaceCount; face++) {
        const ImageInfo& cur = BaseImageInfoAtFace(face);

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

void
WebGLTexture::Bind(TexTarget texTarget)
{
    // This function should only be called by bindTexture(). It assumes that the
    // GL context is already current.

    bool firstTimeThisTextureIsBound = !HasEverBeenBound();

    if (firstTimeThisTextureIsBound) {
        mTarget = texTarget.get();
    } else if (texTarget != Target()) {
        mContext->ErrorInvalidOperation("bindTexture: This texture has already"
                                        " been bound to a different target.");
        // Very important to return here before modifying texture state! This
        // was the place when I lost a whole day figuring very strange "invalid
        // write" crashes.
        return;
    }

    mContext->gl->fBindTexture(texTarget.get(), mGLName);

    if (firstTimeThisTextureIsBound) {
        mFacesCount = (texTarget == LOCAL_GL_TEXTURE_CUBE_MAP) ? 6 : 1;
        EnsureMaxLevelWithCustomImagesAtLeast(0);
        SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);

        // Thanks to the WebKit people for finding this out: GL_TEXTURE_WRAP_R
        // is not present in GLES 2, but is present in GL and it seems as if for
        // cube maps we need to set it to GL_CLAMP_TO_EDGE to get the expected
        // GLES behavior.
        if (mTarget == LOCAL_GL_TEXTURE_CUBE_MAP && !mContext->gl->IsGLES()) {
            mContext->gl->fTexParameteri(texTarget.get(),
                                         LOCAL_GL_TEXTURE_WRAP_R,
                                         LOCAL_GL_CLAMP_TO_EDGE);
        }
    }
}

void
WebGLTexture::SetImageInfo(TexImageTarget texImageTarget, GLint level, GLsizei width,
                           GLsizei height, GLsizei depth,
                           const webgl::FormatUsageInfo* format,
                           WebGLImageDataStatus status)
{
    MOZ_ASSERT(depth == 1 || texImageTarget == LOCAL_GL_TEXTURE_3D);
    MOZ_ASSERT(TexImageTargetToTexTarget(texImageTarget) == mTarget);

    InvalidateStatusOfAttachedFBs();

    EnsureMaxLevelWithCustomImagesAtLeast(level);

    ImageInfoAt(texImageTarget, level) = ImageInfo(width, height, depth, format, status);

    if (level > 0)
        SetCustomMipmap();

    SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);
}

void
WebGLTexture::SetGeneratedMipmap()
{
    if (!mHaveGeneratedMipmap) {
        mHaveGeneratedMipmap = true;
        SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);
    }
}

void
WebGLTexture::SetCustomMipmap()
{
    if (mHaveGeneratedMipmap) {
        if (!IsMipmapRangeValid())
            return;

        // If we were in GeneratedMipmap mode and are now switching to
        // CustomMipmap mode, we now need to compute all the mipmap image info.
        ImageInfo imageInfo = ImageInfoAtFace(0, EffectiveBaseMipmapLevel());
        MOZ_ASSERT(mContext->IsWebGL2() || imageInfo.IsPowerOfTwo(),
                   "This texture is NPOT, so how could GenerateMipmap() ever"
                   " accept it?");

        size_t maxRelativeLevel = MipmapLevelsForSize(imageInfo);
        size_t maxLevel = EffectiveBaseMipmapLevel() + maxRelativeLevel;
        EnsureMaxLevelWithCustomImagesAtLeast(maxLevel);

        for (size_t level = EffectiveBaseMipmapLevel() + 1;
             level <= EffectiveMaxMipmapLevel(); ++level)
        {
            imageInfo.mWidth = std::max(imageInfo.mWidth / 2, 1);
            imageInfo.mHeight = std::max(imageInfo.mHeight / 2, 1);
            imageInfo.mDepth = std::max(imageInfo.mDepth / 2, 1);
            for (size_t face = 0; face < mFacesCount; ++face) {
                ImageInfoAtFace(face, level) = imageInfo;
            }
        }
    }
    mHaveGeneratedMipmap = false;
}

bool
WebGLTexture::AreAllLevel0ImageInfosEqual() const
{
    for (size_t face = 1; face < mFacesCount; ++face) {
        if (ImageInfoAtFace(face, 0) != ImageInfoAtFace(0, 0))
            return false;
    }
    return true;
}

bool
WebGLTexture::IsMipmapComplete() const
{
    MOZ_ASSERT(mTarget == LOCAL_GL_TEXTURE_2D ||
               mTarget == LOCAL_GL_TEXTURE_3D);
    return DoesMipmapHaveAllLevelsConsistentlyDefined(LOCAL_GL_TEXTURE_2D);
}

bool
WebGLTexture::IsCubeComplete() const
{
    MOZ_ASSERT(mTarget == LOCAL_GL_TEXTURE_CUBE_MAP);

    const ImageInfo& first = ImageInfoAt(LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                                         0);
    if (!first.IsPositive() || !first.IsSquare())
        return false;

    return AreAllLevel0ImageInfosEqual();
}

bool
WebGLTexture::IsMipmapCubeComplete() const
{
    // In particular, this checks that this is a cube map:
    if (!IsCubeComplete())
        return false;

    for (int i = 0; i < 6; i++) {
        const TexImageTarget face =
            TexImageTargetForTargetAndFace(LOCAL_GL_TEXTURE_CUBE_MAP, i);
        if (!DoesMipmapHaveAllLevelsConsistentlyDefined(face))
            return false;
    }
    return true;
}

bool
WebGLTexture::IsMipmapRangeValid() const
{
    // In ES3, if a texture is immutable, the mipmap levels are clamped.
    if (IsImmutable())
        return true;
    if (mBaseMipmapLevel > std::min(mMaxLevelWithCustomImages, mMaxMipmapLevel))
        return false;
    return true;
}

WebGLTextureFakeBlackStatus
WebGLTexture::ResolvedFakeBlackStatus()
{
    if (MOZ_LIKELY(mFakeBlackStatus != WebGLTextureFakeBlackStatus::Unknown))
        return mFakeBlackStatus;

    // Determine if the texture needs to be faked as a black texture.
    // See 3.8.2 Shader Execution in the OpenGL ES 2.0.24 spec, and 3.8.13 in
    // the OpenGL ES 3.0.4 spec.
    if (!IsMipmapRangeValid()) {
        mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
        return mFakeBlackStatus;
    }

    for (size_t face = 0; face < mFacesCount; ++face) {
        WebGLImageDataStatus status = ImageInfoAtFace(face, EffectiveBaseMipmapLevel()).mImageDataStatus;
        if (status == WebGLImageDataStatus::NoImageData) {
            // In case of undefined texture image, we don't print any message
            // because this is a very common and often legitimate case
            // (asynchronous texture loading).
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            return mFakeBlackStatus;
        }
    }

    const char preamble[] = "A texture is going to be rendered as if it were"
                            " black, as per the OpenGL ES 2.0.24 spec section"
                            " 3.8.2, because it";

    if (mTarget == LOCAL_GL_TEXTURE_2D ||
        mTarget == LOCAL_GL_TEXTURE_3D)
    {
        int dim = mTarget == LOCAL_GL_TEXTURE_2D ? 2 : 3;
        if (DoesMinFilterRequireMipmap()) {
            if (!IsMipmapComplete()) {
                mContext->GenerateWarning("%s is a %dD texture, with a"
                                          " minification filter requiring a"
                                          " mipmap, and is not mipmap complete"
                                          " (as defined in section 3.7.10).",
                                          preamble, dim);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!mContext->IsWebGL2() &&
                       !ImageInfoBase().IsPowerOfTwo())
            {
                mContext->GenerateWarning("%s is a %dD texture, with a"
                                          " minification filter requiring a"
                                          " mipmap, and either its width or"
                                          " height is not a power of two.",
                                          preamble, dim);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        } else {
            // No mipmap required here.
            if (!ImageInfoBase().IsPositive()) {
                mContext->GenerateWarning("%s is a %dD texture and its width or"
                                          " height is equal to zero.",
                                          preamble, dim);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!AreBothWrapModesClampToEdge() &&
                       !mContext->IsWebGL2() &&
                       !ImageInfoBase().IsPowerOfTwo())
            {
                mContext->GenerateWarning("%s is a %dD texture, with a"
                                          " minification filter not requiring a"
                                          " mipmap, with its width or height"
                                          " not a power of two, and with a wrap"
                                          " mode different from CLAMP_TO_EDGE.",
                                          preamble, dim);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
    } else  {
        // Cube map
        bool legalImageSize = true;
        if (!mContext->IsWebGL2()) {
            for (size_t face = 0; face < mFacesCount; ++face)
                legalImageSize &= ImageInfoAtFace(face, 0).IsPowerOfTwo();
        }

        if (DoesMinFilterRequireMipmap()) {
            if (!IsMipmapCubeComplete()) {
                mContext->GenerateWarning("%s is a cube map texture, with a"
                                          " minification filter requiring a"
                                          " mipmap, and is not mipmap cube"
                                          " complete (as defined in section"
                                          " 3.7.10).", preamble);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!legalImageSize) {
                mContext->GenerateWarning("%s is a cube map texture, with a"
                                          " minification filter requiring a"
                                          " mipmap, and either the width or the"
                                          " height of some level 0 image is not"
                                          " a power of two.", preamble);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
        else // no mipmap required
        {
            if (!IsCubeComplete()) {
                mContext->GenerateWarning("%s is a cube map texture, with a"
                                          " minification filter not requiring a"
                                          " mipmap, and is not cube complete"
                                          " (as defined in section 3.7.10).",
                                          preamble);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!AreBothWrapModesClampToEdge() && !legalImageSize) {
                mContext->GenerateWarning("%s is a cube map texture, with a"
                                          " minification filter not requiring a"
                                          " mipmap, with some level 0 image"
                                          " having width or height not a power"
                                          " of two, and with a wrap mode"
                                          " different from CLAMP_TO_EDGE.",
                                          preamble);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
    }

    TexType type = TypeFromInternalFormat(ImageInfoBase().mEffectiveInternalFormat);

    const char* badFormatText = nullptr;
    const char* extText = nullptr;

    if (type == LOCAL_GL_FLOAT &&
        !Context()->IsExtensionEnabled(WebGLExtensionID::OES_texture_float_linear))
    {
        badFormatText = "FLOAT";
        extText = "OES_texture_float_linear";
    } else if (type == LOCAL_GL_HALF_FLOAT &&
               !Context()->IsExtensionEnabled(WebGLExtensionID::OES_texture_half_float_linear))
    {
        badFormatText = "HALF_FLOAT";
        extText = "OES_texture_half_float_linear";
    }

    const char* badFilterText = nullptr;
    if (badFormatText) {
        if (mMinFilter == LOCAL_GL_LINEAR ||
            mMinFilter == LOCAL_GL_LINEAR_MIPMAP_LINEAR ||
            mMinFilter == LOCAL_GL_LINEAR_MIPMAP_NEAREST ||
            mMinFilter == LOCAL_GL_NEAREST_MIPMAP_LINEAR)
        {
            badFilterText = "minification";
        } else if (mMagFilter == LOCAL_GL_LINEAR) {
            badFilterText = "magnification";
        }
    }

    if (badFilterText) {
        mContext->GenerateWarning("%s is a texture with a linear %s filter,"
                                  " which is not compatible with format %s by"
                                  " default. Try enabling the %s extension, if"
                                  " supported.", preamble, badFilterText,
                                  badFormatText, extText);
        mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
    }

    // We have exhausted all cases of incomplete textures, where we would need opaque black.
    // We may still need transparent black in case of uninitialized image data.
    bool hasUninitializedImageData = false;
    for (size_t level = 0; level <= mMaxLevelWithCustomImages; ++level) {
        for (size_t face = 0; face < mFacesCount; ++face) {
            bool cur = (ImageInfoAtFace(face, level).mImageDataStatus == WebGLImageDataStatus::UninitializedImageData);
            hasUninitializedImageData |= cur;
        }
    }

    if (hasUninitializedImageData) {
        bool hasAnyInitializedImageData = false;
        for (size_t level = 0; level <= mMaxLevelWithCustomImages; ++level) {
            for (size_t face = 0; face < mFacesCount; ++face) {
                if (ImageInfoAtFace(face, level).mImageDataStatus == WebGLImageDataStatus::InitializedImageData) {
                    hasAnyInitializedImageData = true;
                    break;
                }
            }
            if (hasAnyInitializedImageData) {
                break;
            }
        }

        if (hasAnyInitializedImageData) {
            /* The texture contains some initialized image data, and some
             * uninitialized image data. In this case, we have no choice but to
             * initialize all image data now. Fortunately, in this case we know
             * that we can't be dealing with a depth texture per
             * WEBGL_depth_texture and ANGLE_depth_texture (which allow only one
             * image per texture) so we can assume that glTexImage2D is able to
             * upload data to images.
             */
            for (size_t level = 0; level <= mMaxLevelWithCustomImages; ++level)
            {
                for (size_t face = 0; face < mFacesCount; ++face) {
                    TexImageTarget imageTarget = TexImageTargetForTargetAndFace(mTarget,
                                                                                face);
                    const ImageInfo& imageInfo = ImageInfoAt(imageTarget, level);
                    if (imageInfo.mImageDataStatus == WebGLImageDataStatus::UninitializedImageData)
                    {
                        EnsureInitializedImageData(imageTarget, level);
                    }
                }
            }
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::NotNeeded;
        } else {
            // The texture only contains uninitialized image data. In this case,
            // we can use a black texture for it.
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::UninitializedImageData;
        }
    }

    // we have exhausted all cases where we do need fakeblack, so if the status is still unknown,
    // that means that we do NOT need it.
    if (mFakeBlackStatus == WebGLTextureFakeBlackStatus::Unknown) {
        mFakeBlackStatus = WebGLTextureFakeBlackStatus::NotNeeded;
    }

    MOZ_ASSERT(mFakeBlackStatus != WebGLTextureFakeBlackStatus::Unknown);
    return mFakeBlackStatus;
}

static bool
ClearByMask(WebGLContext* webgl, GLbitfield mask)
{
    gl::GLContext* gl = webgl->GL();
    MOZ_ASSERT(gl->IsCurrent());

    GLenum status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return false;

    bool colorAttachmentsMask[WebGLContext::kMaxColorAttachments] = { false };
    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        colorAttachmentsMask[0] = true;
    }

    webgl->ForceClearFramebufferWithDefaultValues(false, mask, colorAttachmentsMask);
    return true;
}

// `mask` from glClear.
static bool
ClearWithTempFB(WebGLContext* webgl, GLuint tex, TexImageTarget texImageTarget,
                GLint level, webgl::UnsizedFormat unsizedFormat, GLsizei width, GLsizei height)
{
    MOZ_ASSERT(texImageTarget == LOCAL_GL_TEXTURE_2D);

    gl::GLContext* gl = webgl->GL();
    MOZ_ASSERT(gl->IsCurrent());

    gl::ScopedFramebuffer fb(gl);
    gl::ScopedBindFramebuffer autoFB(gl, fb.FB());
    GLbitfield mask = 0;

    switch (unsizedFormat) {
    case webgl::UnsizedFormat::R:
    case webgl::UnsizedFormat::RG:
    case webgl::UnsizedFormat::RGB:
    case webgl::UnsizedFormat::RGBA:
    case webgl::UnsizedFormat::LA:
    case webgl::UnsizedFormat::L:
    case webgl::UnsizedFormat::A:
        mask = LOCAL_GL_COLOR_BUFFER_BIT;
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                  texImageTarget.get(), tex, level);
        break;

    case webgl::UnsizedFormat::D:
        mask = LOCAL_GL_DEPTH_BUFFER_BIT;
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                  texImageTarget.get(), tex, level);
        break;

    case webgl::UnsizedFormat::DS:
        mask = LOCAL_GL_DEPTH_BUFFER_BIT |
               LOCAL_GL_STENCIL_BUFFER_BIT;
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                  texImageTarget.get(), tex, level);
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                  texImageTarget.get(), tex, level);
        break;

    case webgl::UnsizedFormat::S:
        return false;
    }
    MOZ_ASSERT(mask);

    if (ClearByMask(webgl, mask))
        return true;

    // Failed to simply build an FB from the tex, but maybe it needs a
    // color buffer to be complete?

    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        // Nope, it already had one.
        return false;
    }

    gl::ScopedRenderbuffer rb(gl);
    {
        // Only GLES guarantees RGBA4, and doesn't guarantee RGBA8.
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


bool
WebGLTexture::EnsureInitializedImageData(TexImageTarget texImageTarget, GLint level)
{
    MOZ_ASSERT(!mImmutable);
    ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    return imageInfo.EnsureInitializedImageData(mContext, texImageTarget, level);
}

bool
WebGLContext::GetDriverUnpackFormatType(const webgl::FormatUsageInfo* formatUsage,
                                        GLenum* const out_driverUnpackFormat,
                                        GLenum* const out_driverUnpackType)
{
    webgl::UnpackTuple& validUnpack = formatUsage->RandomValidUnpack();

    GLenum driverUnpackFormat = validUnpack.format;
    GLenum driverUnpackType = validUnpack.type;

    switch (driverUnpackType) {
    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
        if (!gl->IsSupported(gl::GLFeature::texture_half_float))
            return false;

        if (gl->IsExtensionSupported(gl::GLContext::OES_texture_half_float))
            driverUnpackType = LOCAL_GL_HALF_FLOAT_OES;
        else
            driverUnpackType = LOCAL_GL_HALF_FLOAT;

        break;
    }


}


void
DriverFormatsFromEffectiveInternalFormat(gl::GLContext* gl,
                                         TexInternalFormat effectiveinternalformat,
                                         GLenum* const out_driverInternalFormat,
                                         GLenum* const out_driverFormat,
                                         GLenum* const out_driverType)
{
    MOZ_ASSERT(out_driverInternalFormat);
    MOZ_ASSERT(out_driverFormat);
    MOZ_ASSERT(out_driverType);

    TexInternalFormat unsizedinternalformat = LOCAL_GL_NONE;
    TexType type = LOCAL_GL_NONE;

    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(effectiveinternalformat,
                                                            &unsizedinternalformat,
                                                            &type);

    // driverType: almost always the generic type that we just got, except on ES
    // we must replace HALF_FLOAT by HALF_FLOAT_OES
    GLenum driverType = type.get();
    if (gl->IsGLES() && type == LOCAL_GL_HALF_FLOAT)
        driverType = LOCAL_GL_HALF_FLOAT_OES;

    // driverFormat: always just the unsized internalformat that we just got
    GLenum driverFormat = unsizedinternalformat.get();

    // driverInternalFormat: almost always the same as driverFormat, but on desktop GL,
    // in some cases we must pass a different value. On ES, they are equal by definition
    // as it is an error to pass internalformat!=format.
    GLenum driverInternalFormat = driverFormat;
    if (gl->IsCompatibilityProfile()) {
        // Cases where desktop OpenGL requires a tweak to 'format'
        if (driverFormat == LOCAL_GL_SRGB)
            driverFormat = LOCAL_GL_RGB;
        else if (driverFormat == LOCAL_GL_SRGB_ALPHA)
            driverFormat = LOCAL_GL_RGBA;

        // WebGL2's new formats are not legal values for internalformat,
        // as using unsized internalformat is deprecated.
        if (driverFormat == LOCAL_GL_RED ||
            driverFormat == LOCAL_GL_RG ||
            driverFormat == LOCAL_GL_RED_INTEGER ||
            driverFormat == LOCAL_GL_RG_INTEGER ||
            driverFormat == LOCAL_GL_RGB_INTEGER ||
            driverFormat == LOCAL_GL_RGBA_INTEGER)
        {
            driverInternalFormat = effectiveinternalformat.get();
        }

        // Cases where desktop OpenGL requires a sized internalformat,
        // as opposed to the unsized internalformat that had the same
        // GLenum value as 'format', in order to get the precise
        // semantics that we want. For example, for floating-point formats,
        // we seem to need a sized internalformat to get non-clamped floating
        // point texture sampling. Can't find the spec reference for that,
        // but that's at least the case on my NVIDIA driver version 331.
        if (unsizedinternalformat == LOCAL_GL_DEPTH_COMPONENT ||
            unsizedinternalformat == LOCAL_GL_DEPTH_STENCIL ||
            type == LOCAL_GL_FLOAT ||
            type == LOCAL_GL_HALF_FLOAT)
        {
            driverInternalFormat = effectiveinternalformat.get();
        }
    }

    // OpenGL core profile removed texture formats ALPHA, LUMINANCE and LUMINANCE_ALPHA
    if (gl->IsCoreProfile()) {
        switch (driverFormat) {
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
            driverInternalFormat = driverFormat = LOCAL_GL_RED;
            break;

        case LOCAL_GL_LUMINANCE_ALPHA:
            driverInternalFormat = driverFormat = LOCAL_GL_RG;
            break;
        }
    }

    *out_driverInternalFormat = driverInternalFormat;
    *out_driverFormat = driverFormat;
    *out_driverType = driverType;
}

void
WebGLContext::GetDriverFormats(GLenum internalFormat, GLenum unpackFormat,
                               GLenum unpackType, GLenum* const out_driverInternalFormat,
                               GLenum* const out_driverUnpackFormat,
                               GLenum* const out_driverUnpackType)
{
    GLenum driverInternalFormat = internalFormat;
    GLenum driverUnpackFormat = unpackFormat;
    GLenum driverUnpackType = unpackType;

    if (gl->IsCoreProfile()) {
        switch (driverInternalFormat) {
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
            driverInternalFormat = LOCAL_GL_RED;
            break;

        case LOCAL_GL_LUMINANCE_ALPHA:
            driverInternalFormat = LOCAL_GL_RG;
            break;
        }

        switch (driverUnpackFormat) {
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
            driverUnpackFormat = LOCAL_GL_RED;
            break;

        case LOCAL_GL_LUMINANCE_ALPHA:
            driverUnpackFormat = LOCAL_GL_RG;
            break;
        }
    }

    switch (driverUnpackType) {
    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
        MOZ_ASSERT(gl->IsSupported(gl::GLFeature::texture_half_float));

        if (gl->IsExtensionSupported(gl::GLContext::OES_texture_half_float))
            driverUnpackType = LOCAL_GL_HALF_FLOAT_OES;
        else
            driverUnpackType = LOCAL_GL_HALF_FLOAT;

        break;
    }

    *out_driverInternalFormat = driverInternalFormat;
    *out_driverUnpackFormat = driverUnpackFormat;
    *out_driverUnpackType = driverUnpackType;
}


bool
WebGLTexture::ImageInfo::ClearContents(WebGLContext* webgl, TexImageTarget texImageTarget,
                                       GLint level)
{
    if (mImageDataStatus == WebGLImageDataStatus::InitializedImageData)
        return true;
    MOZ_ASSERT(mImageDataStatus == UninitializedImageData);

    gl::GLContext* gl = webgl->gl;

    gl->MakeCurrent();

    const webgl::FormatInfo* format = imageInfo.mFormat->formatInfo;

    // Try to clear with glClear.
    if (imageTarget == LOCAL_GL_TEXTURE_2D) {
        bool cleared = ClearWithTempFB(webgl, mGLName, texImageTarget, level,
                                       format->unsizedFormat, mHeight, mWidth);
        if (cleared) {
            mImageDataStatus = WebGLImageDataStatus::InitializedImageData;
            return true;
        }
    }

    webgl::UnpackTuple& validUnpack = mFormat->RandomValidUnpack();
    GLenum unpackFormat = validUnpack.format;
    GLenum unpackType = validUnpack.type;

    GLenum internalFormat = 0; // unused
    GLenum driverInternalFormat; // unused
    GLenum driverUnpackFormat;
    GLenum driverUnpackType;
    GetDriverFormats(internalFormat, unpackFormat, unpackType, &driverInternalFormat,
                     &driverUnpackFormat, &driverUnpackType);

    const webgl::FormatInfo* driverUnpackFormat = GetInfoByUnpackTuple(driverUnpackFormat,
                                                                       driverUnpackType);
    if (!driverUnpackFormat) {
        NS_ERROR("Failed to lookup FormatInfo for driver unpackFormat/Type.");
        MOZ_ASSERT(false);

        webgl->ForceLoseContext(true);
        return false;
    }

    // That didn't work. Try uploading zeros then.
    const auto bytesPerPixel = driverUnpackFormat->bytesPerPixel;
    const auto rowByteAlignment = 8; // Largest possible alignment is a decent default for
                                     // speed.
    const auto maybeStridePixelsPerRow = 0;
    const auto maybeStrideRowsPerImage = 0;
    const auto skipPixelsPerRow = 0;
    const auto skipRowsPerImage = 0;
    const auto skipImages = 0;
    const auto usedPixelsPerRow = mWidth;
    const auto usedRowsPerImage = mHeight;
    const auto usedImages = mDepth;

    uint32_t bytesNeeded;
    if (!GetPackedSizeForUnpack(bytesPerPixel, rowByteAlignment,
                                maybeStridePixelsPerRow, maybeStrideRowsPerImage,
                                skipPixelsPerRow, skipRowsPerImage, skipImages,
                                usedPixelsPerRow, usedRowsPerImage, usedImages,
                                &bytesNeeded)
    {
        MOZ_ASSERT(false, "Packed size for needed zeros overflowed.");
        NS_ERROR("Packed size for needed zeros overflowed.");

        webgl->ForceLoseContext(true);
        return false;
    }

    UniquePtr<uint8_t> zeros = (uint8_t*)calloc(1, bytesNeeded);
    if (!zeros) {
        // Failed to allocate memory. Lose the context. Return OOM error.
        NS_WARNING("Failed to calloc for WebGLTexture::ImageInfo::ClearContents.");
        webgl->ForceLoseContext(true);
        return false;
    }

    gl::ScopedBindTexture autoBindTex(gl, mGLName, mTarget);

    gl::GLContext::LocalErrorScope errorScope(*gl);

    switch (texImageTarget.get()) {
    case LOCAL_GL_TEXTURE_3D:
        gl->fTexSubImage3D(texImageTarget.get(), level, 0, 0, 0, mWidth, mHeight, mDepth,
                           driverUnpackFormat, driverUnpackType, zeros.get());
        break;

    default:
        gl->fTexSubImage2D(texImageTarget.get(), level, 0, 0, 0, mWidth, mHeight,
                           driverUnpackFormat, driverUnpackType, zeros.get());
        break;
    }

    GLenum error = errorScope.GetError();
    if (error) {
        // Should only be OUT_OF_MEMORY. Anyway, there's no good way to recover
        // from this here.
        if (error == LOCAL_GL_OUT_OF_MEMORY) {
            // Out-of-memory uploading pixels to GL. Lose context and report OOM.
            webgl->ForceLoseContext(true);
            webgl->ErrorOutOfMemory("EnsureNoUninitializedImageData: Failed to "
                                    "upload texture of width: %u, height: %u, "
                                    "depth: %u to target %s level %d.",
                                    mWidth, mHeight, mDepth,
                                    mContext->EnumName(texImageTarget.get()), level);
            return false;
        }


        // Errors on texture upload have been related to video
        // memory exposure in the past, which is a security issue.
        // Force loss of context.
        webgl->ForceLoseContext(true);
        gfxCriticalError() << "While trying to upload zeros, unexpected error: 0x"
                           << gfx::hexa(error);
        return false;
    }

    mImageDataStatus = WebGLImageDataStatus::InitializedImageData;
    return true;
}

void
WebGLTexture::SetFakeBlackStatus(WebGLTextureFakeBlackStatus x)
{
    mFakeBlackStatus = x;
    mContext->SetFakeBlackStatus(WebGLContextFakeBlackStatus::Unknown);
}

void
WebGLTexture::GenerateMipmap(TexTarget target)
{
    if (mBaseMipmapLevel > mMaxMipmapLevel) {
        ErrorInvalidOperation("generateMipmap: Texture does not have a valid mipmap"
                              " range.");
        return;
    }

    if (IsCubeMap() && !IsCubeComplete()) {
        ErrorInvalidOperation("generateMipmap: Cube maps must be \"cube complete\".");
        return;
    }

    const ImageInfo& baseImageInfo = tex->BaseImageInfo();
    if (!baseImageInfo.IsDefined()) {
        ErrorInvalidOperation("generateMipmap: The base level of the texture is not"
                              " defined.");
        return;
    }

    if (!IsWebGL2() && !baseImageInfo.IsPowerOfTwo()) {
        ErrorInvalidOperation("generateMipmap: The base level of the texture does not"
                              " have power-of-two dimensions.");
        return;
    }

    auto formatUsage = baseImageInfo.Format();
    auto formatInfo = formatUsage->formatInfo;
    if (formatInfo->compression) {
        ErrorInvalidOperation("generateMipmap: Texture data at base level is"
                              " compressed.");
        return;
    }

    if (formatInfo->hasDepth)
        return ErrorInvalidOperation("generateMipmap: Depth textures are not supported");

    // Done with validation. Do the operation.

    MakeContextCurrent();

    if (gl->WorkAroundDriverBugs()) {
        // bug 696495 - to work around failures in the texture-mips.html test on various drivers, we
        // set the minification filter before calling glGenerateMipmap. This should not carry a significant performance
        // overhead so we do it unconditionally.
        //
        // note that the choice of GL_NEAREST_MIPMAP_NEAREST really matters. See Chromium bug 101105.
        gl->fTexParameteri(target.get(), LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST_MIPMAP_NEAREST);
        gl->fGenerateMipmap(target.get());
        gl->fTexParameteri(target.get(), LOCAL_GL_TEXTURE_MIN_FILTER, tex->MinFilter().get());
    } else {
        gl->fGenerateMipmap(target.get());
    }

    // Record the results.

    // Make a copy so we can modify it.
    ImageInfo reference = baseImageInfo;

    for (size_t level = mBaseMipmapLevel; level < mMaxMipmapLevel; level++) {
        for (size_t face = 0; face < mFaceCount; face++) {
            ImageInfo& cur = ImageInfoAtFace(face, level);
            cur = reference;
        }

        // Higher levels are unaffected.
        if (reference.mWidth == 1 &&
            reference.mHeight == 1 &&
            reference.mDepth == 1)
        {
            break;
        }

        reference.mWidth  = std::max(1, reference.mWidth  / 2);
        reference.mHeight = std::max(1, reference.mHeight / 2);
        reference.mDepth  = std::max(1, reference.mDepth  / 2);
    }
}

void
WebGLTexture::TexParameter(TexTarget texTarget, GLenum pname, GLint* maybeIntParam,
                           GLfloat* maybeFloatParam)
{
    MOZ_ASSERT(maybeIntParam || maybeFloatParam);

    GLint   intParam   = maybeIntParam   ? *maybeIntParam   : GLint(*maybeFloatParam);
    GLfloat floatParam = maybeFloatParam ? *maybeFloatParam : GLfloat(*maybeIntParam);


    bool pnameAndParamAreIncompatible = false;
    bool paramValueInvalid = false;

    switch (pname) {
    case LOCAL_GL_TEXTURE_BASE_LEVEL:
    case LOCAL_GL_TEXTURE_MAX_LEVEL:
        if (!mContext->IsWebGL2())
            return ErrorInvalidEnumInfo("texParameter: pname", pname);

        if (intParam < 0) {
            paramValueInvalid = true;
            break;
        }
        if (pname == LOCAL_GL_TEXTURE_BASE_LEVEL)
            mBaseMipmapLevel = intParam;
        else
            mMaxMipmapLevel = intParam;

        InvalidateFakeBlack();
        break;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
        if (!IsWebGL2())
            return ErrorInvalidEnumInfo("texParameter: pname", pname);

        paramValueInvalid = (intParam != LOCAL_GL_NONE &&
                             intParam != LOCAL_GL_COMPARE_REF_TO_TEXTURE);
        break;

    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        if (!IsWebGL2())
            return ErrorInvalidEnumInfo("texParameter: pname", pname);

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
            pnameAndParamAreIncompatible = true;
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
            mMinFilter = intParam;
            InvalidateFakeBlack();
            break;

        default:
            pnameAndParamAreIncompatible = true;
        }
        break;

    case LOCAL_GL_TEXTURE_MAG_FILTER:
        switch (intParam) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
            mMagFilter = intParam;
            InvalidateFakeBlack();
            break;

        default:
            pnameAndParamAreIncompatible = true;
        }
        break;

    case LOCAL_GL_TEXTURE_WRAP_S:
        switch (intParam) {
        case LOCAL_GL_CLAMP_TO_EDGE:
        case LOCAL_GL_MIRRORED_REPEAT:
        case LOCAL_GL_REPEAT:
            mWrapS = intParam;
            InvalidateFakeBlack();
            break;

        default:
            pnameAndParamAreIncompatible = true;
        }
        break;

    case LOCAL_GL_TEXTURE_WRAP_T:
        switch (intParam) {
        case LOCAL_GL_CLAMP_TO_EDGE:
        case LOCAL_GL_MIRRORED_REPEAT:
        case LOCAL_GL_REPEAT:
            mWrapT = intParam;
            InvalidateFakeBlack();
            break;

        default:
            pnameAndParamAreIncompatible = true;
        }
        break;

    case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
        if (IsExtensionEnabled(WebGLExtensionID::EXT_texture_filter_anisotropic)) {
            if (floatParamPtr && floatParam < 1.0f)
                paramValueInvalid = true;
            else if (intParamPtr && intParam < 1)
                paramValueInvalid = true;
        } else {
            pnameAndParamAreIncompatible = true;
        }
        break;

    default:
        return ErrorInvalidEnumInfo("texParameter: pname", pname);
    }


    if (pnameAndParamAreIncompatible) {
        if (maybeIntParam) {
            ErrorInvalidEnum("texParameteri: pname 0x%04x and param %i (0x%x) are"
                             " mutually incompatible.",
                             pname, intParam, intParam);
        } else {
            ErrorInvalidEnum("texParameterf: pname 0x%04x and param %g are mutually"
                             " incompatible.",
                             pname, floatParam);
        }
        return;
    }

    if (paramValueInvalid) {
        if (maybeIntParam) {
            ErrorInvalidValue("texParameteri: pname 0x%04: param %i (0x%x) is invalid.",
                              pname, intParam, intParam);
        } else {
            ErrorInvalidValue("texParameterf: pname 0x%04: param %g is invalid.",
                              pname, floatParam);
        }
        return;
    }

    MakeContextCurrent();
    if (maybeIntParam)
        gl->fTexParameteri(texTarget.get(), pname, intParam);
    else
        gl->fTexParameterf(texTarget.get(), pname, floatParam);
}

void
WebGLTexture::ImageInfo::PrepareForSubImage(WebGLContext* webgl,
                                            TexImageTarget texImageTarget, GLint level,
                                            GLint xOffset, GLint yOffset, GLint zOffset,
                                            GLsizei width, GLsizei height, GLsizei depth)
{
    if (mStatus == WebGLImageDataStatus::InitializedImageData)
        return;
    MOZ_ASSERT(mImageDataStatus == UninitializedImageData);

    if (!xOffset && !yOffset && !zOffset &&
        width == mWidth &&
        height == mHeight &&
        depth == mDepth)
    {
        // We're going to fill it anyway.
        return;
    }

    ClearContents(webgl, texImageTarget, level);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLTexture)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTexture, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTexture, Release)

} // namespace mozilla
