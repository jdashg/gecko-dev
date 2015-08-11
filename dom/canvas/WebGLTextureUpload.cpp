/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTexture.h"

#include <algorithm>
#include "CanvasUtils.h"
#include "GLBlitHelper.h"
#include "GLContext.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Scoped.h"
#include "ScopedGLHelpers.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLTexelConversions.h"

namespace mozilla {

// Map R to A
static const GLenum kLegacyAlphaSwizzle[4] = {
    LOCAL_GL_ZERO, LOCAL_GL_ZERO, LOCAL_GL_ZERO, LOCAL_GL_RED
};
// Map R to RGB
static const GLenum kLegacyLuminanceSwizzle[4] = {
    LOCAL_GL_RED, LOCAL_GL_RED, LOCAL_GL_RED, LOCAL_GL_ONE
};
// Map R to RGB, G to A
static const GLenum kLegacyLuminanceAlphaSwizzle[4] = {
    LOCAL_GL_RED, LOCAL_GL_RED, LOCAL_GL_RED, LOCAL_GL_GREEN
};

static void
SetLegacyTextureSwizzle(gl::GLContext* gl, GLenum target, GLenum internalformat)
{
    if (!gl->IsCoreProfile())
        return;

    /* Only support swizzling on core profiles. */
    // Bug 1159117: Fix this.
    // MOZ_RELEASE_ASSERT(gl->IsSupported(gl::GLFeature::texture_swizzle));

    switch (internalformat) {
    case LOCAL_GL_ALPHA:
        gl->fTexParameteriv(target, LOCAL_GL_TEXTURE_SWIZZLE_RGBA,
                            (GLint*) kLegacyAlphaSwizzle);
        break;

    case LOCAL_GL_LUMINANCE:
        gl->fTexParameteriv(target, LOCAL_GL_TEXTURE_SWIZZLE_RGBA,
                            (GLint*) kLegacyLuminanceSwizzle);
        break;

    case LOCAL_GL_LUMINANCE_ALPHA:
        gl->fTexParameteriv(target, LOCAL_GL_TEXTURE_SWIZZLE_RGBA,
                            (GLint*) kLegacyLuminanceAlphaSwizzle);
        break;
    }
}

bool
DoesTargetMatchDimensions(WebGLContext* webgl, TexImageTarget target, uint8_t funcDims,
                          const char* funcName)
{
    uint8_t targetDims;
    switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        targetDims = 2;
        break;

    case LOCAL_GL_TEXTURE_3D:
        targetDims = 3;
        break;

    default:
        MOZ_CRASH("Unhandled texImageTarget.");
    }

    if (targetDims != funcDims) {
        webgl->ErrorInvalidEnum("%s: `target` must match function dimensions.", funcName);
        return false;
    }

    return true;
}

void
WebGLTexture::SpecifyTexStorage(GLsizei levels, TexInternalFormat internalFormat,
                                GLsizei width, GLsizei height, GLsizei depth)
{
    mImmutable = true;
    mImmutableLevelCount = levels;
    ClampLevelBaseAndMax();

    // GLES 3.0.4, p136:
    // "* Any existing levels that are not replaced are reset to their
    //    initial state."
    for (auto& cur : mImageInfoArr) {
        cur.Clear();
    }

    const bool isDataInitialized = false;
    const ImageInfo baseImageInfo(internalFormat, width, height, depth,
                                  isDataInitialized);

    SetImageInfoAtFace(0, 0, baseImageInfo);
    PopulateMipChain(0, mImmutableLevelCount);
}

void
WebGLTexture::CompressedTexImage2D(TexImageTarget texImageTarget,
                                   GLint level,
                                   GLenum internalFormat,
                                   GLsizei width, GLsizei height, GLint border,
                                   const dom::ArrayBufferViewOrSharedArrayBufferView& view)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::CompTexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    const char funcName[] = "compressedTexImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    if (!mContext->ValidateTexImage(texImageTarget.get(), level, internalFormat,
                          0, 0, 0, width, height, 0,
                          border, LOCAL_GL_NONE,
                          LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    size_t byteLength;
    void* data;
    js::Scalar::Type dataType;
    ComputeLengthAndData(view, &data, &byteLength, &dataType);

    if (!mContext->ValidateCompTexImageDataSize(level, internalFormat, width, height, byteLength, func, dims)) {
        return;
    }

    if (!mContext->ValidateCompTexImageSize(level, internalFormat, 0, 0, width, height, width, height, func, dims))
    {
        return;
    }

    if (mImmutable) {
        return mContext->ErrorInvalidOperation(
            "compressedTexImage2D: disallowed because the texture bound to "
            "this target has already been made immutable by texStorage2D");
    }

    mContext->MakeContextCurrent();
    gl::GLContext* gl = mContext->gl;
    gl->fCompressedTexImage2D(texImageTarget.get(), level, internalFormat, width, height, border, byteLength, data);

    const uint32_t depth = 1;
    const bool isDataInitialized = true;
    const ImageInfo imageInfo(internalFormat, width, height, depth, isDataInitialized);

    SetImageInfoAt(texImageTarget, level, imageInfo);
}

void
WebGLTexture::CompressedTexSubImage2D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                                      GLint yOffset, GLsizei width, GLsizei height,
                                      GLenum internalFormat,
                                      const dom::ArrayBufferViewOrSharedArrayBufferView& view)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::CompTexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    const char funcName[] = "compressedTexSubImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    if (!mContext->ValidateTexImage(texImageTarget.get(),
                          level, internalFormat,
                          xOffset, yOffset, 0,
                          width, height, 0,
                          0, LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    ImageInfo& levelInfo = ImageInfoAt(texImageTarget, level);

    if (internalFormat != levelInfo.mFormat) {
        return mContext->ErrorInvalidOperation("compressedTexImage2D: internalFormat does not match the existing image");
    }

    size_t byteLength;
    void* data;
    js::Scalar::Type dataType;
    ComputeLengthAndData(view, &data, &byteLength, &dataType);

    if (!mContext->ValidateCompTexImageDataSize(level, internalFormat, width, height, byteLength, func, dims))
        return;

    if (!mContext->ValidateCompTexImageSize(level, internalFormat,
                                  xOffset, yOffset,
                                  width, height,
                                  levelInfo.mWidth, levelInfo.mHeight,
                                  func, dims))
    {
        return;
    }

    if (!levelInfo.IsDataInitialized()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                uint32_t(width) == levelInfo.mWidth &&
                                uint32_t(height) == levelInfo.mHeight;
        if (coversWholeImage) {
            levelInfo.SetIsDataInitialized(true, this);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    mContext->MakeContextCurrent();
    gl::GLContext* gl = mContext->gl;
    gl->fCompressedTexSubImage2D(texImageTarget.get(), level, xOffset, yOffset, width, height, internalFormat, byteLength, data);
}

void
WebGLTexture::CopyTexSubImage2D_base(TexImageTarget texImageTarget, GLint level,
                                     GLenum rawInternalFormat,
                                     GLint xOffset, GLint yOffset, GLint x,
                                     GLint y, GLsizei width, GLsizei height, GLint border,
                                     bool sub)
{
    WebGLTexImageFunc func = sub
                             ? WebGLTexImageFunc::CopyTexSubImage
                             : WebGLTexImageFunc::CopyTexImage;
    WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;
    const char* info = InfoFrom(func, dims);

    // TODO: This changes with color_buffer_float. Reassess when the
    // patch lands.
    if (!mContext->ValidateTexImage(texImageTarget, level, rawInternalFormat,
                          xOffset, yOffset, 0,
                          width, height, 0,
                          border,
                          LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    TexInternalFormat internalFormat = rawInternalFormat;

    if (!mContext->mBoundReadFramebuffer)
        mContext->ClearBackbufferIfNeeded();

    mContext->MakeContextCurrent();
    gl::GLContext* gl = mContext->gl;

    if (mImmutable) {
        if (!sub) {
            return mContext->ErrorInvalidOperation("copyTexImage2D: disallowed because the texture bound to this target has already been made immutable by texStorage2D");
        }
    }

    TexInternalFormat srcFormat;
    uint32_t srcWidth;
    uint32_t srcHeight;
    if (!mContext->ValidateCurFBForRead("readPixels", &srcFormat, &srcWidth, &srcHeight))
        return;

    if (!mContext->ValidateCopyTexImage(srcFormat, internalFormat, func, dims))
        return;

    TexFormat intermediateFormat;
    TexType intermediateType;
    CopyTexImageIntermediateFormatAndType(srcFormat, &intermediateFormat,
                                          &intermediateType);

    TexInternalFormat destEffectiveFormat =
        EffectiveInternalFormatFromUnsizedInternalFormatAndType(internalFormat, intermediateType);

    // this should never fail, validation happened earlier.
    MOZ_ASSERT(destEffectiveFormat != LOCAL_GL_NONE);

    const bool widthOrHeightIsZero = (width == 0 || height == 0);
    if (gl->WorkAroundDriverBugs() &&
        sub && widthOrHeightIsZero)
    {
        // NV driver on Linux complains that CopyTexSubImage2D(level=0,
        // xOffset=0, yOffset=2, x=0, y=0, width=0, height=0) from a 300x150 FB
        // to a 0x2 texture. This a useless thing to do, but technically legal.
        // NV331.38 generates INVALID_VALUE.
        return mContext->DummyFramebufferOperation(info);
    }

    // check if the memory size of this texture may change with this call
    bool sizeMayChange = !sub;
    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    if (!sub && imageInfo.IsDefined()) {
        const uint32_t depth = 1;
        sizeMayChange = uint32_t(width) != imageInfo.mWidth ||
                        uint32_t(height) != imageInfo.mHeight ||
                        depth != imageInfo.mDepth ||
                        destEffectiveFormat != imageInfo.mFormat;
    }

    if (sizeMayChange)
        mContext->GetAndFlushUnderlyingGLErrors();

    if (CanvasUtils::CheckSaneSubrectSize(x, y, width, height, srcWidth, srcHeight)) {
        if (sub)
            gl->fCopyTexSubImage2D(texImageTarget.get(), level, xOffset, yOffset, x, y, width, height);
        else
            gl->fCopyTexImage2D(texImageTarget.get(), level, internalFormat.get(), x, y, width, height, 0);
    } else {
        // the rect doesn't fit in the framebuffer

        // first, we initialize the texture as black
        if (!sub) {
            mContext->GenerateWarning("%s: Source rect reaches outside the bounds of the"
                                      " source framebuffer. WebGL requires clearing this"
                                      " out-of-bounds data to zeros, which is slow.",
                                      info);

            // We use CopyTexImage to initialize to ensure we get the right internal
            // format in the driver.
            // We don't need to pass x and y, since we only need to width and height to be
            // right for the TexImage. In cases where x or y is 'tricky' (INT32_MIN), the
            // driver may have issues. (Seen on Win32 Try runs)
            gl->fCopyTexImage2D(texImageTarget.get(), level, internalFormat.get(), 0, 0,
                                width, height, 0);

            // In GLES, read pixels outside the FB bounds are undefined, so we'll need to
            // clear them outselves.
            const uint32_t depth = 1;
            const bool isDataInitialized = false;
            const ImageInfo imageInfo(destEffectiveFormat, width, height, depth,
                                      isDataInitialized);

            SetImageInfoAt(texImageTarget, level, imageInfo);

            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }

        // if we are completely outside of the framebuffer, we can exit now with our black texture
        if (   x >= int32_t(srcWidth)
            || x+width <= 0
            || y >= int32_t(srcHeight)
            || y+height <= 0)
        {
            // we are completely outside of range, can exit now with buffer filled with zeros
            return mContext->DummyFramebufferOperation(info);
        }

        GLint trimmedXOffset = xOffset;
        GLint trimmedYOffset = yOffset;
        GLint trimmedX = x;
        GLint trimmedY = y;
        GLsizei trimmedWidth = width;
        GLsizei trimmedHeight = height;

        if (x < 0) {
            GLint diff = 0 - x;
            MOZ_ASSERT(diff > 0);
            trimmedX += diff;
            trimmedXOffset += diff;
            trimmedWidth -= diff;
        }

        if (y < 0) {
            GLint diff = 0 - y;
            MOZ_ASSERT(diff > 0);
            trimmedY += diff;
            trimmedYOffset += diff;
            trimmedHeight -= diff;
        }

        if (x + width > GLint(srcWidth)) {
            GLint diff = x + width - GLint(srcWidth);
            MOZ_ASSERT(diff > 0);
            trimmedWidth -= diff;
        }

        if (y + height > GLint(srcHeight)) {
            GLint diff = y + height - GLint(srcHeight);
            MOZ_ASSERT(diff > 0);
            trimmedHeight -= diff;
        }

        MOZ_ASSERT(trimmedX >= 0);
        MOZ_ASSERT(trimmedY >= 0);
        MOZ_ASSERT(trimmedWidth >= 0);
        MOZ_ASSERT(trimmedHeight >= 0);

        gl->fCopyTexSubImage2D(texImageTarget.get(), level, trimmedXOffset,
                               trimmedYOffset, trimmedX, trimmedY, trimmedWidth,
                               trimmedHeight);
    }

    if (sizeMayChange) {
        GLenum error = mContext->GetAndFlushUnderlyingGLErrors();
        if (error) {
            mContext->GenerateWarning("copyTexImage2D generated error %s", mContext->ErrorName(error));
            return;
        }
    }

    if (!sub) {
        const uint32_t depth = 1;
        const bool isDataInitialized = true;
        const ImageInfo imageInfo(destEffectiveFormat, width, height, depth,
                                  isDataInitialized);

        SetImageInfoAt(texImageTarget, level, imageInfo);
    }
}

void
WebGLTexture::CopyTexImage2D(TexImageTarget texImageTarget,
                             GLint level,
                             GLenum internalFormat,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLint border)
{
    const char funcName[] = "copyTexImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    CopyTexSubImage2D_base(texImageTarget, level, internalFormat, 0, 0, x, y, width,
                           height, border, false);
}

void
WebGLTexture::CopyTexSubImage2D(TexImageTarget texImageTarget,
                                GLint level,
                                GLint xOffset,
                                GLint yOffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height)
{
    switch (texImageTarget.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        break;
    default:
        return mContext->ErrorInvalidEnumInfo("copyTexSubImage2D: target", texImageTarget.get());
    }

    if (level < 0)
        return mContext->ErrorInvalidValue("copyTexSubImage2D: level may not be negative");

    GLsizei maxTextureSize = mContext->MaxTextureSizeForTarget(TexImageTargetToTexTarget(texImageTarget));
    if (!(maxTextureSize >> level))
        return mContext->ErrorInvalidValue("copyTexSubImage2D: 2^level exceeds maximum texture size");

    if (width < 0 || height < 0)
        return mContext->ErrorInvalidValue("copyTexSubImage2D: width and height may not be negative");

    if (xOffset < 0 || yOffset < 0)
        return mContext->ErrorInvalidValue("copyTexSubImage2D: xOffset and yOffset may not be negative");

    WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    if (!imageInfo.IsDefined())
        return mContext->ErrorInvalidOperation("copyTexSubImage2D: no texture image previously defined for this level and face");

    GLsizei texWidth = imageInfo.mWidth;
    GLsizei texHeight = imageInfo.mHeight;

    if (xOffset + width > texWidth || xOffset + width < 0)
      return mContext->ErrorInvalidValue("copyTexSubImage2D: xOffset+width is too large");

    if (yOffset + height > texHeight || yOffset + height < 0)
      return mContext->ErrorInvalidValue("copyTexSubImage2D: yOffset+height is too large");

    if (!mContext->mBoundReadFramebuffer)
        mContext->ClearBackbufferIfNeeded();

    if (!imageInfo.IsDataInitialized()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                width == texWidth &&
                                height == texHeight;
        if (coversWholeImage) {
            imageInfo.SetIsDataInitialized(true, this);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    TexInternalFormat unsizedInternalFormat;
    TexType type;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(imageInfo.mFormat,
                                                            &unsizedInternalFormat,
                                                            &type);
    const GLint border = 0;
    CopyTexSubImage2D_base(texImageTarget, level, unsizedInternalFormat.get(), xOffset,
                           yOffset, x, y, width, height, border, true);
}


GLenum
WebGLTexture::CheckedTexImage2D(TexImageTarget texImageTarget,
                                       GLint level,
                                       TexInternalFormat internalFormat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border,
                                       TexFormat unpackFormat,
                                       TexType unpackType,
                                       const GLvoid* data)
{
    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);
    bool sizeMayChange = true;

    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    if (imageInfo.IsDefined()) {
        sizeMayChange = uint32_t(width) != imageInfo.mWidth ||
                        uint32_t(height) != imageInfo.mHeight ||
                        effectiveInternalFormat != imageInfo.mFormat;
    }

    gl::GLContext* gl = mContext->gl;

    // Convert to format and type required by OpenGL 'driver'.
    GLenum driverType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             effectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverFormat,
                                             &driverType);

    if (sizeMayChange) {
        mContext->GetAndFlushUnderlyingGLErrors();
    }

    if (driverFormat == LOCAL_GL_ALPHA) {
        MOZ_ASSERT(true);
    }

    gl->fTexImage2D(texImageTarget.get(), level, driverInternalFormat, width, height, border, driverFormat, driverType, data);

    SetLegacyTextureSwizzle(gl, texImageTarget.get(), internalFormat.get());

    GLenum error = LOCAL_GL_NO_ERROR;
    if (sizeMayChange) {
        error = mContext->GetAndFlushUnderlyingGLErrors();
    }

    return error;
}

void
WebGLTexture::TexImage2D_base(TexImageTarget texImageTarget, GLint level,
                              GLenum internalFormat,
                              GLsizei width, GLsizei height, GLsizei srcStrideOrZero,
                              GLint border,
                              GLenum unpackFormat,
                              GLenum unpackType,
                              void* data, uint32_t byteLength,
                              js::Scalar::Type jsArrayType,
                              WebGLTexelFormat srcFormat, bool srcPremultiplied)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::TexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (unpackType == LOCAL_GL_HALF_FLOAT_OES) {
        unpackType = LOCAL_GL_HALF_FLOAT;
    }

    if (!mContext->ValidateTexImage(texImageTarget, level, internalFormat,
                          0, 0, 0,
                          width, height, 0,
                          border, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    const bool isDepthTexture = unpackFormat == LOCAL_GL_DEPTH_COMPONENT ||
                                unpackFormat == LOCAL_GL_DEPTH_STENCIL;

    if (isDepthTexture && !mContext->IsWebGL2()) {
        if (data != nullptr || level != 0)
            return mContext->ErrorInvalidOperation("texImage2D: "
                                         "with format of DEPTH_COMPONENT or DEPTH_STENCIL, "
                                         "data must be nullptr, "
                                         "level must be zero");
    }

    if (!mContext->ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);

    if (effectiveInternalFormat == LOCAL_GL_NONE) {
        return mContext->ErrorInvalidOperation("texImage2D: bad combination of internalFormat and type");
    }

    size_t srcTexelSize = size_t(-1);
    if (srcFormat == WebGLTexelFormat::Auto) {
        // we need to find the exact sized format of the source data. Slightly abusing
        // EffectiveInternalFormatFromInternalFormatAndType for that purpose. Really, an unsized source format
        // is the same thing as an unsized internalFormat.
        TexInternalFormat effectiveSourceFormat =
            EffectiveInternalFormatFromInternalFormatAndType(unpackFormat, unpackType);
        MOZ_ASSERT(effectiveSourceFormat != LOCAL_GL_NONE); // should have validated format/type combo earlier
        const size_t srcbitsPerTexel = GetBitsPerTexel(effectiveSourceFormat);
        MOZ_ASSERT((srcbitsPerTexel % 8) == 0); // should not have compressed formats here.
        srcTexelSize = srcbitsPerTexel / 8;
    } else {
        srcTexelSize = WebGLTexelConversions::TexelBytesForFormat(srcFormat);
    }

    CheckedUint32 checked_neededByteLength =
        mContext->GetImageSize(height, width, 1, srcTexelSize, mContext->mPixelStoreUnpackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * srcTexelSize;
    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize.value(), mContext->mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength && byteLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texImage2D: not enough data for operation (need %d, have %d)",
                                 bytesNeeded, byteLength);

    if (mImmutable) {
        return mContext->ErrorInvalidOperation(
            "texImage2D: disallowed because the texture "
            "bound to this target has already been made immutable by texStorage2D");
    }
    mContext->MakeContextCurrent();

    nsAutoArrayPtr<uint8_t> convertedData;
    void* pixels = nullptr;
    bool isDataInitialized = false;

    WebGLTexelFormat dstFormat = GetWebGLTexelFormat(effectiveInternalFormat);
    WebGLTexelFormat actualSrcFormat = srcFormat == WebGLTexelFormat::Auto ? dstFormat : srcFormat;

    if (byteLength) {
        size_t   bitsPerTexel = GetBitsPerTexel(effectiveInternalFormat);
        MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed formats here.
        size_t   dstTexelSize = bitsPerTexel / 8;
        size_t   srcStride = srcStrideOrZero ? srcStrideOrZero : checked_alignedRowSize.value();
        size_t   dstPlainRowSize = dstTexelSize * width;
        size_t   unpackAlignment = mContext->mPixelStoreUnpackAlignment;
        size_t   dstStride = ((dstPlainRowSize + unpackAlignment-1) / unpackAlignment) * unpackAlignment;

        if (actualSrcFormat == dstFormat &&
            srcPremultiplied == mContext->mPixelStorePremultiplyAlpha &&
            srcStride == dstStride &&
            !mContext->mPixelStoreFlipY)
        {
            // no conversion, no flipping, so we avoid copying anything and just pass the source pointer
            pixels = data;
        } else {
            size_t convertedDataSize = height * dstStride;
            convertedData = new (fallible) uint8_t[convertedDataSize];
            if (!convertedData) {
                mContext->ErrorOutOfMemory("texImage2D: Ran out of memory when allocating"
                                 " a buffer for doing format conversion.");
                return;
            }
            if (!mContext->ConvertImage(width, height, srcStride, dstStride,
                              static_cast<uint8_t*>(data), convertedData,
                              actualSrcFormat, srcPremultiplied,
                              dstFormat, mContext->mPixelStorePremultiplyAlpha, dstTexelSize))
            {
                return mContext->ErrorInvalidOperation("texImage2D: Unsupported texture format conversion");
            }
            pixels = reinterpret_cast<void*>(convertedData.get());
        }
        isDataInitialized = true;
    }

    GLenum error = CheckedTexImage2D(texImageTarget, level, internalFormat, width,
                                     height, border, unpackFormat, unpackType, pixels);

    if (error) {
        mContext->GenerateWarning("texImage2D generated error %s", mContext->ErrorName(error));
        return;
    }

    const uint32_t depth = 1;
    const ImageInfo imageInfo(effectiveInternalFormat, width, height, depth,
                              isDataInitialized);
    SetImageInfoAt(texImageTarget, level, imageInfo);
}

void
WebGLTexture::TexImage2D(TexImageTarget texImageTarget, GLint level,
                         GLenum internalFormat, GLsizei width,
                         GLsizei height, GLint border, GLenum unpackFormat,
                         GLenum unpackType, const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                         ErrorResult* const out_rv)
{
    void* data;
    size_t length;
    js::Scalar::Type jsArrayType;
    if (maybeView.IsNull()) {
        data = nullptr;
        length = 0;
        jsArrayType = js::Scalar::MaxTypedArrayViewType;
    } else {
        const auto& view = maybeView.Value();
        ComputeLengthAndData(view, &data, &length, &jsArrayType);
    }

    const char funcName[] = "texImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    return TexImage2D_base(texImageTarget, level, internalFormat, width, height, 0, border, unpackFormat, unpackType,
                           data, length, jsArrayType,
                           WebGLTexelFormat::Auto, false);
}

void
WebGLTexture::TexImage2D(TexImageTarget texImageTarget, GLint level,
                         GLenum internalFormat, GLenum unpackFormat,
                         GLenum unpackType, dom::ImageData* imageData, ErrorResult* const out_rv)
{
    if (!imageData) {
        // Spec says to generate an INVALID_VALUE error
        return mContext->ErrorInvalidValue("texImage2D: null ImageData");
    }

    dom::Uint8ClampedArray arr;
    DebugOnly<bool> inited = arr.Init(imageData->GetDataObject());
    MOZ_ASSERT(inited);
    arr.ComputeLengthAndData();

    void* pixelData = arr.Data();
    const uint32_t pixelDataLength = arr.Length();

    const char funcName[] = "texImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    return TexImage2D_base(texImageTarget, level, internalFormat, imageData->Width(),
                           imageData->Height(), 4*imageData->Width(), 0,
                           unpackFormat, unpackType, pixelData, pixelDataLength, js::Scalar::MaxTypedArrayViewType,
                           WebGLTexelFormat::RGBA8, false);
}

void
WebGLTexture::TexImage2D(TexImageTarget texImageTarget, GLint level,
                GLenum internalFormat, GLenum unpackFormat, GLenum unpackType,
                dom::Element* elem, ErrorResult* out_rv)
{
    const char funcName[] = "texImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    if (level < 0)
        return mContext->ErrorInvalidValue("texImage2D: level is negative");

    const int32_t maxLevel = mContext->MaxTextureLevelForTexImageTarget(texImageTarget);
    if (level > maxLevel) {
        mContext->ErrorInvalidValue("texImage2D: level %d is too large, max is %d",
                          level, maxLevel);
        return;
    }

    // Trying to handle the video by GPU directly first
    if (TexImageFromVideoElement(texImageTarget, level, internalFormat,
                                 unpackFormat, unpackType, elem))
    {
        return;
    }

    RefPtr<gfx::DataSourceSurface> data;
    WebGLTexelFormat srcFormat;
    nsLayoutUtils::SurfaceFromElementResult res = mContext->SurfaceFromElement(elem);
    *out_rv = mContext->SurfaceFromElementResultToImageSurface(res, data, &srcFormat);
    if (out_rv->Failed() || !data)
        return;

    gfx::IntSize size = data->GetSize();
    uint32_t byteLength = data->Stride() * size.height;
    return TexImage2D_base(texImageTarget, level, internalFormat,
                           size.width, size.height, data->Stride(), 0,
                           unpackFormat, unpackType, data->GetData(), byteLength,
                           js::Scalar::MaxTypedArrayViewType, srcFormat,
                           res.mIsPremultiplied);
}


void
WebGLTexture::TexSubImage2D_base(TexImageTarget texImageTarget, GLint level,
                                 GLint xOffset, GLint yOffset,
                                 GLsizei width, GLsizei height, GLsizei srcStrideOrZero,
                                 GLenum unpackFormat, GLenum unpackType,
                                 void* data, uint32_t byteLength,
                                 js::Scalar::Type jsArrayType,
                                 WebGLTexelFormat srcFormat, bool srcPremultiplied)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::TexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (unpackType == LOCAL_GL_HALF_FLOAT_OES)
        unpackType = LOCAL_GL_HALF_FLOAT;

    const char funcName[] = "texSubImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    if (!imageInfo.IsDefined())
        return mContext->ErrorInvalidOperation("texSubImage2D: no previously defined texture image");

    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.mFormat;

    if (!mContext->ValidateTexImage(texImageTarget, level,
                          existingEffectiveInternalFormat.get(),
                          xOffset, yOffset, 0,
                          width, height, 0,
                          0, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (!mContext->ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    if (unpackType != TypeFromInternalFormat(existingEffectiveInternalFormat)) {
        return mContext->ErrorInvalidOperation("texSubImage2D: type differs from that of the existing image");
    }

    size_t srcTexelSize = size_t(-1);
    if (srcFormat == WebGLTexelFormat::Auto) {
        const size_t bitsPerTexel = GetBitsPerTexel(existingEffectiveInternalFormat);
        MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed formats here.
        srcTexelSize = bitsPerTexel / 8;
    } else {
        srcTexelSize = WebGLTexelConversions::TexelBytesForFormat(srcFormat);
    }

    if (width == 0 || height == 0)
        return; // ES 2.0 says it has no effect, we better return right now

    CheckedUint32 checked_neededByteLength =
        mContext->GetImageSize(height, width, 1, srcTexelSize, mContext->mPixelStoreUnpackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * srcTexelSize;

    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize.value(), mContext->mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, byteLength);

    if (!imageInfo.IsDataInitialized()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                uint32_t(width) == imageInfo.mWidth &&
                                uint32_t(height) == imageInfo.mHeight;
        if (coversWholeImage) {
            imageInfo.SetIsDataInitialized(true, this);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }
    mContext->MakeContextCurrent();
    gl::GLContext* gl = mContext->gl;

    size_t   srcStride = srcStrideOrZero ? srcStrideOrZero : checked_alignedRowSize.value();
    uint32_t dstTexelSize = GetBitsPerTexel(existingEffectiveInternalFormat) / 8;
    size_t   dstPlainRowSize = dstTexelSize * width;
    // There are checks above to ensure that this won't overflow.
    size_t   dstStride = RoundedToNextMultipleOf(dstPlainRowSize, mContext->mPixelStoreUnpackAlignment).value();

    void* pixels = data;
    nsAutoArrayPtr<uint8_t> convertedData;

    WebGLTexelFormat dstFormat = GetWebGLTexelFormat(existingEffectiveInternalFormat);
    WebGLTexelFormat actualSrcFormat = srcFormat == WebGLTexelFormat::Auto ? dstFormat : srcFormat;

    // no conversion, no flipping, so we avoid copying anything and just pass the source pointer
    bool noConversion = (actualSrcFormat == dstFormat &&
                         srcPremultiplied == mContext->mPixelStorePremultiplyAlpha &&
                         srcStride == dstStride &&
                         !mContext->mPixelStoreFlipY);

    if (!noConversion) {
        size_t convertedDataSize = height * dstStride;
        convertedData = new (fallible) uint8_t[convertedDataSize];
        if (!convertedData) {
            mContext->ErrorOutOfMemory("texImage2D: Ran out of memory when allocating"
                             " a buffer for doing format conversion.");
            return;
        }
        if (!mContext->ConvertImage(width, height, srcStride, dstStride,
                          static_cast<const uint8_t*>(data), convertedData,
                          actualSrcFormat, srcPremultiplied,
                          dstFormat, mContext->mPixelStorePremultiplyAlpha, dstTexelSize))
        {
            return mContext->ErrorInvalidOperation("texSubImage2D: Unsupported texture format conversion");
        }
        pixels = reinterpret_cast<void*>(convertedData.get());
    }

    GLenum driverType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             existingEffectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverFormat,
                                             &driverType);

    gl->fTexSubImage2D(texImageTarget.get(), level, xOffset, yOffset, width, height, driverFormat, driverType, pixels);
}

void
WebGLTexture::TexSubImage2D(TexImageTarget texImageTarget, GLint level,
                            GLint xOffset, GLint yOffset,
                            GLsizei width, GLsizei height,
                            GLenum unpackFormat, GLenum unpackType,
                            const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                            ErrorResult* const out_rv)
{
    if (maybeView.IsNull())
        return mContext->ErrorInvalidValue("texSubImage2D: pixels must not be null!");

    const auto& view = maybeView.Value();
    size_t length;
    void* data;
    js::Scalar::Type jsArrayType;
    ComputeLengthAndData(view, &data, &length, &jsArrayType);

    const char funcName[] = "texSubImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    return TexSubImage2D_base(texImageTarget, level, xOffset, yOffset,
                              width, height, 0, unpackFormat, unpackType,
                              data, length, jsArrayType,
                              WebGLTexelFormat::Auto, false);
}

void
WebGLTexture::TexSubImage2D(TexImageTarget texImageTarget, GLint level,
                            GLint xOffset, GLint yOffset,
                            GLenum unpackFormat, GLenum unpackType, dom::ImageData* imageData,
                            ErrorResult* const out_rv)
{
    if (!imageData)
        return mContext->ErrorInvalidValue("texSubImage2D: pixels must not be null!");

    dom::Uint8ClampedArray arr;
    DebugOnly<bool> inited = arr.Init(imageData->GetDataObject());
    MOZ_ASSERT(inited);
    arr.ComputeLengthAndData();

    return TexSubImage2D_base(texImageTarget, level, xOffset, yOffset,
                              imageData->Width(), imageData->Height(),
                              4*imageData->Width(), unpackFormat, unpackType,
                              arr.Data(), arr.Length(),
                              js::Scalar::MaxTypedArrayViewType,
                              WebGLTexelFormat::RGBA8, false);
}



bool
WebGLTexture::TexImageFromVideoElement(TexImageTarget texImageTarget,
                                       GLint level, GLenum internalFormat,
                                       GLenum unpackFormat, GLenum unpackType,
                                       mozilla::dom::Element* elem)
{
    if (unpackType == LOCAL_GL_HALF_FLOAT_OES &&
        !mContext->gl->IsExtensionSupported(gl::GLContext::OES_texture_half_float))
    {
        unpackType = LOCAL_GL_HALF_FLOAT;
    }

    if (!mContext->ValidateTexImageFormatAndType(unpackFormat, unpackType,
                                       WebGLTexImageFunc::TexImage,
                                       WebGLTexDimensions::Tex2D))
    {
        return false;
    }

    dom::HTMLVideoElement* video = dom::HTMLVideoElement::FromContentOrNull(elem);
    if (!video)
        return false;

    uint16_t readyState;
    if (NS_SUCCEEDED(video->GetReadyState(&readyState)) &&
        readyState < nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA)
    {
        //No frame inside, just return
        return false;
    }

    // If it doesn't have a principal, just bail
    nsCOMPtr<nsIPrincipal> principal = video->GetCurrentPrincipal();
    if (!principal)
        return false;

    mozilla::layers::ImageContainer* container = video->GetImageContainer();
    if (!container)
        return false;

    if (video->GetCORSMode() == CORS_NONE) {
        bool subsumes;
        nsresult rv = mContext->mCanvasElement->NodePrincipal()->Subsumes(principal, &subsumes);
        if (NS_FAILED(rv) || !subsumes) {
            mContext->GenerateWarning("It is forbidden to load a WebGL texture from a cross-domain element that has not been validated with CORS. "
                                "See https://developer.mozilla.org/en/WebGL/Cross-Domain_Textures");
            return false;
        }
    }

    layers::AutoLockImage lockedImage(container);
    layers::Image* srcImage = lockedImage.GetImage();
    if (!srcImage) {
      return false;
    }

    const uint32_t width = srcImage->GetSize().width;
    const uint32_t height = srcImage->GetSize().height;

    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    MOZ_ASSERT(level == 0);
    const WebGLTexture::ImageInfo& info = ImageInfoAt(texImageTarget, 0);
    bool dimensionsMatch = (info.mWidth == width &&
                            info.mHeight == height &&
                            info.mDepth == 1);
    if (!dimensionsMatch) {
        // we need to allocation
        gl->fTexImage2D(texImageTarget.get(), level, internalFormat,
                        width, height,
                        0, unpackFormat, unpackType, nullptr);
    }

    const gl::OriginPos destOrigin = mContext->mPixelStoreFlipY ? gl::OriginPos::BottomLeft
                                                      : gl::OriginPos::TopLeft;
    bool ok = gl->BlitHelper()->BlitImageToTexture(srcImage,
                                                   srcImage->GetSize(),
                                                   mGLName,
                                                   texImageTarget.get(),
                                                   destOrigin);
    if (!ok)
        return false;

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat,
                                                         unpackType);
    MOZ_ASSERT(effectiveInternalFormat != LOCAL_GL_NONE);

    const uint32_t depth = 1;
    const bool isDataInitialized = true;
    const ImageInfo imageInfo(effectiveInternalFormat, width, height, depth,
                              isDataInitialized);
    SetImageInfoAt(texImageTarget, level, imageInfo);

    return true;
}

void
WebGLTexture::TexSubImage2D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLenum unpackFormat, GLenum unpackType,
                            dom::Element* elem, ErrorResult* const out_rv)
{
    const char funcName[] = "texSubImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    if (level < 0)
        return mContext->ErrorInvalidValue("texSubImage2D: level is negative");

    const int32_t maxLevel = mContext->MaxTextureLevelForTexImageTarget(texImageTarget);
    if (level > maxLevel) {
        mContext->ErrorInvalidValue("texSubImage2D: level %d is too large, max is %d",
                          level, maxLevel);
        return;
    }

    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    const TexInternalFormat internalFormat = imageInfo.mFormat;

    // Trying to handle the video by GPU directly first
    if (TexImageFromVideoElement(texImageTarget, level, internalFormat.get(), unpackFormat, unpackType, elem))
    {
        return;
    }

    RefPtr<gfx::DataSourceSurface> data;
    WebGLTexelFormat srcFormat;
    nsLayoutUtils::SurfaceFromElementResult res = mContext->SurfaceFromElement(elem);
    *out_rv = mContext->SurfaceFromElementResultToImageSurface(res, data, &srcFormat);
    if (out_rv->Failed() || !data)
        return;

    gfx::IntSize size = data->GetSize();
    uint32_t byteLength = data->Stride() * size.height;
    TexSubImage2D_base(texImageTarget, level, xOffset, yOffset, size.width,
                       size.height, data->Stride(), unpackFormat, unpackType, data->GetData(),
                       byteLength, js::Scalar::MaxTypedArrayViewType, srcFormat,
                       res.mIsPremultiplied);
}

bool
WebGLTexture::ValidateSizedInternalFormat(GLenum internalFormat, const char* info)
{
    switch (internalFormat) {
        // Sized Internal Formats
        // https://www.khronos.org/opengles/sdk/docs/man3/html/glTexStorage2D.xhtml
    case LOCAL_GL_R8:
    case LOCAL_GL_R8_SNORM:
    case LOCAL_GL_R16F:
    case LOCAL_GL_R32F:
    case LOCAL_GL_R8UI:
    case LOCAL_GL_R8I:
    case LOCAL_GL_R16UI:
    case LOCAL_GL_R16I:
    case LOCAL_GL_R32UI:
    case LOCAL_GL_R32I:
    case LOCAL_GL_RG8:
    case LOCAL_GL_RG8_SNORM:
    case LOCAL_GL_RG16F:
    case LOCAL_GL_RG32F:
    case LOCAL_GL_RG8UI:
    case LOCAL_GL_RG8I:
    case LOCAL_GL_RG16UI:
    case LOCAL_GL_RG16I:
    case LOCAL_GL_RG32UI:
    case LOCAL_GL_RG32I:
    case LOCAL_GL_RGB8:
    case LOCAL_GL_SRGB8:
    case LOCAL_GL_RGB565:
    case LOCAL_GL_RGB8_SNORM:
    case LOCAL_GL_R11F_G11F_B10F:
    case LOCAL_GL_RGB9_E5:
    case LOCAL_GL_RGB16F:
    case LOCAL_GL_RGB32F:
    case LOCAL_GL_RGB8UI:
    case LOCAL_GL_RGB8I:
    case LOCAL_GL_RGB16UI:
    case LOCAL_GL_RGB16I:
    case LOCAL_GL_RGB32UI:
    case LOCAL_GL_RGB32I:
    case LOCAL_GL_RGBA8:
    case LOCAL_GL_SRGB8_ALPHA8:
    case LOCAL_GL_RGBA8_SNORM:
    case LOCAL_GL_RGB5_A1:
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB10_A2:
    case LOCAL_GL_RGBA16F:
    case LOCAL_GL_RGBA32F:
    case LOCAL_GL_RGBA8UI:
    case LOCAL_GL_RGBA8I:
    case LOCAL_GL_RGB10_A2UI:
    case LOCAL_GL_RGBA16UI:
    case LOCAL_GL_RGBA16I:
    case LOCAL_GL_RGBA32I:
    case LOCAL_GL_RGBA32UI:
    case LOCAL_GL_DEPTH_COMPONENT16:
    case LOCAL_GL_DEPTH_COMPONENT24:
    case LOCAL_GL_DEPTH_COMPONENT32F:
    case LOCAL_GL_DEPTH24_STENCIL8:
    case LOCAL_GL_DEPTH32F_STENCIL8:
        return true;
    }

    if (IsCompressedTextureFormat(internalFormat))
        return true;

    nsCString name;
    mContext->EnumName(internalFormat, &name);
    mContext->ErrorInvalidEnum("%s: invalid internal format %s", info, name.get());

    return false;
}

bool
WebGLTexture::ValidateTexStorage(TexTarget texTarget, GLsizei levels,
                                 GLenum internalFormat, GLsizei width, GLsizei height,
                                 GLsizei depth, const char* funcName)
{
    // INVALID_OPERATION is generated if the texture object currently bound to target
    // already has TEXTURE_IMMUTABLE_FORMAT set to TRUE.
    // While there isn't an explicit reference to this, it seems to fall out of GLES
    // 3.0.4:
    // p138: "Using [TexImage*] with the same texture will result in an
    //        INVALID_OPERATION error being generated, even if it does not affect the"
    //        dimensions or unpackFormat[.]"
    // p136: "* If executing the pseudo-code would result in any other error, the error is
    //          generated and the command will have no effect."
    // p137-138: The pseudo-code uses TexImage*.
    //
    if (mImmutable) {
        mContext->ErrorInvalidOperation("%s: Texture is already immutable.", funcName);
        return false;
    }

    // INVALID_ENUM is generated if internalformat is not a valid sized internal unpackFormat.
    if (!ValidateSizedInternalFormat(internalFormat, funcName))
        return false;

    // INVALID_VALUE is generated if width, height or levels are less than 1.
    if (width < 1 || height < 1 || depth < 1 || levels < 1) {
        mContext->ErrorInvalidValue("%s: Width, height, depth, and levels must be >= 1.",
                                    funcName);
        return false;
    }

    // INVALID_OPERATION is generated if levels is greater than floor(log2(max(width, height, depth)))+1.
    GLsizei largest = std::max(std::max(width, height), depth);
    GLsizei maxLevels = FloorLog2(largest) + 1;
    if (levels > maxLevels) {
        mContext->ErrorInvalidOperation("%s: Too many levels for given texture dimensions.",
                                        funcName);
        return false;
    }

    return true;
}

void
WebGLTexture::TexStorage2D(TexTarget texTarget, GLsizei levels, GLenum rawInternalFormat,
                           GLsizei width, GLsizei height)
{
    if (texTarget != LOCAL_GL_TEXTURE_2D && texTarget != LOCAL_GL_TEXTURE_CUBE_MAP) {
        mContext->ErrorInvalidEnum("texStorage2D: target must be TEXTURE_2D or"
                                   " TEXTURE_CUBE_MAP.");
        return;
    }

    const GLsizei depth = 1;
    if (!ValidateTexStorage(texTarget, levels, rawInternalFormat, width, height, depth,
                            "texStorage2D"))
    {
        return;
    }

    TexInternalFormat internalFormat(rawInternalFormat);

    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    mContext->GetAndFlushUnderlyingGLErrors();
    gl->fTexStorage2D(texTarget.get(), levels, internalFormat.get(), width, height);
    GLenum error = mContext->GetAndFlushUnderlyingGLErrors();
    if (error) {
        mContext->GenerateWarning("texStorage2D generated error %s",
                                  mContext->ErrorName(error));
        return;
    }

    SpecifyTexStorage(levels, internalFormat, width, height, depth);
}

void
WebGLTexture::TexStorage3D(TexTarget texTarget, GLsizei levels, GLenum rawInternalFormat,
                           GLsizei width, GLsizei height, GLsizei depth)
{
    if (texTarget != LOCAL_GL_TEXTURE_3D)
        return mContext->ErrorInvalidEnum("texStorage3D: target must be TEXTURE_3D.");

    if (!ValidateTexStorage(texTarget, levels, rawInternalFormat, width, height, depth,
                            "texStorage3D"))
    {
        return;
    }

    TexInternalFormat internalFormat(rawInternalFormat);

    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    mContext->GetAndFlushUnderlyingGLErrors();
    gl->fTexStorage3D(texTarget.get(), levels, internalFormat.get(), width, height, depth);
    GLenum error = mContext->GetAndFlushUnderlyingGLErrors();
    if (error) {
        mContext->GenerateWarning("texStorage3D generated error %s",
                                  mContext->ErrorName(error));
        return;
    }

    SpecifyTexStorage(levels, internalFormat, width, height, depth);
}

void
WebGLTexture::TexImage3D(TexImageTarget texImageTarget, GLint level, GLenum internalFormat,
                          GLsizei width, GLsizei height, GLsizei depth,
                          GLint border, GLenum unpackFormat, GLenum unpackType,
                          const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                          ErrorResult* const out_rv)
{
    void* data;
    size_t dataLength;
    js::Scalar::Type jsArrayType;
    if (maybeView.IsNull()) {
        data = nullptr;
        dataLength = 0;
        jsArrayType = js::Scalar::MaxTypedArrayViewType;
    } else {
        const auto& view = maybeView.Value();
        ComputeLengthAndData(view, &data, &dataLength, &jsArrayType);
    }

    const char funcName[] = "texImage3D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 3, funcName))
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::TexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex3D;

    if (!mContext->ValidateTexImage(texImageTarget, level, internalFormat,
                          0, 0, 0,
                          width, height, depth,
                          border, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (!mContext->ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);

    if (effectiveInternalFormat == LOCAL_GL_NONE) {
        return mContext->ErrorInvalidOperation("texImage3D: bad combination of internalFormat and unpackType");
    }

    // we need to find the exact sized format of the source data. Slightly abusing
    // EffectiveInternalFormatFromInternalFormatAndType for that purpose. Really, an unsized source format
    // is the same thing as an unsized internalFormat.
    TexInternalFormat effectiveSourceFormat =
        EffectiveInternalFormatFromInternalFormatAndType(unpackFormat, unpackType);
    MOZ_ASSERT(effectiveSourceFormat != LOCAL_GL_NONE); // should have validated unpack format/type combo earlier
    const size_t srcbitsPerTexel = GetBitsPerTexel(effectiveSourceFormat);
    MOZ_ASSERT((srcbitsPerTexel % 8) == 0); // should not have compressed formats here.
    size_t srcTexelSize = srcbitsPerTexel / 8;

    CheckedUint32 checked_neededByteLength =
        mContext->GetImageSize(height, width, depth, srcTexelSize, mContext->mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (dataLength && dataLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texImage3D: not enough data for operation (need %d, have %d)",
                                 bytesNeeded, dataLength);

    if (mImmutable) {
        return mContext->ErrorInvalidOperation(
            "texImage3D: disallowed because the texture "
            "bound to this target has already been made immutable by texStorage3D");
    }

    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    GLenum driverUnpackType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             effectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverUnpackFormat,
                                             &driverUnpackType);

    mContext->GetAndFlushUnderlyingGLErrors();
    gl->fTexImage3D(texImageTarget.get(), level,
                    driverInternalFormat,
                    width, height, depth,
                    0, driverUnpackFormat, driverUnpackType,
                    data);
    GLenum error = mContext->GetAndFlushUnderlyingGLErrors();
    if (error) {
        return mContext->GenerateWarning("texImage3D generated error %s", mContext->ErrorName(error));
    }

    const bool isDataInitialized = bool(data);
    const ImageInfo imageInfo(effectiveInternalFormat, width, height, depth,
                              isDataInitialized);
    SetImageInfoAt(texImageTarget, level, imageInfo);
}

void
WebGLTexture::TexSubImage3D(TexImageTarget texImageTarget, GLint level,
                             GLint xOffset, GLint yOffset, GLint zOffset,
                             GLsizei width, GLsizei height, GLsizei depth,
                             GLenum unpackFormat, GLenum unpackType,
                             const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                             ErrorResult* const out_rv)
{
    if (maybeView.IsNull())
        return mContext->ErrorInvalidValue("texSubImage3D: pixels must not be null!");

    const auto& view = maybeView.Value();
    void* data;
    size_t dataLength;
    js::Scalar::Type jsArrayType;
    ComputeLengthAndData(view, &data, &dataLength, &jsArrayType);

    const char funcName[] = "texSubImage3D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 3, funcName))
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::TexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex3D;

    WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    if (!imageInfo.IsDefined()) {
        return mContext->ErrorInvalidOperation("texSubImage3D: no previously defined texture image");
    }

    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.mFormat;
    TexInternalFormat existingUnsizedInternalFormat = LOCAL_GL_NONE;
    TexType existingType = LOCAL_GL_NONE;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(existingEffectiveInternalFormat,
                                                            &existingUnsizedInternalFormat,
                                                            &existingType);

    if (!mContext->ValidateTexImage(texImageTarget, level, existingEffectiveInternalFormat.get(),
                          xOffset, yOffset, zOffset,
                          width, height, depth,
                          0, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (unpackType != existingType) {
        return mContext->ErrorInvalidOperation("texSubImage3D: type differs from that of the existing image");
    }

    if (!mContext->ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    const size_t bitsPerTexel = GetBitsPerTexel(existingEffectiveInternalFormat);
    MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed formats here.
    size_t srcTexelSize = bitsPerTexel / 8;

    if (width == 0 || height == 0 || depth == 0)
        return; // no effect, we better return right now

    CheckedUint32 checked_neededByteLength =
        mContext->GetImageSize(height, width, depth, srcTexelSize, mContext->mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (dataLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, dataLength);

    if (!imageInfo.IsDataInitialized()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                zOffset == 0 &&
                                uint32_t(width) == imageInfo.mWidth &&
                                uint32_t(height) == imageInfo.mHeight &&
                                uint32_t(depth) == imageInfo.mDepth;
        if (coversWholeImage) {
            imageInfo.SetIsDataInitialized(true, this);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    GLenum driverUnpackType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(mContext->gl,
                                             existingEffectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverUnpackFormat,
                                             &driverUnpackType);

    mContext->MakeContextCurrent();
    mContext->gl->fTexSubImage3D(texImageTarget.get(), level,
                       xOffset, yOffset, zOffset,
                       width, height, depth,
                       driverUnpackFormat, driverUnpackType, data);
}


} // namespace mozilla
