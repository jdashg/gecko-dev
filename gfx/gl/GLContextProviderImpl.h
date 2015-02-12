/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IN_GL_CONTEXT_PROVIDER_H
#error GLContextProviderImpl.h must only be included from GLContextProvider.h
#endif

#ifndef GL_CONTEXT_PROVIDER_NAME
#error GL_CONTEXT_PROVIDER_NAME not defined
#endif

class GL_CONTEXT_PROVIDER_NAME
{
public:
    /**
     * Create a context that renders to the surface of the widget that is
     * passed in.  The context is always created with an RGB pixel format,
     * with no alpha, depth or stencil.  If any of those features are needed,
     * either use a framebuffer, or use CreateHeadless.
     *
     * This context will attempt to share resources with all other window
     * contexts.  As such, it's critical that resources allocated that are not
     * needed by other contexts be deleted before the context is destroyed.
     *
     * The GetSharedContext() method will return non-null if sharing
     * was successful.
     *
     * Note: a context created for a widget /must not/ hold a strong
     * reference to the widget; otherwise a cycle can be created through
     * a GL layer manager.
     *
     * @param aWidget Widget whose surface to create a context for
     *
     * @return Context to use for the window
     */
    static already_AddRefed<GLContext>
    CreateForWindow(nsIWidget* widget);

    // Just create a context. We'll add offscreen stuff ourselves.
    static already_AddRefed<GLContext>
    CreateHeadless();

    /**
     * Create wrapping Gecko GLContext for external gl context.
     *
     * @param aContext External context which will be wrapped by Gecko GLContext.
     * @param aSurface External surface which is used for external context.
     *
     * @return Wrapping Context to use for rendering
     */
    static already_AddRefed<GLContext>
    CreateWrappingExisting(void* aContext, void* aSurface);

    /**
     * Get a pointer to the global context, creating it if it doesn't exist.
     */
    static GLContext*
    GetGlobalContext();

    /**
     * Free any resources held by this Context Provider.
     */
    static void
    Shutdown();
};
