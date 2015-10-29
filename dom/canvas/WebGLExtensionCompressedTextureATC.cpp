/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

#ifdef FOO
#error FOO is already defined! We use FOO() macros to keep things succinct in this file.
#endif

namespace mozilla {

WebGLExtensionCompressedTextureATC::WebGLExtensionCompressedTextureATC(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    auto& fua = webgl->mFormatUsage;

    const auto fnAdd = [&fua](GLenum sizedFormat, webgl::EffectiveFormat effFormat) {
        auto usage = fua->EditUsage(effFormat);
        fua->AddSizedTexFormat(sizedFormat, usage);
    };

#define FOO(x) LOCAL_GL_ ## x, webgl::EffectiveFormat::x

    fnAdd(FOO(ATC_RGB_AMD));
    fnAdd(FOO(ATC_RGBA_EXPLICIT_ALPHA_AMD));
    fnAdd(FOO(ATC_RGBA_INTERPOLATED_ALPHA_AMD));

#undef FOO
}

WebGLExtensionCompressedTextureATC::~WebGLExtensionCompressedTextureATC()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTextureATC, WEBGL_compressed_texture_atc)

} // namespace mozilla
