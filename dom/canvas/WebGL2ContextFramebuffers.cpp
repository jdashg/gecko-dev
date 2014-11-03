/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------------------------------------------------
// Framebuffer objects

void
WebGL2Context::BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                               GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                               GLbitfield mask, GLenum filter)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::FramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::GetInternalformatParameter(JSContext*, GLenum target, GLenum internalformat, GLenum pname, JS::MutableHandleValue retval)
{
    MOZ_CRASH("Not Implemented.");
}

// Map attachments intended for the default buffer, to attachments for a non-
// default buffer.
static void
TranslateDefaultAttachments(const dom::Sequence<GLenum>& in, dom::Sequence<GLenum>* out)
{
    for (size_t i = 0; i < in.Length(); i++) {
        switch (in[i]) {
            case LOCAL_GL_COLOR:
                out->AppendElement(LOCAL_GL_COLOR_ATTACHMENT0);
                break;
            case LOCAL_GL_DEPTH:
                out->AppendElement(LOCAL_GL_DEPTH_ATTACHMENT);
                break;
            case LOCAL_GL_STENCIL:
                out->AppendElement(LOCAL_GL_STENCIL_ATTACHMENT);
                break;
        }
    }
}

void
WebGL2Context::InvalidateFramebuffer(GLenum target, const dom::Sequence<GLenum>& attachments)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (!ValidateFramebufferTarget(target, "framebufferRenderbuffer"))
        return;

    const WebGLFramebuffer* fb;
    bool isDefaultFB;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        isDefaultFB = gl->Screen()->IsDrawFramebufferDefault();
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        isDefaultFB = gl->Screen()->IsReadFramebufferDefault();
        break;

    default:
        MOZ_CRASH("Bad target.");
    }

    for (size_t i = 0; i < attachments.Length(); i++) {
        if (!ValidateFramebufferAttachment(fb, attachments[i],
                                           "invalidateFramebuffer"))
        {
            return;
        }
    }

    if (!fb && !isDefaultFB) {
        dom::Sequence<GLenum> tmpAttachments;
        TranslateDefaultAttachments(attachments, &tmpAttachments);
        gl->fInvalidateFramebuffer(target, tmpAttachments.Length(), tmpAttachments.Elements());
    } else {
        gl->fInvalidateFramebuffer(target, attachments.Length(), attachments.Elements());
    }
}

void
WebGL2Context::InvalidateSubFramebuffer(GLenum target, const dom::Sequence<GLenum>& attachments,
                                        GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (!ValidateFramebufferTarget(target, "framebufferRenderbuffer"))
        return;

    const WebGLFramebuffer* fb;
    bool isDefaultFB;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        isDefaultFB = gl->Screen()->IsDrawFramebufferDefault();
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        isDefaultFB = gl->Screen()->IsReadFramebufferDefault();
        break;

    default:
        MOZ_CRASH("Bad target.");
    }

    for (size_t i = 0; i < attachments.Length(); i++) {
        if (!ValidateFramebufferAttachment(fb, attachments[i],
                                           "invalidateSubFramebuffer"))
        {
            return;
        }
    }

    if (!fb && !isDefaultFB) {
        dom::Sequence<GLenum> tmpAttachments;
        TranslateDefaultAttachments(attachments, &tmpAttachments);
        gl->fInvalidateSubFramebuffer(target, tmpAttachments.Length(), tmpAttachments.Elements(),
                                      x, y, width, height);
    } else {
        gl->fInvalidateSubFramebuffer(target, attachments.Length(), attachments.Elements(),
                                      x, y, width, height);
    }
}

void
WebGL2Context::ReadBuffer(GLenum mode)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::RenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                              GLsizei width, GLsizei height)
{
    MOZ_CRASH("Not Implemented.");
}
