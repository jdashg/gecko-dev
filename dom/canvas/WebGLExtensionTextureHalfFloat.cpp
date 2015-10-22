/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

using mozilla::webgl::EffectiveFormat;

WebGLExtensionTextureHalfFloat::WebGLExtensionTextureHalfFloat(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    auto& authority = webgl->mFormatUsage;

    authority->EditUsage(EffectiveFormat::RGBA16F)->asTexture = true;
    authority->EditUsage(EffectiveFormat::RGB16F)->asTexture = true;
    authority->EditUsage(EffectiveFormat::Luminance16FAlpha16F)->asTexture = true;
    authority->EditUsage(EffectiveFormat::Luminance16F)->asTexture = true;
    authority->EditUsage(EffectiveFormat::Alpha16F)->asTexture = true;

    GLenum driverUnpackFormat = LOCAL_GL_HALF_FLOAT;
    if (!webgl->gl->IsSupported(gl::GLFeature::texture_half_float)) {
        MOZ_ASSERT(webgl->gl->IsExtensionSupported(gl::GLContext::OES_texture_half_float));
        driverUnpackFormat = LOCAL_GL_HALF_FLOAT_OES;
    }

    PackingInfo pi;
    DriverUnpackInfo dui;

    pi = {LOCAL_GL_RGBA, LOCAL_GL_HALF_FLOAT_OES};
    dui = {LOCAL_GL_RGBA, LOCAL_GL_RGBA, driverUnpackFormat};
    authority->EditUsage(EffectiveFormat::RGBA16F)->AddUnpack(pi, dui);

    pi = {LOCAL_GL_RGB, LOCAL_GL_HALF_FLOAT_OES};
    dui = {LOCAL_GL_RGB, LOCAL_GL_RGB, driverUnpackFormat};
    authority->EditUsage(EffectiveFormat::RGB16F)->AddUnpack(pi, dui);

    if (webgl->gl->IsCoreProfile()) {
        pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_HALF_FLOAT_OES};
        dui = {LOCAL_GL_R16F, LOCAL_GL_RED, driverUnpackFormat};
        authority->EditUsage(EffectiveFormat::Luminance16F)->AddUnpack(pi, dui);

        pi = {LOCAL_GL_ALPHA, LOCAL_GL_HALF_FLOAT_OES};
        dui = {LOCAL_GL_R16F, LOCAL_GL_RED, driverUnpackFormat};
        authority->EditUsage(EffectiveFormat::Alpha16F)->AddUnpack(pi, dui);

        pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_HALF_FLOAT_OES};
        dui = {LOCAL_GL_RG16F, LOCAL_GL_RG, driverUnpackFormat};
        authority->EditUsage(EffectiveFormat::Luminance16FAlpha16F)->AddUnpack(pi, dui);
    } else {
        pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_HALF_FLOAT_OES};
        dui = {LOCAL_GL_LUMINANCE, LOCAL_GL_LUMINANCE, driverUnpackFormat};
        authority->EditUsage(EffectiveFormat::Luminance16F)->AddUnpack(pi, dui);

        pi = {LOCAL_GL_ALPHA, LOCAL_GL_HALF_FLOAT_OES};
        dui = {LOCAL_GL_ALPHA, LOCAL_GL_ALPHA, driverUnpackFormat};
        authority->EditUsage(EffectiveFormat::Alpha16F)->AddUnpack(pi, dui);

        pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_HALF_FLOAT};
        dui = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_LUMINANCE_ALPHA, driverUnpackFormat};
        authority->EditUsage(EffectiveFormat::Luminance16FAlpha16F)->AddUnpack(pi, dui);
    }
}

WebGLExtensionTextureHalfFloat::~WebGLExtensionTextureHalfFloat()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureHalfFloat, OES_texture_half_float)

} // namespace mozilla
