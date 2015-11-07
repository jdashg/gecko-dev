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
    gl::GLContext* gl = webgl->GL();

    webgl::PackingInfo pi;
    webgl::DriverUnpackInfo dui;
    const GLint* swizzle = nullptr;

    const auto fnAdd = [&fua, &pi, &dui, &swizzle](webgl::EffectiveFormat effFormat) {
        MOZ_ASSERT(!pi.type && !dui.unpackType);
        pi.type = LOCAL_GL_FLOAT;
        dui.unpackType = LOCAL_GL_FLOAT;

        auto usage = fua->EditUsage(effFormat);
        fua->AddUnsizedTexFormat(pi, usage);
        usage->AddUnpack(pi, dui);

        usage->textureSwizzleRGBA = swizzle;
    };

    const bool isCore = gl->IsCoreProfile();

    pi = {LOCAL_GL_RGBA, 0};
    dui = {pi.format, pi.format, pi.type};
    fnAdd(webgl::EffectiveFormat::RGBA32F);

    pi = {LOCAL_GL_RGB, 0};
    dui = {pi.format, pi.format, pi.type};
    fnAdd(webgl::EffectiveFormat::RGB32F);

    pi = {LOCAL_GL_LUMINANCE, 0};
    if (isCore) {
        dui = {LOCAL_GL_R32F, LOCAL_GL_RED, 0};
        swizzle = webgl::FormatUsageInfo::kLuminanceSwizzleRGBA;
    } else {
        dui = {pi.format, pi.format, pi.type};
        swizzle = nullptr;
    }
    fnAdd(webgl::EffectiveFormat::Luminance32F);

    pi = {LOCAL_GL_ALPHA, 0};
    if (isCore) {
        dui = {LOCAL_GL_R32F, LOCAL_GL_RED, 0};
        swizzle = webgl::FormatUsageInfo::kAlphaSwizzleRGBA;
    } else {
        dui = {pi.format, pi.format, pi.type};
        swizzle = nullptr;
    }
    fnAdd(webgl::EffectiveFormat::Alpha32F);

    pi = {LOCAL_GL_LUMINANCE_ALPHA, 0};
    dui = {pi.format, pi.format, pi.type};
    if (isCore) {
        dui = {LOCAL_GL_RG32F, LOCAL_GL_RG, 0};
        swizzle = webgl::FormatUsageInfo::kLumAlphaSwizzleRGBA;
    } else {
        dui = {pi.format, pi.format, pi.type};
        swizzle = nullptr;
    }
    fnAdd(webgl::EffectiveFormat::Luminance32FAlpha32F);
}

WebGLExtensionTextureFloat::~WebGLExtensionTextureFloat()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureFloat, OES_texture_float)

} // namespace mozilla
