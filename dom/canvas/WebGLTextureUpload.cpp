/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTexture.h"

#include <algorithm>
#include "GLContext.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Scoped.h"
#include "ScopedGLHelpers.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLTexelConversions.h"

namespace mozilla {

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
    mImageInfoMap.clear();

    ImageInfo imageInfo(width, height, depth, internalFormat,
                        WebGLImageDataStatus::UninitializedImageData);

    for (size_t level = 0; level < mImmutableLevelCount; level++) {
        SetImageInfosAtLevel(level, imageInfo);

        // Higher levels are unaffected.
        if (imageInfo.mWidth == 1 &&
            imageInfo.mHeight == 1 &&
            imageInfo.mDepth == 1)
        {
            MOZ_ASSERT(level + 1 == mImmutableLevelCount);
            break;
        }

        imageInfo.mWidth  = std::max(1, imageInfo.mWidth  / 2);
        imageInfo.mHeight = std::max(1, imageInfo.mHeight / 2);
        imageInfo.mDepth  = std::max(1, imageInfo.mDepth  / 2);
    }
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
    if (!ValidateSizedInternalFormat(internalformat, info))
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
        ErrorInvalidOperation("%s: Too many levels for given texture dimensions.",
                              funcName);
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// GL calls

static bool
DoesTargetMatchDimensions(WebGLContext* webgl, TexImageTarget target, uint8_t dims,
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
WebGLTexture::TexImage2D(TexImageTarget texImageTarget, GLint level,
                         GLenum internalFormat, GLenum unpackFormat, GLenum unpackType,
                         dom::Element* elem, ErrorResult* const out_rv)
{
    if (!DoesTargetMatchDimensions(this, texImageTarget, 2, "texImage2D"))
        return;

    if (level < 0)
        return mContext->ErrorInvalidValue("texImage2D: level is negative");

    const int32_t maxLevel = MaxTextureLevelForTexImageTarget(texImageTarget);
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
    rv = mContext->SurfaceFromElementResultToImageSurface(res, data, &srcFormat);
    if (rv.Failed() || !data)
        return;

    gfx::IntSize size = data->GetSize();
    uint32_t byteLength = data->Stride() * size.height;
    TexImage2D_base(texImageTarget, level, internalFormat,
                    size.width, size.height, data->Stride(), 0,
                    unpackFormat, unpackType, data->GetData(), byteLength,
                    js::Scalar::MaxTypedArrayViewType, srcFormat,
                    res.mIsPremultiplied);
}

bool
WebGLTexture::TexImageFromVideoElement(const TexImageTarget texImageTarget,
                                       GLint level, GLenum internalFormat,
                                       GLenum unpackFormat, GLenum unpackType,
                                       mozilla::dom::Element* elem)
{
    if (!ValidateTexImageFormatAndType(format, unpackType,
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

    gl->MakeCurrent();

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);

    const WebGLTexture::ImageInfo& info = ImageInfoAt(texImageTarget, 0);
    bool dimensionsMatch = info.Width() == srcImage->GetSize().width &&
                           info.Height() == srcImage->GetSize().height;
    if (!dimensionsMatch) {
        // we need to allocation
        gl->fTexImage2D(texImageTarget.get(), level, internalFormat,
                        srcImage->GetSize().width, srcImage->GetSize().height,
                        0, unpackFormat, unpackType, nullptr);
    }

    const gl::OriginPos destOrigin = mPixelStoreFlipY ? gl::OriginPos::BottomLeft
                                                      : gl::OriginPos::TopLeft;
    bool ok = gl->BlitHelper()->BlitImageToTexture(srcImage,
                                                   srcImage->GetSize(),
                                                   mGLName,
                                                   texImageTarget.get(),
                                                   destOrigin);
    if (ok) {
        TexInternalFormat effectiveInternalFormat =
            EffectiveInternalFormatFromInternalFormatAndType(internalFormat,
                                                             unpackType);
        MOZ_ASSERT(effectiveInternalFormat != LOCAL_GL_NONE);
        SetImageInfo(texImageTarget, level, srcImage->GetSize().width,
                          srcImage->GetSize().height, 1,
                          effectiveInternalFormat,
                          WebGLImageDataStatus::InitializedImageData);
        Bind(TexImageTargetToTexTarget(texImageTarget));
    }
    return ok;
}

GLenum
WebGLTexture::CheckedTexImage2D(TexImageTarget texImageTarget, GLint level,
                                TexInternalFormat internalFormat, GLsizei width,
                                GLsizei height, GLint border, TexFormat unpackFormat,
                                TexType unpackType, const GLvoid* data)
{
    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    MOZ_ASSERT(tex, "no texture bound");

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);
    bool sizeMayChange = true;

    if (HasImageInfoAt(texImageTarget, level)) {
        const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget,
                                                                    level);
        sizeMayChange = width != imageInfo.Width() ||
                        height != imageInfo.Height() ||
                        effectiveInternalFormat != imageInfo.EffectiveInternalFormat();
    }

    // Convert to unpackFormat and unpackType required by OpenGL 'driver'.
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
    GLenum driverUnpackType = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl, effectiveInternalFormat,
                                             &driverInternalFormat, &driverUnpackFormat,
                                             &driverUnpackType);

    if (sizeMayChange) {
        GetAndFlushUnderlyingGLErrors();
    }

    gl->fTexImage2D(texImageTarget.get(), level, driverInternalFormat, width, height,
                    border, driverUnpackFormat, driverUnpackType, data);

    if (effectiveInternalFormat != driverInternalFormat) {
        SetLegacyTextureSwizzle(gl, texImageTarget.get(), internalFormat.get());
    }

    GLenum error = LOCAL_GL_NO_ERROR;
    if (sizeMayChange) {
        error = GetAndFlushUnderlyingGLErrors();
    }

    return error;
}

void
WebGLTexture::TexImage2D_base(TexImageTarget texImageTarget, GLint level,
                              GLenum internalFormat, GLsizei width, GLsizei height,
                              GLsizei srcStrideOrZero, GLint border, GLenum unpackFormat,
                              GLenum unpackType, void* data, uint32_t byteLength,
                              js::Scalar::Type jsArrayType, WebGLTexelFormat srcFormat,
                              bool srcPremultiplied)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::TexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (unpackType == LOCAL_GL_HALF_FLOAT_OES) {
        unpackType = LOCAL_GL_HALF_FLOAT;
    }

    const GLint xOffset = 0;
    const GLint yOffset = 0;
    const GLint zOffset = 0;
    const GLsizei depth = 0;
    if (!ValidateTexImage(texImageTarget, level, internalFormat, xOffset, yOffset,
                          zOffset, width, height, depth, border, unpackFormat, unpackType,
                          func, dims))
    {
        return;
    }

    const bool isDepthTexture = (unpackFormat == LOCAL_GL_DEPTH_COMPONENT ||
                                 unpackFormat == LOCAL_GL_DEPTH_STENCIL);

    if (isDepthTexture && !IsWebGL2()) {
        if (data != nullptr || level != 0)
            return mContext->ErrorInvalidOperation("texImage2D: "
                                         "with unpackFormat of DEPTH_COMPONENT or DEPTH_STENCIL, "
                                         "data must be nullptr, "
                                         "level must be zero");
    }

    if (!ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);

    if (effectiveInternalFormat == LOCAL_GL_NONE) {
        return mContext->ErrorInvalidOperation("texImage2D: bad combination of internalFormat and unpackType");
    }

    size_t srcTexelSize = size_t(-1);
    if (srcFormat == WebGLTexelFormat::Auto) {
        // we need to find the exact sized unpackFormat of the source data. Slightly abusing
        // EffectiveInternalFormatFromInternalFormatAndType for that purpose. Really, an unsized source unpackFormat
        // is the same thing as an unsized internalFormat.
        TexInternalFormat effectiveSourceFormat =
            EffectiveInternalFormatFromInternalFormatAndType(unpackFormat, unpackType);
        MOZ_ASSERT(effectiveSourceFormat != LOCAL_GL_NONE); // should have validated unpackFormat/type combo earlier
        const size_t srcbitsPerTexel = GetBitsPerTexel(effectiveSourceFormat);
        MOZ_ASSERT((srcbitsPerTexel % 8) == 0); // should not have compressed unpackFormats here.
        srcTexelSize = srcbitsPerTexel / 8;
    } else {
        srcTexelSize = WebGLTexelConversions::TexelBytesForFormat(srcFormat);
    }

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, 1, srcTexelSize, mPixelStoreUnpackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * srcTexelSize;
    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize.value(), mPixelStoreUnpackAlignment);

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
    MakeContextCurrent();

    nsAutoArrayPtr<uint8_t> convertedData;
    void* pixels = nullptr;
    WebGLImageDataStatus imageInfoStatusIfSuccess = WebGLImageDataStatus::UninitializedImageData;

    WebGLTexelFormat dstFormat = GetWebGLTexelFormat(effectiveInternalFormat);
    WebGLTexelFormat actualSrcFormat = srcFormat == WebGLTexelFormat::Auto ? dstFormat : srcFormat;

    if (byteLength) {
        size_t   bitsPerTexel = GetBitsPerTexel(effectiveInternalFormat);
        MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed unpackFormats here.
        size_t   dstTexelSize = bitsPerTexel / 8;
        size_t   srcStride = srcStrideOrZero ? srcStrideOrZero : checked_alignedRowSize.value();
        size_t   dstPlainRowSize = dstTexelSize * width;
        size_t   unpackAlignment = mPixelStoreUnpackAlignment;
        size_t   dstStride = ((dstPlainRowSize + unpackAlignment-1) / unpackAlignment) * unpackAlignment;

        if (actualSrcFormat == dstFormat &&
            srcPremultiplied == mPixelStorePremultiplyAlpha &&
            srcStride == dstStride &&
            !mPixelStoreFlipY)
        {
            // no conversion, no flipping, so we avoid copying anything and just pass the source pointer
            pixels = data;
        }
        else
        {
            size_t convertedDataSize = height * dstStride;
            convertedData = new (fallible) uint8_t[convertedDataSize];
            if (!convertedData) {
                mContext->ErrorOutOfMemory("texImage2D: Ran out of memory when allocating"
                                 " a buffer for doing unpackFormat conversion.");
                return;
            }
            if (!ConvertImage(width, height, srcStride, dstStride,
                              static_cast<uint8_t*>(data), convertedData,
                              actualSrcFormat, srcPremultiplied,
                              dstFormat, mPixelStorePremultiplyAlpha, dstTexelSize))
            {
                return mContext->ErrorInvalidOperation("texImage2D: Unsupported texture unpackFormat conversion");
            }
            pixels = reinterpret_cast<void*>(convertedData.get());
        }
        imageInfoStatusIfSuccess = WebGLImageDataStatus::InitializedImageData;
    }

    GLenum error = CheckedTexImage2D(texImageTarget, level, internalFormat, width,
                                     height, border, unpackFormat, unpackType, pixels);

    if (error) {
        mContext->GenerateWarning("texImage2D generated error %s", ErrorName(error));
        return;
    }

    // in all of the code paths above, we should have either initialized data,
    // or allocated data and left it uninitialized, but in any case we shouldn't
    // have NoImageData at this point.
    MOZ_ASSERT(imageInfoStatusIfSuccess != WebGLImageDataStatus::NoImageData);

    SetImageInfo(texImageTarget, level, width, height, 1,
                      effectiveInternalFormat, imageInfoStatusIfSuccess);
}

void
WebGLTexture::TexImage2D(GLenum rawTarget, GLint level,
                         GLenum internalFormat, GLsizei width,
                         GLsizei height, GLint border, GLenum unpackFormat,
                         GLenum unpackType, const dom::Nullable<ArrayBufferView>& pixels,
                         ErrorResult& rv)
{
    if (IsContextLost())
        return;

    void* data;
    uint32_t length;
    js::Scalar::Type jsArrayType;
    if (pixels.IsNull()) {
        data = nullptr;
        length = 0;
        jsArrayType = js::Scalar::MaxTypedArrayViewType;
    } else {
        const ArrayBufferView& view = pixels.Value();
        view.ComputeLengthAndData();

        data = view.Data();
        length = view.Length();
        jsArrayType = view.Type();
    }

    if (!ValidateTexImageTarget(rawTarget, WebGLTexImageFunc::TexImage, WebGLTexDimensions::Tex2D))
        return;

    return TexImage2D_base(rawTarget, level, internalFormat, width, height, 0, border, unpackFormat, unpackType,
                           data, length, jsArrayType,
                           WebGLTexelFormat::Auto, false);
}

void
WebGLTexture::TexImage2D(GLenum rawTarget, GLint level,
                         GLenum internalFormat, GLenum unpackFormat,
                         GLenum unpackType, ImageData* pixels, ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (!pixels) {
        // Spec says to generate an INVALID_VALUE error
        return mContext->ErrorInvalidValue("texImage2D: null ImageData");
    }

    Uint8ClampedArray arr;
    DebugOnly<bool> inited = arr.Init(pixels->GetDataObject());
    MOZ_ASSERT(inited);
    arr.ComputeLengthAndData();

    void* pixelData = arr.Data();
    const uint32_t pixelDataLength = arr.Length();

    if (!ValidateTexImageTarget(rawTarget, WebGLTexImageFunc::TexImage, WebGLTexDimensions::Tex2D))
        return;

    return TexImage2D_base(rawTarget, level, internalFormat, pixels->Width(),
                           pixels->Height(), 4*pixels->Width(), 0,
                           unpackFormat, unpackType, pixelData, pixelDataLength, js::Scalar::MaxTypedArrayViewType,
                           WebGLTexelFormat::RGBA8, false);
}


//////////////////////////////////////////////////////////////////////////////////////////
// TexSubImage


void
WebGLTexture::TexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLenum unpackFormat, GLenum unpackType,
                            dom::Element* elt, ErrorResult* const out_rv)
{
    // TODO: Consolidate all the parameter validation
    // checks. Instead of spreading out the cheks in multple
    // places, consolidate into one spot.

    if (IsContextLost())
        return;

    if (!ValidateTexImageTarget(rawTexImageTarget,
                                WebGLTexImageFunc::TexSubImage,
                                WebGLTexDimensions::Tex2D))
    {
        mContext->ErrorInvalidEnumInfo("texSubImage2D: target", rawTexImageTarget);
        return;
    }

    const TexImageTarget texImageTarget(rawTexImageTarget);

    if (level < 0)
        return mContext->ErrorInvalidValue("texSubImage2D: level is negative");

    const int32_t maxLevel = MaxTextureLevelForTexImageTarget(texImageTarget);
    if (level > maxLevel) {
        mContext->ErrorInvalidValue("texSubImage2D: level %d is too large, max is %d",
                          level, maxLevel);
        return;
    }

    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    const TexInternalFormat internalFormat = imageInfo.EffectiveInternalFormat();

    // Trying to handle the video by GPU directly first
    if (TexImageFromVideoElement(texImageTarget, level,
                                 internalFormat.get(), unpackFormat, unpackType, *elt))
    {
        return;
    }

    RefPtr<gfx::DataSourceSurface> data;
    WebGLTexelFormat srcFormat;
    nsLayoutUtils::SurfaceFromElementResult res = SurfaceFromElement(*elt);
    *out_rv = SurfaceFromElementResultToImageSurface(res, data, &srcFormat);
    if (out_rv->Failed() || !data)
        return;

    gfx::IntSize size = data->GetSize();
    uint32_t byteLength = data->Stride() * size.height;
    TexSubImage2D_base(texImageTarget.get(), level, xOffset, yOffset, size.width,
                       size.height, data->Stride(), unpackFormat, unpackType, data->GetData(),
                       byteLength, js::Scalar::MaxTypedArrayViewType, srcFormat,
                       res.mIsPremultiplied);
}

void
WebGLTexture::TexSubImage2D_base(GLenum rawImageTarget, GLint level,
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

    if (!ValidateTexImageTarget(rawImageTarget, func, dims))
        return;

    TexImageTarget texImageTarget(rawImageTarget);

    if (!HasImageInfoAt(texImageTarget, level))
        return mContext->ErrorInvalidOperation("texSubImage2D: no previously defined texture image");

    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.EffectiveInternalFormat();

    if (!ValidateTexImage(texImageTarget, level,
                          existingEffectiveInternalFormat.get(),
                          xOffset, yOffset, 0,
                          width, height, 0,
                          0, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (!ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    if (unpackType != TypeFromInternalFormat(existingEffectiveInternalFormat)) {
        return mContext->ErrorInvalidOperation("texSubImage2D: unpackType differs from that of the existing image");
    }

    size_t srcTexelSize = size_t(-1);
    if (srcFormat == WebGLTexelFormat::Auto) {
        const size_t bitsPerTexel = GetBitsPerTexel(existingEffectiveInternalFormat);
        MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed unpackFormats here.
        srcTexelSize = bitsPerTexel / 8;
    } else {
        srcTexelSize = WebGLTexelConversions::TexelBytesForFormat(srcFormat);
    }

    if (width == 0 || height == 0)
        return; // ES 2.0 says it has no effect, we better return right now

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, 1, srcTexelSize, mPixelStoreUnpackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * srcTexelSize;

    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize.value(), mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, byteLength);

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                width == imageInfo.Width() &&
                                height == imageInfo.Height();
        if (coversWholeImage) {
            SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }
    MakeContextCurrent();

    size_t   srcStride = srcStrideOrZero ? srcStrideOrZero : checked_alignedRowSize.value();
    uint32_t dstTexelSize = GetBitsPerTexel(existingEffectiveInternalFormat) / 8;
    size_t   dstPlainRowSize = dstTexelSize * width;
    // There are checks above to ensure that this won't overflow.
    size_t   dstStride = RoundedToNextMultipleOf(dstPlainRowSize, mPixelStoreUnpackAlignment).value();

    void* pixels = data;
    nsAutoArrayPtr<uint8_t> convertedData;

    WebGLTexelFormat dstFormat = GetWebGLTexelFormat(existingEffectiveInternalFormat);
    WebGLTexelFormat actualSrcFormat = srcFormat == WebGLTexelFormat::Auto ? dstFormat : srcFormat;

    // no conversion, no flipping, so we avoid copying anything and just pass the source pointer
    bool noConversion = (actualSrcFormat == dstFormat &&
                         srcPremultiplied == mPixelStorePremultiplyAlpha &&
                         srcStride == dstStride &&
                         !mPixelStoreFlipY);

    if (!noConversion) {
        size_t convertedDataSize = height * dstStride;
        convertedData = new (fallible) uint8_t[convertedDataSize];
        if (!convertedData) {
            mContext->ErrorOutOfMemory("texImage2D: Ran out of memory when allocating"
                             " a buffer for doing unpackFormat conversion.");
            return;
        }
        if (!ConvertImage(width, height, srcStride, dstStride,
                          static_cast<const uint8_t*>(data), convertedData,
                          actualSrcFormat, srcPremultiplied,
                          dstFormat, mPixelStorePremultiplyAlpha, dstTexelSize))
        {
            return mContext->ErrorInvalidOperation("texSubImage2D: Unsupported texture unpackFormat conversion");
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
WebGLTexture::TexSubImage2D(GLenum rawTarget, GLint level,
                            GLint xOffset, GLint yOffset,
                            GLsizei width, GLsizei height,
                            GLenum unpackFormat, GLenum unpackType,
                            const dom::Nullable<ArrayBufferView>& pixels,
                            ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (pixels.IsNull())
        return mContext->ErrorInvalidValue("texSubImage2D: pixels must not be null!");

    const ArrayBufferView& view = pixels.Value();
    view.ComputeLengthAndData();

    if (!ValidateTexImageTarget(rawTarget, WebGLTexImageFunc::TexSubImage, WebGLTexDimensions::Tex2D))
        return;

    return TexSubImage2D_base(rawTarget, level, xOffset, yOffset,
                              width, height, 0, unpackFormat, unpackType,
                              view.Data(), view.Length(), view.Type(),
                              WebGLTexelFormat::Auto, false);
}

void
WebGLTexture::TexSubImage2D(GLenum target, GLint level,
                            GLint xOffset, GLint yOffset,
                            GLenum unpackFormat, GLenum unpackType, ImageData* pixels,
                            ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (!pixels)
        return mContext->ErrorInvalidValue("texSubImage2D: pixels must not be null!");

    Uint8ClampedArray arr;
    DebugOnly<bool> inited = arr.Init(pixels->GetDataObject());
    MOZ_ASSERT(inited);
    arr.ComputeLengthAndData();

    return TexSubImage2D_base(target, level, xOffset, yOffset,
                              pixels->Width(), pixels->Height(),
                              4*pixels->Width(), unpackFormat, unpackType,
                              arr.Data(), arr.Length(),
                              js::Scalar::MaxTypedArrayViewType,
                              WebGLTexelFormat::RGBA8, false);
}


//////////////////////////////////////////////////////////////////////////////////////////
// CopyTex(Sub)Image


void
WebGLTexture::CopyTexSubImage2D_base(TexImageTarget texImageTarget, GLint level,
                                     TexInternalFormat internalformat,
                                     GLint xOffset, GLint yOffset, GLint x,
                                     GLint y, GLsizei width, GLsizei height,
                                     bool sub)
{
    const WebGLRectangleObject* framebufferRect = CurValidReadFBRectObject();
    GLsizei framebufferWidth = framebufferRect ? framebufferRect->Width() : 0;
    GLsizei framebufferHeight = framebufferRect ? framebufferRect->Height() : 0;

    WebGLTexImageFunc func = sub
                             ? WebGLTexImageFunc::CopyTexSubImage
                             : WebGLTexImageFunc::CopyTexImage;
    WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;
    const char* info = InfoFrom(func, dims);

    // TODO: This changes with color_buffer_float. Reassess when the
    // patch lands.
    if (!ValidateTexImage(texImageTarget, level, internalformat.get(),
                          xOffset, yOffset, 0,
                          width, height, 0,
                          0,
                          LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    if (!ValidateCopyTexImage(internalformat.get(), func, dims))
        return;

    if (!mBoundReadFramebuffer)
        mContext->ClearBackbufferIfNeeded();

    MakeContextCurrent();

    if (mImmutable) {
        if (!sub) {
            return mContext->ErrorInvalidOperation("copyTexImage2D: disallowed because the texture bound to this target has already been made immutable by texStorage2D");
        }
    }

    TexType framebuffertype = LOCAL_GL_NONE;
    if (mBoundReadFramebuffer) {
        TexInternalFormat framebuffereffectiveformat = mBoundReadFramebuffer->ColorAttachment(0).EffectiveInternalFormat();
        framebuffertype = TypeFromInternalFormat(framebuffereffectiveformat);
    } else {
        // FIXME - here we're assuming that the default framebuffer is backed by UNSIGNED_BYTE
        // that might not always be true, say if we had a 16bpp default framebuffer.
        framebuffertype = LOCAL_GL_UNSIGNED_BYTE;
    }

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromUnsizedInternalFormatAndType(internalformat, framebuffertype);

    // this should never fail, validation happened earlier.
    MOZ_ASSERT(effectiveInternalFormat != LOCAL_GL_NONE);

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
    if (!sub && HasImageInfoAt(texImageTarget, level)) {
        const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
        sizeMayChange = width != imageInfo.Width() ||
                        height != imageInfo.Height() ||
                        effectiveInternalFormat != imageInfo.EffectiveInternalFormat();
    }

    if (sizeMayChange)
        GetAndFlushUnderlyingGLErrors();

    if (CanvasUtils::CheckSaneSubrectSize(x, y, width, height, framebufferWidth, framebufferHeight)) {
        if (sub)
            gl->fCopyTexSubImage2D(texImageTarget.get(), level, xOffset, yOffset, x, y, width, height);
        else
            gl->fCopyTexImage2D(texImageTarget.get(), level, internalformat.get(), x, y, width, height, 0);
    } else {

        // the rect doesn't fit in the framebuffer

        // first, we initialize the texture as black
        if (!sub) {
            SetImageInfo(texImageTarget, level, width, height, 1,
                      effectiveInternalFormat,
                      WebGLImageDataStatus::UninitializedImageData);
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }

        // if we are completely outside of the framebuffer, we can exit now with our black texture
        if (   x >= framebufferWidth
            || x+width <= 0
            || y >= framebufferHeight
            || y+height <= 0)
        {
            // we are completely outside of range, can exit now with buffer filled with zeros
            return DummyFramebufferOperation(info);
        }

        GLint   actual_x             = clamped(x, 0, framebufferWidth);
        GLint   actual_x_plus_width  = clamped(x + width, 0, framebufferWidth);
        GLsizei actual_width   = actual_x_plus_width  - actual_x;
        GLint   actual_xoffset = xOffset + actual_x - x;

        GLint   actual_y             = clamped(y, 0, framebufferHeight);
        GLint   actual_y_plus_height = clamped(y + height, 0, framebufferHeight);
        GLsizei actual_height  = actual_y_plus_height - actual_y;
        GLint   actual_yoffset = yOffset + actual_y - y;

        gl->fCopyTexSubImage2D(texImageTarget.get(), level, actual_xoffset, actual_yoffset, actual_x, actual_y, actual_width, actual_height);
    }

    if (sizeMayChange) {
        GLenum error = GetAndFlushUnderlyingGLErrors();
        if (error) {
            mContext->GenerateWarning("copyTexImage2D generated error %s", ErrorName(error));
            return;
        }
    }

    if (!sub) {
        SetImageInfo(texImageTarget, level, width, height, 1,
                          effectiveInternalFormat,
                          WebGLImageDataStatus::InitializedImageData);
    }
}

void
WebGLTexture::CopyTexImage2D(GLenum rawTexImgTarget,
                             GLint level,
                             GLenum internalformat,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLint border)
{
    if (IsContextLost())
        return;

    // copyTexImage2D only generates textures with unpackType = UNSIGNED_BYTE
    const WebGLTexImageFunc func = WebGLTexImageFunc::CopyTexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (!ValidateTexImageTarget(rawTexImgTarget, func, dims))
        return;

    if (!ValidateTexImage(rawTexImgTarget, level, internalformat,
                          0, 0, 0,
                          width, height, 0,
                          border, LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    if (!ValidateCopyTexImage(internalformat, func, dims))
        return;

    if (!mBoundReadFramebuffer)
        mContext->ClearBackbufferIfNeeded();

    CopyTexSubImage2D_base(rawTexImgTarget, level, internalformat, 0, 0, x, y, width, height, false);
}

void
WebGLTexture::CopyTexSubImage2D(GLenum rawTexImgTarget,
                                GLint level,
                                GLint xOffset,
                                GLint yOffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height)
{
    if (IsContextLost())
        return;

    switch (rawTexImgTarget) {
        case LOCAL_GL_TEXTURE_2D:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            return mContext->ErrorInvalidEnumInfo("copyTexSubImage2D: target", rawTexImgTarget);
    }

    const TexImageTarget texImageTarget(rawTexImgTarget);

    if (level < 0)
        return mContext->ErrorInvalidValue("copyTexSubImage2D: level may not be negative");

    GLsizei maxTextureSize = MaxTextureSizeForTarget(TexImageTargetToTexTarget(texImageTarget));
    if (!(maxTextureSize >> level))
        return mContext->ErrorInvalidValue("copyTexSubImage2D: 2^level exceeds maximum texture size");

    if (width < 0 || height < 0)
        return mContext->ErrorInvalidValue("copyTexSubImage2D: width and height may not be negative");

    if (xoffset < 0 || yOffset < 0)
        return mContext->ErrorInvalidValue("copyTexSubImage2D: xOffset and yOffset may not be negative");

    if (!HasImageInfoAt(texImageTarget, level))
        return mContext->ErrorInvalidOperation("copyTexSubImage2D: no texture image previously defined for this level and face");

    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    GLsizei texWidth = imageInfo.Width();
    GLsizei texHeight = imageInfo.Height();

    if (xoffset + width > texWidth || xOffset + width < 0)
      return mContext->ErrorInvalidValue("copyTexSubImage2D: xOffset+width is too large");

    if (yoffset + height > texHeight || yOffset + height < 0)
      return mContext->ErrorInvalidValue("copyTexSubImage2D: yOffset+height is too large");

    if (!mBoundReadFramebuffer)
        mContext->ClearBackbufferIfNeeded();

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                width == texWidth &&
                                height == texHeight;
        if (coversWholeImage) {
            SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    TexInternalFormat internalformat;
    TexType unpackType;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(imageInfo.EffectiveInternalFormat(),
                                             &internalformat, &type);
    CopyTexSubImage2D_base(texImageTarget, level, internalformat, xOffset, yOffset, x, y, width, height, true);
}


//////////////////////////////////////////////////////////////////////////////////////////
// CompressedTex(Sub)Image


void
WebGLTexture::CompressedTexImage2D(GLenum rawTexImgTarget,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width, GLsizei height, GLint border,
                                   const ArrayBufferView& view)
{
    if (IsContextLost())
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::CompTexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (!ValidateTexImageTarget(rawTexImgTarget, func, dims))
        return;

    if (!ValidateTexImage(rawTexImgTarget, level, internalformat,
                          0, 0, 0, width, height, 0,
                          border, LOCAL_GL_NONE,
                          LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    view.ComputeLengthAndData();

    uint32_t byteLength = view.Length();
    if (!ValidateCompTexImageDataSize(level, internalformat, width, height, byteLength, func, dims)) {
        return;
    }

    if (!ValidateCompTexImageSize(level, internalformat, 0, 0, width, height, width, height, func, dims))
    {
        return;
    }

    const TexImageTarget texImageTarget(rawTexImgTarget);

    if (mImmutable) {
        return mContext->ErrorInvalidOperation(
            "compressedTexImage2D: disallowed because the texture bound to "
            "this target has already been made immutable by texStorage2D");
    }

    MakeContextCurrent();
    gl->fCompressedTexImage2D(texImageTarget.get(), level, internalformat, width, height, border, byteLength, view.Data());

    SetImageInfo(texImageTarget, level, width, height, 1, internalformat,
                      WebGLImageDataStatus::InitializedImageData);
}

void
WebGLTexture::CompressedTexSubImage2D(GLenum rawTexImgTarget, GLint level, GLint xOffset,
                                      GLint yOffset, GLsizei width, GLsizei height,
                                      GLenum internalformat,
                                      const ArrayBufferView& view)
{
    if (IsContextLost())
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::CompTexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (!ValidateTexImageTarget(rawTexImgTarget, func, dims))
        return;

    if (!ValidateTexImage(rawTexImgTarget,
                          level, internalformat,
                          xOffset, yOffset, 0,
                          width, height, 0,
                          0, LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    const TexImageTarget texImageTarget(rawTexImgTarget);

    WebGLTexture::ImageInfo& levelInfo = ImageInfoAt(texImageTarget, level);

    if (internalformat != levelInfo.EffectiveInternalFormat()) {
        return mContext->ErrorInvalidOperation("compressedTexImage2D: internalformat does not match the existing image");
    }

    view.ComputeLengthAndData();

    uint32_t byteLength = view.Length();
    if (!ValidateCompTexImageDataSize(level, internalformat, width, height, byteLength, func, dims))
        return;

    if (!ValidateCompTexImageSize(level, internalformat,
                                  xOffset, yOffset,
                                  width, height,
                                  levelInfo.Width(), levelInfo.Height(),
                                  func, dims))
    {
        return;
    }

    if (levelInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                width == levelInfo.Width() &&
                                height == levelInfo.Height();
        if (coversWholeImage) {
            SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    MakeContextCurrent();
    gl->fCompressedTexSubImage2D(texImageTarget.get(), level, xOffset, yOffset, width, height, internalformat, byteLength, view.Data());
}




















































void
WebGLTexture::TexImage3D(TexImageTarget texImageTarget, GLint level,
                         GLenum rawInternalFormat, GLsizei width, GLsizei height,
                         GLsizei depth, GLint border, GLenum unpackFormat,
                         GLenum unpackType,
                         const dom::Nullable<dom::ArrayBufferView>& maybeView,
                         ErrorResult* const out_rv)
{
    if (texImageTarget != LOCAL_GL_TEXTURE_3D)
        return mContext->ErrorInvalidEnum("texStorage3D: target must be TEXTURE_3D.");

    void* data;
    size_t dataLength;
    js::Scalar::Type jsArrayType;
    if (maybeView.IsNull()) {
        data = nullptr;
        dataLength = 0;
        jsArrayType = js::Scalar::MaxTypedArrayViewType;
    } else {
        const dom::ArrayBufferView& view = maybeView.Value();
        view.ComputeLengthAndData();

        data = view.Data();
        dataLength = view.Length();
        jsArrayType = view.Type();
    }

    const WebGLTexImageFunc func = WebGLTexImageFunc::TexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex3D;

    if (!ValidateTexImage(texImageTarget, level, rawInternalFormat,
                          0, 0, 0,
                          width, height, depth,
                          border, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (!ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    TexInternalFormat internalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(rawInternalFormat, unpackType);

    if (internalFormat == LOCAL_GL_NONE) {
        return mContext->ErrorInvalidOperation("texImage3D: bad combination of internalformat and unpackType");
    }

    // we need to find the exact sized unpackFormat of the source data. Slightly abusing
    // EffectiveInternalFormatFromInternalFormatAndType for that purpose. Really, an unsized source unpackFormat
    // is the same thing as an unsized internalformat.
    TexInternalFormat effectiveUnpackFormat =
        EffectiveInternalFormatFromInternalFormatAndType(unpackFormat, unpackType);
    MOZ_ASSERT(effectiveSourceFormat != LOCAL_GL_NONE); // should have validated unpackFormat/type combo earlier
    const size_t srcbitsPerTexel = GetBitsPerTexel(effectiveUnpackFormat);
    MOZ_ASSERT((srcbitsPerTexel % 8) == 0); // should not have compressed unpackFormats here.
    size_t srcTexelSize = srcbitsPerTexel / 8;

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, depth, srcTexelSize, mPixelStoreUnpackAlignment);

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

    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
    GLenum driverUnpackType = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             internalFormat,
                                             &driverInternalFormat,
                                             &driverUnpackFormat,
                                             &driverUnpackType);

    MakeContextCurrent();
    GetAndFlushUnderlyingGLErrors();
    gl->fTexImage3D(texImageTarget.get(), level,
                    driverInternalFormat,
                    width, height, depth,
                    0, driverUnpackFormat, driverUnpackType,
                    data);
    GLenum error = GetAndFlushUnderlyingGLErrors();
    if (error) {
        mContext->GenerateWarning("texImage3D generated error %s", ErrorName(error));
        return;
    }

    SetImageInfo(texImageTarget, level,
                      width, height, depth,
                      internalFormat,
                      data ? WebGLImageDataStatus::InitializedImageData
                           : WebGLImageDataStatus::UninitializedImageData);
}

void
WebGLTexture::TexSubImage3D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                            GLint yOoffset, GLint zOffset, GLsizei width, GLsizei height,
                            GLsizei depth, GLenum unpackFormat, GLenum unpackType,
                            const dom::Nullable<dom::ArrayBufferView>& maybeView,
                            ErrorResult* const out_rv)
{
    if (texImageTarget != LOCAL_GL_TEXTURE_3D)
        return mContext->ErrorInvalidEnum("texStorage3D: target must be TEXTURE_3D.");

    if (maybeView.IsNull())
        return mContext->ErrorInvalidValue("texSubImage3D: pixels must not be null!");

    const dom::ArrayBufferView& view = maybeView.Value();
    view.ComputeLengthAndData();

    const WebGLTexImageFunc func = WebGLTexImageFunc::TexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex3D;

    if (!HasImageInfoAt(texImageTarget, level)) {
        return mContext->ErrorInvalidOperation("texSubImage3D: no previously defined texture image");
    }

    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.EffectiveInternalFormat();
    TexInternalFormat existingUnsizedInternalFormat = LOCAL_GL_NONE;
    TexType existingType = LOCAL_GL_NONE;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(existingEffectiveInternalFormat,
                                                            &existingUnsizedInternalFormat,
                                                            &existingType);

    const GLint border = 0;
    if (!ValidateTexImage(texImageTarget, level, existingEffectiveInternalFormat.get(),
                          xOffset, yOffset, zOffset, width, height, depth, border,
                          unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (unpackType != existingType) {
        return mContext->ErrorInvalidOperation("texSubImage3D: unpackType differs from that of the existing image");
    }

    js::Scalar::Type jsArrayType = view.Type();
    void* data = view.Data();
    size_t dataLength = view.Length();

    if (!ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    const size_t bitsPerTexel = GetBitsPerTexel(existingEffectiveInternalFormat);
    MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed unpackFormats here.
    size_t srcTexelSize = bitsPerTexel / 8;

    if (width == 0 || height == 0 || depth == 0)
        return; // no effect, we better return right now

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, depth, srcTexelSize, mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (dataLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, dataLength);

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                zOffset == 0 &&
                                width == imageInfo.Width() &&
                                height == imageInfo.Height() &&
                                depth == imageInfo.Depth();
        if (coversWholeImage) {
            SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
    GLenum driverUnpackType = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             existingEffectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverUnpackFormat,
                                             &driverUnpackType);

    MakeContextCurrent();
    gl->fTexSubImage3D(texImageTarget.get(), level,
                       xOffset, yOffset, zOffset,
                       width, height, depth,
                       driverUnpackFormat, driverUnpackType, data);
}

void
WebGLTexture::TexStorage2D(TexTarget texTarget, GLsizei levels, GLenum rawInternalFormat,
                           GLsizei width, GLsizei height)
{
    switch (texTarget.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP:
        break;

    default:
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

    GetAndFlushUnderlyingGLErrors();
    gl->fTexStorage2D(target, levels, internalFormat.get(), width, height);
    GLenum error = GetAndFlushUnderlyingGLErrors();
    if (error) {
        mContext->GenerateWarning("texStorage2D generated error %s", ErrorName(error));
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

    if (!ValidateTexStorage(target, levels, rawInternalFormat, width, height, depth,
                            "texStorage3D"))
    {
        return;
    }

    TexInternalFormat internalFormat(rawInternalFormat);

    GetAndFlushUnderlyingGLErrors();
    gl->fTexStorage3D(target, levels, internalFormat.get(), width, height, depth);
    GLenum error = GetAndFlushUnderlyingGLErrors();
    if (error) {
        mContext->GenerateWarning("texStorage3D generated error %s", ErrorName(error));
        return;
    }

    SpecifyTexStorage(levels, internalFormat, width, height, depth);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLTexture)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTexture, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTexture, Release)

} // namespace mozilla
