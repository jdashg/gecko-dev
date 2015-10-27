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
#include "WebGLStrongTypes.h"

namespace mozilla {

class UniqueBuffer;
class WebGLContext;
class WebGLTexture;

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

struct PackingInfo;
struct DriverUnpackInfo;

class TexUnpackBlob
{
public:
    const GLsizei mWidth;
    const GLsizei mHeight;
    const GLsizei mDepth;
    const bool mHasData;

protected:
    TexUnpackBlob(GLsizei width, GLsizei height, GLsizei depth, bool hasData)
        : mWidth(width)
        , mHeight(height)
        , mDepth(depth)
        , mHasData(hasData)
    { }

public:
    virtual bool ValidateUnpack(WebGLContext* webgl, const char* funcName, bool isFunc3D,
                                const webgl::PackingInfo& packingInfo) = 0;

    // Returns false on internal failure OR GL error. If !*out_glError, then it was an
    // internal error.
    virtual bool TexOrSubImage(bool isSubImage, WebGLTexture* tex, TexImageTarget target,
                               GLint level,
                               const webgl::DriverUnpackInfo* driverUnpackInfo,
                               GLint xOffset, GLint yOffset, GLint zOffset,
                               GLenum* const out_glError) = 0;
};

class TexUnpackBuffer : public TexUnpackBlob
{
public:
    const size_t mDataSize;
    const void* const mData;

    TexUnpackBuffer(GLsizei width, GLsizei height, GLsizei depth, size_t dataSize,
                    const void* data)
        : TexUnpackBlob(width, height, depth, bool(data))
        , mDataSize(dataSize)
        , mData(data)
    { }

    virtual bool ValidateUnpack(WebGLContext* webgl, const char* funcName, bool isFunc3D,
                                const webgl::PackingInfo& packingInfo) override;

    virtual bool TexOrSubImage(bool isSubImage, WebGLTexture* tex, TexImageTarget target,
                               GLint level,
                               const webgl::DriverUnpackInfo* driverUnpackInfo,
                               GLint xOffset, GLint yOffset, GLint zOffset,
                               GLenum* const out_glError) override;
};

class TexUnpackImage : public TexUnpackBlob
{
public:
    const RefPtr<mozilla::layers::Image> mImage;
    dom::HTMLMediaElement* const mElem;

    TexUnpackImage(const RefPtr<mozilla::layers::Image>& image,
                   dom::HTMLMediaElement* elem);

    virtual bool ValidateUnpack(WebGLContext* webgl, const char* funcName, bool isFunc3D,
                                const webgl::PackingInfo& packingInfo) override
    {
        return true;
    }

    virtual bool TexOrSubImage(bool isSubImage, WebGLTexture* tex, TexImageTarget target,
                               GLint level,
                               const webgl::DriverUnpackInfo* driverUnpackInfo,
                               GLint xOffset, GLint yOffset, GLint zOffset,
                               GLenum* const out_glError) override;
};

class TexUnpackSrcSurface : public TexUnpackBlob
{
public:
    const RefPtr<gfx::DataSourceSurface> mSurf;

    TexUnpackSrcSurface(const RefPtr<gfx::DataSourceSurface>& surf);

    virtual bool ValidateUnpack(WebGLContext* webgl, const char* funcName, bool isFunc3D,
                                const webgl::PackingInfo& packingInfo) override
    {
        return true;
    }

    virtual bool TexOrSubImage(bool isSubImage, WebGLTexture* tex, TexImageTarget target,
                               GLint level,
                               const webgl::DriverUnpackInfo* driverUnpackInfo,
                               GLint xOffset, GLint yOffset, GLint zOffset,
                               GLenum* const out_glError) override;

protected:
    bool ConvertSurface(WebGLContext* webgl, const webgl::DriverUnpackInfo* dui,
                        UniqueBuffer* const out_convertedBuffer,
                        uint8_t* const out_convertedAlignment,
                        bool* const out_outOfMemory);
    bool UploadDataSurface(bool isSubImage, WebGLContext* webgl, TexImageTarget target,
                           GLint level, const webgl::DriverUnpackInfo* dui, GLint xOffset,
                           GLint yOffset, GLint zOffset, gfx::DataSourceSurface* surf,
                           GLenum* const out_glError);
};

} // namespace webgl
} // namespace mozilla

#endif // TEX_UNPACK_BLOB_H_
