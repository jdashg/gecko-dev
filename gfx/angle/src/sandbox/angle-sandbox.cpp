#include <new> // Must be included before <cstdio>
#include <cstdio>
#include <vector>

#include "libGLESv2/entry_points_egl.h"
#include "libGLESv2/entry_points_egl_ext.h"
#include "libGLESv2/entry_points_gles_2_0.h"
#include "libGLESv2/entry_points_gles_2_0_ext.h"
#include "libGLESv2/entry_points_gles_3_0.h"


#define ASSERT2(expr,text) \
    do {                   \
        if (!(expr)) {     \
            fprintf(stderr, "ASSERT FAILED: %s\n", text); \
            *(uint8_t*)3 = 42; \
        }                      \
    } while (false)

#define ASSERT(expr) ASSERT2(expr, #expr)
#define ALWAYS_TRUE(expr) ASSERT2(expr, #expr)


static void
DumpEGLConfig(const EGLDisplay display, const EGLConfig cfg)
{
  int attrval;
  int err;

#define ATTR(_x) do {                                             \
        egl::GetConfigAttrib(display, cfg, EGL_##_x, &attrval);   \
        if ((err = egl::GetError()) != 0x3000) {                  \
            printf("  %s: ERROR (0x%04x)\n", #_x, err);           \
        } else {                                                  \
            printf("  %s: %d (0x%04x)\n", #_x, attrval, attrval); \
        }                                                         \
    } while(0)

  printf("EGL Config: %p\n", cfg);

  ATTR(BUFFER_SIZE);
  ATTR(ALPHA_SIZE);
  ATTR(BLUE_SIZE);
  ATTR(GREEN_SIZE);
  ATTR(RED_SIZE);
  ATTR(DEPTH_SIZE);
  ATTR(STENCIL_SIZE);
  ATTR(CONFIG_CAVEAT);
  ATTR(CONFIG_ID);
  ATTR(LEVEL);
  ATTR(MAX_PBUFFER_HEIGHT);
  ATTR(MAX_PBUFFER_PIXELS);
  ATTR(MAX_PBUFFER_WIDTH);
  ATTR(NATIVE_RENDERABLE);
  ATTR(NATIVE_VISUAL_ID);
  ATTR(NATIVE_VISUAL_TYPE);
  //ATTR(PRESERVED_RESOURCES);
  ATTR(SAMPLES);
  ATTR(SAMPLE_BUFFERS);
  ATTR(SURFACE_TYPE);
  ATTR(TRANSPARENT_TYPE);
  ATTR(TRANSPARENT_RED_VALUE);
  ATTR(TRANSPARENT_GREEN_VALUE);
  ATTR(TRANSPARENT_BLUE_VALUE);
  ATTR(BIND_TO_TEXTURE_RGB);
  ATTR(BIND_TO_TEXTURE_RGBA);
  ATTR(MIN_SWAP_INTERVAL);
  ATTR(MAX_SWAP_INTERVAL);
  ATTR(LUMINANCE_SIZE);
  ATTR(ALPHA_MASK_SIZE);
  ATTR(COLOR_BUFFER_TYPE);
  ATTR(RENDERABLE_TYPE);
  ATTR(CONFORMANT);

#undef ATTR
}

int
main(int argc, const char* argv[])
{
    const char* const clientExts = (const char*)egl::QueryString(nullptr, EGL_EXTENSIONS);
    printf("Client exts: %s\n", clientExts);

    //const EGLDisplay display = egl::GetDisplay(EGL_D3D11_ONLY_DISPLAY_ANGLE);
    const EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    ALWAYS_TRUE(egl::Initialize(display, nullptr, nullptr));

    const char* const displayExts = (const char*)egl::QueryString(display, EGL_EXTENSIONS);
    printf("Display exts: %s\n", displayExts);

    //////

    ALWAYS_TRUE(egl::BindAPI(EGL_OPENGL_ES_API));

    const EGLint kConfigAttribs[] = {
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_NONE
    };

    EGLConfig configs[1];
    EGLint chosenConfigs;
    ALWAYS_TRUE(egl::ChooseConfig(display, kConfigAttribs, configs, 1, &chosenConfigs));
    ASSERT(chosenConfigs);

    const EGLConfig& config = configs[0];
    DumpEGLConfig(display, config);

    //////

    const EGLint kSurfaceAttribs[] = {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };
    const EGLSurface surf = egl::CreatePbufferSurface(display, config, kSurfaceAttribs);
    ASSERT(surf);

    //////

    const EGLint kContextAttribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
    };
    const EGLContext context = egl::CreateContext(display, config, nullptr,
      kContextAttribs);
    ASSERT(context);

    //////

    ALWAYS_TRUE(egl::MakeCurrent(display, surf, surf, context));

    EGLint errEGL = egl::GetError();
    ASSERT(errEGL == EGL_SUCCESS);

    GLenum err = gl::GetError();
    ASSERT(!err);

    //////

    GLuint rb;
    gl::GenRenderbuffers(1, &rb);
    gl::BindRenderbuffer(GL_RENDERBUFFER, rb);
    gl::RenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 2, 2);

    GLuint fb;
    gl::GenFramebuffers(1, &fb);
    gl::BindFramebuffer(GL_FRAMEBUFFER, fb);
    gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                rb);

    gl::ClearColor(0, 0, 1, 1);
    gl::Clear(GL_COLOR_BUFFER_BIT);

    uint32_t pixel = 0xdeadbeef;
    gl::ReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

    err = gl::GetError();
    ASSERT(!err);

    printf("FB pixel: %08x\n", pixel);
    ASSERT(pixel == 0xffff0000);

    //////

    gl::BindFramebuffer(GL_FRAMEBUFFER, 0);

    gl::ClearColor(0, 1, 0, 1);
    gl::Clear(GL_COLOR_BUFFER_BIT);

    pixel = 0xdeadbeef;
    gl::ReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

    err = gl::GetError();
    ASSERT(!err);

    printf("DFB pixel: %08x\n", pixel);
    ASSERT(pixel == 0xff00ff00); // Breaks on D3D11

    return 0;
}
