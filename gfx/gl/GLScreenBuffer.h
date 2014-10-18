/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* GLScreenBuffer is the abstraction for the "default framebuffer" used
 * by an offscreen GLContext. Since it's only for offscreen GLContext's,
 * it's only useful for things like WebGL, and is NOT used by the
 * compositor's GLContext. Remember that GLContext provides an abstraction
 * so that even if you want to draw to the 'screen', even if that's not
 * actually the screen, just draw to 0. This GLScreenBuffer class takes the
 * logic handling out of GLContext.
*/

#ifndef GL_SCREEN_BUFFER_H_
#define GL_SCREEN_BUFFER_H_

#include "GLContextTypes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace gl {

class GLContext;
class ScreenDrawBuffer;
class ScreenReadBuffer;
class ShSurfHandle;
class SurfaceFactory;

class GLScreenBuffer
{
    friend class UniquePtr<GLScreenBuffer>;
public:
    // Fallible.
    static UniquePtr<GLScreenBuffer> Create(GLContext& gl,
                                            const SurfaceCaps& caps,
                                            const gfx::IntSize& size);

private:
    GLContext& mGL;
public:
    const SurfaceCaps mCaps;
private:
    const GLFormats mFormats;

    UniquePtr<SurfaceFactory> mFactory;

    RefPtr<ShSurfHandle> mFront;

    UniquePtr<ScreenDrawBuffer> mDraw;
    UniquePtr<ScreenReadBuffer> mRead;

    bool mNeedsBlit;

    GLScreenBuffer(GLContext& gl, const SurfaceCaps& caps,
                   const GLFormats& formats, UniquePtr<SurfaceFactory> factory,
                   UniquePtr<ScreenDrawBuffer> draw,
                   UniquePtr<ScreenReadBuffer> read);

public:
    virtual ~GLScreenBuffer();

    void OnAfterDraw() {
        mNeedsBlit = true;
    }

    void OnBeforeRead() {
        AssureBlitted();
    }

    GLsizei Samples() const {
        if (!mDraw)
            return 1;

        return mDraw->mSamples;
    }

    const RefPtr<ShSurfHandle>& Back() const;

    const RefPtr<ShSurfHandle>& Front() const {
        return mFront;
    }

    const gfx::IntSize& Size() const;

    SurfaceFactory* Factory() const;
    void SetFactory(UniquePtr<SurfaceFactory> newFactory);

    bool Resize(const gfx::IntSize& size);
    bool Swap(const gfx::IntSize& size);

    GLuint DrawFB() const;
    GLuint ReadFB() const;

private:
    void AssureBlitted();

    bool CreateDraw(const gfx::IntSize& size,
                    UniquePtr<ScreenDrawBuffer>* out_buffer);

    UniquePtr<ScreenReadBuffer> CreateRead(const RefPtr<ShSurfHandle>& surfHandle);

    bool Attach(const RefPtr<ShSurfHandle>& surfHandle);
    bool Attach_internal(const RefPtr<ShSurfHandle>& surfHandle);
};

}   // namespace gl
}   // namespace mozilla

#endif  // GL_SCREEN_BUFFER_H_
