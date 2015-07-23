/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLBlitHelper.h"
#include "ImageContainer.h"
#include "mozilla/dom/HTMLVideoElement.h"

#include "WebGLContextUtils.h"
#include "WebGLBuffer.h"
#include "WebGLVertexAttribData.h"
#include "WebGLShader.h"
#include "WebGLProgram.h"
#include "WebGLUniformLocation.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShaderPrecisionFormat.h"
#include "WebGLTexture.h"
#include "WebGLExtensions.h"
#include "WebGLVertexArray.h"

#include "nsString.h"
#include "nsDebug.h"
#include "nsReadableUtils.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "GLContext.h"

#include "nsContentUtils.h"
#include "nsError.h"
#include "nsLayoutUtils.h"

#include "CanvasUtils.h"
#include "gfxUtils.h"

#include "jsfriendapi.h"

#include "WebGLTexelConversions.h"
#include "WebGLValidateStrings.h"
#include <algorithm>

// needed to check if current OS is lower than 10.7
#if defined(MOZ_WIDGET_COCOA)
#include "nsCocoaFeatures.h"
#endif

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/Endian.h"

namespace mozilla {


//////////////////////////////////////////////////////////////////////////////////////////
// TexImage

bool
WebGLContext::TexImageFromVideoElement(const TexImageTarget texImageTarget,
                                       GLint level, GLenum internalFormat,
                                       GLenum format, GLenum type,
                                       mozilla::dom::Element& elt)
{
    if (!ValidateTexImageFormatAndType(format, type,
                                       WebGLTexImageFunc::TexImage,
                                       WebGLTexDimensions::Tex2D))
    {
        return false;
    }

    HTMLVideoElement* video = HTMLVideoElement::FromContentOrNull(&elt);
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
        nsresult rv = mCanvasElement->NodePrincipal()->Subsumes(principal, &subsumes);
        if (NS_FAILED(rv) || !subsumes) {
            GenerateWarning("It is forbidden to load a WebGL texture from a cross-domain element that has not been validated with CORS. "
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

    const WebGLTexture::ImageInfo& info = tex->ImageInfoAt(texImageTarget, 0);
    bool dimensionsMatch = info.Width() == srcImage->GetSize().width &&
                           info.Height() == srcImage->GetSize().height;
    if (!dimensionsMatch) {
        // we need to allocation
        gl->fTexImage2D(texImageTarget.get(), level, internalFormat,
                        srcImage->GetSize().width, srcImage->GetSize().height,
                        0, format, type, nullptr);
    }

    const gl::OriginPos destOrigin = mPixelStoreFlipY ? gl::OriginPos::BottomLeft
                                                      : gl::OriginPos::TopLeft;
    bool ok = gl->BlitHelper()->BlitImageToTexture(srcImage,
                                                   srcImage->GetSize(),
                                                   tex->mGLName,
                                                   texImageTarget.get(),
                                                   destOrigin);
    if (ok) {
        TexInternalFormat effectiveInternalFormat =
            EffectiveInternalFormatFromInternalFormatAndType(internalFormat,
                                                             type);
        MOZ_ASSERT(effectiveInternalFormat != LOCAL_GL_NONE);
        tex->SetImageInfo(texImageTarget, level, srcImage->GetSize().width,
                          srcImage->GetSize().height, 1,
                          effectiveInternalFormat,
                          WebGLImageDataStatus::InitializedImageData);
        tex->Bind(TexImageTargetToTexTarget(texImageTarget));
    }
    return ok;
}

GLenum
WebGLContext::CheckedTexImage2D(TexImageTarget texImageTarget, GLint level,
                                TexInternalFormat internalFormat, GLsizei width,
                                GLsizei height, GLint border, TexFormat unpackFormat,
                                TexType unpackType, const GLvoid* data)
{
    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    MOZ_ASSERT(tex, "no texture bound");

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);
    bool sizeMayChange = true;

    if (tex->HasImageInfoAt(texImageTarget, level)) {
        const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget,
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
WebGLContext::TexImage2D_base(TexImageTarget texImageTarget, GLint level,
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
            return ErrorInvalidOperation("texImage2D: "
                                         "with unpackFormat of DEPTH_COMPONENT or DEPTH_STENCIL, "
                                         "data must be nullptr, "
                                         "level must be zero");
    }

    if (!ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);

    if (effectiveInternalFormat == LOCAL_GL_NONE) {
        return ErrorInvalidOperation("texImage2D: bad combination of internalFormat and unpackType");
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
        return ErrorInvalidOperation("texImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength && byteLength < bytesNeeded)
        return ErrorInvalidOperation("texImage2D: not enough data for operation (need %d, have %d)",
                                 bytesNeeded, byteLength);

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);

    if (!tex)
        return ErrorInvalidOperation("texImage2D: no texture is bound to this target");

    if (tex->IsImmutable()) {
        return ErrorInvalidOperation(
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
                ErrorOutOfMemory("texImage2D: Ran out of memory when allocating"
                                 " a buffer for doing unpackFormat conversion.");
                return;
            }
            if (!ConvertImage(width, height, srcStride, dstStride,
                              static_cast<uint8_t*>(data), convertedData,
                              actualSrcFormat, srcPremultiplied,
                              dstFormat, mPixelStorePremultiplyAlpha, dstTexelSize))
            {
                return ErrorInvalidOperation("texImage2D: Unsupported texture unpackFormat conversion");
            }
            pixels = reinterpret_cast<void*>(convertedData.get());
        }
        imageInfoStatusIfSuccess = WebGLImageDataStatus::InitializedImageData;
    }

    GLenum error = CheckedTexImage2D(texImageTarget, level, internalFormat, width,
                                     height, border, unpackFormat, unpackType, pixels);

    if (error) {
        GenerateWarning("texImage2D generated error %s", ErrorName(error));
        return;
    }

    // in all of the code paths above, we should have either initialized data,
    // or allocated data and left it uninitialized, but in any case we shouldn't
    // have NoImageData at this point.
    MOZ_ASSERT(imageInfoStatusIfSuccess != WebGLImageDataStatus::NoImageData);

    tex->SetImageInfo(texImageTarget, level, width, height, 1,
                      effectiveInternalFormat, imageInfoStatusIfSuccess);
}

void
WebGLContext::TexImage2D(GLenum rawTarget, GLint level,
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
WebGLContext::TexImage2D(GLenum rawTarget, GLint level,
                         GLenum internalFormat, GLenum unpackFormat,
                         GLenum unpackType, ImageData* pixels, ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (!pixels) {
        // Spec says to generate an INVALID_VALUE error
        return ErrorInvalidValue("texImage2D: null ImageData");
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
WebGLContext::TexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xoffset,
                            GLint yoffset, GLenum format, GLenum type,
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
        ErrorInvalidEnumInfo("texSubImage2D: target", rawTexImageTarget);
        return;
    }

    const TexImageTarget texImageTarget(rawTexImageTarget);

    if (level < 0)
        return ErrorInvalidValue("texSubImage2D: level is negative");

    const int32_t maxLevel = MaxTextureLevelForTexImageTarget(texImageTarget);
    if (level > maxLevel) {
        ErrorInvalidValue("texSubImage2D: level %d is too large, max is %d",
                          level, maxLevel);
        return;
    }

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    if (!tex)
        return ErrorInvalidOperation("texSubImage2D: no texture bound on active texture unit");

    const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
    const TexInternalFormat internalFormat = imageInfo.EffectiveInternalFormat();

    // Trying to handle the video by GPU directly first
    if (TexImageFromVideoElement(texImageTarget, level,
                                 internalFormat.get(), format, type, *elt))
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
    TexSubImage2D_base(texImageTarget.get(), level, xoffset, yoffset, size.width,
                       size.height, data->Stride(), format, type, data->GetData(),
                       byteLength, js::Scalar::MaxTypedArrayViewType, srcFormat,
                       res.mIsPremultiplied);
}

void
WebGLContext::TexSubImage2D_base(GLenum rawImageTarget, GLint level,
                                 GLint xoffset, GLint yoffset,
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

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    if (!tex)
        return ErrorInvalidOperation("texSubImage2D: no texture bound on active texture unit");

    if (!tex->HasImageInfoAt(texImageTarget, level))
        return ErrorInvalidOperation("texSubImage2D: no previously defined texture image");

    const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.EffectiveInternalFormat();

    if (!ValidateTexImage(texImageTarget, level,
                          existingEffectiveInternalFormat.get(),
                          xoffset, yoffset, 0,
                          width, height, 0,
                          0, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (!ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    if (unpackType != TypeFromInternalFormat(existingEffectiveInternalFormat)) {
        return ErrorInvalidOperation("texSubImage2D: unpackType differs from that of the existing image");
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
        return ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength < bytesNeeded)
        return ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, byteLength);

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xoffset == 0 &&
                                yoffset == 0 &&
                                width == imageInfo.Width() &&
                                height == imageInfo.Height();
        if (coversWholeImage) {
            tex->SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!tex->EnsureInitializedImageData(texImageTarget, level))
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
            ErrorOutOfMemory("texImage2D: Ran out of memory when allocating"
                             " a buffer for doing unpackFormat conversion.");
            return;
        }
        if (!ConvertImage(width, height, srcStride, dstStride,
                          static_cast<const uint8_t*>(data), convertedData,
                          actualSrcFormat, srcPremultiplied,
                          dstFormat, mPixelStorePremultiplyAlpha, dstTexelSize))
        {
            return ErrorInvalidOperation("texSubImage2D: Unsupported texture unpackFormat conversion");
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

    gl->fTexSubImage2D(texImageTarget.get(), level, xoffset, yoffset, width, height, driverFormat, driverType, pixels);
}

void
WebGLContext::TexSubImage2D(GLenum rawTarget, GLint level,
                            GLint xoffset, GLint yoffset,
                            GLsizei width, GLsizei height,
                            GLenum unpackFormat, GLenum unpackType,
                            const dom::Nullable<ArrayBufferView>& pixels,
                            ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (pixels.IsNull())
        return ErrorInvalidValue("texSubImage2D: pixels must not be null!");

    const ArrayBufferView& view = pixels.Value();
    view.ComputeLengthAndData();

    if (!ValidateTexImageTarget(rawTarget, WebGLTexImageFunc::TexSubImage, WebGLTexDimensions::Tex2D))
        return;

    return TexSubImage2D_base(rawTarget, level, xoffset, yoffset,
                              width, height, 0, unpackFormat, unpackType,
                              view.Data(), view.Length(), view.Type(),
                              WebGLTexelFormat::Auto, false);
}

void
WebGLContext::TexSubImage2D(GLenum target, GLint level,
                            GLint xoffset, GLint yoffset,
                            GLenum unpackFormat, GLenum unpackType, ImageData* pixels,
                            ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (!pixels)
        return ErrorInvalidValue("texSubImage2D: pixels must not be null!");

    Uint8ClampedArray arr;
    DebugOnly<bool> inited = arr.Init(pixels->GetDataObject());
    MOZ_ASSERT(inited);
    arr.ComputeLengthAndData();

    return TexSubImage2D_base(target, level, xoffset, yoffset,
                              pixels->Width(), pixels->Height(),
                              4*pixels->Width(), unpackFormat, unpackType,
                              arr.Data(), arr.Length(),
                              js::Scalar::MaxTypedArrayViewType,
                              WebGLTexelFormat::RGBA8, false);
}


//////////////////////////////////////////////////////////////////////////////////////////
// CopyTex(Sub)Image


void
WebGLContext::CopyTexSubImage2D_base(TexImageTarget texImageTarget, GLint level,
                                     TexInternalFormat internalformat,
                                     GLint xoffset, GLint yoffset, GLint x,
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
                          xoffset, yoffset, 0,
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
        ClearBackbufferIfNeeded();

    MakeContextCurrent();

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);

    if (!tex)
        return ErrorInvalidOperation("%s: no texture is bound to this target");

    if (tex->IsImmutable()) {
        if (!sub) {
            return ErrorInvalidOperation("copyTexImage2D: disallowed because the texture bound to this target has already been made immutable by texStorage2D");
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
        // xoffset=0, yoffset=2, x=0, y=0, width=0, height=0) from a 300x150 FB
        // to a 0x2 texture. This a useless thing to do, but technically legal.
        // NV331.38 generates INVALID_VALUE.
        return DummyFramebufferOperation(info);
    }

    // check if the memory size of this texture may change with this call
    bool sizeMayChange = !sub;
    if (!sub && tex->HasImageInfoAt(texImageTarget, level)) {
        const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
        sizeMayChange = width != imageInfo.Width() ||
                        height != imageInfo.Height() ||
                        effectiveInternalFormat != imageInfo.EffectiveInternalFormat();
    }

    if (sizeMayChange)
        GetAndFlushUnderlyingGLErrors();

    if (CanvasUtils::CheckSaneSubrectSize(x, y, width, height, framebufferWidth, framebufferHeight)) {
        if (sub)
            gl->fCopyTexSubImage2D(texImageTarget.get(), level, xoffset, yoffset, x, y, width, height);
        else
            gl->fCopyTexImage2D(texImageTarget.get(), level, internalformat.get(), x, y, width, height, 0);
    } else {

        // the rect doesn't fit in the framebuffer

        // first, we initialize the texture as black
        if (!sub) {
            tex->SetImageInfo(texImageTarget, level, width, height, 1,
                      effectiveInternalFormat,
                      WebGLImageDataStatus::UninitializedImageData);
            if (!tex->EnsureInitializedImageData(texImageTarget, level))
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
        GLint   actual_xoffset = xoffset + actual_x - x;

        GLint   actual_y             = clamped(y, 0, framebufferHeight);
        GLint   actual_y_plus_height = clamped(y + height, 0, framebufferHeight);
        GLsizei actual_height  = actual_y_plus_height - actual_y;
        GLint   actual_yoffset = yoffset + actual_y - y;

        gl->fCopyTexSubImage2D(texImageTarget.get(), level, actual_xoffset, actual_yoffset, actual_x, actual_y, actual_width, actual_height);
    }

    if (sizeMayChange) {
        GLenum error = GetAndFlushUnderlyingGLErrors();
        if (error) {
            GenerateWarning("copyTexImage2D generated error %s", ErrorName(error));
            return;
        }
    }

    if (!sub) {
        tex->SetImageInfo(texImageTarget, level, width, height, 1,
                          effectiveInternalFormat,
                          WebGLImageDataStatus::InitializedImageData);
    }
}

void
WebGLContext::CopyTexImage2D(GLenum rawTexImgTarget,
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

    // copyTexImage2D only generates textures with type = UNSIGNED_BYTE
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
        ClearBackbufferIfNeeded();

    CopyTexSubImage2D_base(rawTexImgTarget, level, internalformat, 0, 0, x, y, width, height, false);
}

void
WebGLContext::CopyTexSubImage2D(GLenum rawTexImgTarget,
                                GLint level,
                                GLint xoffset,
                                GLint yoffset,
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
            return ErrorInvalidEnumInfo("copyTexSubImage2D: target", rawTexImgTarget);
    }

    const TexImageTarget texImageTarget(rawTexImgTarget);

    if (level < 0)
        return ErrorInvalidValue("copyTexSubImage2D: level may not be negative");

    GLsizei maxTextureSize = MaxTextureSizeForTarget(TexImageTargetToTexTarget(texImageTarget));
    if (!(maxTextureSize >> level))
        return ErrorInvalidValue("copyTexSubImage2D: 2^level exceeds maximum texture size");

    if (width < 0 || height < 0)
        return ErrorInvalidValue("copyTexSubImage2D: width and height may not be negative");

    if (xoffset < 0 || yoffset < 0)
        return ErrorInvalidValue("copyTexSubImage2D: xoffset and yoffset may not be negative");

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    if (!tex)
        return ErrorInvalidOperation("copyTexSubImage2D: no texture bound to this target");

    if (!tex->HasImageInfoAt(texImageTarget, level))
        return ErrorInvalidOperation("copyTexSubImage2D: no texture image previously defined for this level and face");

    const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
    GLsizei texWidth = imageInfo.Width();
    GLsizei texHeight = imageInfo.Height();

    if (xoffset + width > texWidth || xoffset + width < 0)
      return ErrorInvalidValue("copyTexSubImage2D: xoffset+width is too large");

    if (yoffset + height > texHeight || yoffset + height < 0)
      return ErrorInvalidValue("copyTexSubImage2D: yoffset+height is too large");

    if (!mBoundReadFramebuffer)
        ClearBackbufferIfNeeded();

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xoffset == 0 &&
                                yoffset == 0 &&
                                width == texWidth &&
                                height == texHeight;
        if (coversWholeImage) {
            tex->SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!tex->EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    TexInternalFormat internalformat;
    TexType type;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(imageInfo.EffectiveInternalFormat(),
                                             &internalformat, &type);
    return CopyTexSubImage2D_base(texImageTarget, level, internalformat, xoffset, yoffset, x, y, width, height, true);
}


//////////////////////////////////////////////////////////////////////////////////////////
// CompressedTex(Sub)Image


void
WebGLContext::CompressedTexImage2D(GLenum rawTexImgTarget,
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

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    MOZ_ASSERT(tex);
    if (tex->IsImmutable()) {
        return ErrorInvalidOperation(
            "compressedTexImage2D: disallowed because the texture bound to "
            "this target has already been made immutable by texStorage2D");
    }

    MakeContextCurrent();
    gl->fCompressedTexImage2D(texImageTarget.get(), level, internalformat, width, height, border, byteLength, view.Data());

    tex->SetImageInfo(texImageTarget, level, width, height, 1, internalformat,
                      WebGLImageDataStatus::InitializedImageData);
}

void
WebGLContext::CompressedTexSubImage2D(GLenum rawTexImgTarget, GLint level, GLint xoffset,
                                      GLint yoffset, GLsizei width, GLsizei height,
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
                          xoffset, yoffset, 0,
                          width, height, 0,
                          0, LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    const TexImageTarget texImageTarget(rawTexImgTarget);

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    MOZ_ASSERT(tex);
    WebGLTexture::ImageInfo& levelInfo = tex->ImageInfoAt(texImageTarget, level);

    if (internalformat != levelInfo.EffectiveInternalFormat()) {
        return ErrorInvalidOperation("compressedTexImage2D: internalformat does not match the existing image");
    }

    view.ComputeLengthAndData();

    uint32_t byteLength = view.Length();
    if (!ValidateCompTexImageDataSize(level, internalformat, width, height, byteLength, func, dims))
        return;

    if (!ValidateCompTexImageSize(level, internalformat,
                                  xoffset, yoffset,
                                  width, height,
                                  levelInfo.Width(), levelInfo.Height(),
                                  func, dims))
    {
        return;
    }

    if (levelInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xoffset == 0 &&
                                yoffset == 0 &&
                                width == levelInfo.Width() &&
                                height == levelInfo.Height();
        if (coversWholeImage) {
            tex->SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!tex->EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    MakeContextCurrent();
    gl->fCompressedTexSubImage2D(texImageTarget.get(), level, xoffset, yoffset, width, height, internalformat, byteLength, view.Data());
}

} // namespace mozilla
