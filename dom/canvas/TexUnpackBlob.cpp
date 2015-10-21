/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TexUnpackBlob.h"

#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLDefs.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "nsLayoutUtils.h"
#include "WebGLContext.h"
#include "WebGLTexelConversions.h"

namespace mozilla {
namespace webgl {

static GLenum
TexImage(gl::GLContext* gl, TexImageTarget target, GLint level,
         const webgl::DriverUnpackInfo* driverUnpackInfo, GLsizei width, GLsizei height,
         GLsizei depth, const void* data)
{
    const GLenum& internalFormat = driverUnpackInfo->internalFormat;
    const GLint border = 0;
    const GLenum& unpackFormat = driverUnpackInfo->unpackFormat;
    const GLenum& unpackType = driverUnpackInfo->unpackType;

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
        MOZ_CRASH('bad target');
    }

    return errorScope.GetError();
}

//////////////////////////////////////////////////////////////////////////////////////////
// TexUnpackBuffer

static bool
GetPackedSizeForUnpack(uint32_t bytesPerPixel, uint8_t rowByteAlignment,
                       uint32_t maybeStridePixelsPerRow, uint32_t maybeStrideRowsPerImage,
                       uint32_t skipPixelsPerRow, uint32_t skipRowsPerImage,
                       uint32_t skipImages, uint32_t usedPixelsPerRow,
                       uint32_t usedRowsPerImage, uint32_t usedImages,
                       uint32_t* const out_packedBytes)
{
    MOZ_RELEASE_ASSERT(rowByteAlignment != 0);

    if (!usedPixelsPerRow || !usedRowsPerImage || !usedImages) {
        *out_packedBytes = 0;
        return true;
    }
    // Now we know there's at least one image.

    CheckedUint32 pixelsPerRow = CheckedUint32(skipPixelsPerRow) + usedPixelsPerRow;
    CheckedUint32 rowsPerImage = CheckedUint32(skipRowsPerImage) + usedRowsPerImage;
    CheckedUint32 images = CheckedUint32(skipImages) + usedImages;

    MOZ_ASSERT_IF(maybeStridePixelsPerRow,
                  (pixelsPerRow.isValid() &&
                   maybeStridePixelsPerRow >= pixelsPerRow.value()));

    MOZ_ASSERT_IF(maybeStrideRowsPerImage,
                  (rowsPerImage.isValid() &&
                   maybeStrideRowsPerImage >= rowsPerImage.value()));

    CheckedUint32 stridePixelsPerRow = maybeStridePixelsPerRow ? maybeStridePixelsPerRow
                                                               : pixelsPerRow;
    CheckedUint32 strideRowsPerImage = maybeStrideRowsPerImage ? maybeStrideRowsPerImage
                                                               : rowsPerImage;

    CheckedUint32 strideBytesPerRow = bytesPerPixel * stridePixelsPerRow;
    strideBytesPerRow = RoundUpToMultipleOf(strideBytesPerRow, rowByteAlignment);

    CheckedUint32 strideBytesPerImage = strideBytesPerRow * strideRowsPerImage;

    CheckedUint32 lastRowBytes = CheckedUint32(bytesPerPixel) * pixelsPerRow;
    CheckedUint32 lastImageBytes = strideBytesPerRow * (rowsPerImage - 1) + lastRowBytes;

    CheckedUint32 packedBytes = strideBytesPerImage * (images - 1) + lastImageBytes;

    if (!packedBytes.isValid())
        return false;

    *out_packedBytes = packedBytes.value();
    return true;
}

////////////////////

bool
TexUnpackBuffer::ValidateUnpack(WebGLContext* webgl, uint8_t funcDims,
                                const webgl::PackingInfo* packingInfo) override
{
    const auto bytesPerPixel           = format->bytesPerPixel;
    const auto rowByteAlignment        = webgl->mPixelStore_UnpackAlignment;
    const auto maybeStridePixelsPerRow = webgl->mPixelStore_UnpackRowLength;
    const auto maybeStrideRowsPerImage = webgl->mPixelStore_UnpackImageHeight;
    const auto skipPixelsPerRow        = webgl->mPixelStore_UnpackSkipPixels;
    const auto skipRowsPerImage        = webgl->mPixelStore_UnpackSkipRows;
    const auto skipImages              = (funcDims == 3) ? webgl->mPixelStore_UnpackSkipImages
                                                         : 0;
    const auto usedPixelsPerRow = mWidth;
    const auto usedRowsPerImage = mHeight;
    const auto usedImages       = mDepth;

    uint32_t bytesNeeded;
    if (!GetPackedSizeForUnpack(bytesPerPixel, rowByteAlignment,
                                maybeStridePixelsPerRow, maybeStrideRowsPerImage,
                                skipPixelsPerRow, skipRowsPerImage, skipImages,
                                usedPixelsPerRow, usedRowsPerImage, usedImages,
                                &bytesNeeded))
    {
        mContext->ErrorInvalidOperation("%s: Overflow while computing the needed"
                                        " buffer size.",
                                        funcName);
        return false;
    }

    if (mData && bytesNeeded > mDataSize) {
        mContext->ErrorInvalidOperation("%s: Provided buffer is too small. (needs %u,"
                                        " has %u)",
                                        funcName, bytesNeeded, mDataSize);
        return false;
    }

    return true;
}

bool
TexUnpackBuffer::TexImage(WebGLTexture* tex, uint8_t funcDims, TexImageTarget target,
                          GLint level, const webgl::DriverUnpackInfo* driverUnpackInfo,
                          GLenum* const out_glError) override
{
    WebGLContext* webgl = tex->mContext;
    gl::GLContext* gl = webgl->gl;

    GLenum error = TexImage(gl, funcDims, target, level, driverUnpackInfo, mWidth,
                            mHeight, mDepth, mData);

    *out_glError = error;
    return !error;
}

//////////////////////////////////////////////////////////////////////////////////////////
// TexUnpackImage

TexUnpackImage::TexUnpackImage(const RefPtr<mozilla::layers::Image>& image,
                               dom::HTMLMediaElement* elem)
    : TexUnpackBlob(image->GetSize().width, image->GetSize().height, 1, true)
    , mImage(image)
    , mElem(elem)
{ }

bool
TexUnpackImage::TexImage(WebGLTexture* tex, uint8_t funcDims, TexImageTarget target,
                         GLint level, const webgl::DriverUnpackInfo* driverUnpackInfo,
                         GLenum* const out_glError) override
{
    *out_glError = 0;

    WebGLContext* webgl = tex->mContext;
    gl::GLContext* gl = webgl->gl;

    TexUnpackImage* unpackImage = unpackBlob->AsTexUnpackImage();
    MOZ_ASSERT(unpackImage);

    if (!webgl->mPixelStore_PrmultiplyAlpha ||
        level != 0)
    {
        return false;
    }

    switch (formatInfo->effectiveFormat) {
    case webgl::EffectiveFormat::RGB8:
    case webgl::EffectiveFormat::RGBA8:
        if (driverUnpackInfo->unpackType == LOCAL_GL_UNSIGNED_BYTE)
            break;

        return false;

    default:
        return false;
    }

    // Looks good so far. We need to allocate the texture to blit into.

    GLenum error = TexImage(gl, target, level, driverUnpackInfo, mWidth, mHeight, mDepth,
                            nullptr);
    if (error)
        return false;

    auto& image = unpackImage->mImage;
    const gl::OriginPos dstOrigin = mPixelStore_FlipY ? gl::OriginPos::BottomLeft
                                                      : gl::OriginPos::TopLeft;
    if (gl->BlitHelper()->BlitImageToTexture(image, image->GetSize(), tex->mGLName,
                                             target, destOrigin))
    {
        return true;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// TexUnpackSrcSurface

static bool
GuessAlignment(const void* data, size_t width, size_t stride, size_t maxAlignment,
               size_t* const out_alignment)
{
    size_t alignmentGuess = maxAlignment;
    while (alignmentGuess) {
        size_t guessStride = RoundUpToMultipleOf(width, alignmentGuess);
        if (guessStride == stride &&
            data % alignmentGuess == 0)
        {
            *out_alignment = alignmentGuess;
            return true;
        }
        alignmentGuess /= 2;
    }
    return false;
}

static bool
SupportsBGRA(gl::GLContext* gl)
{
    if (gl->IsANGLE())
        return true;

    return false;
}

static bool
UploadDataSurface(WebGLContext* webgl, uint8_t funcDims, TexImageTarget target,
                  GLint level, const webgl::DriverUnpackInfo* driverUnpackInfo,
                  GLsizei width, GLsizei height, gfx::DataSourceSurface* surf,
                  GLenum* const out_glError)
{
    *out_glError = 0;

    if (!webgl->mPixelStore_PremultiplyAlpha) {
        // Surfaces are all premultiplied, so if we're not asking for premult, we can't do
        // the upload directly.
        return false;
    }

    gl::GLContext* gl = webgl->gl;

    // This differs from the raw-data upload in that we choose how we do the unpack.
    // (alignment, etc.)

    // Uploading RGBX as RGBA and blitting to RGB is faster than repacking RGBX into
    // RGB on the CPU. However, this is optimization is out-of-scope for now.

    static const webgl::UnpackInfo kInfoBGRA = {
        LOCAL_GL_BGRA,
        LOCAL_GL_BGRA,
        LOCAL_GL_UNSIGNED_BYTE,
        4,
    };

    const webgl::UnpackInfo* chosenInfo = nullptr;

    switch (surf->GetFormat()) {
    case gfx::SurfaceFormat::B8G8R8A8:
        if (driverUnpackInfo->internalFormat == LOCAL_GL_RGBA &&
            driverUnpackInfo->unpackFormat == LOCAL_GL_RGBA &&
            driverUnpackInfo->unpackType == LOCAL_GL_UNSIGNED_BYTE)
        {
            if (gl->IsANGLE()) {
                chosenInfo = &kInfoBGRA;
            }
        }
        break;

    case gfx::SurfaceFormat::R8G8B8A8:
        if (driverUnpackInfo->unpackFormat == LOCAL_GL_RGBA &&
            driverUnpackInfo->unpackType == LOCAL_GL_UNSIGNED_BYTE)
        {
            chosenInfo = driverUnpackInfo;
        }
        break;

    case gfx::SurfaceFormat::R5G6B5:
        if (driverUnpackInfo->unpackFormat == LOCAL_GL_RGB &&
            driverUnpackInfo->unpackType == LOCAL_GL_UNSIGNED_SHORT_5_6_5)
        {
            chosenInfo = driverUnpackInfo;
        }
        break;
    }

    if (!chosenInfo)
        return false;

    gfx::DataSourceSurface::ScopedMap map(surf, gfx::DataSourceSurface::MapType::READ);
    if (!map.IsMapped())
        return false;

    const GLint kMaxUnpackAlignment = 8;
    size_t unpackAlignment;
    if (!GuessAlignment(map.GetData(), width, map.GetStride(), kMaxUnpackAlignment,
                        &unpackAlignment))
    {
        return false;
    }

    gl->MakeCurrent();

    const GLuint pixelUnpackBuffer = 0;
    ScopedUnpackReset scopedReset(webgl, pixelUnpackBuffer);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, unpackAlignment);

    const GLsizei depth = 1;
    GLenum error = TexImage(gl, GLenum(target), level, chosenInfo, width, height, depth,
                            map.GetData());
    if (error) {
        *out_glError = error;
        return false;
    }

    return true;
}

////////////////////

static bool
GetFormatForSurf(gfx::SourceSurface* surf, WebGLTexelFormat* const out_texelFormat)
{
    gfx::SurfaceFormat surfFormat = surf->GetFormat();

    switch (surfFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
        *out_texelFormat = WebGLTexelFormat::BGRA8;
        return true;

    case gfx::SurfaceFormat::B8G8R8X8:
        *out_texelFormat = WebGLTexelFormat::BGRX8;
        return true;

    case gfx::SurfaceFormat::R8G8B8A8:
        *out_texelFormat = WebGLTexelFormat::RGBA8;
        return true;

    case gfx::SurfaceFormat::R8G8B8X8:
        *out_texelFormat = WebGLTexelFormat::RGBX8;
        return true;

    case gfx::SurfaceFormat::R5G6B5:
        *out_texelFormat = WebGLTexelFormat::RGB565;
        return true;

    case gfx::SurfaceFormat::A8:
        *out_texelFormat = WebGLTexelFormat::A8;
        return true;

    case gfx::SurfaceFormat::YUV:
        // Ugh...
        NS_ERROR("We don't handle uploads from YUV sources yet.");
        // When we want to, check out gfx/ycbcr/YCbCrUtils.h. (specifically
        // GetYCbCrToRGBDestFormatAndSize and ConvertYCbCrToRGB)
        return false;

    case gfx::SurfaceFormat::UNKNOWN:
        return false;
    }
}

static bool
GetFormatForPackingTuple(GLenum packingFormat, GLenum packingType,
                         WebGLTexelFormat* const out_texelFormat)
{
    switch (packingType) {
    case LOCAL_GL_UNSIGNED_BYTE:
        switch (packingFormat) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
            *out_texelFormat = WebGLTexelFormat::R8;
            return true;

        case LOCAL_GL_ALPHA:
            *out_texelFormat = WebGLTexelFormat::A8;
            return true;

        case LOCAL_GL_LUMINANCE_ALPHA:
            *out_texelFormat = WebGLTexelFormat::RA8;
            return true;

        case LOCAL_GL_RGB:
            *out_texelFormat = WebGLTexelFormat::RGB8;
            return true;

        case LOCAL_GL_RGBA:
            *out_texelFormat = WebGLTexelFormat::RGBA8;
            return true;

        default:
            break;
        }

    case LOCAL_GL_UNSIGNED_SHORT_565:
        switch (packingFormat) {
        case LOCAL_GL_RGB:
            *out_texelFormat = WebGLTexelFormat::RGB565;
            return true;

        default:
            break;
        }

    case LOCAL_GL_UNSIGNED_SHORT_5551:
        switch (packingFormat) {
        case LOCAL_GL_RGBA:
            *out_texelFormat = WebGLTexelFormat::RGBA5551;
            return true;

        default:
            break;
        }

    case LOCAL_GL_UNSIGNED_SHORT_4444:
        switch (packingFormat) {
        case LOCAL_GL_RGBA:
            *out_texelFormat = WebGLTexelFormat::RGBA4444;
            return true;

        default:
            break;
        }

    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
        switch (packingFormat) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
            *out_texelFormat = WebGLTexelFormat::R16F;
            return true;

        case LOCAL_GL_ALPHA:
            *out_texelFormat = WebGLTexelFormat::A16F;
            return true;

        case LOCAL_GL_LUMINANCE_ALPHA:
            *out_texelFormat = WebGLTexelFormat::RA16F;
            return true;

        case LOCAL_GL_RGB:
            *out_texelFormat = WebGLTexelFormat::RGB16F;
            return true;

        case LOCAL_GL_RGBA:
            *out_texelFormat = WebGLTexelFormat::RGBA16F;
            return true;

        default:
            break;
        }

    case LOCAL_GL_FLOAT:
        switch (packingFormat) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
            *out_texelFormat = WebGLTexelFormat::R32F;
            return true;

        case LOCAL_GL_ALPHA:
            *out_texelFormat = WebGLTexelFormat::A32F;
            return true;

        case LOCAL_GL_LUMINANCE_ALPHA:
            *out_texelFormat = WebGLTexelFormat::RA32F;
            return true;

        case LOCAL_GL_RGB:
            *out_texelFormat = WebGLTexelFormat::RGB32F;
            return true;

        case LOCAL_GL_RGBA:
            *out_texelFormat = WebGLTexelFormat::RGBA32F;
            return true;

        default:
            break;
        }

    default:
        break;
    }

    NS_ERROR("Unsupported EffectiveFormat dest format for DOM element upload.");
    return false;
}

static bool
ConvertSurface(WebGLContext* webgl, gfx::DataSourceSurface* srcSurf,
               const webgl::UnpackInfo* dstInfo,
               UniqueBuffer* const out_convertedBuffer,
               uint8_t* const out_convertedAlignment,
               bool* const out_outOfMemory)
{
    *out_outOfMemory = false;

    const size_t width = srcSurf->GetSize().width;
    const size_t height = srcSurf->GetSize().height;
    const bool yFlip = webgl->mPixelStore_FlipY;

    // Source args:

    gfx::DataSourceSurface::ScopedMap srcMap(srcSurf,
                                             gfx::DataSourceSurface::MapType::READ);
    if (!srcMap.IsMapped())
        return false;

    const void* const srcBegin = srcMap.GetData();
    const size_t srcStride = srcMap.GetStride();

    WebGLTexelFormat srcFormat;
    if (!GetFormatForSurf(srcSurf, &srcFormat))
        return false;

    const bool srcPremultiplied = true;

    // Dest args:

    WebGLTexelFormat dstFormat;
    if (!GetFormatForPackingTuple(dstInfo->unpackFormat, dstInfo->unpackType,
                                  &dstFormat))
    {
        return false;
    }

    const size_t dstAlignment = 8; // Just use the max!

    const size_t dstRowBytes = dstInfo->bytesPerPixel * width;
    const size_t dstStride = Stride(dstRowBytes, dstAlignment);

    CheckedUint32 checkedDstSize = dstStride;
    checkedDstSize *= height;
    if (!checkedDstSize.isValid()) {
        *out_outOfMemory = true;
        return false;
    }

    const size_t dstSize = checkedDstSize.value();

    UniqueBuffer dstBuffer = malloc(dstSize);
    if (!dstBuffer) {
        *out_outOfMemory = true;
        return false;
    }
    const void* const dstBegin = dstBuffer.get();

    const bool dstPremultiplied = webgl->mPixelStore_PremultiplyAlpha;

    // And go!:

    if (!ConvertImage(width, height, yFlip, srcBegin, srcStride, srcFormat,
                      srcPremultiplied, dstBegin, dstStride, dstFormat,
                      dstPremultiplied))
    {
        MOZ_ASSERT(false, "ConvertImage failed unexpectedly.");
        NS_ERROR("ConvertImage failed unexpectedly.");
        *out_outOfMemory = true;
        return false;
    }

    *out_convertedBuffer = Move(dstBuffer);
    *out_alignment = dstAlignment;
    return true;
}


////////////////////

TexUnpackSourceSurface::TexUnpackSourceSurface(const RefPtr<gfx::DataSourceSurface>& surf)
    : TexUnpackBlob(surf->GetSize().width, surf->GetSize().height, 1, true)
    , mSurf(surf)
{ }

bool
TexUnpackSourceSurface::TexImage(WebGLTexture* tex, uint8_t funcDims,
                                 TexImageTarget target, GLint level,
                                 const webgl::DriverUnpackInfo* driverUnpackInfo,
                                 GLenum* const out_glError) override
{
    *out_glError = 0;

    WebGLContext* webgl = tex->mContext;

    GLenum error;
    if (UploadDataSurface(webgl, funcDims, GLenum(target), level, driverUnpackInfo, mSurf,
                          &error))
    {
        return true;
    }
    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        *out_glError = LOCAL_GL_OUT_OF_MEMORY;
        return false;
    }

    // CPU conversion. (++numCopies)

    UniqueBuffer convertedBuffer;
    uint8_t convertedAlignment;
    bool outOfMemory;

    if (!ConvertSurface(webgl, mSurf, driverUnpackInfo, &convertedBuffer,
                        &convertedAlignment, &outOfMemory))
    {
        if (outOfMemory) {
            *out_glError = LOCAL_GL_OUT_OF_MEMORY;
        }
        return false;
    }

    const GLuint pixelUnpackBuffer = 0;
    ScopedUnpackReset scopedReset(webgl, pixelUnpackBuffer);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, convertedAlignment);

    GLenum error = TexImage(gl, target, level, driverUnpackInfo, mWidth, mHeight, mDepth,
                            convertedBuffer.get());
    if (error) {
        *out_glError = error;
        return false;
    }

    return true;
}

} // namespace webgl
} // namespace mozilla
