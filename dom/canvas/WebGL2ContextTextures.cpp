/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"
#include "WebGL2Context.h"
#include "WebGLContextUtils.h"
#include "WebGLTexture.h"

namespace mozilla {

bool
WebGL2Context::ValidateSizedInternalFormat(GLenum internalFormat, const char* info)
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
    EnumName(internalFormat, &name);
    ErrorInvalidEnum("%s: invalid internal format %s", info, name.get());

    return false;
}

/** Validates parameters to texStorage{2D,3D} */
bool
WebGL2Context::ValidateTexStorage(GLenum target, GLsizei levels, GLenum internalFormat,
                                      GLsizei width, GLsizei height, GLsizei depth,
                                      const char* info)
{
    // GL_INVALID_OPERATION is generated if the default texture object is curently bound to target.
    WebGLTexture* tex = ActiveBoundTextureForTarget(target);
    if (!tex) {
        ErrorInvalidOperation("%s: no texture is bound to target %s", info, EnumName(target));
        return false;
    }

    // GL_INVALID_OPERATION is generated if the texture object currently bound to target already has
    // GL_TEXTURE_IMMUTABLE_FORMAT set to GL_TRUE.
    if (tex->IsImmutable()) {
        ErrorInvalidOperation("%s: texture bound to target %s is already immutable", info, EnumName(target));
        return false;
    }

    // GL_INVALID_ENUM is generated if internalFormat is not a valid sized internal format.
    if (!ValidateSizedInternalFormat(internalFormat, info))
        return false;

    // GL_INVALID_VALUE is generated if width, height or levels are less than 1.
    if (width < 1)  { ErrorInvalidValue("%s: width is < 1", info);  return false; }
    if (height < 1) { ErrorInvalidValue("%s: height is < 1", info); return false; }
    if (depth < 1)  { ErrorInvalidValue("%s: depth is < 1", info);  return false; }
    if (levels < 1) { ErrorInvalidValue("%s: levels is < 1", info); return false; }

    // GL_INVALID_OPERATION is generated if levels is greater than floor(log2(max(width, height, depth)))+1.
    if (FloorLog2(std::max(std::max(width, height), depth)) + 1 < levels) {
        ErrorInvalidOperation("%s: too many levels for given texture dimensions", info);
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------
// Texture objects

void
WebGL2Context::TexStorage2D(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    // GL_INVALID_ENUM is generated if target is not one of the accepted target enumerants.
    if (target != LOCAL_GL_TEXTURE_2D && target != LOCAL_GL_TEXTURE_CUBE_MAP)
        return ErrorInvalidEnum("texStorage2D: target is not TEXTURE_2D or TEXTURE_CUBE_MAP");

    if (!ValidateTexStorage(target, levels, internalFormat, width, height, 1, "texStorage2D"))
        return;

    GetAndFlushUnderlyingGLErrors();
    gl->fTexStorage2D(target, levels, internalFormat, width, height);
    GLenum error = GetAndFlushUnderlyingGLErrors();
    if (error) {
        return GenerateWarning("texStorage2D generated error %s", ErrorName(error));
    }

    WebGLTexture* tex = ActiveBoundTextureForTarget(target);
    tex->SetImmutable();

    const size_t facesCount = (target == LOCAL_GL_TEXTURE_2D) ? 1 : 6;
    GLsizei w = width;
    GLsizei h = height;
    for (size_t l = 0; l < size_t(levels); l++) {
        for (size_t f = 0; f < facesCount; f++) {
            tex->SetImageInfo(TexImageTargetForTargetAndFace(target, f),
                              l, w, h, 1,
                              internalFormat,
                              WebGLImageDataStatus::UninitializedImageData);
        }
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }
}

void
WebGL2Context::TexStorage3D(GLenum target, GLsizei levels, GLenum internalFormat,
                            GLsizei width, GLsizei height, GLsizei depth)
{
    if (IsContextLost())
        return;

    // GL_INVALID_ENUM is generated if target is not one of the accepted target enumerants.
    if (target != LOCAL_GL_TEXTURE_3D)
        return ErrorInvalidEnum("texStorage3D: target is not TEXTURE_3D");

    if (!ValidateTexStorage(target, levels, internalFormat, width, height, depth, "texStorage3D"))
        return;

    GetAndFlushUnderlyingGLErrors();
    gl->fTexStorage3D(target, levels, internalFormat, width, height, depth);
    GLenum error = GetAndFlushUnderlyingGLErrors();
    if (error) {
        return GenerateWarning("texStorage3D generated error %s", ErrorName(error));
    }

    WebGLTexture* tex = ActiveBoundTextureForTarget(target);
    tex->SetImmutable();

    GLsizei w = width;
    GLsizei h = height;
    GLsizei d = depth;
    for (size_t l = 0; l < size_t(levels); l++) {
        tex->SetImageInfo(TexImageTargetForTargetAndFace(target, 0),
                          l, w, h, d,
                          internalFormat,
                          WebGLImageDataStatus::UninitializedImageData);
        w = std::max(1, w >> 1);
        h = std::max(1, h >> 1);
        d = std::max(1, d >> 1);
    }
}

void
WebGL2Context::TexImage3D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                          GLsizei width, GLsizei height, GLsizei depth,
                          GLint border, GLenum unpackFormat, GLenum unpackType,
                          const dom::Nullable<dom::ArrayBufferView>& maybeView,
                          ErrorResult& out_rv)
{
    if (IsContextLost())
        return;

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

    const char funcName[] = "texImage3D";
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, funcName, &texImageTarget, &tex))
    {
        return;
    }

    if (!DoesTargetMatchDimensions(this, texImageTarget, 3, funcName))
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::TexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex3D;

    if (!ValidateTexImage(texImageTarget, level, internalFormat,
                          0, 0, 0,
                          width, height, depth,
                          border, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (!ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);

    if (effectiveInternalFormat == LOCAL_GL_NONE) {
        return ErrorInvalidOperation("texImage3D: bad combination of internalFormat and unpackType");
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
        GetImageSize(height, width, depth, srcTexelSize, mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (dataLength && dataLength < bytesNeeded)
        return ErrorInvalidOperation("texImage3D: not enough data for operation (need %d, have %d)",
                                 bytesNeeded, dataLength);

    if (tex->IsImmutable()) {
        return ErrorInvalidOperation(
            "texImage3D: disallowed because the texture "
            "bound to this target has already been made immutable by texStorage3D");
    }

    GLenum driverUnpackType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             effectiveInternalFormat,
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
        return GenerateWarning("texImage3D generated error %s", ErrorName(error));
    }

    tex->SetImageInfo(texImageTarget, level,
                      width, height, depth,
                      effectiveInternalFormat,
                      data ? WebGLImageDataStatus::InitializedImageData
                           : WebGLImageDataStatus::UninitializedImageData);
}

void
WebGL2Context::TexSubImage3D(GLenum rawTexImageTarget, GLint level,
                             GLint xOffset, GLint yOffset, GLint zOffset,
                             GLsizei width, GLsizei height, GLsizei depth,
                             GLenum unpackFormat, GLenum unpackType,
                             const dom::Nullable<dom::ArrayBufferView>& maybeView,
                             ErrorResult& out_rv)
{
    if (IsContextLost())
        return;

    if (maybeView.IsNull())
        return ErrorInvalidValue("texSubImage3D: pixels must not be null!");

    const dom::ArrayBufferView& view = maybeView.Value();
    view.ComputeLengthAndData();

    const char funcName[] = "texSubImage3D";
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, funcName, &texImageTarget, &tex))
    {
        return;
    }

    if (!DoesTargetMatchDimensions(this, texImageTarget, 3, funcName))
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::TexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex3D;

    if (!tex->HasImageInfoAt(texImageTarget, level)) {
        return ErrorInvalidOperation("texSubImage3D: no previously defined texture image");
    }

    const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.EffectiveInternalFormat();
    TexInternalFormat existingUnsizedInternalFormat = LOCAL_GL_NONE;
    TexType existingType = LOCAL_GL_NONE;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(existingEffectiveInternalFormat,
                                                            &existingUnsizedInternalFormat,
                                                            &existingType);

    if (!ValidateTexImage(texImageTarget, level, existingEffectiveInternalFormat.get(),
                          xOffset, yOffset, zOffset,
                          width, height, depth,
                          0, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (unpackType != existingType) {
        return ErrorInvalidOperation("texSubImage3D: type differs from that of the existing image");
    }

    js::Scalar::Type jsArrayType = view.Type();
    void* data = view.Data();
    size_t dataLength = view.Length();

    if (!ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    const size_t bitsPerTexel = GetBitsPerTexel(existingEffectiveInternalFormat);
    MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed formats here.
    size_t srcTexelSize = bitsPerTexel / 8;

    if (width == 0 || height == 0 || depth == 0)
        return; // no effect, we better return right now

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, depth, srcTexelSize, mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (dataLength < bytesNeeded)
        return ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, dataLength);

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                zOffset == 0 &&
                                width == imageInfo.Width() &&
                                height == imageInfo.Height() &&
                                depth == imageInfo.Depth();
        if (coversWholeImage) {
            tex->SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!tex->EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    GLenum driverUnpackType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
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
WebGL2Context::TexSubImage3D(GLenum target, GLint level,
                             GLint xOffset, GLint yOffset, GLint zOffset,
                             GLenum unpackFormat, GLenum unpackType, dom::ImageData* imageData,
                             ErrorResult& out_rv)
{
    GenerateWarning("texSubImage3D: Not implemented.");
}

void
WebGL2Context::CopyTexSubImage3D(GLenum target, GLint level,
                                 GLint xOffset, GLint yOffset, GLint zOffset,
                                 GLint x, GLint y, GLsizei width, GLsizei height)
{
    GenerateWarning("copyTexSubImage3D: Not implemented.");
}

void
WebGL2Context::CompressedTexImage3D(GLenum target, GLint level, GLenum internalFormat,
                                    GLsizei width, GLsizei height, GLsizei depth,
                                    GLint border, GLsizei imageSize, const dom::ArrayBufferView& view)
{
    GenerateWarning("compressedTexImage3D: Not implemented.");
}

void
WebGL2Context::CompressedTexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset, GLint zOffset,
                                       GLsizei width, GLsizei height, GLsizei depth,
                                       GLenum unpackFormat, GLsizei imageSize, const dom::ArrayBufferView& view)
{
    GenerateWarning("compressedTexSubImage3D: Not implemented.");
}

bool
WebGL2Context::IsTexParamValid(GLenum pname) const
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_BASE_LEVEL:
    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
    case LOCAL_GL_TEXTURE_COMPARE_MODE:
    case LOCAL_GL_TEXTURE_IMMUTABLE_FORMAT:
    case LOCAL_GL_TEXTURE_IMMUTABLE_LEVELS:
    case LOCAL_GL_TEXTURE_MAX_LEVEL:
    case LOCAL_GL_TEXTURE_SWIZZLE_A:
    case LOCAL_GL_TEXTURE_SWIZZLE_B:
    case LOCAL_GL_TEXTURE_SWIZZLE_G:
    case LOCAL_GL_TEXTURE_SWIZZLE_R:
    case LOCAL_GL_TEXTURE_WRAP_R:
    case LOCAL_GL_TEXTURE_MAX_LOD:
    case LOCAL_GL_TEXTURE_MIN_LOD:
        return true;

    default:
        return WebGLContext::IsTexParamValid(pname);
    }
}

} // namespace mozilla
