#include <new> // Must be included before <cstdio>
#include <cstdio>
#include <vector>

#include <d3d11.h>

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

template<typename T>
class sp
{
    T* ptr;

public:
    sp()
        : ptr(nullptr)
    { }

    explicit sp(T* const x)
        : ptr(x)
    {
        if (ptr) {
            ptr->AddRef();
        }
    }

    ~sp() {
        if (ptr) {
            ptr->Release();
        }
    }

    T* operator=(T* const x) {
        if (x) {
            x->AddRef();
        }

        if (ptr) {
            ptr->Release();
        }

        ptr = x;
    }

    T*& getterAddRefs() {
        if (ptr) {
            ptr->Release();
            ptr = nullptr;
        }
        
        return ptr;
    }

    T* get() const { return ptr; }

    operator bool() const { return bool(ptr); }
    operator T*() const { return ptr; }

    T* operator ->() const { return ptr; }
};

struct D3D11Map
{
    const sp<ID3D11DeviceContext> context;
    const sp<ID3D11Resource> res;
    const uint32_t subresourceId;
    D3D11_MAPPED_SUBRESOURCE mapping;

    D3D11Map(ID3D11DeviceContext* _context, ID3D11Resource* _res, uint32_t _subresourceId,
             D3D11_MAP mapType, uint32_t mapFlags, HRESULT* const out_hr)
        : context(_context)
        , res(_res)
        , subresourceId(_subresourceId)
        , mapping{ 0 }
    {
        *out_hr = context->Map(res, subresourceId, mapType, mapFlags, &mapping);
    }

    ~D3D11Map() {
        if (mapping.pData) {
            context->Unmap(res, subresourceId);
            mapping = { 0 };
        }
    }
};

struct ScopedLockMutex
{
    const sp<IDXGIKeyedMutex> mutex;

    explicit ScopedLockMutex(IDXGIKeyedMutex* _mutex)
        : mutex(_mutex)
    {
        if (!mutex)
            return;

        HRESULT hr = mutex->AcquireSync(0, 1*1000);
        ASSERT(hr != WAIT_ABANDONED);
        ASSERT(hr != WAIT_TIMEOUT);
        ASSERT(SUCCEEDED(hr));
    }

    ~ScopedLockMutex() {
        if (!mutex)
            return;

        HRESULT hr = mutex->ReleaseSync(0);
        ASSERT(SUCCEEDED(hr));
    }
};

int
main(int argc, const char* argv[])
{
    const char* const clientExts = (const char*)egl::QueryString(nullptr, EGL_EXTENSIONS);
    printf("Client exts: %s\n", clientExts);

    const EGLDisplay display = egl::GetDisplay(EGL_D3D11_ONLY_DISPLAY_ANGLE);
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

    ////////////////////////////////////

    HRESULT hr;

    EGLDeviceEXT eglDevice;
    ALWAYS_TRUE(egl::QueryDisplayAttribEXT(display, EGL_DEVICE_EXT,
                                            (EGLAttrib*)&eglDevice));

    sp<ID3D11Device> d3d;
    ALWAYS_TRUE(egl::QueryDeviceAttribEXT(eglDevice, EGL_D3D11_DEVICE_ANGLE,
                                          (EGLAttrib*)&d3d.getterAddRefs()));
    ASSERT(d3d);

    sp<ID3D11DeviceContext> immContext;
    d3d->GetImmediateContext(&immContext.getterAddRefs());
    ASSERT(immContext);

    /////

    const EGLint kSurfaceAttribs[] = {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };
    const EGLSurface surf = egl::CreatePbufferSurface(display, config, kSurfaceAttribs);
    ASSERT(surf);
    
    /////
    
    HANDLE shareHandle;
    ALWAYS_TRUE( egl::QuerySurfacePointerANGLE(display, surf, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                               &shareHandle) );
    ASSERT(shareHandle);

    sp<IDXGIKeyedMutex> angleMutex;
    ALWAYS_TRUE(egl::QuerySurfacePointerANGLE(display, surf, EGL_DXGI_KEYED_MUTEX_ANGLE,
                                              (void**)&angleMutex.getterAddRefs()));
    ASSERT(angleMutex);

    /////

    sp<ID3D11Texture2D> backbufferTex;
    hr = d3d->OpenSharedResource(shareHandle, __uuidof(ID3D11Texture2D),
                                  (void**)&backbufferTex.getterAddRefs());
    ASSERT(SUCCEEDED(hr));
    ASSERT(backbufferTex);

    ////////////////

    const EGLint kContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    const EGLContext context = egl::CreateContext(display, config, nullptr, kContextAttribs);
    ASSERT(context);

    ////////////////////////////////////

    ALWAYS_TRUE(egl::MakeCurrent(display, surf, surf, context));

    EGLint errEGL = egl::GetError();
    ASSERT(errEGL == EGL_SUCCESS);

    GLenum err = gl::GetError();
    ASSERT(!err);

    ////////////////////////////////////
    ////////////////////////////////////
    // Check readback from user FB

    uint32_t pixel = 0xdeadbeef;
    {
        GLuint rb;
        gl::GenRenderbuffers(1, &rb);
        gl::BindRenderbuffer(GL_RENDERBUFFER, rb);
        gl::RenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 2, 2);

        GLuint fb;
        gl::GenFramebuffers(1, &fb);
        gl::BindFramebuffer(GL_FRAMEBUFFER, fb);
        gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);

        //

        gl::ClearColor(0x11 / 255.0f, 0x22 / 255.0f, 0x33 / 255.0f, 0x44 / 255.0f);
        gl::Clear(GL_COLOR_BUFFER_BIT);

        pixel = 0xdeadbeef;
        gl::ReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

        printf("FB pixel: %08x\n", pixel);
        ASSERT(pixel == 0x44332211);

        //

        err = gl::GetError();
        ASSERT(!err);
    }

    ////////////////////////////////////
    ////////////////////////////////////
    // Check readback from GL default framebuffer (backbuffer)

    gl::BindFramebuffer(GL_FRAMEBUFFER, 0);

    {
        ScopedLockMutex lock(angleMutex);

        gl::ClearColor(0x55 / 255.0f, 0x66 / 255.0f, 0x77 / 255.0f, 0x88 / 255.0f);
        gl::Clear(GL_COLOR_BUFFER_BIT);

        pixel = 0xdeadbeef;
        gl::ReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

        err = gl::GetError();
        ASSERT(!err);

        printf("Backbuffer pixel: %08x\n", pixel);
        ASSERT(pixel == 0x88776655);
    }

    ////////////////////////////////////
    ////////////////////////////////////
    // Check readback via DXGI sharing

    D3D11_TEXTURE2D_DESC stagingDesc;
    backbufferTex->GetDesc(&stagingDesc); // Copy source texture desc
    stagingDesc.BindFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    sp<ID3D11Texture2D> stagingTex;
    hr = d3d->CreateTexture2D(&stagingDesc, nullptr, &stagingTex.getterAddRefs());
    ASSERT(SUCCEEDED(hr));
    ASSERT(stagingTex);

    ////////////

    sp<IDXGIKeyedMutex> backbufferMutex;
    backbufferTex->QueryInterface(&backbufferMutex.getterAddRefs());
    ASSERT(backbufferMutex);

    {
        ScopedLockMutex lock(backbufferMutex);
        immContext->CopyResource(stagingTex, backbufferTex);
    }

    {
        D3D11Map map(immContext, stagingTex, 0, D3D11_MAP_READ, 0, &hr);
        ASSERT(SUCCEEDED(hr));

        auto& data = map.mapping.pData;
        pixel = *(uint32_t*)data;
        printf("workaround DFB: %08x\n", pixel);
    }
    ASSERT(pixel == 0x88556677); // [BB,GG,RR,AA] is 0xAARRGGBB

    ASSERT2(false, "Breaking before exit...");
    return 0;
}
