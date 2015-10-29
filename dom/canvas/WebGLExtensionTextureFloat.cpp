/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionTextureFloat::WebGLExtensionTextureFloat(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    auto& fua = webgl->mFormatUsage;

    webgl::PackingInfo pi;
    webgl::DriverUnpackInfo dui;

    const auto fnAdd = [&fua, &pi, &dui](webgl::EffectiveFormat effFormat) {
        auto usage = fua->EditUsage(effFormat);
        fua->AddUnsizedTexFormat(pi, usage);
        usage->AddUnpack(pi, dui);
    };

    const bool isCore = webgl->GL()->IsCoreProfile();

    pi = {LOCAL_GL_RGBA, LOCAL_GL_FLOAT};
    dui = {LOCAL_GL_RGBA, LOCAL_GL_RGBA, LOCAL_GL_FLOAT};
    fnAdd(webgl::EffectiveFormat::RGBA32F);

    pi = {LOCAL_GL_RGB, LOCAL_GL_FLOAT};
    dui = {LOCAL_GL_RGB, LOCAL_GL_RGB, LOCAL_GL_FLOAT};
    fnAdd(webgl::EffectiveFormat::RGB32F);

    pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_FLOAT};
    if (isCore) dui = {LOCAL_GL_R32F, LOCAL_GL_RED, LOCAL_GL_FLOAT};
    else        dui = {LOCAL_GL_LUMINANCE, LOCAL_GL_LUMINANCE, LOCAL_GL_FLOAT};
    fnAdd(webgl::EffectiveFormat::Luminance32F);

    pi = {LOCAL_GL_ALPHA, LOCAL_GL_FLOAT};
    if (isCore) dui = {LOCAL_GL_R32F, LOCAL_GL_RED, LOCAL_GL_FLOAT};
    else        dui = {LOCAL_GL_ALPHA, LOCAL_GL_ALPHA, LOCAL_GL_FLOAT};
    fnAdd(webgl::EffectiveFormat::Alpha32F);

    pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_FLOAT};
    if (isCore) dui = {LOCAL_GL_RG32F, LOCAL_GL_RG, LOCAL_GL_FLOAT};
    else        dui = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_FLOAT};
    fnAdd(webgl::EffectiveFormat::Luminance32FAlpha32F);
}

WebGLExtensionTextureFloat::~WebGLExtensionTextureFloat()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureFloat, OES_texture_float)

} // namespace mozilla
