/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "ElementTexSource.h"
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
// Utils

bool
GetPackedSizeForUnpack(uint32_t bytesPerPixel, uint32_t rowByteAlignment,
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

    CheckedUint32 strideBytesPerRow = CheckedUint32(bytesPerPixel) * stridePixelsPerRow;
    if (!strideBytesPerRow.isValid())
        return false;

    uint32_t remainder = strideBytesPerRow.value() % rowByteAlignment;
    if (remainder)
        strideBytesPerRow += rowByteAlignment - remainder;

    CheckedUint32 strideBytesPerImage = strideBytesPerRow * strideRowsPerImage;

    CheckedUint32 lastRowBytes = CheckedUint32(bytesPerPixel) * pixelsPerRow;
    CheckedUint32 lastImageBytes = strideBytesPerRow * (rowsPerImage - 1) + lastRowBytes;

    CheckedUint32 packedBytes = strideBytesPerImage * (images - 1) + lastImageBytes;

    if (!packedBytes.isValid())
        return false;

    *out_packedBytes = packedBytes.value();
    return true;
}


bool
WebGLContext::ValidateTexImageTarget(const char* funcName, uint8_t funcDims,
                                     GLenum texImageTarget, bool* const out_isCubeMap,
                                     TexImageTarget* const out_target,
                                     WebGLTexture** const out_texture)
{
    // Check texImageTarget
    bool isTargetValid = (funcDims == 2);
    bool isCubeMap = false;

    switch (rawTexImageTarget) {
    case LOCAL_GL_TEXTURE_2D:
        break;

    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        isCubeMap = true;
        break;

    case LOCAL_GL_TEXTURE_3D:
    case LOCAL_GL_TEXTURE_2D_ARRAY:
        isTargetValid = (funcDims == 3);
        break;

    default:
        isTargetValid = false;
        break;
    }

    if (!isTargetValid) {
        ErrorInvalidEnum("%s: Bad `target`.", funcName);
        return false;
    }
    TexImageTarget target = rawTexImageTarget;

    WebGLTexture* texture = ActiveBoundTextureForTexImageTarget(target);
    if (!texture) {
        ErrorInvalidOperation("%s: No texture is bound to the active target.", funcName);
        return false;
    }

    *out_isCubeMap = isCubeMap;
    *out_target = target;
    *out_texture = texture;
    return true;
}


static bool
ValidateTexImage(const char* funcName, WebGLTexture* texture, TexImageTarget target,
                 GLint level, WebGLContext* webgl,
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

    WebGLTexture::ImageInfo& imageInfo = texture->ImageInfoAt(target, size_t(level));

    *out_imageInfo = &imageInfo;
    return true;
}


// For *TexImage*
bool
WebGLContext::ValidateTexImageSpecification(const char* funcName, uint8_t funcDims,
                                            GLenum texImageTarget, GLint level,
                                            GLsizei width, GLsizei height, GLsizei depth,
                                            GLint border,
                                            TexImageTarget* const out_target,
                                            WebGLTexture** const out_texture,
                                            WebGLTexture::ImageInfo** const out_imageInfo)
{
    bool isCubeMap;
    TexImageTarget target;
    WebGLTexture* texture;
    if (!ValidateTexImageTarget(funcName, funcDims, texImageTarget, level, &isCubeMap,
                                &target, &texture))
    {
        return false;
    }

    if (texture->mImmutable) {
        ErrorInvalidOperation("%s: Specified texture is immutable.", funcName);
        return false;
    }

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImage(funcName, texture, target, level, this, &imageInfo))
        return false;

    // Check border
    if (border != 0) {
        ErrorInvalidValue("%s: `border` must be 0.", funcName);
        return false;
    }

    if (width < 0 || height < 0 || depth < 0) {
        /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
         *   "If wt and ht are the specified image width and height,
         *   and if either wt or ht are less than zero, then the error
         *   INVALID_VALUE is generated."
         */
        ErrorInvalidValue("%s: `width`/`height`/`depth` must be >= 0.", funcName);
        return false;
    }

    /* NOTE: GL_MAX_TEXTURE_SIZE is *not* the max allowed texture size. Rather, it is the
     * max (width/height) size guaranteed not to generate an INVALID_VALUE for too-large
     * dimensions. Sizes larger than GL_MAX_TEXTURE_SIZE *may or may not* result in an
     * INVALID_VALUE, or possibly GL_OOM.
     *
     * However, we have needed to set our maximums lower in the past to prevent resource
     * corruption. Therefore we have mImpleMaxTextureSize, which is neither necessarily
     * lower nor higher than MAX_TEXTURE_SIZE.
     */

    GLsizei maxWidthHeight;
    GLsizei maxDepth;

    switch (texImageTarget) {
    case LOCAL_GL_TEXTURE_2D:
        maxWidthHeight = mImplMaxTextureSize >> level;
        maxDepth = 1;
        break;

    case LOCAL_GL_TEXTURE_3D:
        maxWidthHeight = mImplMax3DTextureSize >> level;
        maxDepth = maxWidthHeight;
        break;

    case LOCAL_GL_TEXTURE_2D_ARRAY:
        maxWidthHeight = mImplMaxTextureSize >> level;
        maxDepth = mGLMaxArrayTextureLayers;
        break;

    default: // cube maps
        MOZ_ASSERT(isCubeMap);
        maxWidthHeight = mImplMaxCubeMapTextureSize >> level;
        maxDepth = 1;
        break;
    }

    if (width > maxWidthHeight || height > maxWidthHeight || depth > maxDepth) {
        ErrorInvalidValue("%s: Requested size is unsupported.", funcName);
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
        bool requirePOT = (!IsWebGL2() && level != 0);

        if (requirePOT) {
            if (!IsPOTAssumingNonnegative(width) || !IsPOTAssumingNonnegative(height)) {
                ErrorInvalidValue("%s: For level > 0, width and height must be powers of"
                                  " two.",
                                  funcName);
                return false;
            }
        }
    }

    *out_target = target;
    *out_texture = texture;
    *out_imageInfo = imageInfo;
    return true;
}

// For *TexSubImage*
bool
WebGLContext::ValidateTexImageSelection(const char* funcName, uint8_t funcDims,
                                        GLenum texImageTarget, GLint level, GLint xOffset,
                                        GLint yOffset, GLint zOffset, GLsizei width,
                                        GLsizei height, GLsizei depth,
                                        TexImageTarget* const out_target,
                                        WebGLTexture** const out_texture,
                                        WebGLTexture::ImageInfo** const out_imageInfo)
{
    bool isCubeMap;
    TexImageTarget target;
    WebGLTexture* texture;
    if (!ValidateTexImageTarget(funcName, funcDims, texImageTarget, &isCubeMap, &target,
                                &texture))
    {
        return false;
    }

    if (texture->mImmutable) {
        ErrorInvalidOperation("%s: Specified texture is immutable.", funcName);
        return false;
    }

    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImage(funcName, texture, target, level, this, &imageInfo))
        return false;

    if (imageInfo->IsValid()) {
        ErrorInvalidOperation("%s: The specified TexImage has not yet been specified.",
                              funcName);
        return false;
    }

    if (xOffset < 0 || yOffset < 0 || zOffset < 0) {
        ErrorInvalidValue("%s: Offsets must be >=0.", funcName);
        return false;
    }

    const auto totalX = CheckedUint32(xOffset) + width;
    const auto totalY = CheckedUint32(yOffset) + height;
    const auto totalZ = CheckedUint32(zOffset) + depth;

    if (!totalX.isValid() || totalX.value() > imageInfo->Width() ||
        !totalY.isValid() || totalY.value() > imageInfo->Height() ||
        !totalZ.isValid() || totalZ.value() > imageInfo->Depth())
    {
        ErrorInvalidValue("%s: Offset+size must be <= the size of the existing"
                          " specified image.",
                          funcName);
        return false;
    }

    *out_target = target;
    *out_texture = texture;
    *out_imageInfo = imageInfo;
    return true;
}


bool
WebGLContext::ValidateTexUnpack(const char* funcName, size_t funcDims, GLsizei width,
                                GLsizei height, GLsizei depth, GLenum unpackFormat,
                                GLenum unpackType, size_t dataSize,
                                ElementTexSource* texSource,
                                const webgl::FormatInfo** const out_format)
{
    const webgl::FormatInfo* format = GetInfoByUnpackTuple(unpackFormat, unpackType);

    if (!format || format->compression) {
        ErrorInvalidOperation("%s: Invalid format/type for unpacking.", funcName);
        return false;
    }

    if (texSource) {
        // The spec forces us to handle arbitrary format conversions for DOM elements.
        *out_format = format;
        return true;
    }

    const auto bytesPerPixel = format->bytesPerPixel;
    const auto rowByteAlignment = mPixelStore_UnpackAlignment;
    const auto maybeStridePixelsPerRow = mPixelStore_UnpackRowLength;
    const auto maybeStrideRowsPerImage = mPixelStore_UnpackImageHeight;
    const auto skipPixelsPerRow = mPixelStore_UnpackSkipPixels;
    const auto skipRowsPerImage = mPixelStore_UnpackSkipRows;
    const auto skipImages = (funcDims == 3) ? mPixelStore_UnpackSkipImages : 0;
    const auto usedPixelsPerRow = width;
    const auto usedRowsPerImage = height;
    const auto usedImages = depth;

    uint32_t bytesNeeded;
    if (!GetPackedSizeForUnpack(bytesPerPixel, rowByteAlignment,
                                maybeStridePixelsPerRow, maybeStrideRowsPerImage,
                                skipPixelsPerRow, skipRowsPerImage, skipImages,
                                usedPixelsPerRow, usedRowsPerImage, usedImages,
                                &bytesNeeded)
    {
        ErrorInvalidOperation("%s: Overflow while computing the needed buffer size.",
                              funcName);
        return false;
    }

    if (byteLength < bytesNeeded) {
        ErrorInvalidOperation("%s: Provided buffer is too small. (needs %u, has %u)",
                              funcName, packedBytes, byteLength);
        return false;
    }
    }

    *out_format = format;
    return true;
}


static bool
ValidateCompressedTexUnpack(const char* funcName, size_t funcDims, GLsizei width,
                            GLsizei height, GLsizei depth, GLenum unpackFormat,
                            size_t dataSize, WebGLContext* webgl,
                            const webgl::FormatInfo** const out_format)
{
    const webgl::FormatInfo* format = GetInfoBySizedFormat(unpackFormat);

    if (!format || !format->compression) {
        ErrorInvalidOperation("%s: Invalid format for unpacking.", funcName);
        return false;
    }
    const CompressedFormatInfo* const compression = format->compression;

    const auto bytesPerBlock = compression->bytesPerBlock;
    CheckedUint32 widthInBlocks = (width % blockWidth) ? width / blockWidth + 1
                                                       : width / blockWidth;
    CheckedUint32 heightInBlocks = (height % blockHeight) ? height / blockHeight + 1
                                                          : height / blockHeight;
    CheckedUint32 blocksPerImage = widthInBlocks * heightInBlocks;
    CheckedUint32 bytesPerImage = bytesPerBlock * blocksPerImage;
    CheckedUint32 bytesNeeded = bytesPerImage * depth;

    uint32_t bytesNeeded;
    if (!bytesNeeded.isValid())
    {
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

    *out_format = format;
    return true;
}


static size_t
PadValueToAlignment(size_t value, size_t alignment) // GetStride(7, 4)
{
    size_t remainder = value % alignment;           // 3 = 7 % 4
    if (remainder)
        value += alignment - remainder;             // 8 = 7 + (4 - 3)

    return value;
}


static bool
GetTexelFormatForSourceSurface(gfx::SourceSurface* surf,
                               WebGLTexelFormat* const out_texelFormat,
                               bool* const out_hasAlpha)
{
    gfx::SurfaceFormat surfFormat = surf->GetFormat();

    switch (surfFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
        *out_texelFormat = WebGLTexelFormat::BGRA8;
        *out_hasAlpha = true;
        return true;

    case gfx::SurfaceFormat::B8G8R8X8:
        *out_texelFormat = WebGLTexelFormat::BGRX8;
        *out_hasAlpha = false;
        return true;

    case gfx::SurfaceFormat::R8G8B8A8:
        *out_texelFormat = WebGLTexelFormat::RGBA8;
        *out_hasAlpha = true;
        return true;

    case gfx::SurfaceFormat::R8G8B8X8:
        *out_texelFormat = WebGLTexelFormat::RGBX8;
        *out_hasAlpha = false;
        return true;

    case gfx::SurfaceFormat::R5G6B5:
        *out_texelFormat = WebGLTexelFormat::RGB565;
        *out_hasAlpha = false;
        return true;

    case gfx::SurfaceFormat::A8:
        *out_texelFormat = WebGLTexelFormat::A8;
        *out_hasAlpha = true;
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
GetTexelFormatForUnpackFormatAndType(webgl::EffectiveFormat effectiveFormat,
                                     WebGLTexelFormat* const out_texelFormat)
{
    switch (unpackType) {
    case webgl::EffectiveFormat::R8:
    case webgl::EffectiveFormat::Luminance8:
        *out_texelFormat = WebGLTexelFormat::R8:
        return true;

    case webgl::EffectiveFormat::Alpha8:
        *out_texelFormat = WebGLTexelFormat::A8:
        return true;

    case webgl::EffectiveFormat::R16F:
        *out_texelFormat = WebGLTexelFormat::R16F:
        return true;

    case webgl::EffectiveFormat::R32F:
    case webgl::EffectiveFormat::DEPTH_COMPONENT32F:
        *out_texelFormat = WebGLTexelFormat::R32F:
        return true;

    case webgl::EffectiveFormat::Luminance8Alpha8:
        *out_texelFormat = WebGLTexelFormat::RA8:
        return true;

    case webgl::EffectiveFormat::RGB8:
    case webgl::EffectiveFormat::SRGB8:
        *out_texelFormat = WebGLTexelFormat::RGB8:
        return true;

    case webgl::EffectiveFormat::RGB565:
        *out_texelFormat = WebGLTexelFormat::RGB565:
        return true;

    case webgl::EffectiveFormat::RGB16F:
        *out_texelFormat = WebGLTexelFormat::RGB16F:
        return true;

    case webgl::EffectiveFormat::RGB32F:
        *out_texelFormat = WebGLTexelFormat::RGB32F:
        return true;

    case webgl::EffectiveFormat::RGBA8:
    case webgl::EffectiveFormat::SRGB8_ALPHA8:
        *out_texelFormat = WebGLTexelFormat::RGBA8:
        return true;

    case webgl::EffectiveFormat::RGB5_A1:
        *out_texelFormat = WebGLTexelFormat::RGBA5551:
        return true;

    case webgl::EffectiveFormat::RGBA4:
        *out_texelFormat = WebGLTexelFormat::RGBA4444:
        return true;

    case webgl::EffectiveFormat::RGBA16F:
        *out_texelFormat = WebGLTexelFormat::RGBA16F:
        return true;

    case webgl::EffectiveFormat::RGBA32F:
        *out_texelFormat = WebGLTexelFormat::RGBA32F:
        return true;

    case webgl::EffectiveFormat::RGBA32I:
    case webgl::EffectiveFormat::RGBA32UI:
    case webgl::EffectiveFormat::RGBA16I:
    case webgl::EffectiveFormat::RGBA16UI:
    case webgl::EffectiveFormat::RGBA8I:
    case webgl::EffectiveFormat::RGBA8UI:
    case webgl::EffectiveFormat::RGB10_A2:
    case webgl::EffectiveFormat::RGB10_A2UI:
    case webgl::EffectiveFormat::RG32I:
    case webgl::EffectiveFormat::RG32UI:
    case webgl::EffectiveFormat::RG16I:
    case webgl::EffectiveFormat::RG16UI:
    case webgl::EffectiveFormat::RG8:
    case webgl::EffectiveFormat::RG8I:
    case webgl::EffectiveFormat::RG8UI:
    case webgl::EffectiveFormat::R32I:
    case webgl::EffectiveFormat::R32UI:
    case webgl::EffectiveFormat::R16I:
    case webgl::EffectiveFormat::R16UI:
    case webgl::EffectiveFormat::R8I:
    case webgl::EffectiveFormat::R8UI:
    case webgl::EffectiveFormat::RGBA8_SNORM:
    case webgl::EffectiveFormat::RGB32I:
    case webgl::EffectiveFormat::RGB32UI:
    case webgl::EffectiveFormat::RGB16I:
    case webgl::EffectiveFormat::RGB16UI:
    case webgl::EffectiveFormat::RGB8_SNORM:
    case webgl::EffectiveFormat::RGB8I:
    case webgl::EffectiveFormat::RGB8UI:
    case webgl::EffectiveFormat::R11F_G11F_B10F:
    case webgl::EffectiveFormat::RGB9_E5:
    case webgl::EffectiveFormat::RG32F:
    case webgl::EffectiveFormat::RG16F:
    case webgl::EffectiveFormat::RG8_SNORM:
    case webgl::EffectiveFormat::R8_SNORM:
    case webgl::EffectiveFormat::DEPTH_COMPONENT24:
    case webgl::EffectiveFormat::DEPTH_COMPONENT16:
    case webgl::EffectiveFormat::DEPTH32F_STENCIL8:
    case webgl::EffectiveFormat::DEPTH24_STENCIL8:
    case webgl::EffectiveFormat::STENCIL_INDEX8:
        NS_ERROR("Unsupported EffectiveFormat dest format for DOM element upload.");
        return false;
    }
}


static bool
GuessAlignmentFromStride(size_t width, size_t stride, size_t maxAlignment,
                         size_t* const out_alignment)
{
    size_t alignmentGuess = maxAlignment;
    while (alignmentGuess) {
        size_t guessStride = PadValueToAlignment(width, alignmentGuess);
        if (guessStride == stide) {
            *out_alignment = alignmentGuess;
            return true;
        }
        alignmentGuess /= 2;
    }
    return false;
}


static void*
ConvertDataToFormat(gfx::DataSourceSurface* srcData,
                    const gfx::DataSourceSurface::ScopedMap& mappedSrcData,
                    bool srcPremultiplied, const webgl::FormatInfo* dstFormatInfo,
                    bool dstPremultiplied, bool needsYFlip,
                    UniquePtr<uint8_t>* out_scopedBuffer, size_t* const out_alignment,
                    bool* const out_outOfMemory)
{
    *out_outOfMemory = false;

    WebGLTexelFormat srcFormat;
    bool srcHasAlpha;
    if (!GetTexelFormatForSourceSurface(srcData, &srcFormat, &srcHasAlpha))
        return nullptr;

    if (!srcHasAlpha)
        needsAlphaPremult = false;

    WebGLTexelFormat dstFormat;
    if (!GetTexelFormatForUnpackFormatAndType(dstFormatInfo->effectiveFormat, &dstFormat))
        return nullptr;

    const size_t kMaxUnpackAlignment = 8;
    size_t width = srcData->GetSize().width;
    size_t srcStride = mappedSrcData.GetStride();
    uint8_t* srcBuffer = mappedSrcData.GetData();

    if (srcFormat == dstFormat &&
        srcPremultiplied == dstPremultiplied &&
        !needsYFlip)
    {
        if (GuessAlignmentForStride(width, srcStride, kMaxUnpackAlignment, out_alignment))
            return srcBuffer;
    }

    const size_t kAlignment = kMaxUnpackAlignment;

    size_t dstBytesPerPixel = dstFormat->bytesPerPixel;
    size_t dstBytesPerRow = dstBytesPerPixel * srcSurf->GetSize().width;
    size_t dstStride = PadValueToAlignment(dstBytesPerRow, kAlignment);
    size_t height = srcSurf->GetSize().height;

    // Let's be safe and just alloc stride*height bytes instead of trying to skip the tail
    // padding.
    *out_scopedBuffer = malloc(dstStride * height);
    uint8_t* dstBuffer = out_scopedBuffer->get();

    if (!dstBuffer) {
        *out_outOfMemory = true;
        return nullptr;
    }

    bool yFlip = mPixelStoreFlipY;
    if (!ConvertImage(width, height, yFlip, srcBuffer, srcStride, srcFormat,
                      srcPremultiplied, dstBuffer, dstStride, dstFormat,
                      dstPremultiplied))
    {
        MOZ_ASSERT(false, "ConvertImage failed unexpectedly.");
        NS_ERROR("ConvertImage failed unexpectedly.");
        return nullptr;
    }

    *out_alignment = kAlignment;
    return dstBuffer;
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

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// Actual (mostly generic) function implementations


/* TexImage2D(target, level, internalFormat, width, height, border, format, type, data)
 * TexImage3D(target, level, internalFormat, width, height, depth, border, format, type,
 *            data)
 * TexSubImage2D(target, level, xOffset, yOffset, width, height, format, type, data)
 * TexSubImage3D(target, level, xOffset, yOffset, zOffset, width, height, depth, format,
 *               type, data)
 *
 * TexStorage2D(target, levels, internalFormat, width, height)
 * TexStorage3D(target, levels, intenralFormat, width, height, depth)
 * TexStorage2DMultisample(target, samples, sizedInternalFormat, width, height,
 *                         fixedSampleLocations)
 *
 * CompressedTexImage2D(target, level, internalFormat, width, height, border, imageSize,
 *                      data)
 * CompressedTexImage3D(target, level, internalFormat, width, height, depth, border,
 *                      imageSize, data)
 * CompressedTexSubImage2D(target, level, xOffset, yOffset, width, height, format,
 *                         imageSize, data)
 * CompressedTexSubImage3D(target, level, xOffset, yOffset, zOffset, width, height, depth,
 *                         format, imageSize, data)
 *
 * CopyTexImage2D(target, level, internalFormat, x, y, width, height, border)
 * CopyTexImage3D - "Because the framebuffer is inhererntly two-dimensional, there is no
 *                   CopyTexImage3D command."
 * CopyTexSubImage2D(target, level, xOffset, yOffset, x, y, width, height)
 * CopyTexSubImage3D(target, level, xOffset, yOffset, zOffset, x, y, width, height)
 */


void
WebGLContext::TexImage_base(const char* funcName, uint8_t funcDims, GLenum texImageTarget,
                            GLint level, GLenum internalFormat, GLsizei width,
                            GLsizei height, GLsizei depth, GLint border,
                            GLenum unpackFormat, GLenum unpackType, void* data,
                            size_t dataSize, ElementTexSource* texSource)
{
    ////////////////////////////////////
    // Get dest info

    TexImageTarget target;
    WebGLTexture* texture;
    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSpecification(funcName, funcDims, texImageTarget, level, width,
                                       height, depth, border, &target, &texture,
                                       &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(texture);
    MOZ_ASSERT(imageInfo);

    ////////////////////////////////////
    // Get source info

    const webgl::FormatInfo* srcFormat;
    if (!ValidateTexUnpack(funcName, funcDims, width, height, depth, unpackFormat,
                           unpackType, dataSize, texSource, &srcFormat)
    {
        return;
    }
    MOZ_ASSERT(!srcFormat->compression);

    ////////////////////////////////////
    // Check that source and dest info are compatible

    const webgl::FormatInfo* dstFormat = GetInfoBySizedFormat(internalFormat);
    if (!dstFormat && internalFormat == unpackFormat)
        dstFormat = srcFormat;

    if (!dstFormat) {
        ErrorInvalidEnum("%s: Unrecognized internalformat/format/type:"
                         " 0x%04x/0x%04x/0x%04x",
                         funcName, internalFormat, unpackFormat, unpackType);
        return;
    }

    const webgl::FormatUsageInfo* dstFormatUsage = mFormatUsage->GetInfo(dstFormat);
    if (!dstFormatUsage || !dstFormatUsage->asTexture) {
        ErrorInvalidEnum("%s: Invalid internalformat: 0x%04x", funcName, internalFormat);
        return;
    }

    if (dstFormat->compression) {
        ErrorInvalidEnum("%s: Specified format must not be a compressed format.",
                         funcName);
        return;
    }

    if (!dstUsage->CanUnpackWith(unpackFormat, unpackType)) {
        ErrorInvalidOperation("%s: Invalid unpack format/type for internalFormat %s.",
                              funcName, usage->formatInfo->name);
        return;
    }

    ////////////////////////////////////
    // Do the thing!

    bool didUpload = false;

    if (texSource) {
        // If the unpack format/type can't be replicated during Blit(), we have to bail
        // and do things in software...
        bool canBlit = (!mPixelStoreFlipY &&
                        mPixelStorePrmultiplyAlpha &&
                        level == 0);
        switch (srcFormat.effectiveFormat) {
        case webgl::EffectiveFormat::RGB8:
        case webgl::EffectiveFormat::RGBA8:
            break;

        default:
            canBlit = false;
            break;
        }

        if (canBlit) {
            const gl::OriginPos dstOrigin = mPixelStoreFlipY ? gl::OriginPos::BottomLeft
                                                             : gl::OriginPos::TopLeft;
            if (!texSource->BlitToTexture(gl, texture->mGLName, GLenum(target),
                                          dstOrigin))
            {
                didUpload = true;
            }
        }
    }

    RefPtr<gfx::DataSourceSurface> srcData;
    UniquePtr<gfx::DataSourceSurface::ScopedMap> scopedMap;
    UniquePtr<uint8_t> scopedBuffer;
    size_t unpackAlignment = mPixelStorageUnpackAlignment;
    if (!didUpload && texSource) {
        // Handle spec-mandated arbitrary conversions! :(

        RefPtr<gfx::DataSourceSurface> srcData = texSource->GetData();
        scopedMap = new gfx::DataSourceSurface::ScopedMap(srcData,
                                                          gfx::DataSourceSurface::READ);
        if (!scopedMap->IsMapped())
            return;

        bool srcPremultiplied = true;
        bool dstPremultiplied = mPixelStorePremultiplyAlpha ? true : false;
        bool needsYFlip = mPixelStoreFlipY;
        bool outOfMemory;

        MOZ_ASSERT(!data);
        data = ConvertDataToFormat(srcData, *scopedMap, srcPremultiplied, srcFormat,
                                   dstPremultiplied, needsYFlip, &scopedBuffer,
                                   &unpackAlignment, &outOfMemory);
        if (!data) {
            if (outOfMemory) {
                ErrorOutOfMemory("%s: Ran out of memory while doing format conversions."
                                 funcName);
                return;
            }
        }
    }

    MOZ_ASSERT(data);

    // Convert to format and type required by the driver.
    GLenum driverInternalFormat;
    GLenum driverUnpackFormat;
    GLenum driverUnpackType;
    GetDriverFormats(internalFormat, unpackFormat, unpackType, &driverInternalFormat,
                     &driverUnpackFormat, &driverUnpackType);

    if (driverInternalFormat != internalFormat)
        SetLegacyTextureSwizzle(gl, texImageTarget, internalformat);


    gl::ScopedUnpackAlignment scopedUnpack(gl, unpackAlignment);

    gl::GLContext::LocalErrorScope errorScope(*gl);
    switch (funcDims) {
    case 2:
        gl->fTexImage2D(GLenum(target), level, driverInternalFormat, width, height,
                        border, driverUnpackFormat, driverUnpackType, data);
        break;

    case 3:
        gl->fTexImage3D(GLenum(target), level, driverInternalFormat, width, height, depth,
                        border, driverUnpackFormat, driverUnpackType, data);
        break;

    default:
        MOZ_CRASH('bad funcDims');
    }

    GLenum error = errorScope.GetError();
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
    // Update our specification data.

    WebGLImageDataStatus status = WebGLImageDataStatus::InitializedImageData;
    *imageInfo = WebGLTexture::ImageInfo(width, height, depth, dstFormatUsage, status);
    return;
}


void
WebGLContext::TexSubImage_base(const char* funcName, uint8_t funcDims,
                               GLenum texImageTarget, GLint level, GLint xOffset,
                               GLint yOffset, GLint zOffset, GLsizei width,
                               GLsizei height, GLsizei depth, GLenum unpackFormat,
                               GLenum unpackType, void* data, size_t dataSize,
                               ElementTexSource* texSource)
{
    ////////////////////////////////////
    // Get dest info
    TexImageTarget target;
    WebGLTexture* texture;
    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSelection(funcName, funcDims, texImageTarget, level, xOffset,
                                   yOffset, zOffset, width, height, depth, &target,
                                   &texture, &imageInfo))
    {
        return false;
    }
    MOZ_ASSERT(texture);
    MOZ_ASSERT(imageInfo);

    const webgl::FormatUsageInfo* dstFormatUsage = imageInfo->Format();
    MOZ_ASSERT(dstFormatUsage && dstFormatUsage->asTexture);

    const webgl::FormatInfo* dstFormat = dstFormatUsage->formatInfo;
    MOZ_ASSERT(dstFormat);

    ////////////////////////////////////
    // Get source info

    const webgl::FormatInfo* srcFormat;
    if (!ValidateTexUnpack(funcName, funcDims, width, height, depth, unpackFormat,
                           unpackType, dataSize, texSource, &srcFormat)
    {
        return;
    }
    MOZ_ASSERT(!srcFormat->compression);

    ////////////////////////////////////
    // Check that source and dest info are compatible

    if (dstFormat->compression) {
        ErrorInvalidEnum("%s: Specified TexImage must not be compressed.", funcName);
        return false;
    }

    if (!dstUsage->CanUnpackWith(unpackFormat, unpackType)) {
        ErrorInvalidOperation("%s: Invalid unpack format/type for internalFormat %s.",
                              funcName, usage->formatInfo->name);
        return false;
    }

    ////////////////////////////////////
    // Do the thing!

    texture->PrepareForSubImage(gl, target, level, xOffset, yOffset, zOffset, width,
                                height, depth);

    RefPtr<gfx::DataSourceSurface> srcData;
    UniquePtr<gfx::DataSourceSurface::ScopedMap> scopedMap;
    UniquePtr<uint8_t> scopedBuffer;
    size_t unpackAlignment = mPixelStorageUnpackAlignment;
    if (texSource) {
        // Handle spec-mandated arbitrary conversions! :(

        RefPtr<gfx::DataSourceSurface> srcData = texSource->GetData();
        scopedMap = new gfx::DataSourceSurface::ScopedMap(srcData,
                                                          gfx::DataSourceSurface::READ);
        if (!scopedMap->IsMapped())
            return false;

        bool srcPremultiplied = true;
        bool dstPremultiplied = mPixelStorePremultiplyAlpha ? true : false;
        bool needsYFlip = mPixelStoreFlipY;
        bool outOfMemory;

        MOZ_ASSERT(!data);
        data = ConvertDataToFormat(srcData, *scopedMap, srcPremultiplied, srcFormat,
                                   dstPremultiplied, needsYFlip, &scopedBuffer,
                                   &unpackAlignment, &outOfMemory);
        if (!data) {
            if (outOfMemory) {
                ErrorOutOfMemory("%s: Ran out of memory while doing format conversions."
                                 funcName);
                return false;
            }
        }
    }

    MOZ_ASSERT(data);

    // Convert to format and type required by the driver.
    GLenum internalFormat = 0; // unused
    GLenum driverInternalFormat; // unused
    GLenum driverUnpackFormat;
    GLenum driverUnpackType;
    GetDriverFormats(internalFormat, unpackFormat, unpackType, &driverInternalFormat,
                     &driverUnpackFormat, &driverUnpackType);


    ScopedUnpackAlignment scopedUnpack(gl, unpackAlignment);

    switch (funcDims) {
    case 2:
        gl->fTexSubImage2D(GLenum(target), level, xOffset, yOffset, width, height,
                           driverUnpackFormat, driverUnpackType, data);
        break;

    case 3:
        gl->fTexSubImage3D(GLenum(target), level, xOffset, yOffset, zOffset, width,
                           height, depth, driverUnpackFormat, driverUnpackType, data);
        break;

    default:
        MOZ_CRASH('bad funcDims');
    }
}


static void
SetTexStorageForTexture(WebGLTexture* texture, TexImageTarget target, GLsizei levels,
                        GLsizei width, GLsizei height, GLsizei depth,
                        webgl::FormatUsageInfo* formatUsage, WebGLImageDataStatus status)
{
    for (GLsizei level = 0; level < levels; level++) {
        WebGLTexture::ImageInfo& imageInfo = texture->ImageInfoAt(target, level);
        imageInfo = WebGLTexture::ImageInfo(width, height, depth, formatUsage, status);

        if (target != LOCAL_GL_TEXTURE_2D_ARRAY) {
            width >>= 1;
            height >>= 1;
            height >>= 1;
        }
    }
}


void
WebGLContext::TexStorage_base(const char* funcName, uint8_t funcDims,
                              GLenum texTarget, GLsizei levels,
                              GLenum internalFormat, GLsizei width, GLsizei height,
                              GLsizei depth)
{
    // Check levels
    if (levels < 1) {
        ErrorInvalidValue("%s: `levels` must be >= 1.", funcName);
        return false;
    }

    if (levels > 32) {
        // Right-shift is only defined for bits-1, so 31 for GLsizei.
        ErrorInvalidValue("%s: `level` is too large.", funcName);
        return false;
    }

    const bool isCubeMap = (texTarget == LOCAL_GL_TEXTURE_CUBE_MAP);

    const GLenum testTexImageTarget = isCubeMap ? LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X
                                                : texTarget;
    const GLint testLevel = 0;
                                                                       :
    TexImageTarget target;
    WebGLTexture* texture;
    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSpecification(funcName, funcDims, testTexImageTarget, testLevel,
                                       width, height, depth, border, &target, &texture,
                                       &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(texture);
    MOZ_ASSERT(imageInfo);

    const webgl::FormatInfo* dstFormat = GetInfoBySizedFormat(internalFormat);
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

    if (dstFormat->compression) {
        ErrorInvalidEnum("%s: Specified format must not be a compressed format.",
                         funcName);
        return;
    }

    ////////////////////////////////////
    // Do the thing!

    // Convert to format and type required by the driver.
    GLenum unpackFormat = 0; // unused
    GLenum unpackType = 0; // unused
    GLenum driverInternalFormat;
    GLenum driverUnpackFormat = 0; // unused
    GLenum driverUnpackType = 0; // unused
    GetDriverFormats(internalFormat, unpackFormat, unpackType, &driverInternalFormat,
                     &driverUnpackFormat, &driverUnpackType);

    if (driverInternalFormat != internalFormat)
        SetLegacyTextureSwizzle(gl, texImageTarget, internalformat);


    ScopedUnpackAlignment scopedUnpack(gl, unpackAlignment);

    GLContext::LocalErrorScope errorScope(*gl);
    switch (funcDims) {
    case 2:
        gl->fTexStorage2D(GLenum(target), levels, driverInternalFormat, width, height);
        break;

    case 3:
        gl->fTexStorage3D(GLenum(target), levels, driverInternalFormat, width, height,
                          depth);
        break;

    default:
        MOZ_CRASH('bad funcDims');
    }

    GLenum error = errorScope.GetError();
    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        ErrorOutOfMemory("%s: Ran out of memory during texture allocation.", funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        GenerateWarning("%s: Unexpected error during texture allocation. Context"
                        " lost.",
                        funcName);
        ForceLoseContext();
        return;
    }

    ////////////////////////////////////
    // Update our specification data.

    texture->ClearImageInfos();

    WebGLImageDataStatus status = WebGLImageDataStatus::UninitializedImageData;
    if (isCubeMap) {
        SetTexStorageForTexture(texture, LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X, levels,
                                width, height, depth, dstFormatUsage, status);

        SetTexStorageForTexture(texture, LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X, levels,
                                width, height, depth, dstFormatUsage, status);

        SetTexStorageForTexture(texture, LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y, levels,
                                width, height, depth, dstFormatUsage, status);

        SetTexStorageForTexture(texture, LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, levels,
                                width, height, depth, dstFormatUsage, status);

        SetTexStorageForTexture(texture, LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z, levels,
                                width, height, depth, dstFormatUsage, status);

        SetTexStorageForTexture(texture, LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, levels,
                                width, height, depth, dstFormatUsage, status);
    } else {
        SetTexStorageForTexture(texture, target, levels, width, height, depth,
                                dstFormatUsage, status);
    }

    texture->mImmutable = true;

    return;
}


void
WebGLContext::TexStorage2DMultisample_base(GLenum texTarget, GLsizei samples,
                                           GLenum sizedInternalFormat, GLsizei width,
                                           GLsizei height,
                                           realGLboolean fixedSampleLocations)
{
    const char funcName[] = "TexStorage2DMultisample";
    const uint8_t funcDims = 2;

    // No TexStorageMultisample_base, as there is no TexStorage3DMultisample.

    // Check samples
    if (samples < 0) {
        ErrorInvalidValue("%s: `samples` must be >= 0.", funcName);
        return false;
    }

    if (samples > mGLMaxSamples) {
        ErrorInvalidValue("%s: `samples` must be <= MAX_SAMPLES.", funcName);
        return false;
    }

    if (texTarget != LOCAL_GL_TEXTURE_2D_MULTISAMPLE) {
        ErrorInvalidEnum("%s: `target` must be TEXTURE_2D_MULTISAMPLE.", funcName);
        return;
    }

    const GLenum testTexImageTarget = LOCAL_GL_TEXTURE_2D;
    const GLint testLevel = 0;
                                                                       :
    TexImageTarget target;
    WebGLTexture* texture;
    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSpecification(funcName, funcDims, testTexImageTarget, testLevel,
                                       width, height, depth, border, &target, &texture,
                                       &imageInfo))
    {
        return;
    }
    target = LOCAL_GL_TEXTURE_2D_MULTISAMPLE;
    MOZ_ASSERT(texture);
    MOZ_ASSERT(imageInfo);

    const webgl::FormatInfo* dstFormat = GetInfoBySizedFormat(internalFormat);
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

    if (dstFormat->compression) {
        ErrorInvalidEnum("%s: Specified format must not be a compressed format.",
                         funcName);
        return;
    }

    ////////////////////////////////////
    // Do the thing!

    // Convert to format and type required by the driver.
    GLenum unpackFormat = 0; // unused
    GLenum unpackType = 0; // unused
    GLenum driverInternalFormat;
    GLenum driverUnpackFormat = 0; // unused
    GLenum driverUnpackType = 0; // unused
    GetDriverFormats(internalFormat, unpackFormat, unpackType, &driverInternalFormat,
                     &driverUnpackFormat, &driverUnpackType);

    if (driverInternalFormat != internalFormat)
        SetLegacyTextureSwizzle(gl, texImageTarget, internalformat);


    ScopedUnpackAlignment scopedUnpack(gl, unpackAlignment);

    GLContext::LocalErrorScope errorScope(*gl);

    gl->fTexStorage2DMultisample(GLenum(target), samples, driverInternalFormat, width,
                                 height, fixedSampleLocations);

    GLenum error = errorScope.GetError();
    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        ErrorOutOfMemory("%s: Ran out of memory during texture allocation.", funcName);
        return;
    }
    if (error) {
        MOZ_RELEASE_ASSERT(false, "We should have caught all other errors.");
        GenerateWarning("%s: Unexpected error during texture allocation. Context"
                        " lost.",
                        funcName);
        ForceLoseContext();
        return;
    }

    ////////////////////////////////////
    // Update our specification data.

    texture->ClearImageInfos();

    WebGLImageDataStatus status = WebGLImageDataStatus::UninitializedImageData;
    SetTexStorageForTexture(texture, target, levels, width, height, depth, dstFormatUsage,
                            status);

    texture->mImmutable = true;

    return;
}


void
WebGLContext::CompressedTexImage_base(const char* funcName, uint8_t funcDims,
                                      GLenum texImageTarget, GLint level,
                                      GLenum internalFormat, GLsizei width,
                                      GLsizei height, GLsizei depth, GLint border,
                                      void* data, size_t dataSize)
{
    ////////////////////////////////////
    // Get dest info

    TexImageTarget target;
    WebGLTexture* texture;
    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSpecification(funcName, funcDims, texImageTarget, level, width,
                                       height, depth, border, &target, &texture,
                                       &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(texture);
    MOZ_ASSERT(imageInfo);

    ////////////////////////////////////
    // Get source info

    const webgl::FormatInfo* format;
    if (!ValidateCompressedTexUnpack(funcName, funcDims, width, height, depth,
                                     internalFormat, dataSize, this, &format))
    {
        return;
    }
    MOZ_ASSERT(format->compression);

    ////////////////////////////////////
    // Check that source and dest info are compatible

    const webgl::FormatUsageInfo* formatUsage = mFormatUsage->GetInfo(format);
    if (!formatUsage || !formatUsage->asTexture) {
        ErrorInvalidEnum("%s: Invalid internalformat: 0x%04x", funcName, internalFormat);
        return;
    }

    ////////////////////////////////////
    // Do the thing!

    ScopedUnpackAlignment scopedUnpack(gl, unpackAlignment);

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

    GLenum error = errorScope.GetError();
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

    WebGLImageDataStatus status = WebGLImageDataStatus::InitializedImageData;
    *imageInfo = WebGLTexture::ImageInfo(width, height, depth, formatUsage, status);
    return;
}

static bool
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
WebGLContext::CompressedTexSubImage_base(const char* funcName, uint8_t funcDims,
                                         GLenum texImageTarget, GLint level,
                                         GLint xOffset, GLint yOffset, GLint zOffset,
                                         GLsizei width, GLsizei height, GLsizei depth,
                                         GLenum unpackFormat, void* data, size_t dataSize)
{
    ////////////////////////////////////
    // Get dest info

    TexImageTarget target;
    WebGLTexture* texture;
    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSelection(funcName, funcDims, texImageTarget, level, xOffset,
                                   yOffset, zOffset, width, height, depth, &target,
                                   &texture, &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(texture);
    MOZ_ASSERT(imageInfo);

    const webgl::FormatUsageInfo* dstFormatUsage = imageInfo->Format();
    MOZ_ASSERT(dstFormatUsage);
    const webgl::FormatInfo* dstFormat = dstFormatUsage->formatInfo;
    MOZ_ASSERT(dstFormat);

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
    // Get source info

    const webgl::FormatInfo* srcFormat;
    if (!ValidateCompressedTexUnpack(funcName, funcDims, width, height, depth,
                                     unpackFormat, dataSize, this, &srcFormat))
    {
        return;
    }

    ////////////////////////////////////
    // Check that source and dest info are compatible

    if (srcFormat != dstFormat) {
        ErrorInvalidValue("%s: `format` must match format of specified texture image.",
                          funcName);
        return;
    }

    ////////////////////////////////////
    // Do the thing!

    texture->PrepareForSubImage(gl, target, level, xOffset, yOffset, zOffset, width,
                                height, depth);


    ScopedUnpackAlignment scopedUnpack(gl, unpackAlignment);

    GLContext::LocalErrorScope errorScope(*gl);
    switch (funcDims) {
    case 2:
        gl->fCompressedTexSubImage2D(GLenum(target), level, xOffset, yOffset, width,
                                     height, unpackFormat, dataSize, data);
        break;

    case 3:
        gl->fCompressedTexSubImage3D(GLenum(target), level, xOffset, yOffset, zOffset,
                                     width, height, depth, unpackFormat, dataSize, data);
        break;

    default:
        MOZ_CRASH('bad funcDims');
    }

    GLenum error = errorScope.GetError();
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
}


void
WebGLContext::CopyTexImage2D_base(GLenum imageTarget, GLint level, GLenum internalFormat,
                                  GLint x, GLint y, GLsizei width, GLsizei height,
                                  GLint border)
{
    const char funcName[] = "CopyTexImage2D";
    const uint8_t funcDims = 2;

    const GLsizei depth = 1;

    // No CopyTexImage_base, as there is no CopyTexImage3D.

    ////////////////////////////////////
    // Get dest info

    TexImageTarget target;
    WebGLTexture* texture;
    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSpecification(funcName, funcDims, texImageTarget, level, width,
                                       height, depth, border, &target, &texture,
                                       &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(texture);
    MOZ_ASSERT(imageInfo);

    const webgl::FormatInfo* dstFormat = GetInfoBySizedFormat(internalFormat);
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

    MOZ_ASSERT(!srcFormat->compression);
    if (dstFormat->compression) {
        ErrorInvalidEnum("%s: Specified format must not be a compressed format.",
                         funcName);
        return;
    }

    if (dstFormat->effectiveFormat == webgl::EffectiveFormat::RGB9_E5) {
        ErrorInvalidOperation("%s: RGB9_E5 is invalid for `internalformat` in"
                              " CopyTexImage. (GLES 3.0.4 p145)",
                              funcName);
        return;
    }

    if (!DoChannelsMatchForCopyTexImage(srcFormat, dstFormat)) {
        ErrorInvalidOperation("%s: Destination channels must be compatible with"
                              " source channels. (GLES 3.0.4 p140 Table 3.16)",
                              funcName);
        return;
    }

    ////////////////////////////////////
    // Do the thing!

    // Convert to format and type required by the driver.
    GLenum unpackFormat = 0; // unused
    GLenum unpackType = 0; // unused
    GLenum driverInternalFormat;
    GLenum driverUnpackFormat = 0; // unused
    GLenum driverUnpackType = 0; // unused
    GetDriverFormats(internalFormat, unpackFormat, unpackType, &driverInternalFormat,
                     &driverUnpackFormat, &driverUnpackType);

    if (driverInternalFormat != internalFormat)
        SetLegacyTextureSwizzle(gl, texImageTarget, internalformat);


    GLContext::LocalErrorScope errorScope(*gl);

    gl->fCopyTexImage2D(GLenum(target), level, driverInternalFormat, x, y, width, height,
                        border);

    GLenum error = errorScope.GetError();
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

    WebGLImageDataStatus status = WebGLImageDataStatus::InitializedImageData;
    *imageInfo = WebGLTexture::ImageInfo(width, height, depth, dstFormatUsage, status);
}

void
WebGLContext::CopyTexSubImage_base(const char* funcName, uint8_t funcDims,
                                   GLenum texImageTarget, GLint level, GLint xOffset,
                                   GLint yOffset, GLint zOffset, GLint x, GLint y,
                                   GLsizei width, GLsizei height, GLsizei depth)
{
    const GLsizei depth = 1;

    ////////////////////////////////////
    // Get dest info

    TexImageTarget target;
    WebGLTexture* texture;
    WebGLTexture::ImageInfo* imageInfo;
    if (!ValidateTexImageSelection(funcName, funcDims, texImageTarget, level, xOffset,
                                   yOffset, zOffset, width, height, depth, &target,
                                   &texture, &imageInfo))
    {
        return;
    }
    MOZ_ASSERT(texture);
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

    MOZ_ASSERT(!srcFormat->compression);
    if (dstFormat->compression) {
        ErrorInvalidEnum("%s: Specified destination must not have a compressed format.",
                         funcName);
        return;
    }

    if (dstFormat->effectiveFormat == webgl::EffectiveFormat::RGB9_E5) {
        ErrorInvalidOperation("%s: RGB9_E5 is an invalid destination for CopyTexImage."
                              " (GLES 3.0.4 p145)",
                              funcName);
        return;
    }

    if (!DoChannelsMatchForCopyTexImage(srcFormat, dstFormat)) {
        ErrorInvalidOperation("%s: Destination channels must be compatible with"
                              " source channels. (GLES 3.0.4 p140 Table 3.16)",
                              funcName);
        return;
    }

    ////////////////////////////////////
    // Do the thing!

    texture->PrepareForSubImage(gl, target, level, xOffset, yOffset, zOffset, width,
                                height, depth);

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
}

//////////////////////////////////////////////////////////////////////////////////////////
// Specific->Generic redirection functions.

void
WebGLContext::TexImage2D_base(GLenum texImageTarget, GLint level, GLenum internalFormat,
                              GLsizei width, GLsizei height, GLint border,
                              GLenum unpackFormat, GLenum unpackType, const void* data,
                              size_t dataSize, ElementTexSource* texSource)
{
    const char funcName[] = "TexImage2D";
    const uint8_t funcDims = 2;

    const GLsizei depth = 1;

    TexImage_base(funcName, funcDims, texImageTarget, width, height, depth, border,
                  unpackFormat, unpackType, data, dataSize, texSource);
}

void
WebGLContext::TexImage3D_base(GLenum texImagetTarget, GLint level, GLenum internalFormat,
                              GLsizei width, GLsizei height, GLsizei depth,
                              GLint border, GLenum unpackFormat, GLenum unpackType,
                              const void* data, size_t dataSize)
{
    const char funcName[] = "TexImage3D";
    const uint8_t funcDims = 3;

    ElementTexSource* const texSource = nullptr;

    TexImage_base(funcName, funcDims, texImageTarget, width, height, depth, border,
                  unpackFormat, unpackType, data, dataSize, texSource);
}

//////////

void
WebGLContext::TexSubImage2D_base(GLenum texImageTarget, GLint level, GLint xOffset,
                                 GLint yOffset, GLsizei width, GLsizei height,
                                 GLenum unpackFormat, GLenum unpackType, const void* data,
                                 size_t dataSize, ElementTexSource* texSource))
{
    const char funcName[] = "TexSubImage2D";
    const uint8_t funcDims = 2;

    const GLint zOffset = 0;
    const GLsizei depth = 1;

    TexSubImage_base(funcName, funcDims, texImageTarget, xOffset, yOffset, zOffset, width,
                     height, depth, border, unpackFormat, unpackType, data, dataSize,
                     texSource);

}

void
WebGLContext::TexSubImage3D_base(GLenum texImagetTarget, GLint level, GLint xOffset,
                                 GLint yOffset, GLint zOffset, GLsizei width,
                                 GLsizei height, GLsizei depth, GLenum unpackFormat,
                                 GLenum unpackType, const void* data, size_t dataSize,
                                 ElementTexSource* texSource)
{
    const char funcName[] = "TexSubImage3D";
    const uint8_t funcDims = 3;

    TexSubImage_base(funcName, funcDims, texImageTarget, xOffset, yOffset, zOffset, width,
                     height, depth, border, unpackFormat, unpackType, data, dataSize,
                     texSource);
}

//////////

void
WebGLContext::TexStorage2D_base(GLenum texTarget, GLsizei levels, GLenum internalFormat,
                                GLsizei width, GLsizei height)
{
    const char funcName[] = "TexStorage2D";
    const uint8_t funcDims = 2;

    const GLsizei depth = 1;

    TexStorage_base(funcName, funcDims, texTarget, levels, internalFormat, width, height,
                    depth);
}
void
WebGLContext::TexStorage3D_base(GLenum texTarget, GLsizei levels, GLenum internalFormat,
                                GLsizei width, GLsizei height, GLsizei depth)
{
    const char funcName[] = "TexStorage3D";
    const uint8_t funcDims = 3;

    TexStorage_base(funcName, funcDims, texTarget, levels, internalFormat, width, height,
                    depth);
}

//////////

void
WebGLContext::CompressedTexImage2D_base(GLenum texImagetTarget, GLint level,
                                        GLenum internalFormat, GLsizei width,
                                        GLsizei height, GLint border, const void* data,
                                        size_t dataSize)
{
    const char funcName[] = "CompressedTexImage2D";
    const uint8_t funcDims = 2;

    const GLsizei depth = 1;

    CompressedTexImage_base(funcName, funcDims, texImageTarget, level, internalFormat,
                            width, height, depth, border, data, dataSize);
}

void
WebGLContext::CompressedTexImage3D_base(GLenum texImagetTarget, GLint level,
                                        GLenum internalFormat, GLsizei width,
                                        GLsizei height, GLsizei depth, GLint border,
                                        const void* data, size_t dataSize)
{
    const char funcName[] = "CompressedTexImage3D";
    const uint8_t funcDims = 3;

    CompressedTexImage_base(funcName, funcDims, texImageTarget, level, internalFormat,
                            width, height, depth, border, data, dataSize);
}

//////////

void
WebGLContext::CompressedTexSubImage2D_base(GLenum texImagetTarget, GLint level,
                                           GLint xOffset, GLint yOffset, GLsizei width,
                                           GLsizei height, GLenum unpackFormat,
                                           const void* data, size_t dataSize)
{
    const char funcName[] = "CompressedTexSubImage2D";
    const uint8_t funcDims = 2;

    const GLint zOffset = 0;
    const GLsizei depth = 1;

    CompressedTexSubImage_base(funcName, funcDims, texImageTarget, level, xOffset,
                               yOffset, zOffset, width, height, depth, unpackFormat, data,
                               dataSize);
}
void
WebGLContext::CompressedTexSubImage3D_base(GLenum texImagetTarget, GLint level,
                                           GLint xOffset, GLint yOffset, GLint zOffset,
                                           GLsizei width, GLsizei height, GLsizei depth,
                                           GLenum unpackFormat, const void* data,
                                           size_t dataSize)
{
    const char funcName[] = "CompressedTexSubImage3D";
    const uint8_t funcDims = 3;

    CompressedTexSubImage_base(funcName, funcDims, texImageTarget, level, xOffset,
                               yOffset, zOffset, width, height, depth, unpackFormat, data,
                               dataSize);
}

////////////////////

void
WebGLContext::CopyTexSubImage2D_base(GLenum texImagetTarget, GLint level, GLint xOffset,
                                     GLint yOffset, GLint x, GLint y, GLsizei width,
                                     GLsizei height)
{
    const char funcName[] = "CopyTexSubImage2D";
    const uint8_t funcDims = 2;

    const GLint zOffset = 0;

    CopyTexSubImage_base(funcName, funcDims, texImageTarget, level, xOffset, yOffset,
                         zOffset, x, y, width, height);
}

void
WebGLContext::CopyTexSubImage3D_base(GLenum texImagetTarget, GLint level, GLint xOffset,
                                     GLint yOffset, GLint zOffset, GLint x, GLint y,
                                     GLsizei width, GLsizei height)
{
    const char funcName[] = "CopyTexSubImage3D";
    const uint8_t funcDims = 3;

    CopyTexSubImage_base(funcName, funcDims, texImageTarget, level, xOffset, yOffset,
                         zOffset, x, y, width, height);
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// WebGL entrypoints

bool
DoesUnpackTypeMatchArrayType(GLenum unpackType, js::Scalar::Type arrayType)
{
      js::Scalar::Type requiredArrayType;

      switch (type) {
      case LOCAL_GL_UNSIGNED_BYTE:
          requiredArrayType = js::Scalar::Uint8;
          break;

      case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
      case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
      case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
          requiredArrayType = js::Scalar::Uint16;
          break;

      case LOCAL_GL_FLOAT:
          requiredArrayType = js::Scalar::Float32;
          break;

      case LOCAL_GL_HALF_FLOAT:
      case LOCAL_GL_HALF_FLOAT_OES:
          requiredArrayType = js::Scalar::Uint16;
          break;

      default:
          return false;
      }

      return arrayType == requiredArrayType;
}

////////////////////////////////////////

void
WebGLContext::TexImage2D(GLenum texImageTarget, GLint level, GLenum internalFormat,
                         GLsizei width, GLsizei height, GLint border, GLenum unpackFormat,
                         GLenum unpackType, const Nullable<ArrayBufferView>& maybeView,
                         ErrorResult& out_rv)
{
    if (IsContextLost())
        return;

    const void* data;
    uint32_t dataSize;

    if (maybeView.IsNull()) {
        data = nullptr;
        dataSize = 0;
    } else {
        const ArrayBufferView& view = maybeView.Value();
        view.ComputeLengthAndData();

        data = view.Data();
        dataSize = view.Length();

        const js::Scalar::Type arrayType = JS_GetArrayBufferViewType(view.Obj());

        if (!DoesUnpackTypeMatchArrayType(unpackType, arrayType)) {
            ErrorInvalidEnum("texImage2D: `type` must be compatible with TypedArray"
                             " type.",
                             funcName);
            return;
        }
    }

    TexImage2D_base(texImageTarget, level, internalFormat, width, height, border,
                    unpackFormat, unpackType,

                    data, dataSize, nullptr);
}

void
WebGLContext::TexImage2D(GLenum texImageTarget, GLint level, GLenum internalFormat,
                         GLenum unpackFormat, GLenum unpackType, ImageData* pixels,
                         ErrorResult& out_rv)
{
    if (IsContextLost())
        return;

    if (!pixels) {
        ErrorInvalidValue("texImage2D: Null `pixels`.");
        return
    }

    Uint8ClampedArray arr;
    MOZ_ALWAYS_TRUE( arr.Init(imageData->GetDataObject()) );
    arr.ComputeLengthAndData();

    const GLsizei width = imageData->Width();
    const GLsizei height = imageData->Height();
    const GLint border = 0;
    const void* const data = arr.Data();
    const uint32_t dataSize = arr.Length();

    TexImage2D_base(texImageTarget, level, internalFormat, width, height, border,
                    unpackFormat, unpackType,

                    data, dataSize, nullptr);
}

void
WebGLContext::TexImage2D(GLenum texImageTarget, GLint level, GLenum internalFormat,
                         GLenum unpackFormat, GLenum unpackType, dom::Element* elem,
                         ErrorResult* const out_rv)
{
    webgl::ElementTexSource* texSource = nullptr;
    bool badCORS;
    webgl::ElementTexSource scopedTexSource(elem, mCanvasElement, this, &texSource,
                                            &badCORS);

    if (!texSource) {
        if (badCORS) {
            // Nothing in the spec details this as generating a GL error.
            GenerateWarning("It is forbidden to load a WebGL texture from a cross-domain"
                            " element that has not been validated with CORS. See"
                            " https://developer.mozilla.org/en/WebGL/Cross-Domain_Textures");
            *out_rv = NS_ERROR_DOM_SECURITY_ERR;
            return;
        }

        // Nothing in the spec details this as generating a GL error.
        GenerateWarning("Could not extract pixel data from the specified DOM element.");
        *out_rv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        return;
    }

    const GLsizei width = texSource->Size().width;
    const GLsizei height = texSource->Size().height;
    const GLint border = 0;

    TexImage2D_base(texImageTarget, level, internalFormat, width, height, border,
                    unpackFormat, unpackType,

                    nullptr, 0, texSource);
}

////////////////////////////////////////

void
WebGLContext::TexSubImage2D(GLenum texImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLsizei width, GLsizei height,
                            GLenum unpackFormat, GLenum unpackType,
                            const Nullable<dom::ArrayBufferView>& maybeView,
                            ErrorResult& out_rv)
{
    if (IsContextLost())
        return;

    if (maybeView.IsNull()) {
        ErrorInvalidValue("texSubImage2D: Null `pixels`.");
        return
    }

    const ArrayBufferView& view = maybeView.Value();

    view.ComputeLengthAndData();
    const void* data = view.Data();
    uint32_t dataSize = view.Length();

    const js::Scalar::Type arrayType = JS_GetArrayBufferViewType(view.Obj());

    if (!DoesUnpackTypeMatchArrayType(unpackType, arrayType)) {
        ErrorInvalidEnum("texSubImage2D: `type` must be compatible with TypedArray type.",
                         funcName);
        return;
    }

    TexSubImage2D_base(texImageTarget, level, xOffset, yOffset, width, height,
                       unpackFormat, unpackType,

                       data, dataSize, nullptr);
}
void
WebGLContext::TexSubImage2D(GLenum texImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLenum unpackFormat, GLenum unpackType,
                            dom::ImageData* imageData, ErrorResult& out_rv)
{
    if (IsContextLost())
        return;

    if (!pixels) {
        ErrorInvalidValue("texSubImage2D: Null `pixels`.");
        return
    }

    Uint8ClampedArray arr;
    MOZ_ALWAYS_TRUE( arr.Init(imageData->GetDataObject()) );
    arr.ComputeLengthAndData();

    const GLsizei width = imageData->Width();
    const GLsizei height = imageData->Height();
    const void* const data = arr.Data();
    const uint32_t dataSize = arr.Length();

    TexSubImage2D_base(texImageTarget, level, xOffset, yOffset, width, height,
                       unpackFormat, unpackType,

                       data, dataSize, nullptr);
}
void
WebGLContext::TexSubImage2D(GLenum texImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLenum unpackFormat, GLenum unpackType,
                            dom::Element* elem, ErrorResult* const out_rv)
{
    webgl::ElementTexSource* texSource = nullptr;
    bool badCORS;
    webgl::ElementTexSource scopedTexSource(elem, mCanvasElement, this, &texSource,
                                            &badCORS);

    if (!texSource) {
        if (badCORS) {
            // Nothing in the spec details this as generating a GL error.
            GenerateWarning("It is forbidden to load a WebGL texture from a cross-domain"
                            " element that has not been validated with CORS. See"
                            " https://developer.mozilla.org/en/WebGL/Cross-Domain_Textures");
            *out_rv = NS_ERROR_DOM_SECURITY_ERR;
            return;
        }

        // Nothing in the spec details this as generating a GL error.
        GenerateWarning("Could not extract pixel data from the specified DOM element.");
        *out_rv = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        return;
    }

    const GLsizei width = texSource->Size().width;
    const GLsizei height = texSource->Size().height;

    TexSubImage2D_base(texImageTarget, level, xOffset, yOffset, width, height,
                       unpackFormat, unpackType,

                       nullptr, 0, texSource);
}

////////////////////////////////////////

void
WebGLContext::CompressedTexImage2D(GLenum texImageTarget, GLint level,
                                   GLenum internalFormat, GLsizei width, GLsizei height,
                                   GLint border, const dom::ArrayBufferView& view)
{
    if (IsContextLost())
        return;

    view.ComputeLengthAndData();
    const void* const data = arr.Data();
    const uint32_t dataSize = arr.Length();

    CompressedTexImage2D_base(texImageTarget, level, internalFormat, width, height,
                              border, data, dataSize);
}
void
WebGLContext::CompressedTexSubImage2D(GLenum texImageTarget, GLint level, GLint xOffset,
                                      GLint yOffset, GLsizei width, GLsizei height,
                                      GLenum unpackFormat,
                                      const dom::ArrayBufferView& view)
{
    if (IsContextLost())
        return;

    view.ComputeLengthAndData();
    const void* const data = arr.Data();
    const uint32_t dataSize = arr.Length();

    CompressedTexSubImage2D_base(texImageTarget, level, xOffset, yOffset, width, height,
                                 unpackFormat, data, dataSize);
}

////////////////////////////////////////

void
WebGLContext::CopyTexImage2D(GLenum texImageTarget, GLint level, GLenum internalformat,
                             GLint x, GLint y, GLsizei width, GLsizei height,
                             GLint border)
{
    if (IsContextLost())
        return;

    CopyTexImage2D_base(texImageTarget, level, internalFormat, x, y, width, height,
                        border);
}

void
WebGLContext::CopyTexSubImage2D(GLenum texImageTarget, GLint level, GLint xOffset,
                                GLint yOffset, GLint x, GLint y, GLsizei width,
                                GLsizei height)
{
    if (IsContextLost())
        return;

    CopyTexSubImage2D_base(texImageTarget, level, xOffset, yOffset, x, y, width, height);
}

} // namespace mozilla
