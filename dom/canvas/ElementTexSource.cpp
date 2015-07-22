/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ElementTexSource.h"

#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLDefs.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "nsLayoutUtils.h"
#include "WebGLContext.h"
#include "WebGLTexelConversions.h"

namespace mozilla {
namespace webgl {

ElementTexSource::ElementTexSource(dom::Element* elem, dom::HTMLCanvasElement* canvas,
                                   WebGLContext* webgl,
                                   ElementTexSource** const out_onlyIfValid,
                                   bool* const out_badCORS)
    : mElem(elem)
    , mWebGL(webgl)
{
    MOZ_ASSERT(mElem);

    *out_onlyIfValid = nullptr;
    *out_badCORS = false;

    dom::HTMLVideoElement* media = dom::HTMLVideoElement::FromContent(elem);;
    if (!media)
        return;

    if (media->GetCORSMode() == CORS_NONE) {
        nsCOMPtr<nsIPrincipal> principal = media->GetCurrentPrincipal();
        if (!principal)
            return;

        bool subsumes;
        nsresult rv = canvas->NodePrincipal()->Subsumes(principal, &subsumes);
        if (NS_FAILED(rv) || !subsumes) {
            *out_badCORS = true;
            return;
        }
    }

    uint16_t readyState;
    if (NS_SUCCEEDED(media->GetReadyState(&readyState)) &&
        readyState < nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA)
    {
        // No frame inside, just return
        return;
    }

    RefPtr<layers::ImageContainer> container = media->GetImageContainer();
    if (container) {
        nsAutoTArray<layers::ImageContainer::OwningImage, 4> currentImages;
        container->GetCurrentImages(&currentImages);

        if (currentImages.Length()) {
            mImage = currentImages[0].mImage.get();

            *out_onlyIfValid = this;
            return;
        }
    }

    if (!GetData())
        return;
    MOZ_ASSERT(mData);

    *out_onlyIfValid = this;
    return;
}

ElementTexSource::~ElementTexSource()
{
}

const gfx::IntSize&
ElementTexSource::Size() const
{
    if (mImage)
        return mImage->GetSize();

    MOZ_ASSERT(mData);
    return mData->GetSize();
}

bool
ElementTexSource::BlitToTexture(gl::GLContext* gl, GLuint destTex, GLenum texImageTarget,
                                gl::OriginPos destOrigin) const
{
    if (!mImage)
        return false;

    return gl->BlitHelper()->BlitImageToTexture(mImage, mImage->GetSize(), destTex,
                                                texImageTarget, destOrigin);
}

gfx::DataSourceSurface*
ElementTexSource::GetData()
{
    if (!mData) {
        const nsLayoutUtils::SurfaceFromElementResult sfeRes = mWebGL->SurfaceFromElement(mElem);

        RefPtr<gfx::DataSourceSurface> data;
        WebGLTexelFormat srcFormat;
        nsresult rv = mWebGL->SurfaceFromElementResultToImageSurface(sfeRes, &data,
                                                                     &srcFormat);
        if (!NS_FAILED(rv) && data)
            mData = data;
    }

    return mData;
}

} // namespace webgl
} // namespace mozilla
