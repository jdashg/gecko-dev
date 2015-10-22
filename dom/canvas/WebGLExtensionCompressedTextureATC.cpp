/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionCompressedTextureATC::WebGLExtensionCompressedTextureATC(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    auto& authority = webgl->mFormatUsage;

    authority->EditUsage(EffectiveFormat::ATC_RGB_AMD)->asTexture = true;
    authority->EditUsage(EffectiveFormat::ATC_RGBA_EXPLICIT_ALPHA_AMD)->asTexture = true;
    authority->EditUsage(EffectiveFormat::ATC_RGBA_INTERPOLATED_ALPHA_AMD)->asTexture = true;
}

WebGLExtensionCompressedTextureATC::~WebGLExtensionCompressedTextureATC()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTextureATC, WEBGL_compressed_texture_atc)

} // namespace mozilla
