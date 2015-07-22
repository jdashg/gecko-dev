/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLTexture.h"

namespace mozilla {

JS::Value
WebGL2Context::GetTexParameterInternal(const TexTarget& target, GLenum pname)
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
        {
            GLint i = 0;
            gl->fGetTexParameteriv(target.get(), pname, &i);
            return JS::NumberValue(uint32_t(i));
        }

    case LOCAL_GL_TEXTURE_MAX_LOD:
    case LOCAL_GL_TEXTURE_MIN_LOD:
        {
            GLfloat f = 0.0f;
            gl->fGetTexParameterfv(target.get(), pname, &f);
            return JS::NumberValue(float(f));
        }
    }

    return WebGLContext::GetTexParameterInternal(target, pname);
}


// -------------------------------------------------------------------------
// Texture objects

void
WebGL2Context::TexStorage2D(GLenum texTarget, GLsizei levels, GLenum internalFormat,
                            GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    TexStorage2D_base(texTarget, levels, internalFormat, width, height);
}

void
WebGL2Context::TexStorage3D(GLenum texTarget, GLsizei levels, GLenum internalFormat,
                            GLsizei width, GLsizei height, GLsizei depth)
{
    if (IsContextLost())
        return;

    TexStorage3D_base(texTarget, levels, internalFormat, width, height, depth);
}

void
WebGL2Context::TexImage3D(GLenum texImageTarget, GLint level, GLenum internalFormat,
                          GLsizei width, GLsizei height, GLsizei depth, GLint border,
                          GLenum unpackFormat, GLenum unpackType,
                          const Nullable<dom::ArrayBufferView>& maybeView,
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
        const dom::ArrayBufferView& view = maybeView.Value();
        view.ComputeLengthAndData();

        data = view.Data();
        dataSize = view.Length();

        const js::Scalar::Type arrayType = JS_GetArrayBufferViewType(view.Obj());

        if (!DoesUnpackTypeMatchArrayType(unpackType, arrayType)) {
            ErrorInvalidEnum("texImage3D: `type` must be compatible with TypedArray type.",
                             funcName);
            return;
        }
    }

    TexImage3D_base(texImageTarget, level, internalFormat, width, height, depth, border,
                    unpackFormat, unpackType,

                    data, dataSize, nullptr);
}

void
WebGL2Context::TexSubImage3D(GLenum texImageTarget, GLint level, GLint xOffset,
                             GLint yOffset, GLint zOffset, GLsizei width, GLsizei height,
                             GLsizei depth, GLenum unpackFormat, GLenum unpackType,
                             const Nullable<dom::ArrayBufferView>& maybeView,
                             ErrorResult& out_rv)
{
    if (IsContextLost())
        return;

    if (maybeView.IsNull()) {
        ErrorInvalidValue("texSubImage3D: Null `pixels`.");
        return
    }

    const dom::ArrayBufferView& view = maybeView.Value();

    view.ComputeLengthAndData();
    const void* data = view.Data();
    uint32_t dataSize = view.Length();

    const js::Scalar::Type arrayType = JS_GetArrayBufferViewType(view.Obj());

    if (!DoesUnpackTypeMatchArrayType(unpackType, arrayType)) {
        ErrorInvalidEnum("texSubImage3D: `type` must be compatible with TypedArray type.",
                         funcName);
        return;
    }

    TexSubImage3D_base(texImageTarget, level, xOffset, yOffset, zOffset, width, height,
                       depth, unpackFormat, unpackType,

                       data, dataSize, nullptr);
}

void
WebGL2Context::TexSubImage3D(GLenum texImageTarget, GLint level, GLint xOffset,
                             GLint yOffset, GLint zOffset, GLenum unpackFormat,
                             GLenum unpackType, dom::ImageData* imageData,
                             ErrorResult& out_rv)
{
    if (IsContextLost())
        return;

    if (!pixels) {
        ErrorInvalidValue("texSubImage3D: Null `pixels`.");
        return
    }

    Uint8ClampedArray arr;
    MOZ_ALWAYS_TRUE( arr.Init(imageData->GetDataObject()) );
    arr.ComputeLengthAndData();

    const GLsizei width = imageData->Width();
    const GLsizei height = imageData->Height();
    const void* const data = arr.Data();
    const uint32_t dataSize = arr.Length();

    TexSubImage2D_base(texImageTarget, level, xOffset, yOffset, zOffset, width, height,
                       depth, unpackFormat, unpackType,

                       data, dataSize, nullptr);
}

void
WebGL2Context::TexSubImage3D(GLenum texImageTarget, GLint level, GLint xOffset,
                             GLint yOffset, GLint zOffset, GLenum unpackFormat,
                             GLenum unpackType, dom::Element* elem,
                             ErrorResult* const out_rv)
{
    ElementTexSource* texSource = nullptr;
    bool badCORS;
    ElementTexSource scopedTexSource(elem, &texSource, &badCORS);

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
    const GLsizei depth = 1;

    TexSubImage3D_base(texImageTarget, level, xOffset, yOffset, zOffset, width, height,
                       depth, unpackFormat, unpackType,

                       nullptr, 0, texSource);
}

void
WebGL2Context::CompressedTexImage3D(GLenum texImageTarget, GLint level,
                                    GLenum internalFormat, GLsizei width, GLsizei height,
                                    GLsizei depth, GLint border,
                                    const dom::ArrayBufferView& view)
{
    if (IsContextLost())
        return;

    view.ComputeLengthAndData();
    const void* const data = arr.Data();
    const uint32_t dataSize = arr.Length();

    CompressedTexImage3D_base(texImageTarget, level, internalFormat, width, height, depth,
                              border, data, dataSize);
}

void
WebGL2Context::CompressedTexSubImage3D(GLenum texImageTarget, GLint level, GLint xOffset,
                                       GLint yOffset, GLint zOffset, GLsizei width,
                                       GLsizei height, GLsizei depth, GLenum unpackFormat,
                                       const dom::ArrayBufferView& view)
{
    if (IsContextLost())
        return;

    view.ComputeLengthAndData();
    const void* const data = arr.Data();
    const uint32_t dataSize = arr.Length();

    CompressedTexSubImage3D_base(texImageTarget, level, xOffset, yOffset, zOffset, width,
                                 height, depth, unpackFormat, data, dataSize);
}

void
WebGL2Context::CopyTexSubImage3D(GLenum texImageTarget, GLint level,
                                 GLint xOffset, GLint yOffset, GLint zOffset,
                                 GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    CopyTexSubImage3D_base(texImageTarget, level, xOffset, yOffset, zOffset, x, y, width,
                           height);
}

} // namespace mozilla
