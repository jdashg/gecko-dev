/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXT_TYPES_H_
#define GLCONTEXT_TYPES_H_

#include "GLTypes.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace layers {
class ISurfaceAllocator;
}
namespace gl {
class GLContext;
struct SurfaceCaps;

enum class GLContextType {
    Unknown,
    WGL,
    CGL,
    GLX,
    EGL
};

enum class OriginPos : uint8_t {
  TopLeft,
  BottomLeft
};

struct GLFormats MOZ_FINAL
{
    // Constructs a zeroed object:
    GLFormats();

    GLenum color_texInternalFormat;
    GLenum color_texFormat;
    GLenum color_texType;
    GLenum color_rbFormat;

    GLenum depthStencil;
    GLenum depth;
    GLenum stencil;

    GLsizei samples;

    static GLFormats Choose(GLContext* gl, const SurfaceCaps& caps);
};

struct PixelBufferFormat MOZ_FINAL
{
    // Constructs a zeroed object:
    PixelBufferFormat();

    int red, green, blue;
    int alpha;
    int depth, stencil;
    int samples;

    int ColorBits() const { return red + green + blue; }
};

struct SurfaceCaps MOZ_FINAL
{
    bool any;
    bool color, alpha;
    bool bpp16;
    bool depth, stencil;
    bool antialias;
    bool premultAlpha;
    bool preserve;

    // The surface allocator that we want to create this
    // for.  May be null.
    RefPtr<layers::ISurfaceAllocator> surfaceAllocator;

    SurfaceCaps();
    SurfaceCaps(const SurfaceCaps& other);
    ~SurfaceCaps();

    void Clear();

    SurfaceCaps& operator=(const SurfaceCaps& other);

    // We can't use just 'RGB' here, since it's an ancient Windows macro.
    static SurfaceCaps ForRGB() {
        SurfaceCaps caps;

        caps.color = true;

        return caps;
    }

    static SurfaceCaps ForRGBA() {
        SurfaceCaps caps;

        caps.color = true;
        caps.alpha = true;

        return caps;
    }

    static SurfaceCaps Any() {
        SurfaceCaps caps;

        caps.any = true;

        return caps;
    }
};

} // namespace gl
} // namespace mozilla

#endif // GLCONTEXT_TYPES_H_
