/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

using mozilla::webgl::EffectiveFormat;a

WebGLExtensionTextureFloat::WebGLExtensionTextureFloat(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    auto& authority = webgl->mFormatUsage;

    authority->EditUsage(EffectiveFormat::RGBA32F)->asTexture = true;
    authority->EditUsage(EffectiveFormat::RGB32F)->asTexture = true;
    authority->EditUsage(EffectiveFormat::Luminance32FAlpha32F)->asTexture = true;
    authority->EditUsage(EffectiveFormat::Luminance32F)->asTexture = true;
    authority->EditUsage(EffectiveFormat::Alpha32F)->asTexture = true;

    PackingInfo pi;
    DriverUnpackInfo dui;

    pi = {LOCAL_GL_RGBA, LOCAL_GL_FLOAT};
    dui = {LOCAL_GL_RGBA, LOCAL_GL_RGBA, LOCAL_GL_FLOAT};
    authority->EditUsage(EffectiveFormat::RGBA32F)->AddUnpack(pi, dui);

    pi = {LOCAL_GL_RGB, LOCAL_GL_FLOAT};
    dui = {LOCAL_GL_RGB, LOCAL_GL_RGB, LOCAL_GL_FLOAT};
    authority->EditUsage(EffectiveFormat::RGB32F)->AddUnpack(pi, dui);

    if (webgl->gl->IsCoreProfile()) {
        pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_FLOAT};
        dui = {LOCAL_GL_R32F, LOCAL_GL_RED, LOCAL_GL_FLOAT};
        authority->EditUsage(EffectiveFormat::Luminance32F)->AddUnpack(pi, dui);

        pi = {LOCAL_GL_ALPHA, LOCAL_GL_FLOAT};
        dui = {LOCAL_GL_R32F, LOCAL_GL_RED, LOCAL_GL_FLOAT};
        authority->EditUsage(EffectiveFormat::Alpha32F)->AddUnpack(pi, dui);

        pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_FLOAT};
        dui = {LOCAL_GL_RG32F, LOCAL_GL_RG, LOCAL_GL_FLOAT};
        authority->EditUsage(EffectiveFormat::Luminance32FAlpha32F)->AddUnpack(pi, dui);
    } else {
        pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_FLOAT};
        dui = {LOCAL_GL_LUMINANCE, LOCAL_GL_LUMINANCE, LOCAL_GL_FLOAT};
        authority->EditUsage(EffectiveFormat::Luminance32F)->AddUnpack(pi, dui);

        pi = {LOCAL_GL_ALPHA, LOCAL_GL_FLOAT};
        dui = {LOCAL_GL_ALPHA, LOCAL_GL_ALPHA, LOCAL_GL_FLOAT};
        authority->EditUsage(EffectiveFormat::Alpha32F)->AddUnpack(pi, dui);

        pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_FLOAT};
        dui = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_FLOAT};
        authority->EditUsage(EffectiveFormat::Luminance32FAlpha32F)->AddUnpack(pi, dui);
    }
}

WebGLExtensionTextureFloat::~WebGLExtensionTextureFloat()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureFloat, OES_texture_float)

} // namespace mozilla
