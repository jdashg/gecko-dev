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
#include "mozilla/gfx/SourceSurfaceRawData.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Scoped.h"
#include "ScopedGLHelpers.h"
#include "TexUnpackBlob.h"
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
        RefPtr<nsIPrincipal> srcPrincipal = elem->GetCurrentPrincipal();
        if (!srcPrincipal)
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

    static const char kInfoURL[] = "https://developer.mozilla.org/en/WebGL/Cross-Domain_Textures";
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

    RefPtr<mozilla::layers::Image> ret = currentImages[0].mImage;
    return ret.forget();
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

static UniquePtr<webgl::TexUnpackBlob>
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

        if (!ValidateUnpackArrayType(webgl, funcName, unpackType, view.Type()))
            return nullptr;

        view.ComputeLengthAndData();

        dataSize = view.Length();
        data = view.Data();
    }

    UniquePtr<webgl::TexUnpackBlob> ret;
    ret.reset(new webgl::TexUnpackBuffer(width, height, depth, dataSize, data));
    return Move(ret);
}

void
WebGLTexture::TexOrSubImage(bool isSubImage, const char* funcName, TexImageTarget target,
                            GLint level, GLenum internalFormat, GLint xOffset,
                            GLint yOffset, GLint zOffset, GLsizei width, GLsizei height,
                            GLsizei depth, GLint border, GLenum unpackFormat,
                            GLenum unpackType,
                            const dom::Nullable<dom::ArrayBufferView>& maybeView)
{
    UniquePtr<webgl::TexUnpackBlob> unpackBlob;
    unpackBlob = UnpackBlobFromMaybeView(mContext, funcName, width, height, depth,
                                         unpackType, maybeView);
    if (!unpackBlob)
        return;

    TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset, yOffset,
                  zOffset, border, unpackFormat, unpackType, unpackBlob.get());
}

////////////////////////////////////////
// ImageData

static UniquePtr<webgl::TexUnpackBlob>
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

    const RefPtr<gfx::SourceSurfaceRawData> surf = new gfx::SourceSurfaceRawData;

    uint8_t* wrappableData = (uint8_t*)data;
    surf->InitWrappingData(wrappableData, size, stride, surfFormat, ownsData);

    UniquePtr<webgl::TexUnpackBlob> ret;
    ret.reset(new webgl::TexUnpackSrcSurface(surf));
    return Move(ret);
}

void
WebGLTexture::TexOrSubImage(bool isSubImage, const char* funcName, TexImageTarget target,
                            GLint level, GLenum internalFormat, GLint xOffset,
                            GLint yOffset, GLint zOffset, GLenum unpackFormat,
                            GLenum unpackType, dom::ImageData* imageData)
{
    dom::Uint8ClampedArray scopedArr;
    UniquePtr<webgl::TexUnpackBlob> unpackBlob;
    unpackBlob = UnpackBlobFromImageData(mContext, funcName, unpackType, imageData,
                                         &scopedArr);
    if (!unpackBlob)
        return;

    const GLint border = 0;
    TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset, yOffset,
                  zOffset, border, unpackFormat, unpackType, unpackBlob.get());
}

////////////////////////////////////////
// HTMLMediaElement

void
WebGLTexture::TexOrSubImage(bool isSubImage, const char* funcName, TexImageTarget target,
                            GLint level, GLenum internalFormat, GLint xOffset,
                            GLint yOffset, GLint zOffset, GLenum unpackFormat,
                            GLenum unpackType, dom::HTMLMediaElement* elem,
                            ErrorResult* const out_rv)
{
    if (!ValidateElemForCORS(mContext, funcName, elem, out_rv))
        return;

    UniquePtr<webgl::TexUnpackBlob> unpackBlob;

    RefPtr<mozilla::layers::Image> image = ImageFromElement(elem, mContext);
    if (image) {
        unpackBlob.reset(new webgl::TexUnpackImage(image, elem));

        const GLint border = 0;
        if (TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset,
                          yOffset, zOffset, border, unpackFormat, unpackType,
                          unpackBlob.get()))
        {
            return;
        }
    }

    RefPtr<gfx::DataSourceSurface> dataSurf = DataFromElement(elem, mContext);
    if (!dataSurf) {
        mContext->ErrorInvalidOperation("%s: Failed to get data from DOM element.",
                                        funcName);
        return;
    }

    unpackBlob.reset(new webgl::TexUnpackSrcSurface(dataSurf));

    const GLint border = 0;
    if (TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset,
                      yOffset, zOffset, border, unpackFormat, unpackType,
                      unpackBlob.get()))
    {
        return;
    }

    MOZ_ASSERT(false, "Should not get here.");
    mContext->ErrorInvalidOperation("%s: Failed to get upload from DOM element.",
                                    funcName);
}


//////////////////////////////////////////////////////////////////////////////////////////

bool
WebGLTexture::TexOrSubImage(bool isSubImage, const char* funcName, TexImageTarget target,
                            GLint level, GLenum internalFormat, GLint xOffset,
                            GLint yOffset, GLint zOffset, GLint border,
                            GLenum unpackFormat, GLenum unpackType,
                            webgl::TexUnpackBlob* unpackBlob)
{
    if (isSubImage) {
        return TexSubImage(funcName, target, level, xOffset, yOffset, zOffset,
                           unpackFormat, unpackType, unpackBlob);
    } else {
        return TexImage(funcName, target, level, internalFormat, border, unpackFormat,
                        unpackType, unpackBlob);
    }
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
            if (!IsPowerOfTwo(uint32_t(width)) || !IsPowerOfTwo(uint32_t(height))) {
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
EnsureImageDataInitializedForUpload(WebGLTexture* tex, const char* funcName,
                                    TexImageTarget target, GLint level, GLint xOffset,
                                    GLint yOffset, GLint zOffset, GLsizei width,
                                    GLsizei height, GLsizei depth,
                                    WebGLTexture::ImageInfo* imageInfo,
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
DoTexStorage(gl::GLContext* gl, TexTarget target, GLsizei levels, GLenum sizedFormat,
             GLsizei width, GLsizei height, GLsizei depth)
{
    gl->MakeCurrent();

    gl::GLContext::LocalErrorScope errorScope(*gl);

    switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP:
        MOZ_ASSERT(depth == 1);
        gl->fTexStorage2D(target.get(), levels, sizedFormat, width, height);
        break;

    case LOCAL_GL_TEXTURE_3D:
    case LOCAL_GL_TEXTURE_2D_ARRAY:
        gl->fTexStorage3D(target.get(), levels, sizedFormat, width, height, depth);
        break;

    default:
        MOZ_CRASH("bad target");
    }

    return errorScope.GetError();
}

static bool
Is3D(TexImageTarget target)
{
    switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return false;

    case LOCAL_GL_TEXTURE_3D:
    case LOCAL_GL_TEXTURE_2D_ARRAY:
        return true;

    default:
        MOZ_CRASH("bad target");
    }
}

GLenum
DoTexImage(gl::GLContext* gl, TexImageTarget target, GLint level, GLenum internalFormat,
           GLsizei width, GLsizei height, GLsizei depth, GLenum unpackFormat,
           GLenum unpackType, const void* data)
{
    const GLint border = 0;

    gl::GLContext::LocalErrorScope errorScope(*gl);

    if (Is3D(target)) {
        gl->fTexImage3D(target.get(), level, internalFormat, width, height, depth,
                        border, unpackFormat, unpackType, data);
    } else {
        MOZ_ASSERT(depth == 1);
        gl->fTexImage2D(target.get(), level, internalFormat, width, height, border,
                        unpackFormat, unpackType, data);
    }

    return errorScope.GetError();
}

GLenum
DoTexSubImage(gl::GLContext* gl, TexImageTarget target, GLint level, GLint xOffset,
              GLint yOffset, GLint zOffset, GLsizei width, GLsizei height, GLsizei depth,
              GLenum unpackFormat, GLenum unpackType, const void* data)
{
    gl::GLContext::LocalErrorScope errorScope(*gl);

    if (Is3D(target)) {
        gl->fTexSubImage3D(target.get(), level, xOffset, yOffset, zOffset, width, height,
                           depth, unpackFormat, unpackType, data);
    } else {
        MOZ_ASSERT(zOffset == 0);
        MOZ_ASSERT(depth == 1);
        gl->fTexSubImage2D(target.get(), level, xOffset, yOffset, width, height,
                           unpackFormat, unpackType, data);
    }

    return errorScope.GetError();
}

static inline GLenum
DoCompressedTexImage(gl::GLContext* gl, TexImageTarget target, GLint level,
                     GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth,
                     GLint border, GLsizei dataSize, const void* data)
{
    gl::GLContext::LocalErrorScope errorScope(*gl);

    if (Is3D(target)) {
        gl->fCompressedTexImage3D(target.get(), level, internalFormat, width, height,
                                  depth, border, dataSize, data);
    } else {
        MOZ_ASSERT(depth == 1);
        gl->fCompressedTexImage2D(target.get(), level, internalFormat, width, height,
                                  border, dataSize, data);
    }

    return errorScope.GetError();
}

GLenum
DoCompressedTexSubImage(gl::GLContext* gl, TexImageTarget target, GLint level,
                        GLint xOffset, GLint yOffset, GLint zOffset, GLsizei width,
                        GLsizei height, GLsizei depth, GLenum sizedUnpackFormat,
                        GLsizei dataSize, const void* data)
{
    gl::GLContext::LocalErrorScope errorScope(*gl);

    if (Is3D(target)) {
        gl->fCompressedTexSubImage3D(target.get(), level, xOffset, yOffset, zOffset,
                                     width, height, depth, sizedUnpackFormat, dataSize,
                                     data);
    } else {
        MOZ_ASSERT(zOffset == 0);
        MOZ_ASSERT(depth == 1);
        gl->fCompressedTexSubImage2D(target.get(), level, xOffset, yOffset, width,
                                     height, sizedUnpackFormat, dataSize, data);
    }

    return errorScope.GetError();
}

static inline GLenum
DoCopyTexImage2D(gl::GLContext* gl, TexImageTarget target, GLint level,
                 GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height,
                 GLint border)
{
    gl->MakeCurrent();

    gl::GLContext::LocalErrorScope errorScope(*gl);

    MOZ_ASSERT(!Is3D(target));
    gl->fCopyTexImage2D(target.get(), level, internalFormat, x, y, width, height,
                        border);

    return errorScope.GetError();
}

static inline GLenum
DoCopyTexSubImage(gl::GLContext* gl, TexImageTarget target, GLint level, GLint xOffset,
                  GLint yOffset, GLint zOffset, GLint x, GLint y, GLsizei width,
                  GLsizei height)
{
    gl->MakeCurrent();

    gl::GLContext::LocalErrorScope errorScope(*gl);

    if (Is3D(target)) {
        gl->fCopyTexSubImage3D(target.get(), level, xOffset, yOffset, zOffset, x, y,
                               width, height);
    } else {
        MOZ_ASSERT(zOffset == 0);
        gl->fCopyTexSubImage2D(target.get(), level, xOffset, yOffset, x, y, width,
                               height);
    }

    return errorScope.GetError();
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// Actual (mostly generic) function implementations

void
WebGLTexture::TexStorage(const char* funcName, TexTarget target, GLsizei levels,
                         GLenum sizedFormat, GLsizei width, GLsizei height, GLsizei depth)
{
    // Check levels
    if (levels < 1) {
        mContext->ErrorInvalidValue("%s: `levels` must be >= 1.", funcName);
        return;
    }

    if (levels > 31) {
        // Right-shift is only defined for bits-1, so 31 for GLsizei.
        // Besides,
        mContext->ErrorInvalidValue("%s: `level` is too large.", funcName);
        return;
    }

    const TexImageTarget testTarget = IsCubeMap() ? LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X
                                                  : target.get();
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

    auto dstFormat = webgl::GetSizedFormat(sizedFormat);
    if (!dstFormat) {
        mContext->ErrorInvalidEnum("%s: Unrecognized internalformat: 0x%04x",
                                   funcName, sizedFormat);
        return;
    }

    auto dstUsage = mContext->mFormatUsage->GetUsage(dstFormat);
    if (!dstUsage || !dstUsage->asTexture) {
        mContext->ErrorInvalidEnum("%s: Invalid internalformat: 0x%04x", funcName,
                                   sizedFormat);
        return;
    }

    auto compression = dstFormat->compression;
    if (compression) {
        if (compression->subImageUpdateBehavior == webgl::SubImageUpdateBehavior::Forbidden) {
            mContext->ErrorInvalidOperation("%s: This format forbids"
                                            " compressedTexSubImage and thus would be"
                                            " useless after calling texStorage.",
                                            funcName);
        }
        if (compression->requirePOT) {
            if (!IsPowerOfTwo(uint32_t(width)) || !IsPowerOfTwo(uint32_t(height))) {
                mContext->ErrorInvalidOperation("%s: This format requires power-of-two"
                                                " width and height.",
                                                funcName);
                return;
            }
        }
    }

    ////////////////////////////////////
    // Do the thing!

    GLenum error = DoTexStorage(mContext->gl, target.get(), levels, sizedFormat, width,
                                height, depth);

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
    const WebGLTexture::ImageInfo newInfo(dstUsage, width, height, depth,
                                          isDataInitialized);
    SetImageInfosAtLevel(0, newInfo);

    PopulateMipChain(0, levels-1);
}

////////////////////////////////////////
// Tex(Sub)Image

bool
WebGLTexture::TexImage(const char* funcName, TexImageTarget target, GLint level,
                       GLenum internalFormat, GLint border, GLenum unpackFormat,
                       GLenum unpackType, webgl::TexUnpackBlob* unpackBlob)
{
    const webgl::PackingInfo srcPacking = { unpackFormat, unpackType };

    ////////////////////////////////////
    // Get dest info

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSpecification(funcName, target, level, unpackBlob->mWidth,
                                       unpackBlob->mHeight, unpackBlob->mDepth, border,
                                       &imageInfo))
    {
        return true;
    }
    MOZ_ASSERT(imageInfo);

    auto dstFormat = webgl::GetSizedFormat(internalFormat);
    if (!dstFormat && internalFormat == unpackFormat) {
        dstFormat = webgl::GetUnsizedFormat(srcPacking);
    }

    if (!dstFormat) {
        mContext->ErrorInvalidEnum("%s: Unrecognized internalformat/format/type:"
                                   " 0x%04x/0x%04x/0x%04x",
                                   funcName, internalFormat, unpackFormat, unpackType);
        return true;
    }

    auto dstUsage = mContext->mFormatUsage->GetUsage(dstFormat);
    if (!dstUsage || !dstUsage->asTexture) {
        mContext->ErrorInvalidEnum("%s: Invalid internalformat/format/type:"
                                   " 0x%04x/0x%04x/0x%04x",
                                   funcName, internalFormat, unpackFormat, unpackType);
        return true;
    }

    ////////////////////////////////////
    // Get source info
    const bool isFunc3D = Is3D(target);
    if (!unpackBlob->ValidateUnpack(mContext, funcName, isFunc3D, srcPacking))
        return true;

    ////////////////////////////////////
    // Check that source and dest info are compatible

    const webgl::DriverUnpackInfo* driverUnpackInfo;
    if (!dstUsage->IsUnpackValid(srcPacking, &driverUnpackInfo)) {
        mContext->ErrorInvalidOperation("%s: Mismatched internalFormat and format/type:"
                                        " 0x%04x and 0x%04x/0x%04x",
                                        funcName, internalFormat, unpackFormat,
                                        unpackType);
        return true;
    }

    ////////////////////////////////////
    // Do the thing!

    // It's tempting to do allocation first, and TexSubImage second, but this is generally
    // slower.

    const bool isSubImage = false;
    const GLint xOffset = 0;
    const GLint yOffset = 0;
    const GLint zOffset = 0;

    GLenum glError;
    if (!unpackBlob->TexOrSubImage(isSubImage, this, target, level, driverUnpackInfo,
                                   xOffset, yOffset, zOffset, &glError))
    {
        if (!glError)
            return false;

        if (glError == LOCAL_GL_OUT_OF_MEMORY) {
            mContext->ErrorOutOfMemory("%s: Driver ran out of memory during upload.",
                                       funcName);
            return true;
        }

        mContext->ErrorInvalidOperation("%s: Unexpected error during upload: 0x04x",
                                        funcName, glError);
        MOZ_ASSERT(false, "Unexpected GL error.");
        return true;
    }

    ////////////////////////////////////
    // Update our specification data.

    const ImageInfo newImageInfo(dstUsage, unpackBlob->mWidth, unpackBlob->mHeight,
                                 unpackBlob->mDepth, unpackBlob->mHasData);
    SetImageInfo(imageInfo, newImageInfo);
    return true;
}

bool
WebGLTexture::TexSubImage(const char* funcName, TexImageTarget target, GLint level,
                          GLint xOffset, GLint yOffset, GLint zOffset,
                          GLenum unpackFormat, GLenum unpackType,
                          webgl::TexUnpackBlob* unpackBlob)
{
    const webgl::PackingInfo srcPacking = { unpackFormat, unpackType };

    ////////////////////////////////////
    // Get dest info

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSelection(funcName, target, level, xOffset, yOffset, zOffset,
                                   unpackBlob->mWidth, unpackBlob->mHeight,
                                   unpackBlob->mDepth, &imageInfo))
    {
        return true;
    }
    MOZ_ASSERT(imageInfo);

    auto dstUsage = imageInfo->mFormat;
    MOZ_ASSERT(dstUsage && dstUsage->asTexture);

    auto dstFormat = dstUsage->format;
    MOZ_ASSERT(dstFormat);

    ////////////////////////////////////
    // Get source info

    const bool isFunc3D = Is3D(target);
    if (!unpackBlob->ValidateUnpack(mContext, funcName, isFunc3D, srcPacking))
        return true;

    ////////////////////////////////////
    // Check that source and dest info are compatible

    if (dstFormat->compression) {
        mContext->ErrorInvalidEnum("%s: Specified TexImage must not be compressed.",
                                   funcName);
        return true;
    }

    const webgl::DriverUnpackInfo* driverUnpackInfo;
    if (!dstUsage->IsUnpackValid(srcPacking, &driverUnpackInfo)) {
        mContext->ErrorInvalidOperation("%s: Mismatched internalFormat and format/type:"
                                        " %s and 0x%04x/0x%04x",
                                        funcName, dstFormat->name, unpackFormat,
                                        unpackType);
        return true;
    }

    ////////////////////////////////////
    // Do the thing!

    bool uploadWillInitialize;
    if (!EnsureImageDataInitializedForUpload(this, funcName, target, level, xOffset,
                                             yOffset, zOffset, unpackBlob->mWidth,
                                             unpackBlob->mHeight, unpackBlob->mDepth,
                                             imageInfo, &uploadWillInitialize))
    {
        return true;
    }

    const bool isSubImage = true;

    GLenum glError;
    if (!unpackBlob->TexOrSubImage(isSubImage, this, target, level, driverUnpackInfo,
                                   xOffset, yOffset, zOffset, &glError))
    {
        if (!glError)
            return false;

        if (glError == LOCAL_GL_OUT_OF_MEMORY) {
            mContext->ErrorOutOfMemory("%s: Driver ran out of memory during upload.",
                                       funcName);
            return true;
        }

        mContext->ErrorInvalidOperation("%s: Unexpected error during upload: 0x04x",
                                        funcName, glError);
        MOZ_ASSERT(false, "Unexpected GL error.");
        return true;
    }

    ////////////////////////////////////
    // Update our specification data?

    if (uploadWillInitialize) {
        imageInfo->SetIsDataInitialized(true, this);
    }
    return true;
}

////////////////////////////////////////
// CompressedTex(Sub)Image

void
WebGLTexture::CompressedTexImage(const char* funcName, TexImageTarget target, GLint level,
                                 GLenum internalFormat, GLsizei width, GLsizei height,
                                 GLsizei depth, GLint border,
                                 const dom::ArrayBufferView& view)
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

    auto format = webgl::GetSizedFormat(internalFormat);
    if (!format || !format->compression) {
        mContext->ErrorInvalidOperation("%s: Invalid format for unpacking.", funcName);
        return;
    }

    auto usage = mContext->mFormatUsage->GetUsage(format);
    if (!usage || !usage->asTexture) {
        mContext->ErrorInvalidEnum("%s: Invalid internalformat: 0x%04x", funcName,
                                   internalFormat);
        return;
    }

    ////////////////////////////////////
    // Get source info

    view.ComputeLengthAndData();
    size_t dataSize = view.Length();
    const void* data = view.Data();

    if (format->compression->requirePOT) {
        if (!IsPowerOfTwo(uint32_t(width)) || !IsPowerOfTwo(uint32_t(height))) {
            mContext->ErrorInvalidOperation("%s: This format requires power-of-two width"
                                            " and height.",
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

    GLenum error = DoCompressedTexImage(mContext->gl, target, level, internalFormat,
                                        width, height, depth, border, dataSize, data);
    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        mContext->ErrorOutOfMemory("%s: Ran out of memory during upload.", funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        mContext->GenerateWarning("%s: Unexpected error during texture upload. Context"
                                  " lost.",
                                  funcName);
        mContext->ForceLoseContext();
        return;
    }

    ////////////////////////////////////
    // Update our specification data.

    const bool isDataInitialized = true;
    const ImageInfo newImageInfo(usage, width, height, depth, isDataInitialized);
    SetImageInfo(imageInfo, newImageInfo);
}

static inline bool
IsSubImageBlockAligned(const webgl::CompressedFormatInfo* compression,
                       const WebGLTexture::ImageInfo* imageInfo, GLint xOffset,
                       GLint yOffset, GLsizei width, GLsizei height)
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
WebGLTexture::CompressedTexSubImage(const char* funcName, TexImageTarget target,
                                    GLint level, GLint xOffset, GLint yOffset,
                                    GLint zOffset, GLsizei width, GLsizei height,
                                    GLsizei depth, GLenum sizedUnpackFormat,
                                    const dom::ArrayBufferView& view)
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

    auto dstUsage = imageInfo->mFormat;
    MOZ_ASSERT(dstUsage);
    auto dstFormat = dstUsage->format;
    MOZ_ASSERT(dstFormat);

    ////////////////////////////////////
    // Get source info

    view.ComputeLengthAndData();
    size_t dataSize = view.Length();
    const void* data = view.Data();

    auto srcFormat = webgl::GetSizedFormat(sizedUnpackFormat);
    if (srcFormat != dstFormat) {
        mContext->ErrorInvalidValue("%s: `format` must match format of specified texture"
                                    " image.",
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
        mContext->ErrorInvalidOperation("%s: Format does not allow sub-image"
                                        " updates.", funcName);
        return;

    case webgl::SubImageUpdateBehavior::FullOnly:
        if (xOffset || yOffset ||
            width != imageInfo->mWidth ||
            height != imageInfo->mHeight)
        {
            mContext->ErrorInvalidOperation("%s: Format does not allow partial sub-image"
                                            " updates.",
                                            funcName);
            return;
        }
        break;

    case webgl::SubImageUpdateBehavior::BlockAligned:
        if (!IsSubImageBlockAligned(dstFormat->compression, imageInfo, xOffset, yOffset,
                                    width, height))
        {
            mContext->ErrorInvalidOperation("%s: Format requires block-aligned sub-image"
                                            " updates.",
                                            funcName);
            return;
        }
        break;
    }

    ////////////////////////////////////
    // Do the thing!

    bool uploadWillInitialize;
    if (!EnsureImageDataInitializedForUpload(this, funcName, target, level, xOffset,
                                             yOffset, zOffset, width, height, depth,
                                             imageInfo, &uploadWillInitialize))
    {
        return;
    }

    GLenum error = DoCompressedTexSubImage(mContext->gl, target, level, xOffset, yOffset,
                                           zOffset, width, height, depth,
                                           sizedUnpackFormat, dataSize, data);
    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        mContext->ErrorOutOfMemory("%s: Ran out of memory during upload.", funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        mContext->GenerateWarning("%s: Unexpected error during texture upload. Context"
                                  " lost.",
                                  funcName);
        mContext->ForceLoseContext();
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

    auto dstFormat = webgl::GetSizedFormat(internalFormat);
    if (!dstFormat) {
        mContext->ErrorInvalidEnum("%s: Unrecognized internalformat: 0x%04x", funcName,
                         internalFormat);
        return;
    }

    auto dstUsage = mContext->mFormatUsage->GetUsage(dstFormat);
    if (!dstUsage || !dstUsage->asTexture) {
        mContext->ErrorInvalidEnum("%s: Invalid internalformat: 0x%04x", funcName,
                                   internalFormat);
        return;
    }

    ////////////////////////////////////
    // Get source info

    const webgl::FormatInfo* srcFormat;
    if (!mContext->GetSrcFBFormat(funcName, &srcFormat))
        return;

    ////////////////////////////////////
    // Check that source and dest info are compatible

    if (!ValidateCopyTexImageFormats(mContext, funcName, srcFormat, dstFormat))
        return;

    ////////////////////////////////////
    // Do the thing!

    GLenum error = DoCopyTexImage2D(mContext->gl, target, level, internalFormat, x, y,
                                    width, height, border);
    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        mContext->ErrorOutOfMemory("%s: Ran out of memory during texture copy.",
                                   funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        mContext->GenerateWarning("%s: Unexpected error during texture copy. Context"
                                  " lost.",
                                  funcName);
        mContext->ForceLoseContext();
        return;
    }

    ////////////////////////////////////
    // Update our specification data.

    const bool isDataInitialized = true;
    const ImageInfo newImageInfo(dstUsage, width, height, depth, isDataInitialized);
    SetImageInfo(imageInfo, newImageInfo);
}

void
WebGLTexture::CopyTexSubImage(const char* funcName, TexImageTarget target, GLint level,
                              GLint xOffset, GLint yOffset, GLint zOffset, GLint x,
                              GLint y, GLsizei width, GLsizei height)
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

    auto dstUsage = imageInfo->mFormat;
    MOZ_ASSERT(dstUsage);
    auto dstFormat = dstUsage->format;
    MOZ_ASSERT(dstFormat);

    ////////////////////////////////////
    // Get source info

    const webgl::FormatInfo* srcFormat;
    if (!mContext->GetSrcFBFormat(funcName, &srcFormat))
        return;

    ////////////////////////////////////
    // Check that source and dest info are compatible

    if (!ValidateCopyTexImageFormats(mContext, funcName, srcFormat, dstFormat))
        return;

    ////////////////////////////////////
    // Do the thing!

    bool uploadWillInitialize;
    if (!EnsureImageDataInitializedForUpload(this, funcName, target, level, xOffset,
                                             yOffset, zOffset, width, height, depth,
                                             imageInfo, &uploadWillInitialize))
    {
        return;
    }

    GLenum error = DoCopyTexSubImage(mContext->gl, target, level, xOffset, yOffset,
                                     zOffset, x, y, width, height);

    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        mContext->ErrorOutOfMemory("%s: Ran out of memory during texture copy.",
                                   funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        mContext->GenerateWarning("%s: Unexpected error during texture copy. Context"
                                  " lost.",
                                  funcName);
        mContext->ForceLoseContext();
        return;
    }

    ////////////////////////////////////
    // Update our specification data?

    if (uploadWillInitialize) {
        imageInfo->SetIsDataInitialized(true, this);
    }
}

} // namespace mozilla
