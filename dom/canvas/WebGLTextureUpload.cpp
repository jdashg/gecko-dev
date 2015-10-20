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

/* This file handles:
 * TexStorage2D(texTarget, levels, internalFormat, width, height)
 * TexStorage3D(texTarget, levels, intenralFormat, width, height, depth)
 *
 * TexImage2D(texImageTarget, level, internalFormat, width, height, border, unpackFormat,
 *            unpackType, data)
 * TexImage3D(texImageTarget, level, internalFormat, width, height, depth, border,
 *            unpackFormat, unpackType, data)
 * TexSubImage2D(texImageTarget, level, xOffset, yOffset, width, height, unpackFormat,
 *               unpackType, data)
 * TexSubImage3D(texImageTarget, level, xOffset, yOffset, zOffset, width, height, depth,
 *               unpackFormat, unpackType, data)
 *
 * CompressedTexImage2D(texImageTarget, level, internalFormat, width, height, border,
 *                      imageSize, data)
 * CompressedTexImage3D(texImageTarget, level, internalFormat, width, height, depth,
 *                      border, imageSize, data)
 * CompressedTexSubImage2D(texImageTarget, level, xOffset, yOffset, width, height,
 *                         sizedUnpackFormat, imageSize, data)
 * CompressedTexSubImage3D(texImageTarget, level, xOffset, yOffset, zOffset, width,
 *                         height, depth, sizedUnpackFormat, imageSize, data)
 *
 * CopyTexImage2D(texImageTarget, level, internalFormat, x, y, width, height, border)
 * CopyTexImage3D - "Because the framebuffer is inhererntly two-dimensional, there is no
 *                   CopyTexImage3D command."
 * CopyTexSubImage2D(texImageTarget, level, xOffset, yOffset, x, y, width, height)
 * CopyTexSubImage3D(texImageTarget, level, xOffset, yOffset, zOffset, x, y, width,
 *                   height)
 */

//////////////////////////////////////////////////////////////////////////////////////////
// Some functions need an extra level of indirection, particularly for DOM Elements.

static bool
IsElemValidForCORS(WebGLContext* webgl, dom::HTMLMediaElement* elem)
{
    if (elem->GetCORSMode() == CORS_NONE) {
        nsIPrincipal* srcPrincipal = elem->GetCurrentPrincipal();
        if (!principal)
            return false;

        nsIPrincipal* dstPrincipal = webgl->GetCanvas()->NodePrincipal();

        bool subsumes;
        nsresult rv = dstPrincipal->Subsumes(srcPrincipal, &subsumes);
        if (NS_FAILED(rv) || !subsumes)
            return false;
    }

    return true;
}

static bool
ValidateElemForCORS(WebGLContext* webgl, const char* funcName,
                    dom::HTMLMediaElement* elem, ErrorResult* const out_rv)
{
    if (IsElemValidForCORS(webgl, elem))
        return true;

    static const char[] kInfoURL = "https://developer.mozilla.org/en/WebGL/Cross-Domain_Textures";
    webgl->GenerateWarning("%s: It is forbidden to load a WebGL texture from a"
                           " cross-domain element that has not been validated with CORS."
                           " See %s",
                           funcName, kInfoURL);

    webgl->ErrorInvalidOperation("%s: Cannot upload CORS-invalid data.", funcName);
    out_rv->Throw(NS_ERROR_DOM_SECURITY_ERR);
    return false;
}

static already_AddRefed<mozilla::layers::Image>
ImageFromElement(dom::HTMLMediaElement* mediaElem, WebGLContext* webgl)
{
    uint16_t readyState;
    if (NS_SUCCEEDED(mediaElem->GetReadyState(&readyState)) &&
        readyState < nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA)
    {
        // No frame inside, just return
        return nullptr;
    }

    RefPtr<layers::ImageContainer> container = mediaElem->GetImageContainer();
    if (!container)
        return nullptr;

    nsAutoTArray<layers::ImageContainer::OwningImage, 4> currentImages;
    container->GetCurrentImages(&currentImages);

    if (!currentImages.Length())
        return nullptr;

    return currentImages[0].mImage;
}

static already_AddRefed<gfx::DataSourceSurface>
DataFromElement(dom::HTMLMediaElement* mediaElem, WebGLContext* webgl)
{
    const auto sfeRes = webgl->SurfaceFromElement(mediaElem);

    RefPtr<gfx::DataSourceSurface> data;
    WebGLTexelFormat srcFormat;
    nsresult rv = webgl->SurfaceFromElementResultToImageSurface(sfeRes, &data,
                                                                &srcFormat);
    if (NS_FAILED(rv) || !data)
        return nullptr;

    return data.forget();
}

////////////////////////////////////////
// ArrayBufferView?

static inline bool
DoesJSTypeMatchUnpackType(GLenum unpackType, js::Scalar::Type jsType)
{
    switch (unpackType) {
    case LOCAL_GL_BYTE:
        return jsType == js::Scalar::Type::Int8;

    case LOCAL_GL_UNSIGNED_BYTE:
        return jsType == js::Scalar::Type::Uint8 ||
               jsType == js::Scalar::Type::Uint8Clamped;

    case LOCAL_GL_SHORT:
        return jsType == js::Scalar::Type::Int16;

    case LOCAL_GL_UNSIGNED_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
        return jsType == js::Scalar::Type::Uint16;

    case LOCAL_GL_INT:
        return jsType == js::Scalar::Type::Int32;

    case LOCAL_GL_UNSIGNED_INT:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
    case LOCAL_GL_UNSIGNED_INT_5_9_9_9_REV:
    case LOCAL_GL_UNSIGNED_INT_24_8:
        return jsType == js::Scalar::Type::Uint32;

    case LOCAL_GL_FLOAT:
        return jsType == js::Scalar::Type::Float32;

    default:
        return false;
    }
}

static bool
ValidateUnpackArrayType(WebGLContext* webgl, const char* funcName, GLenum unpackType,
                        js::Scalar::Type jsType)
{
    if (DoesJSTypeMatchUnpackType(unpackType, jsType))
        return true;

    webgl->ErrorInvalidOperation("%s: `pixels` be compatible with unpack `type`.",
                                 funcName);
    return false;
}

static UniquePtr<TexUnpackBlob>
UnpackBlobFromMaybeView(WebGLContext* webgl, const char* funcName, GLsizei width,
                        GLsizei height, GLsizei depth, GLenum unpackType,
                        const dom::Nullable<dom::ArrayBufferView>& maybeView)
{
    size_t dataSize;
    const void* data;
    if (maybeView.IsNull()) {
        dataSize = 0;
        data = nullptr;
    } else {
        const dom::ArrayBufferView& view = maybeView.Value();

        if (!ValidateUnpackArrayType(mContext, funcName, unpackType, view.Type()))
            return nullptr;

        view.ComputeLengthAndData();

        dataSize = view.Length();
        data = view.Data();
    }

    return new TexUnpackBuffer(width, height, depth, dataSize, data));
}

void
WebGLTexture::TexImage(const char* funcName, uint8_t funcDims, TexImageTarget target,
                       GLint level, GLenum internalFormat, GLsizei width, GLsizei height,
                       GLsizei depth, GLint border, GLenum unpackFormat,
                       GLenum unpackType,
                       const dom::Nullable<dom::ArrayBufferView>& maybeView)
{
    UniquePtr<TexUnpackBlob> unpackBlob = UnpackBlobFromMaybeView(mContext, funcName,
                                                                  width, height, depth,
                                                                  unpackType, maybeView);
    if (!unpackBlob)
        return;

    TexImage(funcName, funcDims, target, level, internalFormat, unpackFormat,
             unpackType, unpackBlob);
}

void
WebGLTexture::TexSubImage(const char* funcName, uint8_t funcDims, TexImageTarget target,
                          GLint level, GLint xOffset, GLint yOffset, GLint zOffset,
                          GLsizei width, GLsizei height, GLsizei depth,
                          GLenum unpackFormat, GLenum unpackType,
                          const dom::Nullable<dom::ArrayBufferView>& maybeView)
{
    UniquePtr<TexUnpackBlob> unpackBlob = UnpackBlobFromMaybeView(mContext, funcName,
                                                                  width, height, depth,
                                                                  unpackType, maybeView);
    if (!unpackBlob)
        return;

    TexSubImage(funcName, funcDims, target, level, xOffset, yOffset, zOffset,
                unpackFormat, unpackType, unpackBlob);
}

////////////////////////////////////////
// ImageData

static UniquePtr<TexUnpackBlob>
UnpackBlobFromImageData(WebGLContext* webgl, const char* funcName, GLenum unpackType,
                        dom::ImageData* imageData, dom::Uint8ClampedArray* scopedArr)
{
    if (!imageData) {
        // Spec says to generate an INVALID_VALUE error
        webgl->ErrorInvalidValue("%s: null ImageData", funcName);
        return nullptr;
    }

    DebugOnly<bool> inited = scopedArr->Init(imageData->GetDataObject());
    MOZ_ASSERT(inited);

    scopedArr->ComputeLengthAndData();
    const DebugOnly<size_t> dataSize = scopedArr->Length();
    const void* const data = scopedArr->Data();

    const gfx::IntSize size(imageData->Width(), imageData->Height());
    const size_t stride = size.width * 4;
    const gfx::SurfaceFormat surfFormat = gfx::SurfaceFormat::R8G8B8A8;
    const bool ownsData = false;

    MOZ_ASSERT(dataSize == stride * size.height);

    const RefPtr<SourceSurfaceRawData> surf = new SourceSurfaceRawData;
    surf->InitWrappingData(data, size, stride, surfFormat, ownsData);

    return new TexUnpackSourceSurface(surf);
}

void
WebGLTexture::TexImage(const char* funcName, uint8_t funcDims, TexImageTarget target,
                       GLint level, GLenum internalFormat, GLenum unpackFormat,
                       GLenum unpackType, dom::ImageData* imageData)
{
    dom::Uint8ClampedArray scopedArr;
    UniquePtr<TexUnpackBlob> unpackBlob; = UnpackBlobFromImageData(mContext, funcName,
                                                                   unpackType, imageData,
                                                                   &scopedArr);
    if (!unpackBlob)
        return;

    TexImage(funcName, funcDims, target, level, internalFormat, unpackFormat,
             unpackType, unpackBlob);
}

void
WebGLTexture::TexSubImage(const char* funcName, uint8_t funcDims, TexImageTarget target,
                          GLint level, GLint xOffset, GLint yOffset, GLint zOffset,
                          GLenum unpackFormat, GLenum unpackType,
                          dom::ImageData* imageData)
{
    dom::Uint8ClampedArray scopedArr;
    UniquePtr<TexUnpackBlob> unpackBlob; = UnpackBlobFromImageData(mContext, funcName,
                                                                   unpackType, imageData,
                                                                   &scopedArr);
    if (!unpackBlob)
        return;

    TexImage(funcName, funcDims, target, level, internalFormat, unpackFormat,
             unpackType, unpackBlob);
}

////////////////////////////////////////
// HTMLMediaElement

void
WebGLTexture::TexImage(const char* funcName, uint8_t funcDims, TexImageTarget target,
                       GLint level, GLenum internalFormat, GLenum unpackFormat,
                       GLenum unpackType, dom::HTMLMediaElement* elem,
                       ErrorResult* const out_rv)
{
    if (!ValidateElemForCORS(this, funcName, elem, out_rv))
        return;

    UniquePtr<TexUnpackBlob> unpackBlob;

    RefPtr<mozilla::layers::Image> image = ImageFromElement(elem, this);
    if (image) {
        unpackBlob.reset(new TexUnpackImage(image, elem));

        if (TexImage(funcName, funcDims, target, level, internalFormat,
                     unpackFormat, unpackType, unpackBlob))
        {
            return;
        }
    }

    RefPtr<gfx::DataSourceSurface> dataSurf = DataFromElement(elem, this);
    if (!dataSurf) {
        ErrorInvalidOperation("%s: Failed to get data from DOM element.", funcName);
        return;
    }

    unpackBlob.reset(new TexUnpackSourceSurface(dataSurf));

    if (TexImage(funcName, funcDims, target, level, internalFormat, unpackFormat,
                 unpackType, unpackBlob))
    {
        return;
    }

    MOZ_ASSERT(false, "Should not get here.");
    ErrorInvalidOperation("%s: Failed to upload data from DOM element.", funcName);
}

void
WebGLTexture::TexSubImage(const char* funcName, uint8_t funcDims, TexImageTarget target,
                          GLint level, GLenum internalFormat, GLint xOffset,
                          GLint yOffset, GLint zOffset, GLenum unpackFormat,
                          GLenum unpackType, dom::HTMLMediaElement* elem,
                          ErrorResult* const out_rv)
{
    if (!ValidateElemForCORS(this, funcName, elem, out_rv))
        return;

    UniquePtr<TexUnpackBlob> unpackBlob;

    RefPtr<mozilla::layers::Image> image = ImageFromElement(elem, this);
    if (image) {
        unpackBlob.reset(new TexUnpackImage(image, elem));

        if (TexImage(funcName, funcDims, target, level, xOffset, yOffset, zOffset,
                     unpackFormat, unpackType, unpackBlob))
        {
            return;
        }
    }

    RefPtr<gfx::DataSourceSurface> dataSurf = DataFromElement(elem, this);
    if (!dataSurf) {
        ErrorInvalidOperation("%s: Failed to get data from DOM element.", funcName);
        return;
    }

    unpackBlob.reset(new TexUnpackSourceSurface(dataSurf));

    if (TexImage(funcName, funcDims, target, level, xOffset, yOffset, zOffset,
                 unpackFormat, unpackType, unpackBlob))
    {
        return;
    }

    MOZ_ASSERT(false, "Should not get here.");
    ErrorInvalidOperation("%s: Failed to get upload from DOM element.", funcName);
}


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////


/* This needs to be done (but cached) per texture per draw call.

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

    // Only support swizzling on core profiles.
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
*/

//////////////////////////////////////////////////////////////////////////////////////////
// Utils

static bool
ValidateTexImage(WebGLContext* webgl, WebGLTexture* texture, const char* funcName,
                 TexImageTarget target, GLint level,
                 WebGLTexture::ImageInfo** const out_imageInfo)
{
    // Check level
    if (level < 0) {
        webgl->ErrorInvalidValue("%s: `level` must be >= 0.", funcName);
        return false;
    }

    if (level > 31) {
        // Right-shift is only defined for bits-1, so 31 for GLsizei.
        webgl->ErrorInvalidValue("%s: `level` is too large.", funcName);
        return false;
    }

    WebGLTexture::ImageInfo& imageInfo = texture->ImageInfoAt(target, level);

    *out_imageInfo = &imageInfo;
    return true;
}


// For *TexImage*
bool
WebGLTexture::ValidateTexImageSpecification(const char* funcName, TexImageTarget target,
                                            GLint level, GLsizei width, GLsizei height,
                                            GLsizei depth, GLint border,
                                            WebGLTexture::ImageInfo** const out_imageInfo)
{
    if (mImmutable) {
        mContext->ErrorInvalidOperation("%s: Specified texture is immutable.", funcName);
        return false;
    }

    // Check border
    if (border != 0) {
        mContext->ErrorInvalidValue("%s: `border` must be 0.", funcName);
        return false;
    }

    if (level < 0 || width < 0 || height < 0 || depth < 0) {
        /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
         *   "If wt and ht are the specified image width and height,
         *   and if either wt or ht are less than zero, then the error
         *   INVALID_VALUE is generated."
         */
        mContext->ErrorInvalidValue("%s: `level`/`width`/`height`/`depth` must be >= 0.",
                                    funcName);
        return false;
    }

    /* GLES 3.0.4, p133-134:
     * GL_MAX_TEXTURE_SIZE is *not* the max allowed texture size. Rather, it is the
     * max (width/height) size guaranteed not to generate an INVALID_VALUE for too-large
     * dimensions. Sizes larger than GL_MAX_TEXTURE_SIZE *may or may not* result in an
     * INVALID_VALUE, or possibly GL_OOM.
     *
     * However, we have needed to set our maximums lower in the past to prevent resource
     * corruption. Therefore we have mImplMaxTextureSize, which is neither necessarily
     * lower nor higher than MAX_TEXTURE_SIZE.
     *
     * Note that mImplMaxTextureSize must be >= than the advertized MAX_TEXTURE_SIZE.
     * For simplicity, we advertize MAX_TEXTURE_SIZE as mImplMaxTextureSize.
     */

    uint32_t maxWidthHeight = 0;
    uint32_t maxDepth = 0;

    if (level <= 31) {
        switch (target.get()) {
        case LOCAL_GL_TEXTURE_2D:
            maxWidthHeight = mContext->mImplMaxTextureSize >> level;
            maxDepth = 1;
            break;

        case LOCAL_GL_TEXTURE_3D:
            maxWidthHeight = mContext->mImplMax3DTextureSize >> level;
            maxDepth = maxWidthHeight;
            break;

        case LOCAL_GL_TEXTURE_2D_ARRAY:
            maxWidthHeight = mContext->mImplMaxTextureSize >> level;
            // "The maximum number of layers for two-dimensional array textures (depth)
            //  must be at least MAX_ARRAY_TEXTURE_LAYERS for all levels."
            maxDepth = mContext->mImplMaxArrayTextureLayers;
            break;

        default: // cube maps
            MOZ_ASSERT(IsCubeMap());
            maxWidthHeight = mContext->mImplMaxCubeMapTextureSize >> level;
            maxDepth = 1;
            break;
        }
    }

    if (uint32_t(width) > maxWidthHeight ||
        uint32_t(height) > maxWidthHeight ||
        uint32_t(depth) > maxDepth)
    {
        mContext->ErrorInvalidValue("%s: Requested size at this level is unsupported.",
                                    funcName);
        return false;
    }

    {
        /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
         *   "If level is greater than zero, and either width or
         *   height is not a power-of-two, the error INVALID_VALUE is
         *   generated."
         *
         * This restriction does not apply to GL ES Version 3.0+.
         */
        bool requirePOT = (!mContext->IsWebGL2() && level != 0);

        if (requirePOT) {
            if (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)) {
                mContext->ErrorInvalidValue("%s: For level > 0, width and height must be"
                                            " powers of two.",
                                            funcName);
                return false;
            }
        }
    }

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImage(mContext, this, funcName, target, level, &imageInfo))
        return false;

    *out_imageInfo = imageInfo;
    return true;
}

// For *TexSubImage*
bool
WebGLTexture::ValidateTexImageSelection(const char* funcName, TexImageTarget target,
                                        GLint level, GLint xOffset, GLint yOffset,
                                        GLint zOffset, GLsizei width, GLsizei height,
                                        GLsizei depth,
                                        WebGLTexture::ImageInfo** const out_imageInfo)
{
    if (mImmutable) {
        mContext->ErrorInvalidOperation("%s: Specified texture is immutable.", funcName);
        return false;
    }

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImage(mContext, this, funcName, target, level, &imageInfo))
        return false;

    if (imageInfo->IsDefined()) {
        mContext->ErrorInvalidOperation("%s: The specified TexImage has not yet been"
                                        " specified.",
                                        funcName);
        return false;
    }

    if (xOffset < 0 || yOffset < 0 || zOffset < 0) {
        mContext->ErrorInvalidValue("%s: Offsets must be >=0.", funcName);
        return false;
    }

    const auto totalX = CheckedUint32(xOffset) + width;
    const auto totalY = CheckedUint32(yOffset) + height;
    const auto totalZ = CheckedUint32(zOffset) + depth;

    if (!totalX.isValid() || totalX.value() > imageInfo->mWidth ||
        !totalY.isValid() || totalY.value() > imageInfo->mHeight ||
        !totalZ.isValid() || totalZ.value() > imageInfo->mDepth)
    {
        mContext->ErrorInvalidValue("%s: Offset+size must be <= the size of the existing"
                                    " specified image.",
                                    funcName);
        return false;
    }

    *out_imageInfo = imageInfo;
    return true;
}

static bool
ValidateCompressedTexUnpack(WebGLContext* webgl, const char* funcName, GLsizei width,
                            GLsizei height, GLsizei depth,
                            const webgl::FormatInfo* format, size_t dataSize)
{
    auto compression = format->compression;

    auto bytesPerBlock = compression->bytesPerBlock;
    auto blockWidth = compression->blockWidth;
    auto blockHeight = compression->blockHeight;

    CheckedUint32 widthInBlocks = (width % blockWidth) ? width / blockWidth + 1
                                                       : width / blockWidth;
    CheckedUint32 heightInBlocks = (height % blockHeight) ? height / blockHeight + 1
                                                          : height / blockHeight;
    CheckedUint32 blocksPerImage = widthInBlocks * heightInBlocks;
    CheckedUint32 bytesPerImage = bytesPerBlock * blocksPerImage;
    CheckedUint32 bytesNeeded = bytesPerImage * depth;

    if (!bytesNeeded.isValid()) {
        webgl->ErrorInvalidOperation("%s: Overflow while computing the needed buffer"
                                     " size.",
                                     funcName);
        return false;
    }

    if (dataSize != bytesNeeded.value()) {
        webgl->ErrorInvalidOperation("%s: Provided buffer's size must match expected"
                                     " size. (needs %u, has %u)",
                                     funcName, bytesNeeded.value(), dataSize);
        return false;
    }

    return true;
}

static bool
DoChannelsMatchForCopyTexImage(const webgl::FormatInfo* srcFormat,
                               const webgl::FormatInfo* dstFormat)
{
    // GLES 3.0.4 p140 Table 3.16 "Valid CopyTexImage source framebuffer/destination
    // texture base internal format combinations."

    switch (srcFormat->unsizedFormat) {
    case webgl::UnsizedFormat::RGBA:
        switch (dstFormat->unsizedFormat) {
        case webgl::UnsizedFormat::A:
        case webgl::UnsizedFormat::L:
        case webgl::UnsizedFormat::LA:
        case webgl::UnsizedFormat::R:
        case webgl::UnsizedFormat::RG:
        case webgl::UnsizedFormat::RGB:
        case webgl::UnsizedFormat::RGBA:
            return true;
        default:
            return false;
        }

    case webgl::UnsizedFormat::RGB:
        switch (dstFormat->unsizedFormat) {
        case webgl::UnsizedFormat::L:
        case webgl::UnsizedFormat::R:
        case webgl::UnsizedFormat::RG:
        case webgl::UnsizedFormat::RGB:
            return true;
        default:
            return false;
        }

    case webgl::UnsizedFormat::RG:
        switch (dstFormat->unsizedFormat) {
        case webgl::UnsizedFormat::L:
        case webgl::UnsizedFormat::R:
        case webgl::UnsizedFormat::RG:
            return true;
        default:
            return false;
        }

    case webgl::UnsizedFormat::R:
        switch (dstFormat->unsizedFormat) {
        case webgl::UnsizedFormat::L:
        case webgl::UnsizedFormat::R:
            return true;
        default:
            return false;
        }

    default:
        return false;
    }
}

static bool
EnsureImageDataInitialized(WebGLTexture* tex, const char* funcName,
                           TexImageTarget target, GLint level, GLint xOffset,
                           GLint yOffset, GLint zOffset, GLsizei width, GLsizei height,
                           GLsizei depth, WebGLTexture::ImageInfo* imageInfo,
                           bool* const out_uploadWillInitialize)
{
    *out_uploadWillInitialize = false;

    if (!imageInfo->IsDataInitialized()) {
        const bool isFullUpload = (!xOffset && !yOffset && !zOffset &&
                                   width == imageInfo->mWidth &&
                                   height == imageInfo->mHeight &&
                                   depth == imageInfo->mDepth);
        if (isFullUpload) {
            *out_uploadWillInitialize = true;
        } else {
            WebGLContext* webgl = tex->mContext;
            webgl->GenerateWarning("%s: Texture has not been initialized prior to a"
                                      " partial upload, forcing the browser to clear"
                                      " it. This may be slow.",
                                      funcName);
            if (!tex->InitializeImageData(target, level)) {
                MOZ_ASSERT(false, "Unexpected failure to init image data.");
                webgl->GenerateWarning("%s: Failed to initialize image data. Losing"
                                       " context...", funcName);
                webgl->ForceLoseContext();
                return false;
            }
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// Actual calls

static inline GLenum
DoTexStorage(gl::GLContext* gl, uint8_t funcDims, GLenum texTarget, GLsizei levels,
             GLenum sizedFormat, GLsizei width, GLsizei height, GLsizei depth)
{
    gl->MakeCurrent();

    GLContext::LocalErrorScope errorScope(*gl);

    switch (funcDims) {
    case 2:
        gl->fTexStorage2D(texTarget, levels, sizedFormat, width, height);
        break;

    case 3:
        gl->fTexStorage3D(texTarget, levels, sizedFormat, width, height, depth);
        break;

    default:
        MOZ_CRASH('bad funcDims');
    }

    return errorScope.GetError();
}

static inline GLenum
DoCompressedTexImage(gl::GLContext* gl, uint8_t funcDims, TexImageTarget target,
                     GLint level, GLenum internalFormat, GLsizei width, GLsizei height,
                     GLsizei depth, GLint border, GLsizei dataSize, const void* data)
{
    GLContext::LocalErrorScope errorScope(*gl);

    switch (funcDims) {
    case 2:
        gl->fCompressedTexImage2D(GLenum(target), level, internalFormat, width, height,
                                  border, dataSize, data);
        break;

    case 3:
        gl->fCompressedTexImage3D(GLenum(target), level, internalFormat, width, height,
                                  depth, border, dataSize, data);
        break;

    default:
        MOZ_CRASH('bad funcDims');
    }

    return errorScope.GetError();
}

static inline GLenum
DoCompressedTexSubImage(gl::GLContext* gl, uint8_t funcDims, TexImageTarget target,
                        GLint level, GLint xOffset, GLint yOffset, GLint zOffset,
                        GLsizei width, GLsizei height, GLenum sizedUnpackFormat,
                        GLsizei dataSize, const void* data)
{
    GLContext::LocalErrorScope errorScope(*gl);

    switch (funcDims) {
    case 2:
        gl->fCompressedTexSubImage2D(GLenum(target), level, xOffset, yOffset, width,
                                     height, sizedUnpackFormat, dataSize, data);
        break;

    case 3:
        gl->fCompressedTexSubImage3D(GLenum(target), level, xOffset, yOffset, zOffset,
                                     width, height, depth, sizedUnpackFormat, dataSize,
                                     data);
        break;

    default:
        MOZ_CRASH('bad funcDims');
    }

    return errorScope.GetError();
}

static inline GLenum
DoCopyTexImage2D(gl::GLContext* gl, TexImageTarget target, GLint level,
                 GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height,
                 GLint border)
{
    gl->MakeCurrent();

    GLContext::LocalErrorScope errorScope(*gl);

    gl->fCopyTexImage2D(GLenum(target), level, internalFormat, x, y, width, height,
                        border);

    return errorScope.GetError();
}

static inline GLenum
DoCopyTexSubImage(gl::GLContext* gl, TexImageTarget target, GLint level, GLint xOffset,
                  GLint yOffset, GLint zOffset, GLint x, GLint y, GLsizei width,
                  GLsizei height, GLsizei depth)
{
    gl->MakeCurrent();

    GLContext::LocalErrorScope errorScope(*gl);

    switch (funcDims) {
    case 2:
        gl->fCopyTexSubImage2D(GLenum(target), level, xOffset, yOffset, x, y, width,
                               height);
        break;

    case 3:
        gl->fCopyTexSubImage3D(GLenum(target), level, xOffset, yOffset, zOffset, x, y,
                               width, height);
        break;

    default:
        MOZ_CRASH('bad funcDims');
    }

    return errorScope.GetError();
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// Actual (mostly generic) function implementations

void
WebGLTexture::TexStorage(const char* funcName, uint8_t funcDims, TexTarget target,
                         GLsizei levels, GLenum sizedFormat, GLsizei width,
                         GLsizei height, GLsizei depth)
{
    // Check levels
    if (levels < 1) {
        ErrorInvalidValue("%s: `levels` must be >= 1.", funcName);
        return false;
    }

    if (levels > 31) {
        // Right-shift is only defined for bits-1, so 31 for GLsizei.
        // Besides,
        ErrorInvalidValue("%s: `level` is too large.", funcName);
        return false;
    }

    const TexImageTarget testTarget = IsCubeMap() ? LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X
                                                  : texTarget;
    const GLint testLevel = 0;
    const GLint border = 0;

    WebGLTexture::ImageInfo* testImageInfo;
    if (!ValidateTexImageSpecification(funcName, testTarget, testLevel, width, height,
                                       depth, border, &testImageInfo))
    {
        return;
    }
    MOZ_ASSERT(testImageInfo);
    mozilla::unused << testImageInfo;

    const webgl::FormatInfo* dstFormat = GetInfoBySizedFormat(sizedFormat);
    if (!dstFormat) {
        ErrorInvalidEnum("%s: Unrecognized internalformat: 0x%04x",
                         funcName, internalFormat, unpackFormat, unpackType);
        return;
    }

    const webgl::FormatUsageInfo* dstFormatUsage = mFormatUsage->GetInfo(dstFormat);
    if (!dstFormatUsage || !dstFormatUsage->asTexture) {
        ErrorInvalidEnum("%s: Invalid internalformat: 0x%04x", funcName, internalFormat);
        return;
    }

    auto compression = dstFormat->compression;
    if (compression) {
        if (compression->subImageUpdateBehavior == SubImageUpdateBehavior::Forbidden) {
            webgl->ErrorInvalidOperation("%s: This format forbids compressedTexSubImage, "
                                         " and thus would be useless after calling"
                                         " texStorage.",
                                         funcName);
        }
        if (compression->requirePOT) {
            if (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)) {
                webgl->ErrorInvalidOperation("%s: This format requires power-of-two width"
                                             " and height.",
                                             funcName);
                return;
            }
        }
    }

    ////////////////////////////////////
    // Do the thing!

    GLenum error = DoTexStorage(mContext->gl, funcDims, target, levels, sizedFormat,
                                width, height, depth);

    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        mContext->ErrorOutOfMemory("%s: Ran out of memory during texture allocation.",
                                   funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        mContext->ErrorInvalidOperation("%s: Unexpected error during texture allocation.",
                                        funcName);
        return;
    }

    ////////////////////////////////////
    // Update our specification data.

    const bool isDataInitialized = false;
    const WebGLTexture::ImageInfo newInfo(format, width, height, depth,
                                          isDataInitialized);
    SetImageInfosAtLevel(0, newInfo);

    PopulateMipChain(0, levels-1);
}

////////////////////////////////////////
// Tex(Sub)Image

void
WebGLTexture::TexImage(const char* funcName, uint8_t funcDims, TexImageTarget target,
                       GLint level, GLenum internalFormat, GLenum unpackFormat,
                       GLenum unpackType, TexUnpackBlob* unpackBlob,
                       bool* const out_tryAgain)
{
    if (out_tryAgain) {
        *out_tryAgain = false;
    }

    ////////////////////////////////////
    // Get dest info

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSpecification(funcName, target, level, unpackBlob->mWidth,
                                       unpackBlob->mHeight, unpackBlob->mDepth, border,
                                       &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(imageInfo);

    auto dstFormat = GetInfoBySizedFormat(internalFormat);
    if (!dstFormat && internalFormat == unpackFormat) {
        dstFormat = srcPacking->formatForUnsized;
    }

    if (!dstFormat) {
        ErrorInvalidEnum("%s: Unrecognized internalformat/format/type:"
                         " 0x%04x/0x%04x/0x%04x",
                         funcName, internalFormat, unpackFormat, unpackType);
        return;
    }

    const webgl::FormatUsageInfo* dstFormatUsage = mFormatUsage->GetInfo(dstFormat);
    if (!dstFormatUsage || !dstFormatUsage->asTexture) {
        ErrorInvalidEnum("%s: Invalid internalformat/format/type: 0x%04x/0x%04x/0x%04x",
                         funcName, internalFormat, unpackFormat, unpackType);
        return;
    }

    ////////////////////////////////////
    // Get source info

    // We must validate unpackType through mFormatUsage to catch HALF_FLOAT/HALF_FLOAT_OES
    // mismatches.
    auto srcPacking = mFormatUsage->GetPackingInfo(unpackFormat, unpackType);

    //const webgl::FormatInfo* srcFormat = webgl::GetInfoByUnpackTuple(unpackFormat,
    //                                                                 unpackType);
    if (!srcPacking) {
        mContext->ErrorInvalidOperation("%s: Invalid format/type for unpacking.",
                                        funcName);
        return;
    }

    if (!unpackBlob->ValidateUnpack(mContext, funcDims, srcPacking))
        return;

    ////////////////////////////////////
    // Check that source and dest info are compatible

    const webgl::UnpackInfo* unpackInfo;
    if (!dstFormatUsage->IsUnpackValid(srcPacking, &unpackInfo)) {
        ErrorInvalidOperation("%s: Mismatched internalFormat and format/type: 0x%04x and"
                              " 0x%04x/0x%04x",
                              funcName, internalFormat, unpackFormat, unpackType);
        return;
    }

    ////////////////////////////////////
    // Do the thing!

    // It's tempting to do allocation first, and TexSubImage second, but this is generally
    // slower.

    GLenum glError;
    if (!unpackBlob->TexImage(this, funcDims, target, level, unpackInfo, &glError)) {
        if (out_tryAgain) {
            *out_tryAgain = true;
        } else if (glError == LOCAL_GL_OUT_OF_MEMORY) {
            ErrorOutOfMemory("%s: Driver ran out of memory during upload.", funcName);
        } else if (glError) {
            ErrorInvalidOperation("%s: Unexpected error during upload: 0x04x", funcName,
                                  glError);
            MOZ_ASSERT(false, "Unexpected GL error.");
        } else {
            ErrorInvalidOperation("%s: Internal error during upload.", funcName);
        }
        return;
    }

    ////////////////////////////////////
    // Update our specification data.

    *imageInfo = ImageInfo(dstFormatUsage, unpackBlob->mWidth, unpackBlob->mHeight,
                           unpackBlob->mDepth, unpackBlob->HasData());
}


void
WebGLTexture::TexSubImage(const char* funcName, uint8_t funcDims,
                          TexImageTarget target, GLint level, GLint xOffset,
                          GLint yOffset, GLint zOffset, GLenum unpackFormat,
                          GLenum unpackType, TexUnpackBlob* unpackBlob)
{
    ////////////////////////////////////
    // Get dest info

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSelection(funcName, target, level, xOffset, yOffset, zOffset,
                                   unpackBlob->mWidth, unpackBlob->mHeight,
                                   unpackBlob->mDepth, &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(imageInfo);

    const webgl::FormatUsageInfo* dstFormatUsage = imageInfo->mFormat;
    MOZ_ASSERT(dstFormatUsage && dstFormatUsage->asTexture);

    auto dstFormat = dstFormatUsage->formatInfo;
    MOZ_ASSERT(dstFormat);

    ////////////////////////////////////
    // Get source info

    auto srcPacking = mFormatUsage->GetPackingInfo(unpackFormat, unpackType);
    if (!srcPacking) {
        mContext->ErrorInvalidOperation("%s: Invalid format/type for unpacking.",
                                        funcName);
        return;
    }

    if (!unpackBlob->ValidateTexUnpack(mContext, funcDims, srcPacking))
        return;

    ////////////////////////////////////
    // Check that source and dest info are compatible

    if (dstFormat->compression) {
        ErrorInvalidEnum("%s: Specified TexImage must not be compressed.", funcName);
        return;
    }

    const webgl::UnpackInfo* unpackInfo;
    if (!dstFormatUsage->IsUnpackValid(srcPacking, &unpackInfo)) {
        ErrorInvalidOperation("%s: Mismatched internalFormat and format/type: 0x%04x and"
                              " 0x%04x/0x%04x",
                              funcName, internalFormat, unpackFormat, unpackType);
        return;
    }

    ////////////////////////////////////
    // Do the thing!

    bool uploadWillInitialize;
    if (!EnsureImageDataInitialized(this, funcName, target, level, xOffset, yOffset,
                                    zOffset, unpackBlob->mWidth, unpackBlob->mHeight,
                                    unpackBlob->mDepth, imageInfo, &uploadWillInitialize))
    {
        return;
    }

    if (!unpackBlob->TexSubImage(this, funcDims, target, level, unpackInfo, xOffset,
                                 yOffset, zOffset))
    {
        return;
    }

    ////////////////////////////////////
    // Update our specification data?

    if (uploadWillInitialize) {
        imageInfo->SetIsDataInitialized(true, this);
    }
}

////////////////////////////////////////
// CompressedTex(Sub)Image

void
WebGLTexture::CompressedTexImage(const char* funcName, uint8_t funcDims,
                                 TexImageTarget target, GLint level,
                                 GLenum internalFormat, GLsizei width, GLsizei height,
                                 GLsizei depth, GLint border, void* data, size_t dataSize)
{
    ////////////////////////////////////
    // Get dest info

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSpecification(funcName, target, level, width, height, depth,
                                       border, &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(imageInfo);

    auto format = webgl::GetInfoBySizedFormat(internalFormat);
    if (!format || !format->compression) {
        webgl->ErrorInvalidOperation("%s: Invalid format for unpacking.", funcName);
        return false;
    }

    const webgl::FormatUsageInfo* formatUsage = formatUsage = mFormatUsage->GetInfo(format);
    if (!formatUsage || !formatUsage->asTexture) {
        webgl->ErrorInvalidEnum("%s: Invalid internalformat: 0x%04x", funcName,
                                internalFormat);
        return;
    }

    ////////////////////////////////////
    // Get source info

    if (format->compression->requirePOT) {
        if (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)) {
            webgl->ErrorInvalidOperation("%s: This format requires power-of-two width and"
                                         " height.",
                                         funcName);
            return;
        }
    }

    if (!ValidateCompressedTexUnpack(mContext, funcName, width, height, depth, format,
                                     dataSize))
    {
        return;
    }

    ////////////////////////////////////
    // Do the thing!

    GLenum error = DoCompressedTexImage(mContext->gl, funcDims, target, level,
                                        internalFormat, width, height, depth, border,
                                        dataSize, data);
    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        ErrorOutOfMemory("%s: Ran out of memory during upload.", funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        GenerateWarning("%s: Unexpected error during texture upload. Context lost.",
                        funcName);
        ForceLoseContext();
        return;
    }

    ////////////////////////////////////
    // Update our specification data.

    const bool isDataInitialized = true;
    const ImageInfo newImageInfo(formatUsage, width, height, depth, isDataInitialized);
    SetImageInfo(imageInfo, newImageInfo);
}

static inline bool
IsSubImageBlockAligned(const webgl::CompressedFormatInfo* compression,
                       WebGLTexture::ImageInfo* imageInfo, GLint xOffset, GLint yOffset,
                       GLsizei width, GLsizei height)
{
    if (xOffset % compression->blockWidth != 0 ||
        yOffset % compression->blockHeight != 0)
    {
        return false;
    }

    if (width % compression->blockWidth != 0 && xOffset + width != imageInfo->mWidth)
        return false;

    if (height % compression->blockHeight != 0 && yOffset + height != imageInfo->mHeight)
        return false;

    return true;
}

void
WebGLTexture::CompressedTexSubImage(const char* funcName, uint8_t funcDims,
                                         GLenum texImageTarget, GLint level,
                                         GLint xOffset, GLint yOffset, GLint zOffset,
                                         GLsizei width, GLsizei height, GLsizei depth,
                                         GLenum sizedUnpackFormat, size_t dataSize,
                                         const void* data)
{
    ////////////////////////////////////
    // Get dest info

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSelection(funcName, target, level, xOffset, yOffset, zOffset,
                                   width, height, depth, &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(imageInfo);

    const webgl::FormatUsageInfo* dstFormatUsage = imageInfo->Format();
    MOZ_ASSERT(dstFormatUsage);
    const webgl::FormatInfo* dstFormat = dstFormatUsage->formatInfo;
    MOZ_ASSERT(dstFormat);

    ////////////////////////////////////
    // Get source info

    auto srcFormat = webgl::GetInfoBySizedFormat(sizedUnpackFormat);
    if (srcFormat != dstFormat) {
        ErrorInvalidValue("%s: `format` must match format of specified texture image.",
                          funcName);
        return;
    }

    if (!ValidateCompressedTexUnpack(mContext, funcName, width, height, depth, srcFormat,
                                     dataSize))
    {
        return;
    }

    ////////////////////////////////////
    // Check that source and dest info are compatible

    switch (dstFormat->compression->subImageUpdateBehavior) {
    case webgl::SubImageUpdateBehavior::Forbidden:
        if (isSubFunc) {
            ErrorInvalidOperation("%s: Format does not allow sub-image updates.",
                                  funcName);
            return;
        }
        break;

    case webgl::SubImageUpdateBehavior::FullOnly:
        if (xOffset || yOffset ||
            width != imageInfo->mWidth ||
            height != imageInfo->mHeight)
        {
            ErrorInvalidOperation("%s: Format does not allow partial sub-image updates.",
                                  funcName);
            return;
        }
        break;

    case webgl::SubImageUpdateBehavior::BlockAligned:
        if (!IsSubImageBlockAligned(dstFormat->compression, imageInfo, xOffset, yOffset,
                                    width, height))
        {
            ErrorInvalidOperation("%s: Format requires block-aligned sub-image updates.",
                                  funcName);
            return;
        }
        break;
    }

    ////////////////////////////////////
    // Do the thing!

    bool uploadWillInitialize;
    if (!EnsureImageDataInitialized(this, funcName, target, level, xOffset, yOffset,
                                    zOffset, width, height, depth, imageInfo,
                                    &uploadWillInitialize))
    {
        return;
    }

    GLenum error = DoCompressedTexSubImage(mContext->gl, funcDims, target, level, xOffset,
                                           yOffset, zOffset, width, height, depth,
                                           sizedUnpackFormat, dataSize, data);
    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        ErrorOutOfMemory("%s: Ran out of memory during upload.", funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        GenerateWarning("%s: Unexpected error during texture upload. Context"
                        " lost.",
                        funcName);
        ForceLoseContext();
        return;
    }

    ////////////////////////////////////
    // Update our specification data?

    if (uploadWillInitialize) {
        imageInfo->SetIsDataInitialized(true, this);
    }
}

////////////////////////////////////////
// CopyTex(Sub)Image

static bool
ValidateCopyTexImageFormats(WebGLContext* webgl, const char* funcName,
                            const webgl::FormatInfo* srcFormat,
                            const webgl::FormatInfo* dstFormat)
{
    MOZ_ASSERT(!srcFormat->compression);
    if (dstFormat->compression) {
        webgl->ErrorInvalidEnum("%s: Specified destination must not have a compressed"
                                " format.",
                                funcName);
        return false;
    }

    if (dstFormat->effectiveFormat == webgl::EffectiveFormat::RGB9_E5) {
        webgl->ErrorInvalidOperation("%s: RGB9_E5 is an invalid destination for"
                                     " CopyTex(Sub)Image. (GLES 3.0.4 p145)",
                                     funcName);
        return false;
    }

    if (!DoChannelsMatchForCopyTexImage(srcFormat, dstFormat)) {
        webgl->ErrorInvalidOperation("%s: Destination channels must be compatible with"
                                     " source channels. (GLES 3.0.4 p140 Table 3.16)",
                                     funcName);
        return false;
    }

    return true;
}

// There is no CopyTexImage3D.
void
WebGLTexture::CopyTexImage2D(TexImageTarget target, GLint level, GLenum internalFormat,
                             GLint x, GLint y, GLsizei width, GLsizei height,
                             GLint border)
{
    const char funcName[] = "CopyTexImage2D";
    const uint8_t funcDims = 2;

    const GLsizei depth = 1;

    ////////////////////////////////////
    // Get dest info

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSpecification(funcName, target, level, width, height, depth,
                                       border, &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(imageInfo);

    auto dstFormat = GetInfoBySizedFormat(internalFormat);
    if (!dstFormat) {
        ErrorInvalidEnum("%s: Unrecognized internalformat: 0x%04x", funcName,
                         internalFormat);
        return;
    }

    const webgl::FormatUsageInfo* dstFormatUsage = mFormatUsage->GetInfo(dstFormat);
    if (!dstFormatUsage || !dstFormatUsage->asTexture) {
        ErrorInvalidEnum("%s: Invalid internalformat: 0x%04x", funcName, internalFormat);
        return;
    }

    ////////////////////////////////////
    // Get source info

    const webgl::FormatInfo* srcFormat;
    if (mBoundReadFramebuffer) {
        if (!mBoundReadFramebuffer->ValidateForRead(funcName, &srcFormat))
            return false;
    } else {
        auto srcEffFormat = mOptions.alpha ? webgl::EffectiveFormat::RGBA8
                                           : webgl::EffectiveFormat::RGB8;
        srcFormat = webgl::GetFormatInfo(srcEffFormat);
    }
    MOZ_ASSERT(srcFormat);

    ////////////////////////////////////
    // Check that source and dest info are compatible

    if (!ValidateCopyTexImageFormats(mContext, funcName, srcFormat, dstFormat))
        return;

    ////////////////////////////////////
    // Do the thing!

    GLenum error = DoCopyTexSubImage2D(mContext->gl, target, level, internalFormat, x, y,
                                       width, height, border);
    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        ErrorOutOfMemory("%s: Ran out of memory during texture copy.", funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        GenerateWarning("%s: Unexpected error during texture copy. Context lost.",
                        funcName);
        ForceLoseContext();
        return;
    }

    ////////////////////////////////////
    // Update our specification data.

    const bool hasUninitData = false;
    const ImageInfo imageInfo(dstFormatUsage, width, height, depth, hasUninitData);
    SetImageInfoAt(texImageTarget, level, imageInfo);
}

void
WebGLTexture::CopyTexSubImage(const char* funcName, uint8_t funcDims,
                              TexImageTarget target, GLint level, GLint xOffset,
                              GLint yOffset, GLint zOffset, GLint x, GLint y,
                              GLsizei width, GLsizei height)
{
    const GLsizei depth = 1;

    ////////////////////////////////////
    // Get dest info

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSelection(funcName, target, level, xOffset, yOffset, zOffset,
                                   width, height, depth, &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(imageInfo);

    const webgl::FormatUsageInfo* dstFormatUsage = imageInfo->Format();
    MOZ_ASSERT(dstFormatUsage);
    const webgl::FormatInfo* dstFormat = dstFormatUsage->formatInfo;
    MOZ_ASSERT(dstFormat);

    ////////////////////////////////////
    // Get source info

    const webgl::FormatInfo* srcFormat;
    if (mBoundReadFramebuffer) {
        if (!mBoundReadFramebuffer->ValidateForRead(funcName, &srcFormat))
            return false;
    } else {
        auto srcEffFormat = mOptions.alpha ? webgl::EffectiveFormat::RGBA8
                                           : webgl::EffectiveFormat::RGB8;
        srcFormat = webgl::GetFormatInfo(srcEffFormat);
    }
    MOZ_ASSERT(srcFormat);

    ////////////////////////////////////
    // Check that source and dest info are compatible

    if (!ValidateCopyTexImageFormats(mContext, funcName, srcFormat, dstFormat))
        return;

    ////////////////////////////////////
    // Do the thing!

    bool uploadWillInitialize;
    if (!EnsureImageDataInitialized(this, funcName, target, level, xOffset, yOffset,
                                    zOffset, width, height, depth, imageInfo,
                                    &uploadWillInitialize))
    {
        return;
    }

    GLenum error = DoCopyTexSubImage(gl, target, level, xOffset, yOffset, zOffset, width,
                                     height, depth);

    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        ErrorOutOfMemory("%s: Ran out of memory during texture copy.", funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        GenerateWarning("%s: Unexpected error during texture copy. Context lost.",
                        funcName);
        ForceLoseContext();
        return;
    }

    ////////////////////////////////////
    // Update our specification data?

    if (uploadWillInitialize) {
        imageInfo->SetIsDataInitialized(true, this);
    }
}

} // namespace mozilla
