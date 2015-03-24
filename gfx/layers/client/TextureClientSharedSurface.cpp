/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClientSharedSurface.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"        // for gfxDebug
#include "mozilla/layers/ISurfaceAllocator.h"
#include "SharedSurface.h"
#include "GLContext.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

/*static*/ void
SharedSurfaceTextureClient::RecycleCallback(TextureClient* tc,
                                            void* /*closure*/)
{
  MOZ_ASSERT(NS_IsMainThread());

  tc->ClearRecycleCallback();

  SharedSurfaceTextureClient* sstc = (SharedSurfaceTextureClient*)tc;

  if (sstc->mFactory) {
    sstc->mFactory->Recycle(sstc);
  }
}

SharedSurfaceTextureClient::SharedSurfaceTextureClient(ISurfaceAllocator* aAllocator,
                                                       TextureFlags aFlags,
                                                       UniquePtr<gl::SharedSurface> surf)
  : TextureClient(aAllocator, aFlags)
  , mSurf(Move(surf))
{
  AddFlags(TextureFlags::RECYCLE);
  SetRecycleCallback(&SharedSurfaceTextureClient::RecycleCallback, nullptr);
}

SharedSurfaceTextureClient::~SharedSurfaceTextureClient()
{
  // Free the ShSurf.
}

gfx::IntSize
SharedSurfaceTextureClient::GetSize() const
{
  return mSurf->mSize;
}

bool
SharedSurfaceTextureClient::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  return mSurf->ToSurfaceDescriptor(&aOutDescriptor);
}

} // namespace layers
} // namespace mozilla
