/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
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

static bool
IsValidTexTarget(WebGLContext* webgl, GLenum rawTexTarget,
                 TexTarget* const out)
{
    switch (rawTexTarget) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP:
        break;

    case LOCAL_GL_TEXTURE_3D:
        if (!webgl->IsWebGL2())
            return false;

        break;

    default:
        return false;
    }

    *out = rawTexTarget;
    return true;
}

static bool
IsValidTexImageTarget(WebGLContext* webgl, uint8_t funcDims, GLenum rawTexImageTarget,
                      TexImageTarget* const out)
{
    uint8_t targetDims;

    switch (rawTexImageTarget) {
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
        if (!webgl->IsWebGL2())
            return false;

        targetDims = 3;
        break;

    default:
        return false;
    }

    if (targetDims != funcDims)
        return false;

    *out = rawTexImageTarget;
    return true;
}

bool
ValidateTexTarget(WebGLContext* webgl, const char* funcName, GLenum rawTexTarget,
                  TexTarget* const out_texTarget, WebGLTexture** const out_tex)
{
    if (webgl->IsContextLost())
        return false;

    TexTarget texTarget;
    if (!IsValidTexTarget(webgl, rawTexTarget, &texTarget)) {
        webgl->ErrorInvalidEnum("%s: Invalid texTarget.", funcName);
        return false;
    }

    WebGLTexture* tex = webgl->ActiveBoundTextureForTarget(texTarget);
    if (!tex) {
        webgl->ErrorInvalidOperation("%s: No texture is bound to this target.", funcName);
        return false;
    }

    *out_texTarget = texTarget;
    *out_tex = tex;
    return true;
}

bool
ValidateTexImageTarget(WebGLContext* webgl, const char* funcName, uint8_t funcDims,
                       GLenum rawTexImageTarget, TexImageTarget* const out_texImageTarget,
                       WebGLTexture** const out_tex)
{
    if (webgl->IsContextLost())
        return false;

    TexImageTarget texImageTarget;
    if (!IsValidTexImageTarget(webgl, funcDims, rawTexImageTarget, &texImageTarget)) {
        webgl->ErrorInvalidEnum("%s: Invalid texImageTarget.", funcName);
        return false;
    }

    WebGLTexture* tex = webgl->ActiveBoundTextureForTexImageTarget(texImageTarget);
    if (!tex) {
        webgl->ErrorInvalidOperation("%s: No texture is bound to this target.", funcName);
        return false;
    }

    *out_texImageTarget = texImageTarget;
    *out_tex = tex;
    return true;
}

bool
WebGLContext::IsTexParamValid(GLenum pname) const
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
    case LOCAL_GL_TEXTURE_MAG_FILTER:
    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
        return true;

    case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
        return IsExtensionEnabled(WebGLExtensionID::EXT_texture_filter_anisotropic);

    default:
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// GL calls

void
WebGLContext::BindTexture(GLenum rawTarget, WebGLTexture* newTex)
{
    if (IsContextLost())
        return;

     if (!ValidateObjectAllowDeletedOrNull("bindTexture", newTex))
        return;

    // Need to check rawTarget first before comparing against newTex->Target() as
    // newTex->Target() returns a TexTarget, which will assert on invalid value.
    WebGLRefPtr<WebGLTexture>* currentTexPtr = nullptr;
    switch (rawTarget) {
        case LOCAL_GL_TEXTURE_2D:
            currentTexPtr = &mBound2DTextures[mActiveTexture];
            break;

       case LOCAL_GL_TEXTURE_CUBE_MAP:
            currentTexPtr = &mBoundCubeMapTextures[mActiveTexture];
            break;

       case LOCAL_GL_TEXTURE_3D:
            if (!IsWebGL2())
                return ErrorInvalidEnum("bindTexture: target TEXTURE_3D is only available in WebGL version 2.0 or newer");

            currentTexPtr = &mBound3DTextures[mActiveTexture];
            break;

       default:
            return ErrorInvalidEnumInfo("bindTexture: target", rawTarget);
    }
    const TexTarget texTarget(rawTarget);

    MakeContextCurrent();

    if (newTex) {
        if (!newTex->BindTexture(texTarget))
            return;
    } else {
        gl->fBindTexture(texTarget.get(), 0);
    }

    *currentTexPtr = newTex;
}

void
WebGLContext::GenerateMipmap(GLenum rawTexTarget)
{
    const char funcName[] = "generateMipmap";

    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, funcName, rawTexTarget, &texTarget, &tex))
        return;

    tex->GenerateMipmap(texTarget);
}

JS::Value
WebGLContext::GetTexParameter(GLenum rawTexTarget, GLenum pname)
{
    const char funcName[] = "getTexParameter";

    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, funcName, rawTexTarget, &texTarget, &tex))
        return JS::NullValue();

    if (!IsTexParamValid(pname)) {
        ErrorInvalidEnumInfo("getTexParameter: pname", pname);
        return JS::NullValue();
    }

    return tex->GetTexParameter(texTarget, pname);
}

bool
WebGLContext::IsTexture(WebGLTexture* tex)
{
    if (IsContextLost())
        return false;

    if (!ValidateObjectAllowDeleted("isTexture", tex))
        return false;

    return tex->IsTexture();
}

void
WebGLContext::TexParameter_base(GLenum rawTexTarget, GLenum pname, GLint* maybeIntParam,
                                GLfloat* maybeFloatParam)
{
    MOZ_ASSERT(maybeIntParam || maybeFloatParam);

    const char funcName[] = "texParameter";

    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, funcName, rawTexTarget, &texTarget, &tex))
        return;

    tex->TexParameter(texTarget, pname, maybeIntParam, maybeFloatParam);
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// Uploads


//////////////////////////////////////////////////////////////////////////////////////////
// TexImage

static bool
IsElemValidForCORS(dom::HTMLMediaElement* elem, WebGLContext* webgl)
{
    if (elem->GetCORSMode() == CORS_NONE) {
        nsIPrincipal* srcPrincipal = elem->GetCurrentPrincipal();
        if (!principal)
            return false;

        nsIPrincipal* dstPrincipal = webgl->GetCanvas()->NodePrincipal();

        bool subsumes;
        nsresult rv = dstPrincipal->Subsumes(srcPrincipal, &subsumes);
        if (NS_FAILED(rv) || !subsumes) {
            return false;
        }
    }

    return true;
}

static bool
ValidateCORSForElem(dom::HTMLMediaElement* elem, WebGLContext* webgl,
                    const char* funcName)
{
    if (!IsElemValidForCORS(elem, webgl)) {
        ErrorInvalidOperation("%s: Bad CORS for DOM element.", funcName);
        return false;
    }

    return true;
}

static UniquePtr<TexUnpackBlob>
UnpackBlobFromElement();

struct PrepareForUnpack
{
    gfx::DataSourceSurface::ScopedMap scopedMap;
    nsTArray<uint8_t> scratchBuffer;

    PrepareForUnpack(const RefPtr<gfx::DataSourceSurface>& src, GLenum unpackFormat,
                     GLenum unpackType,
                     const void** const out_data, size_t* const out_size,
                     uint8_t* const out_alignment)
        : scopedMap(src, gfx::DataSourceSurface::MapType::READ)
    {
        *out_data = nullptr;

        if (!scopedMap.IsMapped())
            return;



        switch (src->GetFormat()) {
        case gfx::SurfaceFormat::B8G8R8A8:



    }

};



void
WebGLContext::TexImage2D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                         GLenum unpackFormat, GLenum unpackType,
                         dom::HTMLMediaElement* elem, ErrorResult* const out_rv)
{
    const char funcName[] = "texImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget,
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->TexImage(funcName, funcDims, texImageTarget, level, internalFormat, unpackFormat,
                  unpackType, elem, out_rv);
}







// dom::ArrayBufferView

static bool
DoesJSTypeMatchUnpackType(js::Scalar::Type jsType, GLenum unpackType)
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

static UniquePtr<TexUnpackBuffer>
ToUnpackBuffer(GLsizei width, GLsizei height, GLenum unpackType,
               dom::ArrayBufferView* view, WebGLContext* webgl, const char* funcName)
{
    if (!DoesJSTypeMatchUnpackType(view->Type(), unpackType)) {
        webgl->ErrorInvalidOperation("%s: Invalid unpack `type` for given array.",
                                     funcName);
        return nullptr;
    }

    view->ComputeLengthAndData();

    void* data = view->Data();
    uint32_t length = view->Length();
    return MakeUnique<TexUnpackBuffer>(webgl, width, height, border,
}







void
WebGLContext::TexImage2D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                         GLsizei width, GLsizei height, GLint border, GLenum unpackFormat,
                         GLenum unpackType,
                         const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                         ErrorResult& out_rv)
{
    const char funcName[] = "texImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget,
                                &texImageTarget, &tex))
    {
        return;
    }

    size_t dataSize;
    void* data;
    if (maybeView.IsNull()) {
        dataSize = 0;
        data = nullptr;
    } else {
        const dom::ArrayBufferView& view = maybeView.Value();

        if (!ValidateTexInputData(unpackType, view.Type(), funcName, funcDims))
            return;

        view.ComputeLengthAndData();

        dataSize = view.Length();
        data = view.Data();
    }

    const GLsizei depth = 1;

    tex->TexImage(funcName, funcDims, texImageTarget, level, internalFormat, width,
                  height, depth, border, unpackFormat, unpackType, dataSize, data,
                  out_rv);
}

void
WebGLContext::TexImage2D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                         GLenum unpackFormat, GLenum unpackType,
                         dom::ImageData* imageData, ErrorResult& out_rv)
{
    const char funcName[] = "texImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget,
                                &texImageTarget, &tex))
    {
        return;
    }

    if (!imageData) {
        // Spec says to generate an INVALID_VALUE error
        mContext->ErrorInvalidValue("%s: null ImageData", funcName);
        return;
    }

    dom::Uint8ClampedArray arr;
    DebugOnly<bool> inited = arr.Init(imageData->GetDataObject());
    MOZ_ASSERT(inited);

    GLsizei width = imageData->Width();
    GLsizei height = imageData->Height();
    GLsizei depth = 1;
    GLint border = 0;

    arr.ComputeLengthAndData();
    size_t dataSize = arr.Length();
    void* data = arr.Data();

    tex->TexImage(funcName, funcDims, texImageTarget, level, internalFormat, width,
                  height, depth, border, unpackFormat, unpackType, dataSize, data,
                  out_rv);
}


//////////////////////////////////////////////////////////////////////////////////////////
// TexSubImage


void
WebGLContext::TexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLenum unpackFormat, GLenum unpackType,
                            dom::Element* elem, ErrorResult* const out_rv)
{
    const char funcName[] = "texSubImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget,
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->TexSubImage2D(texImageTarget, level, xOffset, yOffset, unpackFormat, unpackType,
                       elem, out_rv);
}

void
WebGLContext::TexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLsizei width, GLsizei height,
                            GLenum unpackFormat, GLenum unpackType,
                            const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                            ErrorResult& out_rv)
{
    const char funcName[] = "texSubImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget,
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->TexSubImage2D(texImageTarget, level, xOffset, yOffset, width, height,
                       unpackFormat, unpackType, maybeView, &out_rv);
}

void
WebGLContext::TexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xOffset, GLint yOffset,
                            GLenum unpackFormat, GLenum unpackType, dom::ImageData* imageData,
                            ErrorResult& out_rv)
{
    const char funcName[] = "texSubImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget,
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->TexSubImage2D(texImageTarget, level, xOffset, yOffset, unpackFormat, unpackType,
                       imageData, &out_rv);
}


//////////////////////////////////////////////////////////////////////////////////////////
// CopyTex(Sub)Image


void
WebGLContext::CopyTexImage2D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                             GLint x, GLint y, GLsizei width, GLsizei height,
                             GLint border)
{
    const char funcName[] = "copyTexImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget,
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->CopyTexImage2D(texImageTarget, level, internalFormat, x, y, width, height,
                        border);
}

void
WebGLContext::CopyTexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                                GLint yOffset, GLint x, GLint y, GLsizei width,
                                GLsizei height)
{
    const char funcName[] = "copyTexSubImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget,
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->CopyTexSubImage2D(texImageTarget, level, xOffset, yOffset, x, y, width, height);
}


//////////////////////////////////////////////////////////////////////////////////////////
// CompressedTex(Sub)Image


void
WebGLContext::CompressedTexImage2D(GLenum rawTexImageTarget, GLint level,
                                   GLenum internalFormat, GLsizei width, GLsizei height,
                                   GLint border, const dom::ArrayBufferViewOrSharedArrayBufferView& view)
{
    const char funcName[] = "compressedTexImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget,
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->CompressedTexImage2D(texImageTarget, level, internalFormat, width, height,
                              border, view);
}

void
WebGLContext::CompressedTexSubImage2D(GLenum rawTexImageTarget, GLint level,
                                      GLint xOffset, GLint yOffset, GLsizei width,
                                      GLsizei height, GLenum unpackFormat,
                                      const dom::ArrayBufferViewOrSharedArrayBufferView& view)
{
    const char funcName[] = "compressedTexSubImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget,
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->CompressedTexSubImage2D(texImageTarget, level, xOffset, yOffset, width, height,
                                 unpackFormat, view);
}

} // namespace mozilla
