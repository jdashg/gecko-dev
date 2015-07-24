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
    //        dimensions or format[.]"
    // p136: "* If executing the pseudo-code would result in any other error, the error is
    //          generated and the command will have no effect."
    // p137-138: The pseudo-code uses TexImage*.
    //
    if (mImmutable) {
        mContext->ErrorInvalidOperation("%s: Texture is already immutable.", funcName);
        return false;
    }

    // INVALID_ENUM is generated if internalformat is not a valid sized internal format.
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
        return mContext->ErrorInvalidOperation("texImage3D: bad combination of internalformat and type");
    }

    // we need to find the exact sized format of the source data. Slightly abusing
    // EffectiveInternalFormatFromInternalFormatAndType for that purpose. Really, an unsized source format
    // is the same thing as an unsized internalformat.
    TexInternalFormat effectiveUnpackFormat =
        EffectiveInternalFormatFromInternalFormatAndType(unpackFormat, unpackType);
    MOZ_ASSERT(effectiveSourceFormat != LOCAL_GL_NONE); // should have validated format/type combo earlier
    const size_t srcbitsPerTexel = GetBitsPerTexel(effectiveUnpackFormat);
    MOZ_ASSERT((srcbitsPerTexel % 8) == 0); // should not have compressed formats here.
    size_t srcTexelSize = srcbitsPerTexel / 8;

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, depth, srcTexelSize, mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (dataLength && dataLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texImage3D: not enough data for operation (need %d, have %d)",
                                 bytesNeeded, dataLength);

    if (tex->IsImmutable()) {
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

    tex->SetImageInfo(texImageTarget, level,
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

    const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.EffectiveInternalFormat();
    TexInternalFormat existingUnsizedInternalFormat = LOCAL_GL_NONE;
    TexType existingType = LOCAL_GL_NONE;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(existingEffectiveInternalFormat,
                                                            &existingUnsizedInternalFormat,
                                                            &existingType);

    const GLint border = 0;
    if (!ValidateTexImage(texImageTarget, level, existingEffectiveInternalFormat.get(),
                          xoffset, yoffset, zoffset, width, height, depth, border,
                          unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (unpackType != existingType) {
        return mContext->ErrorInvalidOperation("texSubImage3D: type differs from that of the existing image");
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
        return mContext->ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (dataLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, dataLength);

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xoffset == 0 &&
                                yoffset == 0 &&
                                zoffset == 0 &&
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
                       xoffset, yoffset, zoffset,
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
