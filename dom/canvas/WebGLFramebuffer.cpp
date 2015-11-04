/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLFramebuffer.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLExtensions.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"

namespace mozilla {

WebGLFBAttachPoint::WebGLFBAttachPoint(WebGLFramebuffer* fb,
                                       FBAttachment attachmentPoint)
    : mFB(fb)
    , mAttachmentPoint(attachmentPoint)
    , mTexImageTarget(LOCAL_GL_NONE)
{ }

WebGLFBAttachPoint::~WebGLFBAttachPoint()
{
    MOZ_ASSERT(!mRenderbufferPtr);
    MOZ_ASSERT(!mTexturePtr);
}

void
WebGLFBAttachPoint::Unlink()
{
    Clear();
}

bool
WebGLFBAttachPoint::IsDeleteRequested() const
{
    return Texture() ? Texture()->IsDeleteRequested()
         : Renderbuffer() ? Renderbuffer()->IsDeleteRequested()
         : false;
}

bool
WebGLFBAttachPoint::IsDefined() const
{
    return (Renderbuffer() && Renderbuffer()->IsDefined()) ||
           (Texture() && Texture()->ImageInfoAt(mTexImageTarget,
                                                mTexImageLevel).IsDefined());
}

const webgl::FormatUsageInfo*
WebGLFBAttachPoint::Format() const
{
    MOZ_ASSERT(IsDefined());

    if (Texture())
        return Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel).mFormat;

    if (Renderbuffer())
        return Renderbuffer()->Format();

    return nullptr;
}

bool
WebGLFBAttachPoint::HasAlpha() const
{
    return Format()->format->hasAlpha;
}

const webgl::FormatUsageInfo*
WebGLFramebuffer::GetFormatForAttachment(const WebGLFBAttachPoint& attachment) const
{
    MOZ_ASSERT(attachment.IsDefined());
    MOZ_ASSERT(attachment.Texture() || attachment.Renderbuffer());

    return attachment.Format();
}

bool
WebGLFBAttachPoint::IsReadableFloat() const
{
    auto formatUsage = Format();
    MOZ_ASSERT(formatUsage);

    auto format = formatUsage->format;
    if (!format->isColorFormat)
        return false;

    return format->componentType == webgl::ComponentType::Float;
}

void
WebGLFBAttachPoint::Clear()
{
    if (mRenderbufferPtr) {
        MOZ_ASSERT(!mTexturePtr);
        mRenderbufferPtr->UnmarkAttachment(*this);
    } else if (mTexturePtr) {
        mTexturePtr->ImageInfoAt(mTexImageTarget, mTexImageLevel).RemoveAttachPoint(this);
    }

    mTexturePtr = nullptr;
    mRenderbufferPtr = nullptr;

    OnBackingStoreRespecified();
}

void
WebGLFBAttachPoint::SetTexImage(WebGLTexture* tex, TexImageTarget target, GLint level)
{
    SetTexImageLayer(tex, target, level, 0);
}

void
WebGLFBAttachPoint::SetTexImageLayer(WebGLTexture* tex, TexImageTarget target,
                                     GLint level, GLint layer)
{
    Clear();

    mTexturePtr = tex;
    mTexImageTarget = target;
    mTexImageLevel = level;
    mTexImageLayer = layer;

    if (mTexturePtr) {
        mTexturePtr->ImageInfoAt(mTexImageTarget, mTexImageLevel).AddAttachPoint(this);
    }
}

void
WebGLFBAttachPoint::SetRenderbuffer(WebGLRenderbuffer* rb)
{
    Clear();

    mRenderbufferPtr = rb;

    if (mRenderbufferPtr) {
        mRenderbufferPtr->MarkAttachment(*this);
    }
}

bool
WebGLFBAttachPoint::HasUninitializedImageData() const
{
    if (!HasImage())
        return false;

    if (mRenderbufferPtr)
        return mRenderbufferPtr->HasUninitializedImageData();

    MOZ_ASSERT(mTexturePtr);

    auto& imageInfo = mTexturePtr->ImageInfoAt(mTexImageTarget, mTexImageLevel);
    MOZ_ASSERT(imageInfo.IsDefined());

    return !imageInfo.IsDataInitialized();
}

void
WebGLFBAttachPoint::SetImageDataStatus(WebGLImageDataStatus newStatus)
{
    if (!HasImage())
        return;

    if (mRenderbufferPtr) {
        mRenderbufferPtr->mImageDataStatus = newStatus;
        return;
    }

    MOZ_ASSERT(mTexturePtr);

    auto& imageInfo = mTexturePtr->ImageInfoAt(mTexImageTarget, mTexImageLevel);
    MOZ_ASSERT(imageInfo.IsDefined());

    const bool isDataInitialized = (newStatus == WebGLImageDataStatus::InitializedImageData);
    imageInfo.SetIsDataInitialized(isDataInitialized, mTexturePtr);
}

bool
WebGLFBAttachPoint::HasImage() const
{
    if (Texture() && Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel).IsDefined())
        return true;

    if (Renderbuffer() && Renderbuffer()->IsDefined())
        return true;

    return false;
}

void
WebGLFBAttachPoint::Size(uint32_t* const out_width, uint32_t* const out_height) const
{
    MOZ_ASSERT(HasImage());

    if (Renderbuffer()) {
        *out_width = Renderbuffer()->Width();
        *out_height = Renderbuffer()->Height();
        return;
    }

    MOZ_ASSERT(Texture());
    MOZ_ASSERT(Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel).IsDefined());
    const auto& imageInfo = Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel);

    *out_width = imageInfo.mWidth;
    *out_height = imageInfo.mHeight;
}

void
WebGLFBAttachPoint::OnBackingStoreRespecified() const
{
    mFB->InvalidateFramebufferStatus();
}

bool
WebGLFBAttachPoint::IsComplete() const
{
    if (!HasImage())
        return false;

    uint32_t width;
    uint32_t height;
    Size(&width, &height);
    if (!width || !height)
        return false;

    auto formatUsage = Format();
    if (!formatUsage->isRenderable)
        return false;

    auto format = formatUsage->format;

    if (mAttachmentPoint >= LOCAL_GL_COLOR_ATTACHMENT0 &&
        mAttachmentPoint <= FBAttachment(LOCAL_GL_COLOR_ATTACHMENT0 - 1 +
                                         WebGLContext::kMaxColorAttachments))
    {
        return format->isColorFormat;
    }

    if (mAttachmentPoint == LOCAL_GL_DEPTH_ATTACHMENT)
        return format->hasDepth && !format->hasStencil;

    if (mAttachmentPoint == LOCAL_GL_STENCIL_ATTACHMENT)
        return !format->hasDepth && format->hasStencil;

    if (mAttachmentPoint == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
        return format->hasDepth && format->hasStencil;

    MOZ_CRASH("Invalid WebGL attachment point?");
}

void
WebGLFBAttachPoint::FinalizeAttachment(gl::GLContext* gl,
                                       FBAttachment attachmentLoc) const
{
    if (!HasImage()) {
        switch (attachmentLoc.get()) {
        case LOCAL_GL_DEPTH_ATTACHMENT:
        case LOCAL_GL_STENCIL_ATTACHMENT:
        case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
            break;

        default:
            gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, attachmentLoc.get(),
                                         LOCAL_GL_RENDERBUFFER, 0);
            break;
        }

        return;
    }
    MOZ_ASSERT(HasImage());

    if (Texture()) {
        MOZ_ASSERT(gl == Texture()->mContext->GL());

        const GLenum imageTarget = ImageTarget().get();
        const GLint mipLevel = MipLevel();
        const GLint layer = Layer();
        const GLuint glName = Texture()->mGLName;

        switch (imageTarget) {
        case LOCAL_GL_TEXTURE_2D:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            if (attachmentLoc == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
                gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                          imageTarget, glName, mipLevel);
                gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                          imageTarget, glName, mipLevel);
            } else {
                gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, attachmentLoc.get(),
                                          imageTarget, glName, mipLevel);
            }
            break;

        case LOCAL_GL_TEXTURE_2D_ARRAY:
        case LOCAL_GL_TEXTURE_3D:
            if (attachmentLoc == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
                gl->fFramebufferTextureLayer(LOCAL_GL_FRAMEBUFFER,
                                             LOCAL_GL_DEPTH_ATTACHMENT,
                                             glName, mipLevel, layer);
                gl->fFramebufferTextureLayer(LOCAL_GL_FRAMEBUFFER,
                                             LOCAL_GL_STENCIL_ATTACHMENT,
                                             glName, mipLevel, layer);
            } else {
                gl->fFramebufferTextureLayer(LOCAL_GL_FRAMEBUFFER, attachmentLoc.get(),
                                             glName, mipLevel, layer);
            }
            break;
        }
        return ;
    }

    if (Renderbuffer()) {
        Renderbuffer()->FramebufferRenderbuffer(attachmentLoc);
        return;
    }

    MOZ_CRASH();
}

JS::Value
WebGLFBAttachPoint::GetParameter(WebGLContext* context, GLenum target, GLenum attachment,
                                 GLenum pname)
{
    WebGLTexture* tex = Texture();

    bool isPNameValid = false;
    switch (pname) {
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
        isPNameValid = true;
        break;

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
        if (tex->mContext->IsWebGL2() ||
            tex->mContext->IsExtensionEnabled(WebGLExtensionID::EXT_sRGB))
        {
            isPNameValid = true;
        }
        break;

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        if (tex) {
            return JS::Int32Value(MipLevel());
        }
        break;

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        if (tex) {
            int32_t face = 0;
            if (tex->Target() == LOCAL_GL_TEXTURE_CUBE_MAP) {
                face = ImageTarget().get();
            }
            return JS::Int32Value(face);
        }
        break;

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
        if (tex) {
            int32_t layer = 0;
            if (tex->Target() == LOCAL_GL_TEXTURE_2D_ARRAY ||
                tex->Target() == LOCAL_GL_TEXTURE_3D)
            {
                layer = Layer();
            }
            return JS::Int32Value(layer);
        }
        break;
    }

    if (!isPNameValid) {
        context->ErrorInvalidEnum("getFramebufferParameter: Invalid combination of "
                                  "attachment and pname.");
        return JS::NullValue();
    }

    gl::GLContext* gl = tex->mContext->GL();
    gl->MakeCurrent();

    GLint ret = 0;
    gl->fGetFramebufferAttachmentParameteriv(target, attachment, pname, &ret);
    return JS::Int32Value(ret);
}

////////////////////////////////////////////////////////////////////////////////
// WebGLFramebuffer

WebGLFramebuffer::WebGLFramebuffer(WebGLContext* webgl, GLuint fbo)
    : WebGLContextBoundObject(webgl)
    , mGLName(fbo)
    , mIsKnownFBComplete(false)
    , mReadBufferMode(LOCAL_GL_COLOR_ATTACHMENT0)
    , mColorAttachment0(this, LOCAL_GL_COLOR_ATTACHMENT0)
    , mDepthAttachment(this, LOCAL_GL_DEPTH_ATTACHMENT)
    , mStencilAttachment(this, LOCAL_GL_STENCIL_ATTACHMENT)
    , mDepthStencilAttachment(this, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
#ifdef ANDROID
    , mIsFB(false)
#endif

{
    mContext->mFramebuffers.insertBack(this);
}

void
WebGLFramebuffer::Delete()
{
    mColorAttachment0.Clear();
    mDepthAttachment.Clear();
    mStencilAttachment.Clear();
    mDepthStencilAttachment.Clear();

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        mMoreColorAttachments[i].Clear();
    }

    mContext->MakeContextCurrent();
    mContext->gl->fDeleteFramebuffers(1, &mGLName);
    LinkedListElement<WebGLFramebuffer>::removeFrom(mContext->mFramebuffers);

#ifdef ANDROID
    mIsFB = false;
#endif
}

void
WebGLFramebuffer::FramebufferRenderbuffer(FBAttachment attachPointEnum,
                                          RBTarget rbtarget,
                                          WebGLRenderbuffer* rb)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!mContext->ValidateObjectAllowNull("framebufferRenderbuffer: renderbuffer", rb))
        return;

    // `attachPoint` is validated by ValidateFramebufferAttachment().
    WebGLFBAttachPoint& attachPoint = GetAttachPoint(attachPointEnum);
    attachPoint.SetRenderbuffer(rb);

    InvalidateFramebufferStatus();
}

void
WebGLFramebuffer::FramebufferTexture2D(FBAttachment attachPointEnum,
                                       TexImageTarget texImageTarget,
                                       WebGLTexture* tex, GLint level)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!mContext->ValidateObjectAllowNull("framebufferTexture2D: texture", tex))
        return;

    if (tex) {
        bool isTexture2D = tex->Target() == LOCAL_GL_TEXTURE_2D;
        bool isTexTarget2D = texImageTarget == LOCAL_GL_TEXTURE_2D;
        if (isTexture2D != isTexTarget2D) {
            mContext->ErrorInvalidOperation("framebufferTexture2D: Mismatched"
                                            " texture and texture target.");
            return;
        }
    }

    WebGLFBAttachPoint& attachPoint = GetAttachPoint(attachPointEnum);
    attachPoint.SetTexImage(tex, texImageTarget, level);

    InvalidateFramebufferStatus();
}

void
WebGLFramebuffer::FramebufferTextureLayer(FBAttachment attachment, WebGLTexture* tex,
                                          GLint level, GLint layer)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);
    MOZ_ASSERT(tex);

    WebGLFBAttachPoint& attachPoint = GetAttachPoint(attachment);
    TexImageTarget texImageTarget = tex->Target().get();
    attachPoint.SetTexImageLayer(tex, texImageTarget, level, layer);

    InvalidateFramebufferStatus();
}

WebGLFBAttachPoint&
WebGLFramebuffer::GetAttachPoint(FBAttachment attachPoint)
{
    switch (attachPoint.get()) {
    case LOCAL_GL_COLOR_ATTACHMENT0:
        return mColorAttachment0;

    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        return mDepthStencilAttachment;

    case LOCAL_GL_DEPTH_ATTACHMENT:
        return mDepthAttachment;

    case LOCAL_GL_STENCIL_ATTACHMENT:
        return mStencilAttachment;

    default:
        break;
    }

    if (attachPoint >= LOCAL_GL_COLOR_ATTACHMENT1) {
        size_t colorAttachmentId = attachPoint.get() - LOCAL_GL_COLOR_ATTACHMENT0;
        if (colorAttachmentId < (size_t)mContext->mGLMaxColorAttachments) {
            EnsureColorAttachPoints(colorAttachmentId);
            return mMoreColorAttachments[colorAttachmentId - 1];
        }
    }

    MOZ_CRASH("bad `attachPoint` validation");
}

void
WebGLFramebuffer::DetachTexture(const WebGLTexture* tex)
{
    if (mColorAttachment0.Texture() == tex)
        mColorAttachment0.Clear();

    if (mDepthAttachment.Texture() == tex)
        mDepthAttachment.Clear();

    if (mStencilAttachment.Texture() == tex)
        mStencilAttachment.Clear();

    if (mDepthStencilAttachment.Texture() == tex)
        mDepthStencilAttachment.Clear();

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        if (mMoreColorAttachments[i].Texture() == tex)
            mMoreColorAttachments[i].Clear();
    }
}

void
WebGLFramebuffer::DetachRenderbuffer(const WebGLRenderbuffer* rb)
{
    if (mColorAttachment0.Renderbuffer() == rb)
        mColorAttachment0.Clear();

    if (mDepthAttachment.Renderbuffer() == rb)
        mDepthAttachment.Clear();

    if (mStencilAttachment.Renderbuffer() == rb)
        mStencilAttachment.Clear();

    if (mDepthStencilAttachment.Renderbuffer() == rb)
        mDepthStencilAttachment.Clear();

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        if (mMoreColorAttachments[i].Renderbuffer() == rb)
            mMoreColorAttachments[i].Clear();
    }
}

bool
WebGLFramebuffer::HasDefinedAttachments() const
{
    bool hasAttachments = false;

    hasAttachments |= mColorAttachment0.IsDefined();
    hasAttachments |= mDepthAttachment.IsDefined();
    hasAttachments |= mStencilAttachment.IsDefined();
    hasAttachments |= mDepthStencilAttachment.IsDefined();

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        hasAttachments |= mMoreColorAttachments[i].IsDefined();
    }

    return hasAttachments;
}

static bool
IsIncomplete(const WebGLFBAttachPoint& cur)
{
    return cur.IsDefined() && !cur.IsComplete();
}

bool
WebGLFramebuffer::HasIncompleteAttachments() const
{
    bool hasIncomplete = false;

    hasIncomplete |= IsIncomplete(mColorAttachment0);
    hasIncomplete |= IsIncomplete(mDepthAttachment);
    hasIncomplete |= IsIncomplete(mStencilAttachment);
    hasIncomplete |= IsIncomplete(mDepthStencilAttachment);

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        hasIncomplete |= IsIncomplete(mMoreColorAttachments[i]);
    }

    return hasIncomplete;
}

static bool
MatchOrReplaceSize(const WebGLFBAttachPoint& cur, uint32_t* const out_width,
                   uint32_t* const out_height)
{
    if (!cur.HasImage())
        return true;

    uint32_t width;
    uint32_t height;
    cur.Size(&width, &height);

    if (!*out_width) {
        MOZ_ASSERT(!*out_height);
        *out_width = width;
        *out_height = height;
        return true;
    }

    return (width == *out_width &&
            height == *out_height);
}

bool
WebGLFramebuffer::AllImageRectsMatch() const
{
    MOZ_ASSERT(HasDefinedAttachments());
    MOZ_ASSERT(!HasIncompleteAttachments());

    uint32_t width = 0;
    uint32_t height = 0;
    bool imageRectsMatch = true;

    imageRectsMatch &= MatchOrReplaceSize(mColorAttachment0,       &width, &height);
    imageRectsMatch &= MatchOrReplaceSize(mDepthAttachment,        &width, &height);
    imageRectsMatch &= MatchOrReplaceSize(mStencilAttachment,      &width, &height);
    imageRectsMatch &= MatchOrReplaceSize(mDepthStencilAttachment, &width, &height);

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        imageRectsMatch &= MatchOrReplaceSize(mMoreColorAttachments[i], &width, &height);
    }

    return imageRectsMatch;
}

FBStatus
WebGLFramebuffer::PrecheckFramebufferStatus() const
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!HasDefinedAttachments())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT; // No attachments

    if (HasIncompleteAttachments())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

    if (!AllImageRectsMatch())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS; // Inconsistent sizes

    if (HasDepthStencilConflict())
        return LOCAL_GL_FRAMEBUFFER_UNSUPPORTED;

    return LOCAL_GL_FRAMEBUFFER_COMPLETE;
}

FBStatus
WebGLFramebuffer::CheckFramebufferStatus() const
{
    if (mIsKnownFBComplete)
        return LOCAL_GL_FRAMEBUFFER_COMPLETE;

    FBStatus ret = PrecheckFramebufferStatus();
    if (ret != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return ret;

    // Looks good on our end. Let's ask the driver.
    mContext->MakeContextCurrent();

    // Ok, attach our chosen flavor of {DEPTH, STENCIL, DEPTH_STENCIL}.
    FinalizeAttachments();

    // TODO: This should not be unconditionally GL_FRAMEBUFFER.
    ret = mContext->gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);

    if (ret == LOCAL_GL_FRAMEBUFFER_COMPLETE)
        mIsKnownFBComplete = true;

    return ret;
}

bool
WebGLFramebuffer::HasCompletePlanes(GLbitfield mask)
{
    if (CheckFramebufferStatus() != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return false;

    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    bool hasPlanes = true;
    if (mask & LOCAL_GL_COLOR_BUFFER_BIT)
        hasPlanes &= mColorAttachment0.IsDefined();

    if (mask & LOCAL_GL_DEPTH_BUFFER_BIT) {
        hasPlanes &= mDepthAttachment.IsDefined() ||
                     mDepthStencilAttachment.IsDefined();
    }

    if (mask & LOCAL_GL_STENCIL_BUFFER_BIT) {
        hasPlanes &= mStencilAttachment.IsDefined() ||
                     mDepthStencilAttachment.IsDefined();
    }

    return hasPlanes;
}

bool
WebGLFramebuffer::CheckAndInitializeAttachments()
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (CheckFramebufferStatus() != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return false;

    // Cool! We've checked out ok. Just need to initialize.
    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();

    // Check if we need to initialize anything
    {
        bool hasUninitializedAttachments = false;

        if (mColorAttachment0.HasImage())
            hasUninitializedAttachments |= mColorAttachment0.HasUninitializedImageData();
        if (mDepthAttachment.HasImage())
            hasUninitializedAttachments |= mDepthAttachment.HasUninitializedImageData();
        if (mStencilAttachment.HasImage())
            hasUninitializedAttachments |= mStencilAttachment.HasUninitializedImageData();
        if (mDepthStencilAttachment.HasImage())
            hasUninitializedAttachments |= mDepthStencilAttachment.HasUninitializedImageData();

        for (size_t i = 0; i < moreColorAttachmentCount; i++) {
            if (mMoreColorAttachments[i].HasImage())
                hasUninitializedAttachments |= mMoreColorAttachments[i].HasUninitializedImageData();
        }

        if (!hasUninitializedAttachments)
            return true;
    }

    // Get buffer-bit-mask and color-attachment-mask-list
    uint32_t mask = 0;
    bool colorAttachmentsMask[WebGLContext::kMaxColorAttachments] = { false };
    MOZ_ASSERT(1 + moreColorAttachmentCount <= WebGLContext::kMaxColorAttachments);

    if (mColorAttachment0.HasUninitializedImageData()) {
        colorAttachmentsMask[0] = true;
        mask |= LOCAL_GL_COLOR_BUFFER_BIT;
    }

    if (mDepthAttachment.HasUninitializedImageData() ||
        mDepthStencilAttachment.HasUninitializedImageData())
    {
        mask |= LOCAL_GL_DEPTH_BUFFER_BIT;
    }

    if (mStencilAttachment.HasUninitializedImageData() ||
        mDepthStencilAttachment.HasUninitializedImageData())
    {
        mask |= LOCAL_GL_STENCIL_BUFFER_BIT;
    }

    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        if (mMoreColorAttachments[i].HasUninitializedImageData()) {
          colorAttachmentsMask[1 + i] = true;
          mask |= LOCAL_GL_COLOR_BUFFER_BIT;
        }
    }

    // Clear!
    mContext->ForceClearFramebufferWithDefaultValues(false, mask, colorAttachmentsMask);

    // Mark all the uninitialized images as initialized.
    if (mColorAttachment0.HasUninitializedImageData())
        mColorAttachment0.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    if (mDepthAttachment.HasUninitializedImageData())
        mDepthAttachment.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    if (mStencilAttachment.HasUninitializedImageData())
        mStencilAttachment.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    if (mDepthStencilAttachment.HasUninitializedImageData())
        mDepthStencilAttachment.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);

    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        if (mMoreColorAttachments[i].HasUninitializedImageData())
            mMoreColorAttachments[i].SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    }

    return true;
}

void WebGLFramebuffer::EnsureColorAttachPoints(size_t colorAttachmentId)
{
    size_t maxColorAttachments = mContext->mGLMaxColorAttachments;

    MOZ_ASSERT(colorAttachmentId < maxColorAttachments);

    if (colorAttachmentId < ColorAttachmentCount())
        return;

    while (ColorAttachmentCount() < maxColorAttachments) {
        GLenum nextAttachPoint = LOCAL_GL_COLOR_ATTACHMENT0 + ColorAttachmentCount();
        mMoreColorAttachments.AppendElement(WebGLFBAttachPoint(this, nextAttachPoint));
    }

    MOZ_ASSERT(ColorAttachmentCount() == maxColorAttachments);
}

static void
FinalizeDrawAndReadBuffers(gl::GLContext* gl, bool isColorBufferDefined)
{
    MOZ_ASSERT(gl, "Expected a valid GLContext ptr.");
    // GLES don't support DrawBuffer()/ReadBuffer.
    // According to http://www.opengl.org/wiki/Framebuffer_Object
    //
    // Each draw buffers must either specify color attachment points that have images
    // attached or must be GL_NONE​. (GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER​ when false).
    //
    // If the read buffer is set, then it must specify an attachment point that has an
    // image attached. (GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER​ when false).
    //
    // Note that this test is not performed if OpenGL 4.2 or ARB_ES2_compatibility is
    // available.
    if (gl->IsGLES() ||
        gl->IsSupported(gl::GLFeature::ES2_compatibility) ||
        gl->IsAtLeast(gl::ContextProfile::OpenGL, 420))
    {
        return;
    }

    // TODO(djg): Assert that fDrawBuffer/fReadBuffer is not NULL.
    GLenum colorBufferSource = isColorBufferDefined ? LOCAL_GL_COLOR_ATTACHMENT0
                                                    : LOCAL_GL_NONE;
    gl->fDrawBuffer(colorBufferSource);
    gl->fReadBuffer(colorBufferSource);
}

void
WebGLFramebuffer::FinalizeAttachments() const
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    gl::GLContext* gl = mContext->gl;

    // Nuke the depth and stencil attachment points.
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, 0);
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, 0);

    // Call finalize.
    mColorAttachment0.FinalizeAttachment(gl, LOCAL_GL_COLOR_ATTACHMENT0);
    mDepthAttachment.FinalizeAttachment(gl, LOCAL_GL_DEPTH_ATTACHMENT);
    mStencilAttachment.FinalizeAttachment(gl, LOCAL_GL_STENCIL_ATTACHMENT);
    mDepthStencilAttachment.FinalizeAttachment(gl, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT);

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        GLenum attachPoint = LOCAL_GL_COLOR_ATTACHMENT0 + 1 + i;
        mMoreColorAttachments[i].FinalizeAttachment(gl, attachPoint);
    }

    FinalizeDrawAndReadBuffers(gl, mColorAttachment0.IsDefined());
}

bool
WebGLFramebuffer::ValidateForRead(const char* funcName,
                                  const webgl::FormatUsageInfo** const out_format,
                                  uint32_t* const out_width, uint32_t* const out_height)
{
    if (!CheckAndInitializeAttachments()) {
        mContext->ErrorInvalidFramebufferOperation("%s: Incomplete framebuffer.",
                                                   funcName);
        return false;
    }

    if (mReadBufferMode == LOCAL_GL_NONE) {
        mContext->ErrorInvalidOperation("%s: Read buffer mode must not be"
                                        " NONE.", funcName);
        return false;
    }

    const auto& attachPoint = GetAttachPoint(mReadBufferMode);

    if (!attachPoint.IsDefined()) {
        mContext->ErrorInvalidOperation("%s: THe attachment specified for reading is"
                                        " null.", funcName);
        return false;
    }

    *out_format = attachPoint.Format();
    attachPoint.Size(out_width, out_height);
    return true;
}

static bool
AttachmentsDontMatch(const WebGLFBAttachPoint& a, const WebGLFBAttachPoint& b)
{
    if (a.Texture()) {
        return (a.Texture() != b.Texture());
    }

    if (a.Renderbuffer()) {
        return (a.Renderbuffer() != b.Renderbuffer());
    }

    return false;
}

JS::Value
WebGLFramebuffer::GetAttachmentParameter(JSContext* cx, GLenum target, GLenum attachment,
                                         GLenum pname, ErrorResult* const out_error)
{
    // "If a framebuffer object is bound to target, then attachment must be one of the
    // attachment points of the framebuffer listed in table 4.6."
    switch (attachment) {
    case LOCAL_GL_DEPTH_ATTACHMENT:
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        break;

    case LOCAL_GL_STENCIL_ATTACHMENT:
        // "If attachment is DEPTH_STENCIL_ATTACHMENT, and different objects are bound to
        //  the depth and stencil attachment points of target, the query will fail and
        //  generate an INVALID_OPERATION error. If the same object is bound to both
        //  attachment points, information about that object will be returned."

        // Does this mean it has to be the same level or layer? Because the queries are
        // independent of level or layer.
        if (AttachmentsDontMatch(DepthAttachment(), StencilAttachment())) {
            mContext->ErrorInvalidOperation("getFramebufferAttachmentParameter: "
                                            "DEPTH_ATTACHMENT and STENCIL_ATTACHMENT "
                                            "have different objects bound.");
            return JS::NullValue();
        }
        break;

    default:
        if (attachment < LOCAL_GL_COLOR_ATTACHMENT0 ||
            attachment > mContext->LastColorAttachment())
        {
            mContext->ErrorInvalidEnum("getFramebufferAttachmentParameter: Can only "
                                       "query COLOR_ATTACHMENTi, DEPTH_ATTACHMENT, "
                                       "DEPTH_STENCIL_ATTACHMENT, or STENCIL_ATTACHMENT "
                                       "on framebuffer.");
            return JS::NullValue();
        }
    }

    if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT &&
        pname == LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE)
    {
        mContext->ErrorInvalidOperation("getFramebufferAttachmentParameter: Querying "
                                        "FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE against "
                                        "DEPTH_STENCIL_ATTACHMENT is an error.");
        return JS::NullValue();
    }

    GLenum objectType = LOCAL_GL_NONE;
    auto& fba = GetAttachPoint(attachment);
    if (fba.Texture()) {
        objectType = LOCAL_GL_TEXTURE;
    } else if (fba.Renderbuffer()) {
        objectType = LOCAL_GL_RENDERBUFFER;
    }

    switch (pname) {
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return JS::Int32Value(objectType);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        if (objectType == LOCAL_GL_NONE) {
            return JS::NullValue();
        }

        if (objectType == LOCAL_GL_RENDERBUFFER) {
            const WebGLRenderbuffer* rb = fba.Renderbuffer();
            return mContext->WebGLObjectAsJSValue(cx, rb, *out_error);
        }

        /* If the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is TEXTURE, then */
        if (objectType == LOCAL_GL_TEXTURE) {
            const WebGLTexture* tex = fba.Texture();
            return mContext->WebGLObjectAsJSValue(cx, tex, *out_error);
        }
        break;
    }

    if (objectType == LOCAL_GL_NONE) {
        mContext->ErrorInvalidOperation("getFramebufferAttachmentParameter: No "
                                        "attachment at %s",
                                        mContext->EnumName(attachment));
        return JS::NullValue();
    }

    return fba.GetParameter(mContext, target, attachment, pname);
}

////////////////////////////////////////////////////////////////////////////////
// Goop.

JSObject*
WebGLFramebuffer::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLFramebufferBinding::Wrap(cx, this, givenProto);
}

inline void
ImplCycleCollectionUnlink(mozilla::WebGLFBAttachPoint& field)
{
    field.Unlink();
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                            mozilla::WebGLFBAttachPoint& field,
                            const char* name,
                            uint32_t flags = 0)
{
    CycleCollectionNoteChild(callback, field.Texture(), name, flags);
    CycleCollectionNoteChild(callback, field.Renderbuffer(), name, flags);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLFramebuffer,
                                      mColorAttachment0,
                                      mDepthAttachment,
                                      mStencilAttachment,
                                      mDepthStencilAttachment,
                                      mMoreColorAttachments)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLFramebuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLFramebuffer, Release)

} // namespace mozilla
