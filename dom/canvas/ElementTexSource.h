/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ELEMENT_TEX_SOURCE_H_
#define ELEMENT_TEX_SOURCE_H_

#include "GLContextTypes.h"
#include "GLDefs.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Point.h"

namespace mozilla {

class WebGLContext;

namespace dom {
class Element;
class HTMLCanvasElement;
} // namespace dom

namespace gfx {
class DataSourceSurface;
} // namespace gfx

namespace gl {
class GLContext;
} // namespace gl

namespace layers {
class Image;
class ImageContainer;
} // namespace layers

namespace webgl {

class ElementTexSource
{
    mozilla::dom::Element* const mElem;
    WebGLContext* const mWebGL;
    RefPtr<mozilla::layers::Image> mImage;
    RefPtr<gfx::DataSourceSurface> mData;

public:
    ElementTexSource(dom::Element* elem, dom::HTMLCanvasElement* canvas,
                     WebGLContext* webgl, ElementTexSource** const out_onlyIfValid,
                     bool* const out_badCORS);

    ~ElementTexSource();

    const gfx::IntSize& Size() const;
    bool BlitToTexture(gl::GLContext* gl, GLuint destTex, GLenum texImageTarget,
                       gl::OriginPos destOrigin) const;
    gfx::DataSourceSurface* ElementTexSource::GetData();
};

} // namespace webgl
} // namespace mozilla

#endif // ELEMENT_TEX_SOURCE_H_
