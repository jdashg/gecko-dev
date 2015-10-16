/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEX_UNPACK_BLOB_H_
#define TEX_UNPACK_BLOB_H_

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

class TexUnpackBlob
{
public:
    WebGLContext* const mWebGL;
    const GLsizei mWidth;
    const GLsizei mHeight;
    const GLsizei mDepth;
    const bool mHasData;

protected:
    TexUnpackBlob(WebGLContext* webgl, GLsizei width, GLsizei height, GLsizei depth,
                  bool hasData)
        : mWebGL(webgl)
        , mWidth(width)
        , mHeight(height)
        , mDepth(depth)
        , mHasData(hasData)
    { }

public:
    virtual bool ValidateUnpack(WebGLContext* webgl, uint8_t funcDims,
                                const webgl::PackingInfo* packingInfo) = 0;

    // Returns false on internal failure OR GL error. If !*out_glError, then it was an
    // internal error.
    virtual bool TexImage(WebGLTexture* tex, uint8_t funcDims, TexImageTarget target,
                          GLint level, const webgl::TexImageInfo* texImageInfo,
                          GLenum* const out_glError) = 0;
};

class TexUnpackBuffer : public TexUnpackBlob
{
public:
    const size_t mDataSize;
    const void* const mData;

    TexUnpackBuffer(WebGLContext* webgl, GLsizei width, GLsizei height, GLsizei depth,
                    size_t dataSize, const void* data)
        : TexUnpackBlob(webgl, width, height, depth, bool(data))
        , mDataSize(dataSize)
        , mData(data)
    { }

    virtual bool ValidateUnpack(WebGLContext* webgl, uint8_t funcDims,
                                const webgl::PackingInfo* packingInfo) override;

    virtual bool TexImage(WebGLTexture* tex, uint8_t funcDims, TexImageTarget target,
                          GLint level, const webgl::TexImageInfo* texImageInfo,
                          GLenum* const out_glError) override;
};

class TexUnpackImage : public TexUnpackBlob
{
public:
    const RefPtr<mozilla::layers::Image> mImage;
    dom::HTMLMediaElement* const mElem;

    TexUnpackImage(WebGLContext* webgl, const RefPtr<mozilla::layers::Image>& image,
                   dom::HTMLMediaElement* elem);

    virtual bool ValidateUnpack(WebGLContext* webgl, uint8_t funcDims,
                                const webgl::PackingInfo* packingInfo) override
    {
        return true;
    }

    virtual bool TexImage(WebGLTexture* tex, uint8_t funcDims, TexImageTarget target,
                          GLint level, const webgl::TexImageInfo* texImageInfo,
                          GLenum* const out_glError) override;
};

class TexUnpackSrcSurface : public TexUnpackBlob
{
public:
    const RefPtr<gfx::DataSourceSurface> mSurf;

    TexUnpackSourceSurface(WebGLContext* webgl,
                           const RefPtr<gfx::DataSourceSurface>& surf);

    virtual bool ValidateUnpack(WebGLContext* webgl, uint8_t funcDims,
                                const webgl::PackingInfo* packingInfo) override
    {
        return true;
    }

    virtual bool TexImage(WebGLTexture* tex, uint8_t funcDims, TexImageTarget target,
                          GLint level, const webgl::TexImageInfo* texImageInfo,
                          GLenum* const out_glError) override;
};

} // namespace webgl
} // namespace mozilla

#endif // TEX_UNPACK_BLOB_H_
