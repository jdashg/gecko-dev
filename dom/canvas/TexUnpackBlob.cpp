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
#include "WebGLTexture.h"

namespace mozilla {
namespace webgl {

static GLenum
DoTexOrSubImage(bool isSubImage, gl::GLContext* gl, TexImageTarget target, GLint level,
                const DriverUnpackInfo* dui, GLint xOffset, GLint yOffset, GLint zOffset,
                GLsizei width, GLsizei height, GLsizei depth, const void* data)
{
    auto internalFormat = dui->internalFormat;
    auto unpackFormat = dui->unpackFormat;
    auto unpackType = dui->unpackType;

    if (isSubImage) {
        return DoTexSubImage(gl, target, level, xOffset, yOffset, zOffset, width, height,
                             depth, unpackFormat, unpackType, data);
    } else {
        return DoTexImage(gl, target, level, internalFormat, width, height, depth,
                          unpackFormat, unpackType, data);
    }
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
TexUnpackBuffer::ValidateUnpack(WebGLContext* webgl, const char* funcName, bool isFunc3D,
                                const webgl::PackingInfo& packingInfo)
{
    const auto bytesPerPixel           = webgl::BytesPerPixel(packingInfo);
    const auto rowByteAlignment        = webgl->mPixelStore_UnpackAlignment;
    const auto maybeStridePixelsPerRow = webgl->mPixelStore_UnpackRowLength;
    const auto maybeStrideRowsPerImage = webgl->mPixelStore_UnpackImageHeight;
    const auto skipPixelsPerRow        = webgl->mPixelStore_UnpackSkipPixels;
    const auto skipRowsPerImage        = webgl->mPixelStore_UnpackSkipRows;
    const auto skipImages              = isFunc3D ? webgl->mPixelStore_UnpackSkipImages
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
        webgl->ErrorInvalidOperation("%s: Overflow while computing the needed buffer"
                                     " size.",
                                     funcName);
        return false;
    }

    if (mData && bytesNeeded > mDataSize) {
        webgl->ErrorInvalidOperation("%s: Provided buffer is too small. (needs %u, has"
                                     " %u)",
                                     funcName, bytesNeeded, mDataSize);
        return false;
    }

    return true;
}

bool
TexUnpackBuffer::TexOrSubImage(bool isSubImage, WebGLTexture* tex, TexImageTarget target,
                               GLint level,
                               const webgl::DriverUnpackInfo* driverUnpackInfo,
                               GLint xOffset, GLint yOffset, GLint zOffset,
                               GLenum* const out_glError)
{
    WebGLContext* webgl = tex->mContext;
    gl::GLContext* gl = webgl->gl;

    GLenum error = DoTexOrSubImage(isSubImage, gl, target, level, driverUnpackInfo,
                                   xOffset, yOffset, zOffset, mWidth, mHeight, mDepth,
                                   mData);
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
TexUnpackImage::TexOrSubImage(bool isSubImage, WebGLTexture* tex, TexImageTarget target,
                              GLint level, const webgl::DriverUnpackInfo* dui,
                              GLint xOffset, GLint yOffset, GLint zOffset,
                              GLenum* const out_glError)
{
    *out_glError = 0;

    WebGLContext* webgl = tex->mContext;
    gl::GLContext* gl = webgl->GL();

    if (!webgl->mPixelStore_PremultiplyAlpha ||
        level != 0)
    {
        return false;
    }

    auto internalFormat = dui->internalFormat;
    auto unpackFormat = dui->unpackFormat;
    auto unpackType = dui->unpackType;

    switch (unpackType) {
    case LOCAL_GL_UNSIGNED_BYTE:
        switch (unpackFormat) {
        case LOCAL_GL_RGB:
        case LOCAL_GL_RGBA:
            break;

        default:
            return false;
        }

    default:
        return false;
    }

    if (isSubImage)
        return false; // TODO: Not supported by out blit functions yet!

    // Looks good so far. We need to allocate the texture to blit into.

    GLenum error = DoTexImage(gl, target, level, internalFormat, mWidth, mHeight, mDepth,
                              unpackFormat, unpackType, nullptr);
    if (error)
        return false;

    const gl::OriginPos dstOrigin = webgl->mPixelStore_FlipY ? gl::OriginPos::BottomLeft
                                                             : gl::OriginPos::TopLeft;
    if (gl->BlitHelper()->BlitImageToTexture(mImage, mImage->GetSize(), tex->mGLName,
                                             target.get(), dstOrigin))
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
            uintptr_t(data) % alignmentGuess == 0)
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

bool
TexUnpackSrcSurface::UploadDataSurface(bool isSubImage, WebGLContext* webgl,
                                       TexImageTarget target, GLint level,
                                       const webgl::DriverUnpackInfo* dui, GLint xOffset,
                                       GLint yOffset, GLint zOffset,
                                       gfx::DataSourceSurface* surf,
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

    static const webgl::DriverUnpackInfo kInfoBGRA = {
        LOCAL_GL_BGRA,
        LOCAL_GL_BGRA,
        LOCAL_GL_UNSIGNED_BYTE,
    };

    const webgl::DriverUnpackInfo* chosenDUI = nullptr;

    switch (surf->GetFormat()) {
    case gfx::SurfaceFormat::B8G8R8A8:
        if (dui->internalFormat == LOCAL_GL_RGBA &&
            dui->unpackFormat == LOCAL_GL_RGBA &&
            dui->unpackType == LOCAL_GL_UNSIGNED_BYTE)
        {
            if (gl->IsANGLE()) {
                chosenDUI = &kInfoBGRA;
            }
        }
        break;

    case gfx::SurfaceFormat::R8G8B8A8:
        if (dui->unpackFormat == LOCAL_GL_RGBA &&
            dui->unpackType == LOCAL_GL_UNSIGNED_BYTE)
        {
            chosenDUI = dui;
        }
        break;

    case gfx::SurfaceFormat::R5G6B5:
        if (dui->unpackFormat == LOCAL_GL_RGB &&
            dui->unpackType == LOCAL_GL_UNSIGNED_SHORT_5_6_5)
        {
            chosenDUI = dui;
        }
        break;
    }

    if (!chosenDUI)
        return false;

    gfx::DataSourceSurface::ScopedMap map(surf, gfx::DataSourceSurface::MapType::READ);
    if (!map.IsMapped())
        return false;

    const GLint kMaxUnpackAlignment = 8;
    size_t unpackAlignment;
    if (!GuessAlignment(map.GetData(), mWidth, map.GetStride(), kMaxUnpackAlignment,
                        &unpackAlignment))
    {
        return false;
    }

    gl->MakeCurrent();

    const GLuint pixelUnpackBuffer = 0;
    ScopedUnpackReset scopedReset(webgl, pixelUnpackBuffer);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, unpackAlignment);

    MOZ_ASSERT(mDepth == 1);
    GLenum error = DoTexOrSubImage(isSubImage, gl, target.get(), level, chosenDUI,
                                   xOffset, yOffset, zOffset, mWidth, mHeight, mDepth,
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

    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
        switch (packingFormat) {
        case LOCAL_GL_RGB:
            *out_texelFormat = WebGLTexelFormat::RGB565;
            return true;

        default:
            break;
        }

    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
        switch (packingFormat) {
        case LOCAL_GL_RGBA:
            *out_texelFormat = WebGLTexelFormat::RGBA5551;
            return true;

        default:
            break;
        }

    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
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

bool
TexUnpackSrcSurface::ConvertSurface(WebGLContext* webgl,
                                    const webgl::DriverUnpackInfo* dui,
                                    UniqueBuffer* const out_convertedBuffer,
                                    uint8_t* const out_convertedAlignment,
                                    bool* const out_outOfMemory)
{
    *out_outOfMemory = false;

    const size_t width = mSurf->GetSize().width;
    const size_t height = mSurf->GetSize().height;

    // Source args:

    gfx::DataSourceSurface::ScopedMap srcMap(mSurf,
                                             gfx::DataSourceSurface::MapType::READ);
    if (!srcMap.IsMapped())
        return false;

    const void* const srcBegin = srcMap.GetData();
    const size_t srcStride = srcMap.GetStride();

    WebGLTexelFormat srcFormat;
    if (!GetFormatForSurf(mSurf, &srcFormat))
        return false;

    const bool srcPremultiplied = true;

    // Dest args:

    WebGLTexelFormat dstFormat;
    if (!GetFormatForPackingTuple(dui->unpackFormat, dui->unpackType, &dstFormat))
        return false;

    const auto bytesPerPixel = webgl::BytesPerPixel({dui->unpackFormat, dui->unpackType});
    const size_t dstRowBytes = bytesPerPixel * width;

    const size_t dstAlignment = 8; // Just use the max!
    const size_t dstStride = RoundUpToMultipleOf(dstRowBytes, dstAlignment);

    CheckedUint32 checkedDstSize = dstStride;
    checkedDstSize *= height;
    if (!checkedDstSize.isValid()) {
        *out_outOfMemory = true;
        return false;
    }

    const size_t dstSize = checkedDstSize.value();

    UniqueBuffer dstBuffer(malloc(dstSize));
    if (!dstBuffer) {
        *out_outOfMemory = true;
        return false;
    }
    void* const dstBegin = dstBuffer.get();

    const auto srcOrigin = gl::OriginPos::TopLeft; // As spec'd for DOM sources.
    const auto dstOrigin = webgl->mPixelStore_FlipY ? gl::OriginPos::BottomLeft
                                                    : gl::OriginPos::TopLeft;
    const bool dstPremultiplied = webgl->mPixelStore_PremultiplyAlpha;

    // And go!:

    if (!ConvertImage(width, height,
                      srcBegin, srcStride, srcOrigin, srcFormat, srcPremultiplied,
                      dstBegin, dstStride, dstOrigin, dstFormat, dstPremultiplied))
    {
        MOZ_ASSERT(false, "ConvertImage failed unexpectedly.");
        NS_ERROR("ConvertImage failed unexpectedly.");
        *out_outOfMemory = true;
        return false;
    }

    *out_convertedBuffer = Move(dstBuffer);
    *out_convertedAlignment = dstAlignment;
    return true;
}


////////////////////

TexUnpackSrcSurface::TexUnpackSrcSurface(const RefPtr<gfx::DataSourceSurface>& surf)
    : TexUnpackBlob(surf->GetSize().width, surf->GetSize().height, 1, true)
    , mSurf(surf)
{ }

bool
TexUnpackSrcSurface::TexOrSubImage(bool isSubImage, WebGLTexture* tex,
                                   TexImageTarget target, GLint level,
                                   const webgl::DriverUnpackInfo* dui, GLint xOffset,
                                   GLint yOffset, GLint zOffset,
                                   GLenum* const out_glError)
{
    *out_glError = 0;

    WebGLContext* webgl = tex->mContext;

    GLenum error;
    if (UploadDataSurface(isSubImage, webgl, target, level, dui, xOffset, yOffset,
                          zOffset, mWidth, mHeight, mSurf, &error))
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

    if (!ConvertSurface(webgl, dui, &convertedBuffer, &convertedAlignment,
                        &outOfMemory))
    {
        if (outOfMemory) {
            *out_glError = LOCAL_GL_OUT_OF_MEMORY;
        }
        return false;
    }

    const GLuint pixelUnpackBuffer = 0;
    ScopedUnpackReset scopedReset(webgl, pixelUnpackBuffer);
    webgl->gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, convertedAlignment);

    GLenum error = DoTexOrSubImage(isSubImage, gl, target.get(), level, dui, Offset,
                                   yOffset, zOffset, mWidth, mHeight, mDepth,
                                   convertedBuffer.get());
    if (error) {
        *out_glError = error;
        return false;
    }

    return true;
}

} // namespace webgl
} // namespace mozilla
