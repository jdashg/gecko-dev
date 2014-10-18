/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextTypes.h"

#include <cstring>
#include "GLContext.h"
#include "mozilla/layers/ISurfaceAllocator.h"

namespace mozilla {
namespace gl {

////////////////////////////////////////
// GLFormats

GLFormats::GLFormats()
{
    std::memset(this, 0, sizeof(GLFormats));
}

/*static*/ GLFormats
GLFormats::Choose(GLContext* gl, const SurfaceCaps& caps)
{
    GLFormats formats;

    // If we're on ES2 hardware and we have an explicit request for 16 bits of
    // color or less OR we don't support full 8-bit color, return a 4444 or 565
    // format.
    bool bpp16 = caps.bpp16;
    if (gl->IsGLES()) {
        if (!gl->IsExtensionSupported(GLContext::OES_rgb8_rgba8))
            bpp16 = true;
    } else {
        // RGB565 is uncommon on desktop, requiring ARB_ES2_compatibility.
        // Since it's also vanishingly useless there, let's not support it.
        bpp16 = false;
    }

    if (bpp16) {
        MOZ_ASSERT(gl->IsGLES());
        if (caps.alpha) {
            formats.color_texInternalFormat = LOCAL_GL_RGBA;
            formats.color_texFormat = LOCAL_GL_RGBA;
            formats.color_texType   = LOCAL_GL_UNSIGNED_SHORT_4_4_4_4;
            formats.color_rbFormat  = LOCAL_GL_RGBA4;
        } else {
            formats.color_texInternalFormat = LOCAL_GL_RGB;
            formats.color_texFormat = LOCAL_GL_RGB;
            formats.color_texType   = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
            formats.color_rbFormat  = LOCAL_GL_RGB565;
        }
    } else {
        formats.color_texType = LOCAL_GL_UNSIGNED_BYTE;

        if (caps.alpha) {
            formats.color_texInternalFormat = gl->IsGLES() ? LOCAL_GL_RGBA
                                                          : LOCAL_GL_RGBA8;
            formats.color_texFormat = LOCAL_GL_RGBA;
            formats.color_rbFormat  = LOCAL_GL_RGBA8;
        } else {
            formats.color_texInternalFormat = gl->IsGLES() ? LOCAL_GL_RGB
                                                          : LOCAL_GL_RGB8;
            formats.color_texFormat = LOCAL_GL_RGB;
            formats.color_rbFormat  = LOCAL_GL_RGB8;
        }
    }

    uint32_t msaaLevel = gfxPrefs::MSAALevel();
    GLsizei samples = msaaLevel * msaaLevel;
    samples = std::min(samples, gl->MaxSamples());

    // Bug 778765.
    if (gl->WorkAroundDriverBugs() && samples == 1) {
        samples = 0;
    }
    formats.samples = samples;


    // Be clear that these are 0 if unavailable.
    formats.depthStencil = 0;
    if (!gl->IsGLES() ||
        gl->IsExtensionSupported(GLContext::OES_packed_depth_stencil))
    {
        formats.depthStencil = LOCAL_GL_DEPTH24_STENCIL8;
    }

    formats.depth = 0;
    if (gl->IsGLES()) {
        if (gl->IsExtensionSupported(GLContext::OES_depth24)) {
            formats.depth = LOCAL_GL_DEPTH_COMPONENT24;
        } else {
            formats.depth = LOCAL_GL_DEPTH_COMPONENT16;
        }
    } else {
        formats.depth = LOCAL_GL_DEPTH_COMPONENT24;
    }

    formats.stencil = LOCAL_GL_STENCIL_INDEX8;

    return formats;
}

////////////////////////////////////////
// PixelBufferFormat

PixelBufferFormat::PixelBufferFormat()
{
    std::memset(this, 0, sizeof(PixelBufferFormat));
}

////////////////////////////////////////
// SurfaceCaps

SurfaceCaps::SurfaceCaps()
{
    Clear();
}

SurfaceCaps::SurfaceCaps(const SurfaceCaps& other)
{
    *this = other;
}

SurfaceCaps&
SurfaceCaps::operator=(const SurfaceCaps& other)
{
    any = other.any;
    color = other.color;
    alpha = other.alpha;
    bpp16 = other.bpp16;
    depth = other.depth;
    stencil = other.stencil;
    antialias = other.antialias;
    premultAlpha = other.premultAlpha;
    preserve = other.preserve;
    surfaceAllocator = other.surfaceAllocator;

    return *this;
}

void
SurfaceCaps::Clear()
{
    any = false;
    color = false;
    alpha = false;
    bpp16 = false;
    depth = false;
    stencil = false;
    antialias = false;
    premultAlpha = true;
    preserve = false;
    surfaceAllocator = nullptr;
}

SurfaceCaps::~SurfaceCaps()
{}

} // namespace gl
} // namespace mozilla
