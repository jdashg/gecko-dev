/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionTextureHalfFloat::WebGLExtensionTextureHalfFloat(WebGLContext* webgl)
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

    GLenum driverUnpackType = LOCAL_GL_HALF_FLOAT;
    if (!webgl->GL()->IsSupported(gl::GLFeature::texture_half_float)) {
        MOZ_ASSERT(webgl->GL()->IsExtensionSupported(gl::GLContext::OES_texture_half_float));
        driverUnpackType = LOCAL_GL_HALF_FLOAT_OES;
    }

    const bool isCore = webgl->GL()->IsCoreProfile();

    pi = {LOCAL_GL_RGBA, LOCAL_GL_HALF_FLOAT_OES};
    dui = {LOCAL_GL_RGBA, LOCAL_GL_RGBA, driverUnpackType};
    fnAdd(webgl::EffectiveFormat::RGBA16F);

    pi = {LOCAL_GL_RGB, LOCAL_GL_HALF_FLOAT_OES};
    dui = {LOCAL_GL_RGB, LOCAL_GL_RGB, driverUnpackType};
    fnAdd(webgl::EffectiveFormat::RGB16F);

    pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_HALF_FLOAT_OES};
    if (isCore) dui = {LOCAL_GL_R16F, LOCAL_GL_RED, driverUnpackType};
    else        dui = {LOCAL_GL_LUMINANCE, LOCAL_GL_LUMINANCE, driverUnpackType};
    fnAdd(webgl::EffectiveFormat::Luminance16F);

    pi = {LOCAL_GL_ALPHA, LOCAL_GL_HALF_FLOAT_OES};
    if (isCore) dui = {LOCAL_GL_R16F, LOCAL_GL_RED, driverUnpackType};
    else        dui = {LOCAL_GL_ALPHA, LOCAL_GL_ALPHA, driverUnpackType};
    fnAdd(webgl::EffectiveFormat::Alpha16F);

    pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_HALF_FLOAT_OES};
    if (isCore) dui = {LOCAL_GL_RG16F, LOCAL_GL_RG, driverUnpackType};
    else        dui = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_LUMINANCE_ALPHA, driverUnpackType};
    fnAdd(webgl::EffectiveFormat::Luminance16FAlpha16F);
}

WebGLExtensionTextureHalfFloat::~WebGLExtensionTextureHalfFloat()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureHalfFloat, OES_texture_half_float)

} // namespace mozilla
