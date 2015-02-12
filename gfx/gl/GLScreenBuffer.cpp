/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLScreenBuffer.h"

#include <cstring>
#include "CompositorTypes.h"
#include "gfx2DGlue.h"
#include "GLContext.h"
#include "ScopedGLHelpers.h"
#include "SharedSurfaceGL.h"

#ifdef MOZ_WIDGET_GONK
#include "nsXULAppAPI.h"
#include "SharedSurfaceGralloc.h"
#include "../layers/ipc/ShadowLayers.h"
#endif

namespace mozilla {
namespace gl {

class ScreenDrawBuffer
{
public:
    // Fallible.
    static UniquePtr<ScreenDrawBuffer> Create(GLContext& gl,
                                              const SurfaceCaps& caps,
                                              const GLFormats& formats,
                                              const gfx::IntSize& size);

private:
    GLContext& mGL;
public:
    const SurfaceCaps mCaps;
    const gfx::IntSize mSize;
    const GLsizei mSamples;
    const GLuint mFB;
private:
    const GLuint mColorMSRB;
    const GLuint mDepthRB;
    const GLuint mStencilRB;

    ScreenDrawBuffer(GLContext& gl,
                     const SurfaceCaps& caps,
                     const gfx::IntSize& size,
                     GLsizei samples,
                     GLuint fb,
                     GLuint colorMSRB,
                     GLuint depthRB,
                     GLuint stencilRB)
        : mGL(gl)
        , mCaps(caps)
        , mSize(size)
        , mSamples(samples)
        , mFB(fb)
        , mColorMSRB(colorMSRB)
        , mDepthRB(depthRB)
        , mStencilRB(stencilRB)
    {}

public:
    ~ScreenDrawBuffer() {
        if (!mGL.MakeCurrent())
            return;

        GLuint fb = mFB;
        GLuint rbs[] = {
            mColorMSRB,
            mDepthRB,
            mStencilRB
        };

        mGL.fDeleteFramebuffers(1, &fb);
        mGL.fDeleteRenderbuffers(3, rbs);
    }
};

class ScreenReadBuffer
{
public:
    // Fallible.
    static UniquePtr<ScreenReadBuffer> Create(GLContext& gl,
                                              const SurfaceCaps& caps,
                                              const GLFormats& formats,
                                              const RefPtr<ShSurfHandle>& surfHandle);

private:
    GLContext& mGL;
public:
    const SurfaceCaps mCaps;
    const GLuint mFB;
private:
    // mFB has the following attachments:
    const GLuint mDepthRB;
    const GLuint mStencilRB;
    // note no mColorRB here: this is provided by mSurf.
    RefPtr<ShSurfHandle> mSurfHandle;

    ScreenReadBuffer(GLContext& gl,
                     const SurfaceCaps& caps,
                     GLuint fb,
                     GLuint depthRB,
                     GLuint stencilRB,
                     const RefPtr<ShSurfHandle>& surfHandle)
        : mGL(gl)
        , mCaps(caps)
        , mFB(fb)
        , mDepthRB(depthRB)
        , mStencilRB(stencilRB)
        , mSurfHandle(surfHandle)
    {}

public:
    ~ScreenReadBuffer() {
        if (!mGL.MakeCurrent())
            return;

        GLuint fb = mFB;
        GLuint rbs[] = {
            mDepthRB,
            mStencilRB
        };

        mGL.fDeleteFramebuffers(1, &fb);
        mGL.fDeleteRenderbuffers(2, rbs);
    }

    // Cannot attach a surf of a different AttachType or Size than before.
    void Attach(const RefPtr<ShSurfHandle>& surfHandle);

    const gfx::IntSize& Size() const {
        return mSurfHandle->Surf()->mSize;
    }

    const RefPtr<ShSurfHandle>& SurfHandle() const {
        return mSurfHandle;
    }
};

////////////////////////////////////////////////////////////////////////////////
// GLScreenBuffer

UniquePtr<GLScreenBuffer>
GLScreenBuffer::Create(GLContext& gl,
                       const SurfaceCaps& caps,
                       const gfx::IntSize& size)
{
    UniquePtr<GLScreenBuffer> ret;

    const GLFormats formats = GLFormats::Choose(&gl, caps);

    UniquePtr<ScreenDrawBuffer> draw;
    SurfaceCaps readCaps = caps;

    const bool needsDraw = caps.antialias;
    if (needsDraw) {
        MOZ_ASSERT(formats.samples > 1);

        readCaps.antialias = false;
        readCaps.depth = false;
        readCaps.stencil = false;

        draw = ScreenDrawBuffer::Create(gl, caps, formats, size);

        if (!draw)
            return Move(ret);
    }

    UniquePtr<SurfaceFactory> factory;

#ifdef MOZ_WIDGET_GONK
    // On B2G, we want a Gralloc factory, and we want one right at the start.
    layers::ISurfaceAllocator* allocator = caps.surfaceAllocator;
    if (!factory &&
        allocator &&
        XRE_GetProcessType() != GeckoProcessType_Default)
    {
        layers::TextureFlags flags = layers::TextureFlags::DEALLOCATE_CLIENT |
                                     layers::TextureFlags::ORIGIN_BOTTOM_LEFT;
        if (!caps.premultAlpha) {
            flags |= layers::TextureFlags::NON_PREMULTIPLIED;
        }

        factory = MakeUnique<SurfaceFactory_Gralloc>(&gl, readCaps, flags,
                                                     allocator);
    }
#endif

    if (!factory) {
        factory = MakeUnique<SurfaceFactory_Basic>(&gl, readCaps);
    }
    MOZ_ASSERT(factory);

    RefPtr<ShSurfHandle> surfHandle = factory->NewShSurfHandle(size);
    MOZ_ASSERT(surfHandle->Surf());

    surfHandle->Surf()->ProducerAcquire();
    surfHandle->Surf()->LockProd();

    UniquePtr<ScreenReadBuffer> read = ScreenReadBuffer::Create(gl, readCaps,
                                                                formats,
                                                                surfHandle);
    if (!read) {
        surfHandle->Surf()->UnlockProd();
        return Move(ret);
    }

    ret.reset(new GLScreenBuffer(gl, caps, formats, Move(factory), Move(draw),
                                 Move(read)));
    return Move(ret);
}


GLScreenBuffer::GLScreenBuffer(GLContext& gl, const SurfaceCaps& caps,
                               const GLFormats& formats,
                               UniquePtr<SurfaceFactory> factory,
                               UniquePtr<ScreenDrawBuffer> draw,
                               UniquePtr<ScreenReadBuffer> read)
    : mGL(gl)
    , mCaps(caps)
    , mFormats(formats)
    , mFactory(Move(factory))
    , mDraw(Move(draw))
    , mRead(Move(read))
    , mNeedsBlit(true)
{
}

GLScreenBuffer::~GLScreenBuffer()
{
    mDraw = nullptr;
    mRead = nullptr;

    // bug 914823: it is crucial to destroy the Factory _after_ we destroy
    // the SharedSurfaces around here! Reason: the shared surfaces will want
    // to ask the Allocator (e.g. the ClientLayerManager) to destroy their
    // buffers, but that Allocator may be kept alive by the Factory,
    // as it currently the case in SurfaceFactory_Gralloc holding a nsRefPtr
    // to the Allocator!
    mFactory = nullptr;
}

void
GLScreenBuffer::AssureBlitted()
{
    if (!mNeedsBlit)
        return;

    if (mDraw) {
        GLuint srcFB = mDraw->mFB;
        GLuint destFB = mRead->mFB;

        MOZ_ASSERT(srcFB != 0);
        MOZ_ASSERT(srcFB != destFB);
        MOZ_ASSERT(mGL.IsSupported(GLFeature::framebuffer_blit));

        ScopedBindFramebuffer boundFB(&mGL);
        ScopedGLState scissor(&mGL, LOCAL_GL_SCISSOR_TEST, false);

        mGL.fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, srcFB);
        mGL.fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER, destFB);

        const gfx::IntSize& srcSize = mDraw->mSize;
        const gfx::IntSize& destSize = mRead->Size();
        MOZ_ASSERT(srcSize == destSize);

        mGL.fBlitFramebuffer(0, 0,  srcSize.width,  srcSize.height,
                             0, 0, destSize.width, destSize.height,
                             LOCAL_GL_COLOR_BUFFER_BIT, LOCAL_GL_NEAREST);
    }

    mNeedsBlit = false;
}

SurfaceFactory*
GLScreenBuffer::Factory() const
{
    return mFactory.get();
}

void
GLScreenBuffer::SetFactory(UniquePtr<SurfaceFactory> factory)
{
    MOZ_ASSERT(factory);
    mFactory = Move(factory);
}

bool
GLScreenBuffer::CreateDraw(const gfx::IntSize& size,
                           UniquePtr<ScreenDrawBuffer>* out)
{
    if (!mDraw) {
        *out = nullptr;
        return true;
    }

    const SurfaceCaps& caps = mDraw->mCaps;
    *out = ScreenDrawBuffer::Create(mGL, caps, mFormats, size);
    if (!*out)
        return false;

    return true;
}

UniquePtr<ScreenReadBuffer>
GLScreenBuffer::CreateRead(const RefPtr<ShSurfHandle>& surfHandle)
{
    const SurfaceCaps& caps = mRead->mCaps;
    return ScreenReadBuffer::Create(mGL, caps, mFormats, surfHandle);
}

bool
GLScreenBuffer::Attach_internal(const RefPtr<ShSurfHandle>& surfHandle)
{
    MOZ_ASSERT(surfHandle->Surf());

    MOZ_ASSERT(mRead->SurfHandle()->Surf());

    SharedSurface& curSurf = *mRead->SurfHandle()->Surf();
    SharedSurface& newSurf = *surfHandle->Surf();

    curSurf.UnlockProd();
    curSurf.ProducerRelease();
    newSurf.ProducerAcquire();
    newSurf.LockProd();

    if (newSurf.mAttachType == curSurf.mAttachType &&
        newSurf.mSize == curSurf.mSize)
    {
        // Same size, same type, ready for reuse!
        mRead->Attach(surfHandle);
        return true;
    }

    // Something is different, so resize.
    UniquePtr<ScreenDrawBuffer> newDraw;
    bool drawOk = CreateDraw(newSurf.mSize, &newDraw);  // Can be null.

    UniquePtr<ScreenReadBuffer> newRead = CreateRead(surfHandle);
    bool readOk = !!newRead;

    if (!drawOk || !readOk) {
        newSurf.UnlockProd();
        newSurf.ProducerRelease();
        curSurf.ProducerAcquire();
        curSurf.LockProd();

        return false;
    }

    mDraw = Move(newDraw);
    mRead = Move(newRead);

    return true;
}

bool
GLScreenBuffer::Attach(const RefPtr<ShSurfHandle>& surfHandle)
{
    GLuint combined = 0;
    GLuint draw = 0;
    GLuint read = 0;

    GLuint* pCombined = nullptr;
    GLuint* pDraw = nullptr;
    GLuint* pRead = nullptr;

    const bool isSplit = mGL.IsSupported(GLFeature::framebuffer_blit);
    if (isSplit) {
        mGL.fGetIntegerv(LOCAL_GL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&draw);
        if (draw == DrawFB())
            pDraw = &draw;

        mGL.fGetIntegerv(LOCAL_GL_READ_FRAMEBUFFER_BINDING, (GLint*)&read);
        if (read == ReadFB())
            pRead = &read;
    } else {
        MOZ_ASSERT(DrawFB() == ReadFB());

        mGL.fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, (GLint*)&combined);
        if (combined == ReadFB())
            pCombined = &combined;
    }

    const bool ret = Attach_internal(surfHandle);

    if (pCombined)
        mGL.fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, ReadFB());
    if (pDraw)
        mGL.fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER, DrawFB());
    if (pRead)
        mGL.fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, ReadFB());

    return ret;
}

const RefPtr<ShSurfHandle>&
GLScreenBuffer::Back() const
{
    return mRead->SurfHandle();
}

GLsizei
GLScreenBuffer::Samples() const
{
    if (!mDraw)
        return 1;

    return mDraw->mSamples;
}

const gfx::IntSize&
GLScreenBuffer::Size() const
{
    MOZ_ASSERT(mRead);
    MOZ_ASSERT_IF(mDraw, mDraw->mSize == mRead->Size());
    return mRead->Size();
}

// Resize tries to scrap the old backbuffer and replace it with a new one.
bool
GLScreenBuffer::Resize(const gfx::IntSize& size)
{
    RefPtr<ShSurfHandle> newBack = mFactory->NewShSurfHandle(size);
    if (!newBack)
        return false;

    if (!Attach(newBack))
        return false;

    mNeedsBlit = true;

    return true;
}

// Swap tries to create a new backbuffer, and promote the old backbuffer to
// front.
bool
GLScreenBuffer::Swap(const gfx::IntSize& size)
{
    AssureBlitted();

    RefPtr<ShSurfHandle> newBack = mFactory->NewShSurfHandle(size);
    if (!newBack)
        return false;

    RefPtr<ShSurfHandle> oldBack = Back();
    if (!Attach(newBack))
        return false;
    // Attach was successful.

    mNeedsBlit = true;

    mFront = oldBack;

    if (mCaps.preserve &&
        mFront)
    {
        const auto& src  = mFront->Surf();
        const auto& dest = Back()->Surf();
        SharedSurface::ProdCopy(src, dest, mCaps);
    }

    return true;
}

GLuint
GLScreenBuffer::DrawFB() const
{
    if (!mDraw)
        return ReadFB();

    return mDraw->mFB;
}

GLuint
GLScreenBuffer::ReadFB() const
{
    return mRead->mFB;
}

////////////////////////////////////////////////////////////////////////////////
// Internals

static GLuint
CreateRenderbuffer(GLContext& gl, GLenum internalFormat, GLsizei samples,
                   const gfx::IntSize& size)
{
    MOZ_ASSERT_IF(samples, gl.IsSupported(GLFeature::framebuffer_multisample));

    GLuint rb = 0;
    gl.fGenRenderbuffers(1, &rb);
    ScopedBindRenderbuffer autoRB(&gl, rb);

    if (samples) {
        gl.fRenderbufferStorageMultisample(LOCAL_GL_RENDERBUFFER,
                                           samples,
                                           internalFormat,
                                           size.width, size.height);
    } else {
        gl.fRenderbufferStorage(LOCAL_GL_RENDERBUFFER,
                                internalFormat,
                                size.width, size.height);
    }

    return rb;
}

static void
CreateRenderbuffersForOffscreen(GLContext& gl, const GLFormats& formats,
                                const gfx::IntSize& size, bool isMultisampled,
                                GLuint* pColorMSRB, GLuint* pDepthRB,
                                GLuint* pStencilRB)
{
    GLsizei samples = isMultisampled ? formats.samples : 0;
    if (pColorMSRB) {
        MOZ_ASSERT(formats.samples > 0);
        MOZ_ASSERT(formats.color_rbFormat);

        *pColorMSRB = CreateRenderbuffer(gl, formats.color_rbFormat, samples,
                                         size);
    }

    if (pDepthRB &&
        pStencilRB &&
        formats.depthStencil)
    {
        *pDepthRB = CreateRenderbuffer(gl, formats.depthStencil, samples, size);
        *pStencilRB = *pDepthRB;
    } else {
        if (pDepthRB) {
            MOZ_ASSERT(formats.depth);

            *pDepthRB = CreateRenderbuffer(gl, formats.depth, samples, size);
        }

        if (pStencilRB) {
            MOZ_ASSERT(formats.stencil);

            *pStencilRB = CreateRenderbuffer(gl, formats.stencil, samples,
                                             size);
        }
    }
}

static void
AttachBuffersToFB(GLContext& gl, GLuint colorTex, GLuint colorRB,
                  GLuint depthRB, GLuint stencilRB, GLenum texTarget)
{
    if (colorTex) {
        MOZ_ASSERT(!colorRB);

        MOZ_ASSERT(gl.fIsTexture(colorTex));
        MOZ_ASSERT(texTarget == LOCAL_GL_TEXTURE_2D ||
                   texTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB);
        gl.fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_COLOR_ATTACHMENT0, texTarget,
                                 colorTex, 0);
    } else if (colorRB) {
        MOZ_ASSERT(gl.fIsRenderbuffer(colorRB));
        gl.fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                    LOCAL_GL_COLOR_ATTACHMENT0,
                                    LOCAL_GL_RENDERBUFFER, colorRB);
    } else {
        MOZ_ASSERT(false, "No color buffers?");
    }

    if (depthRB) {
        MOZ_ASSERT(gl.fIsRenderbuffer(depthRB));
       gl. fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                    LOCAL_GL_DEPTH_ATTACHMENT,
                                    LOCAL_GL_RENDERBUFFER, depthRB);
    }

    if (stencilRB) {
        MOZ_ASSERT(gl.fIsRenderbuffer(stencilRB));
        gl.fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                    LOCAL_GL_STENCIL_ATTACHMENT,
                                    LOCAL_GL_RENDERBUFFER, stencilRB);
    }
}

static bool
IsFramebufferComplete(GLContext& gl)
{
    GLenum status = gl.fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    return status == LOCAL_GL_FRAMEBUFFER_COMPLETE;
}

////////////////////////////////////////
// ScreenDrawBuffer

UniquePtr<ScreenDrawBuffer>
ScreenDrawBuffer::Create(GLContext& gl,
                         const SurfaceCaps& caps,
                         const GLFormats& formats,
                         const gfx::IntSize& size)
{
    MOZ_ASSERT(caps.color);
    MOZ_ASSERT(caps.antialias);
    MOZ_ASSERT(formats.samples > 1);

    UniquePtr<ScreenDrawBuffer> ret;

    if (!gl.IsSupported(GLFeature::framebuffer_multisample))
        return Move(ret);

    GLuint colorMSRB = 0;
    GLuint depthRB   = 0;
    GLuint stencilRB = 0;

    GLuint* pColorMSRB = caps.antialias ? &colorMSRB : nullptr;
    GLuint* pDepthRB   = caps.depth     ? &depthRB   : nullptr;
    GLuint* pStencilRB = caps.stencil   ? &stencilRB : nullptr;

    bool isMissingFormats = false;

    if (!formats.color_rbFormat)
        isMissingFormats = true;

    if (pDepthRB && pStencilRB) {
        if (!formats.depth && !formats.depthStencil)
            isMissingFormats = true;

        if (!formats.stencil && !formats.depthStencil)
            isMissingFormats = true;
    } else {
        if (!formats.depth)
            isMissingFormats = true;

        if (!formats.stencil)
            isMissingFormats = true;
    }

    if (isMissingFormats)
        return Move(ret);

    GLContext::LocalErrorScope localError(gl);

    CreateRenderbuffersForOffscreen(gl, formats, size, caps.antialias,
                                    pColorMSRB, pDepthRB, pStencilRB);

    GLuint fb = 0;
    gl.fGenFramebuffers(1, &fb);
    ScopedBindFramebuffer scopedFB(&gl, fb);
    AttachBuffersToFB(gl, 0, colorMSRB, depthRB, stencilRB, 0);

    GLsizei samples = formats.samples;
    if (!samples)
        samples = 1;

    ret.reset(new ScreenDrawBuffer(gl, caps, size, samples, fb, colorMSRB, depthRB,
                                   stencilRB));

    GLenum err = localError.GetError();
    MOZ_ASSERT_IF(err != LOCAL_GL_NO_ERROR, err == LOCAL_GL_OUT_OF_MEMORY);
    if (err || !IsFramebufferComplete(gl))
        ret = nullptr;

    return Move(ret);
}

////////////////////////////////////////
// ScreenReadBuffer

UniquePtr<ScreenReadBuffer>
ScreenReadBuffer::Create(GLContext& gl,
                         const SurfaceCaps& caps,
                         const GLFormats& formats,
                         const RefPtr<ShSurfHandle>& surfHandle)
{
    MOZ_ASSERT(caps.color);
    MOZ_ASSERT(!caps.antialias);
    MOZ_ASSERT(surfHandle);

    UniquePtr<ScreenReadBuffer> ret;

    SharedSurface& surf = *surfHandle->Surf();

    if (surf.mAttachType == AttachmentType::Screen) {
        // Don't need anything. Our read buffer will be the 'screen'.
        ret.reset(new ScreenReadBuffer(gl, caps, 0, 0, 0, surfHandle));
        return Move(ret);
    }

    GLuint colorTex = 0;
    GLuint colorRB = 0;
    GLenum target = 0;

    switch (surf.mAttachType) {
    case AttachmentType::GLTexture:
        colorTex = surf.ProdTexture();
        target = surf.ProdTextureTarget();
        break;
    case AttachmentType::GLRenderbuffer:
        colorRB = surf.ProdRenderbuffer();
        break;
    default:
        MOZ_CRASH("Unknown attachment type?");
    }
    MOZ_ASSERT(colorTex || colorRB);

    GLuint depthRB = 0;
    GLuint stencilRB = 0;

    GLuint* pDepthRB   = caps.depth   ? &depthRB   : nullptr;
    GLuint* pStencilRB = caps.stencil ? &stencilRB : nullptr;

    bool isMissingFormats = false;
    if (pDepthRB && pStencilRB) {
        if (!formats.depth && !formats.depthStencil)
            isMissingFormats = true;

        if (!formats.stencil && !formats.depthStencil)
            isMissingFormats = true;
    } else {
        if (!formats.depth)
            isMissingFormats = true;

        if (!formats.stencil)
            isMissingFormats = true;
    }

    if (isMissingFormats)
        return Move(ret);

    GLContext::LocalErrorScope localError(gl);

    CreateRenderbuffersForOffscreen(gl, formats, surf.mSize, false, nullptr,
                                    pDepthRB, pStencilRB);

    GLuint fb = 0;
    gl.fGenFramebuffers(1, &fb);
    ScopedBindFramebuffer scopedFB(&gl, fb);
    AttachBuffersToFB(gl, colorTex, colorRB, depthRB, stencilRB, target);

    ret.reset(new ScreenReadBuffer(gl, caps, fb, depthRB, stencilRB,
                                   surfHandle));

    GLenum err = localError.GetError();
    MOZ_ASSERT_IF(err != LOCAL_GL_NO_ERROR, err == LOCAL_GL_OUT_OF_MEMORY);
    if (err || !IsFramebufferComplete(gl))
        ret = nullptr;

    return Move(ret);
}

void
ScreenReadBuffer::Attach(const RefPtr<ShSurfHandle>& surfHandle)
{
    SharedSurface& newSurf = *surfHandle->Surf();

    MOZ_ASSERT(newSurf.mAttachType == mSurfHandle->Surf()->mAttachType);
    MOZ_ASSERT(newSurf.mSize == mSurfHandle->Surf()->mSize);

    // Nothing else is needed for AttachType Screen.
    if (newSurf.mAttachType != AttachmentType::Screen) {
        GLuint colorTex = 0;
        GLuint colorRB = 0;
        GLenum target = 0;

        switch (newSurf.mAttachType) {
        case AttachmentType::GLTexture:
            colorTex = newSurf.ProdTexture();
            target = newSurf.ProdTextureTarget();
            break;
        case AttachmentType::GLRenderbuffer:
            colorRB = newSurf.ProdRenderbuffer();
            break;
        default:
            MOZ_CRASH("Unknown attachment type?");
        }

        ScopedBindFramebuffer scopedFB(&mGL, mFB);

        AttachBuffersToFB(mGL, colorTex, colorRB, 0, 0, target);
        MOZ_ASSERT(IsFramebufferComplete(mGL));
    }

    mSurfHandle = surfHandle;
}

} // namespace gl
} // namespace mozilla
