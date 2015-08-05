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

static bool
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
WebGLTexture::ValidateTexImageTarget(const char* funcName, uint8_t funcDims,
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

    WebGLTexture::ImageInfo& imageInfo = texture->ImageInfoAt(target, level);

    *out_imageInfo = &imageInfo;
    return true;
}


// For *TexImage*
bool
WebGLTexture::ValidateTexImageSpecification(const char* funcName, uint8_t funcDims,
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
WebGLTexture::ValidateTexImageSelection(const char* funcName, uint8_t funcDims,
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
WebGLTexture::ValidateTexUnpack(const char* funcName, size_t funcDims, GLsizei width,
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
WebGLTexture::TexImage_base(const char* funcName, uint8_t funcDims, GLenum texImageTarget,
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
    bool hasUninitData = true;

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
            if (texSource->BlitToTexture(gl, texture->mGLName, GLenum(target), dstOrigin))
            {
                didUpload = true;
                hasUninitData = false;
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

    if (data)
        hasUninitData = false;

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

    const ImageInfo imageInfo(dstFormatUsage, width, height, depth, hasUninitData);
    SetImageInfoAt(texImageTarget, level, imageInfo);
    return;
}


void
WebGLTexture::TexSubImage_base(const char* funcName, uint8_t funcDims,
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

    imageInfo->PrepareForSubImage(gl, target, level, xOffset, yOffset, zOffset, width,
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
WebGLTexture::TexStorage_base(const char* funcName, uint8_t funcDims,
                              GLenum texTarget, GLsizei levels,
                              GLenum internalFormat, GLsizei width, GLsizei height,
                              GLsizei depth)
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

    SpecifyTexStorage(levels, dstFormatUsage, width, height, depth);
}


void
WebGLTexture::CompressedTexImage_base(const char* funcName, uint8_t funcDims,
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

    const bool hasUninitData = false;
    const ImageInfo imageInfo(formatUsage, width, height, depth, hasUninitData);
    SetImageInfoAt(texImageTarget, level, imageInfo);
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
WebGLTexture::CompressedTexSubImage_base(const char* funcName, uint8_t funcDims,
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

    imageInfo->PrepareForSubImage(gl, target, level, xOffset, yOffset, zOffset, width,
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
WebGLTexture::CopyTexImage2D_base(GLenum imageTarget, GLint level, GLenum internalFormat,
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

    const bool hasUninitData = false;
    const ImageInfo imageInfo(dstFormatUsage, width, height, depth, hasUninitData);
    SetImageInfoAt(texImageTarget, level, imageInfo);
}

void
WebGLTexture::CopyTexSubImage_base(const char* funcName, uint8_t funcDims,
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
//////////////////////////////////////////////////////////////////////////////////////////
// Specific->Generic redirection functions.

void
WebGLTexture::TexStorage2D(TexTarget texTarget, GLsizei levels, GLenum internalFormat,
                                GLsizei width, GLsizei height)
{
    const char funcName[] = "TexStorage2D";
    const uint8_t funcDims = 2;

    const GLsizei depth = 1;

    TexStorage_base(funcName, funcDims, texTarget, levels, internalFormat, width, height,
                    depth);
}
void
WebGLTexture::TexStorage3D(TexTarget texTarget, GLsizei levels, GLenum internalFormat,
                                GLsizei width, GLsizei height, GLsizei depth)
{
    const char funcName[] = "TexStorage3D";
    const uint8_t funcDims = 3;

    TexStorage_base(funcName, funcDims, texTarget, levels, internalFormat, width, height,
                    depth);
}

//////////

void
WebGLTexture::TexImage2D(TexImageTarget texImageTarget, GLint level, GLenum internalFormat,
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
WebGLTexture::TexImage3D(TexImageTarget texImagetTarget, GLint level, GLenum internalFormat,
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

void
WebGLTexture::CompressedTexImage2D(TexImageTarget texImagetTarget, GLint level,
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
WebGLTexture::CompressedTexImage3D(TexImageTarget texImagetTarget, GLint level,
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
WebGLTexture::CompressedTexSubImage2D(TexImageTarget texImagetTarget, GLint level,
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
WebGLTexture::CompressedTexSubImage3D(TexImageTarget texImagetTarget, GLint level,
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
WebGLTexture::CopyTexSubImage2D(TexImageTarget texImagetTarget, GLint level, GLint xOffset,
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
WebGLTexture::CopyTexSubImage3D(TexImageTarget texImagetTarget, GLint level, GLint xOffset,
                                     GLint yOffset, GLint zOffset, GLint x, GLint y,
                                     GLsizei width, GLsizei height)
{
    const char funcName[] = "CopyTexSubImage3D";
    const uint8_t funcDims = 3;

    CopyTexSubImage_base(funcName, funcDims, texImageTarget, level, xOffset, yOffset,
                         zOffset, x, y, width, height);
}

//////////

void
WebGLTexture::TexSubImage2D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
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
WebGLTexture::TexSubImage3D(TexImageTarget texImagetTarget, GLint level, GLint xOffset,
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


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////




























































/*

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

    auto format = GetInfoBySizedFormat(unpackFormat);
    if (!format || !format->compression) {
        mContext->ErrorInvalidEnum("%s: Bad `format`.", funcName);
        return;
    }

    if (!mContext->ValidateTexImage(texImageTarget.get(), level,
                                    xOffset, yOffset, 0,
                                    width, height, 0, 0,
                                    func, dims))
    {
        return;
    }

    ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    if (!imageInfo.IsDefined()) {
        mContext->ErrorInvalidOperation("%s: Destination tex-image is not defined.",
                                        funcName);
        return;
    }

    if (format != imageInfo.mFormat->formatInfo) {
        mContext->ErrorInvalidOperation("%s: Unpack format does not match format of"
                                        " existing tex-image.",
                                        funcName);
        return;
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
WebGLTexture::TexStorage_base(const char* funcName, uint8_t funcDims,
                              GLenum texTarget, GLsizei levels,
                              GLenum rawInternalFormat, GLsizei width, GLsizei height,
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
WebGLTexture::TexStorage2D(TexTarget texTarget, GLsizei levels, GLenum rawInternalFormat,
                           GLsizei width, GLsizei height)
{
    if (texTarget != LOCAL_GL_TEXTURE_2D && texTarget != LOCAL_GL_TEXTURE_CUBE_MAP) {
        mContext->ErrorInvalidEnum("texStorage2D: target must be TEXTURE_2D or"
                                   " TEXTURE_CUBE_MAP.");
        return;
    }

    const char funcName[] = "TexStorage2D";
    const uint8_t funcDims = 2;

    const GLsizei depth = 1;

    TexStorage_base(funcName, funcDims, texTarget, levels, internalFormat, width, height,
                    depth);

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
*/

} // namespace mozilla
