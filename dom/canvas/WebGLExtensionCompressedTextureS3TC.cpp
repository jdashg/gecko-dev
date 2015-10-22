/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionCompressedTextureS3TC::WebGLExtensionCompressedTextureS3TC(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    auto& authority = webgl->mFormatUsage;

    authority->EditUsage(EffectiveFormat::COMPRESSED_RGB_S3TC_DXT1)->asTexture = true;
    authority->EditUsage(EffectiveFormat::COMPRESSED_RGBA_S3TC_DXT1)->asTexture = true;
    authority->EditUsage(EffectiveFormat::COMPRESSED_RGBA_S3TC_DXT3)->asTexture = true;
    authority->EditUsage(EffectiveFormat::COMPRESSED_RGBA_S3TC_DXT5)->asTexture = true;
}

WebGLExtensionCompressedTextureS3TC::~WebGLExtensionCompressedTextureS3TC()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTextureS3TC, WEBGL_compressed_texture_s3tc)

} // namespace mozilla
