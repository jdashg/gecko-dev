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
            if (!IsWebGL2()) {
                return ErrorInvalidEnum("bindTexture: target TEXTURE_3D is only available in WebGL version 2.0 or newer");
            }
            currentTexPtr = &mBound3DTextures[mActiveTexture];
            break;
       default:
            return ErrorInvalidEnumInfo("bindTexture: target", rawTarget);
    }
    const TexTarget target(rawTarget);

    if (newTex) {
        // silently ignore a deleted texture
        if (newTex->IsDeleted())
            return;

        if (newTex->HasEverBeenBound() && newTex->Target() != rawTarget)
            return ErrorInvalidOperation("bindTexture: this texture has already been bound to a different target");
    }

    *currentTexPtr = newTex;

    MakeContextCurrent();

    if (newTex) {
        SetFakeBlackStatus(WebGLContextFakeBlackStatus::Unknown);
        newTex->Bind(target);
    } else {
        gl->fBindTexture(target.get(), 0);
    }
}

// here we have to support all pnames with both int and float params.
// See this discussion:
//  https://www.khronos.org/webgl/public-mailing-list/archives/1008/msg00014.html
void WebGLContext::TexParameter_base(GLenum rawTexTarget, GLenum pname,
                                     GLint* maybeIntParam, GLfloat* maybeFloatParam)
{
    MOZ_ASSERT(maybeIntParam || maybeFloatParam);

    if (IsContextLost())
        return;

    if (!ValidateTextureTargetEnum(rawTexTarget, "texParameter: target"))
        return;

    const TexTarget texTarget(rawTexTarget);

    WebGLTexture* tex = ActiveBoundTextureForTarget(texTarget);
    if (!tex)
        return ErrorInvalidOperation("texParameter: No texture is bound to this target.");

    tex->TexParameter(texTarget, GLenum pname, maybeIntParam, maybeFloatParam);
}

JS::Value
WebGLContext::GetTexParameter(GLenum rawTarget, GLenum pname)
{
    if (IsContextLost())
        return JS::NullValue();

    MakeContextCurrent();

    if (!ValidateTextureTargetEnum(rawTarget, "getTexParameter: target"))
        return JS::NullValue();

    const TexTarget target(rawTarget);

    if (!ActiveBoundTextureForTarget(target)) {
        ErrorInvalidOperation("getTexParameter: no texture bound");
        return JS::NullValue();
    }

    return GetTexParameterInternal(target, pname);
}

JS::Value
WebGLContext::GetTexParameterInternal(const TexTarget& target, GLenum pname)
{
    switch (pname) {
        case LOCAL_GL_TEXTURE_MIN_FILTER:
        case LOCAL_GL_TEXTURE_MAG_FILTER:
        case LOCAL_GL_TEXTURE_WRAP_S:
        case LOCAL_GL_TEXTURE_WRAP_T:
        {
            GLint i = 0;
            gl->fGetTexParameteriv(target.get(), pname, &i);
            return JS::NumberValue(uint32_t(i));
        }
        case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
            if (IsExtensionEnabled(WebGLExtensionID::EXT_texture_filter_anisotropic)) {
                GLfloat f = 0.f;
                gl->fGetTexParameterfv(target.get(), pname, &f);
                return JS::DoubleValue(f);
            }

            ErrorInvalidEnumInfo("getTexParameter: parameter", pname);
            break;

        default:
            ErrorInvalidEnumInfo("getTexParameter: parameter", pname);
    }

    return JS::NullValue();
}


} // namespace mozilla
