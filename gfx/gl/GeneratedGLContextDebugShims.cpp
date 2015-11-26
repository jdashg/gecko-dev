// GENERATED FILE - `python GeneratedGLContextDebugShims.cpp.py`
#include "GLContext.h"

namespace mozilla {
namespace gl {

struct GLContextDebugShims
{
    static void
    glActiveTexture(GLenum texture)
    {
        const char kFuncName[] = "glActiveTexture";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fActiveTexture(texture);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glAttachShader(GLuint program, GLuint shader)
    {
        const char kFuncName[] = "glAttachShader";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fAttachShader(program, shader);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBeginQuery(GLenum target, GLuint id)
    {
        const char kFuncName[] = "glBeginQuery";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBeginQuery(target, id);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBeginTransformFeedback(GLenum primitiveMode)
    {
        const char kFuncName[] = "glBeginTransformFeedback";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBeginTransformFeedback(primitiveMode);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindAttribLocation(GLuint program, GLuint index, const GLchar* name)
    {
        const char kFuncName[] = "glBindAttribLocation";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindAttribLocation(program, index, name);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindBuffer(GLenum target, GLuint buffer)
    {
        const char kFuncName[] = "glBindBuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindBuffer(target, buffer);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
    {
        const char kFuncName[] = "glBindBufferBase";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindBufferBase(target, index, buffer);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindBufferOffset(GLenum target, GLuint index, GLuint buffer, GLintptr offset)
    {
        const char kFuncName[] = "glBindBufferOffset";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindBufferOffset(target, index, buffer, offset);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
    {
        const char kFuncName[] = "glBindBufferRange";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindBufferRange(target, index, buffer, offset, size);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindFramebuffer(GLenum target, GLuint framebuffer)
    {
        const char kFuncName[] = "glBindFramebuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindFramebuffer(target, framebuffer);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindRenderbuffer(GLenum target, GLuint renderbuffer)
    {
        const char kFuncName[] = "glBindRenderbuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindRenderbuffer(target, renderbuffer);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindSampler(GLuint unit, GLuint sampler)
    {
        const char kFuncName[] = "glBindSampler";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindSampler(unit, sampler);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindTexture(GLenum target, GLuint texture)
    {
        const char kFuncName[] = "glBindTexture";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindTexture(target, texture);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindTransformFeedback(GLenum target, GLuint id)
    {
        const char kFuncName[] = "glBindTransformFeedback";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindTransformFeedback(target, id);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBindVertexArray(GLuint array)
    {
        const char kFuncName[] = "glBindVertexArray";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBindVertexArray(array);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
    {
        const char kFuncName[] = "glBlendColor";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBlendColor(red, green, blue, alpha);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBlendEquation(GLenum mode)
    {
        const char kFuncName[] = "glBlendEquation";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBlendEquation(mode);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
    {
        const char kFuncName[] = "glBlendEquationSeparate";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBlendEquationSeparate(modeRGB, modeAlpha);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBlendFunc(GLenum sfactor, GLenum dfactor)
    {
        const char kFuncName[] = "glBlendFunc";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBlendFunc(sfactor, dfactor);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
    {
        const char kFuncName[] = "glBlendFuncSeparate";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
    {
        const char kFuncName[] = "glBlitFramebuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
    {
        const char kFuncName[] = "glBufferData";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBufferData(target, size, data, usage);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data)
    {
        const char kFuncName[] = "glBufferSubData";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fBufferSubData(target, offset, size, data);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static GLenum
    glCheckFramebufferStatus(GLenum target)
    {
        const char kFuncName[] = "glCheckFramebufferStatus";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fCheckFramebufferStatus(target);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glClear(GLbitfield mask)
    {
        const char kFuncName[] = "glClear";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fClear(mask);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
    {
        const char kFuncName[] = "glClearBufferfi";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fClearBufferfi(buffer, drawbuffer, depth, stencil);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value)
    {
        const char kFuncName[] = "glClearBufferfv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fClearBufferfv(buffer, drawbuffer, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value)
    {
        const char kFuncName[] = "glClearBufferiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fClearBufferiv(buffer, drawbuffer, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value)
    {
        const char kFuncName[] = "glClearBufferuiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fClearBufferuiv(buffer, drawbuffer, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
    {
        const char kFuncName[] = "glClearColor";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fClearColor(r, g, b, a);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glClearDepth(GLclampd depth)
    {
        const char kFuncName[] = "glClearDepth";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fClearDepth(depth);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glClearDepthf(GLclampf depth)
    {
        const char kFuncName[] = "glClearDepthf";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fClearDepthf(depth);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glClearStencil(GLint s)
    {
        const char kFuncName[] = "glClearStencil";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fClearStencil(s);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glClientActiveTexture(GLenum texture)
    {
        const char kFuncName[] = "glClientActiveTexture";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fClientActiveTexture(texture);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static GLenum
    glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
    {
        const char kFuncName[] = "glClientWaitSync";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fClientWaitSync(sync, flags, timeout);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glColorMask(realGLboolean red, realGLboolean green, realGLboolean blue, realGLboolean alpha)
    {
        const char kFuncName[] = "glColorMask";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fColorMask(red, green, blue, alpha);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glCompileShader(GLuint shader)
    {
        const char kFuncName[] = "glCompileShader";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fCompileShader(shader);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid* pixels)
    {
        const char kFuncName[] = "glCompressedTexImage2D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, pixels);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid* data)
    {
        const char kFuncName[] = "glCompressedTexImage3D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid* pixels)
    {
        const char kFuncName[] = "glCompressedTexSubImage2D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, pixels);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid* data)
    {
        const char kFuncName[] = "glCompressedTexSubImage3D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
    {
        const char kFuncName[] = "glCopyBufferSubData";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
    {
        const char kFuncName[] = "glCopyTexImage2D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
    {
        const char kFuncName[] = "glCopyTexSubImage2D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
    {
        const char kFuncName[] = "glCopyTexSubImage3D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static GLuint
    glCreateProgram()
    {
        const char kFuncName[] = "glCreateProgram";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fCreateProgram();
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static GLuint
    glCreateShader(GLenum type)
    {
        const char kFuncName[] = "glCreateShader";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fCreateShader(type);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glCullFace(GLenum mode)
    {
        const char kFuncName[] = "glCullFace";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fCullFace(mode);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDebugMessageCallback(GLDEBUGPROC callback, const GLvoid* userParam)
    {
        const char kFuncName[] = "glDebugMessageCallback";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDebugMessageCallback(callback, userParam);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, realGLboolean enabled)
    {
        const char kFuncName[] = "glDebugMessageControl";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDebugMessageControl(source, type, severity, count, ids, enabled);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf)
    {
        const char kFuncName[] = "glDebugMessageInsert";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDebugMessageInsert(source, type, id, severity, length, buf);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteBuffers(GLsizei n, const GLuint* buffers)
    {
        const char kFuncName[] = "glDeleteBuffers";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteBuffers(n, buffers);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteFences(GLsizei n, const GLuint* fences)
    {
        const char kFuncName[] = "glDeleteFences";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteFences(n, fences);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteFramebuffers(GLsizei n, const GLuint* ids)
    {
        const char kFuncName[] = "glDeleteFramebuffers";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteFramebuffers(n, ids);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteProgram(GLuint program)
    {
        const char kFuncName[] = "glDeleteProgram";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteProgram(program);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteQueries(GLsizei n, const GLuint* queries)
    {
        const char kFuncName[] = "glDeleteQueries";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteQueries(n, queries);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteRenderbuffers(GLsizei n, const GLuint* ids)
    {
        const char kFuncName[] = "glDeleteRenderbuffers";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteRenderbuffers(n, ids);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteSamplers(GLsizei count, const GLuint* samplers)
    {
        const char kFuncName[] = "glDeleteSamplers";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteSamplers(count, samplers);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteShader(GLuint shader)
    {
        const char kFuncName[] = "glDeleteShader";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteShader(shader);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteSync(GLsync sync)
    {
        const char kFuncName[] = "glDeleteSync";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteSync(sync);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteTextures(GLsizei n, const GLuint* textures)
    {
        const char kFuncName[] = "glDeleteTextures";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteTextures(n, textures);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteTransformFeedbacks(GLsizei n, const GLuint* ids)
    {
        const char kFuncName[] = "glDeleteTransformFeedbacks";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteTransformFeedbacks(n, ids);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDeleteVertexArrays(GLsizei n, const GLuint* arrays)
    {
        const char kFuncName[] = "glDeleteVertexArrays";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDeleteVertexArrays(n, arrays);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDepthFunc(GLenum func)
    {
        const char kFuncName[] = "glDepthFunc";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDepthFunc(func);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDepthMask(realGLboolean flag)
    {
        const char kFuncName[] = "glDepthMask";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDepthMask(flag);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDepthRange(GLclampd nearVal, GLclampd farVal)
    {
        const char kFuncName[] = "glDepthRange";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDepthRange(nearVal, farVal);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDepthRangef(GLclampf nearVal, GLclampf farVal)
    {
        const char kFuncName[] = "glDepthRangef";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDepthRangef(nearVal, farVal);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDetachShader(GLuint program, GLuint shader)
    {
        const char kFuncName[] = "glDetachShader";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDetachShader(program, shader);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDisable(GLenum cap)
    {
        const char kFuncName[] = "glDisable";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDisable(cap);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDisableClientState(GLenum capability)
    {
        const char kFuncName[] = "glDisableClientState";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDisableClientState(capability);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDisableVertexAttribArray(GLuint index)
    {
        const char kFuncName[] = "glDisableVertexAttribArray";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDisableVertexAttribArray(index);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDrawArrays(GLenum mode, GLint first, GLsizei count)
    {
        const char kFuncName[] = "glDrawArrays";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDrawArrays(mode, first, count);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
    {
        const char kFuncName[] = "glDrawArraysInstanced";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDrawArraysInstanced(mode, first, count, primcount);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDrawBuffer(GLenum mode)
    {
        const char kFuncName[] = "glDrawBuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDrawBuffer(mode);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDrawBuffers(GLsizei n, const GLenum* bufs)
    {
        const char kFuncName[] = "glDrawBuffers";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDrawBuffers(n, bufs);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
    {
        const char kFuncName[] = "glDrawElements";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDrawElements(mode, count, type, indices);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices, GLsizei primcount)
    {
        const char kFuncName[] = "glDrawElementsInstanced";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDrawElementsInstanced(mode, count, type, indices, primcount);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid* indices)
    {
        const char kFuncName[] = "glDrawRangeElements";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fDrawRangeElements(mode, start, end, count, type, indices);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glEGLImageTargetRenderbufferStorage(GLenum target, GLeglImage image)
    {
        const char kFuncName[] = "glEGLImageTargetRenderbufferStorage";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fEGLImageTargetRenderbufferStorage(target, image);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glEGLImageTargetTexture2D(GLenum target, GLeglImage image)
    {
        const char kFuncName[] = "glEGLImageTargetTexture2D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fEGLImageTargetTexture2D(target, image);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glEnable(GLenum cap)
    {
        const char kFuncName[] = "glEnable";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fEnable(cap);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glEnableClientState(GLenum capability)
    {
        const char kFuncName[] = "glEnableClientState";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fEnableClientState(capability);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glEnableVertexAttribArray(GLuint index)
    {
        const char kFuncName[] = "glEnableVertexAttribArray";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fEnableVertexAttribArray(index);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glEndQuery(GLenum target)
    {
        const char kFuncName[] = "glEndQuery";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fEndQuery(target);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glEndTransformFeedback()
    {
        const char kFuncName[] = "glEndTransformFeedback";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fEndTransformFeedback();
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static GLsync
    glFenceSync(GLenum condition, GLbitfield flags)
    {
        const char kFuncName[] = "glFenceSync";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fFenceSync(condition, flags);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glFinish()
    {
        const char kFuncName[] = "glFinish";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, true);
        gl->mOriginalSymbols.fFinish();
        gl->Debug_AfterGLCall(kFuncName, true);
    }

    static void
    glFinishFence(GLuint fence)
    {
        const char kFuncName[] = "glFinishFence";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fFinishFence(fence);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glFlush()
    {
        const char kFuncName[] = "glFlush";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fFlush();
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
    {
        const char kFuncName[] = "glFlushMappedBufferRange";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fFlushMappedBufferRange(target, offset, length);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glFramebufferRenderbuffer(GLenum target, GLenum attachmentPoint, GLenum renderbufferTarget, GLuint renderbuffer)
    {
        const char kFuncName[] = "glFramebufferRenderbuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fFramebufferRenderbuffer(target, attachmentPoint, renderbufferTarget, renderbuffer);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glFramebufferTexture2D(GLenum target, GLenum attachmentPoint, GLenum textureTarget, GLuint texture, GLint level)
    {
        const char kFuncName[] = "glFramebufferTexture2D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fFramebufferTexture2D(target, attachmentPoint, textureTarget, texture, level);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
    {
        const char kFuncName[] = "glFramebufferTextureLayer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fFramebufferTextureLayer(target, attachment, texture, level, layer);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glFrontFace(GLenum mode)
    {
        const char kFuncName[] = "glFrontFace";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fFrontFace(mode);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGenBuffers(GLsizei n, GLuint* buffers)
    {
        const char kFuncName[] = "glGenBuffers";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGenBuffers(n, buffers);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGenerateMipmap(GLenum target)
    {
        const char kFuncName[] = "glGenerateMipmap";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGenerateMipmap(target);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGenFences(GLsizei n, GLuint* fences)
    {
        const char kFuncName[] = "glGenFences";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGenFences(n, fences);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGenFramebuffers(GLsizei n, GLuint* ids)
    {
        const char kFuncName[] = "glGenFramebuffers";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGenFramebuffers(n, ids);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGenQueries(GLsizei n, GLuint* queries)
    {
        const char kFuncName[] = "glGenQueries";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGenQueries(n, queries);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGenRenderbuffers(GLsizei n, GLuint* ids)
    {
        const char kFuncName[] = "glGenRenderbuffers";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGenRenderbuffers(n, ids);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGenSamplers(GLsizei count, GLuint* samplers)
    {
        const char kFuncName[] = "glGenSamplers";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGenSamplers(count, samplers);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGenTextures(GLsizei n, GLuint* textures)
    {
        const char kFuncName[] = "glGenTextures";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGenTextures(n, textures);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGenTransformFeedbacks(GLsizei n, GLuint* ids)
    {
        const char kFuncName[] = "glGenTransformFeedbacks";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGenTransformFeedbacks(n, ids);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGenVertexArrays(GLsizei n, GLuint* arrays)
    {
        const char kFuncName[] = "glGenVertexArrays";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGenVertexArrays(n, arrays);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetActiveAttrib(GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
    {
        const char kFuncName[] = "glGetActiveAttrib";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetActiveAttrib(program, index, maxLength, length, size, type, name);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetActiveUniform(GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
    {
        const char kFuncName[] = "glGetActiveUniform";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetActiveUniform(program, index, maxLength, length, size, type, name);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetActiveUniformBlockiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName)
    {
        const char kFuncName[] = "glGetActiveUniformBlockName";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetActiveUniformsiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders)
    {
        const char kFuncName[] = "glGetAttachedShaders";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetAttachedShaders(program, maxCount, count, shaders);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static GLint
    glGetAttribLocation(GLuint program, const GLchar* name)
    {
        const char kFuncName[] = "glGetAttribLocation";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fGetAttribLocation(program, name);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glGetBooleanv(GLenum pname, realGLboolean* params)
    {
        const char kFuncName[] = "glGetBooleanv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetBooleanv(pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetBufferParameteriv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetBufferParameteriv(target, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static GLuint
    glGetDebugMessageLog(GLuint count, GLsizei bufsize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog)
    {
        const char kFuncName[] = "glGetDebugMessageLog";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fGetDebugMessageLog(count, bufsize, sources, types, ids, severities, lengths, messageLog);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static GLenum
    glGetError()
    {
        const char kFuncName[] = "glGetError";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, true);
        const auto ret = gl->mOriginalSymbols.fGetError();
        gl->Debug_AfterGLCall(kFuncName, true);

        return ret;
    }

    static void
    glGetFenceiv(GLuint fence, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetFenceiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetFenceiv(fence, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetFloatv(GLenum pname, GLfloat* params)
    {
        const char kFuncName[] = "glGetFloatv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetFloatv(pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static GLint
    glGetFragDataLocation(GLuint program, const GLchar* name)
    {
        const char kFuncName[] = "glGetFragDataLocation";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fGetFragDataLocation(program, name);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* value)
    {
        const char kFuncName[] = "glGetFramebufferAttachmentParameteriv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetFramebufferAttachmentParameteriv(target, attachment, pname, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static GLenum
    glGetGraphicsResetStatus()
    {
        const char kFuncName[] = "glGetGraphicsResetStatus";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fGetGraphicsResetStatus();
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glGetInteger64i_v(GLenum target, GLuint index, GLint64* data)
    {
        const char kFuncName[] = "glGetInteger64i_v";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetInteger64i_v(target, index, data);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetInteger64v(GLenum pname, GLint64* params)
    {
        const char kFuncName[] = "glGetInteger64v";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetInteger64v(pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetIntegeri_v(GLenum param, GLuint index, GLint* values)
    {
        const char kFuncName[] = "glGetIntegeri_v";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetIntegeri_v(param, index, values);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetIntegerv(GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetIntegerv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetIntegerv(pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint* params)
    {
        const char kFuncName[] = "glGetInternalformativ";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetInternalformativ(target, internalformat, pname, bufSize, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, GLchar* label)
    {
        const char kFuncName[] = "glGetObjectLabel";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetObjectLabel(identifier, name, bufSize, length, label);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetObjectPtrLabel(const GLvoid* ptr, GLsizei bufSize, GLsizei* length, GLchar* label)
    {
        const char kFuncName[] = "glGetObjectPtrLabel";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetObjectPtrLabel(ptr, bufSize, length, label);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetPointerv(GLenum pname, GLvoid** params)
    {
        const char kFuncName[] = "glGetPointerv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetPointerv(pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog)
    {
        const char kFuncName[] = "glGetProgramInfoLog";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetProgramInfoLog(program, bufSize, length, infoLog);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetProgramiv(GLuint program, GLenum pname, GLint* param)
    {
        const char kFuncName[] = "glGetProgramiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetProgramiv(program, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetQueryiv(GLenum target, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetQueryiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetQueryiv(target, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64* params)
    {
        const char kFuncName[] = "glGetQueryObjecti64v";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetQueryObjecti64v(id, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetQueryObjectiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetQueryObjectiv(id, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params)
    {
        const char kFuncName[] = "glGetQueryObjectui64v";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetQueryObjectui64v(id, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params)
    {
        const char kFuncName[] = "glGetQueryObjectuiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetQueryObjectuiv(id, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* value)
    {
        const char kFuncName[] = "glGetRenderbufferParameteriv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetRenderbufferParameteriv(target, pname, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat* params)
    {
        const char kFuncName[] = "glGetSamplerParameterfv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetSamplerParameterfv(sampler, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetSamplerParameteriv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetSamplerParameteriv(sampler, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog)
    {
        const char kFuncName[] = "glGetShaderInfoLog";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetShaderInfoLog(shader, bufSize, length, infoLog);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetShaderiv(GLuint shader, GLenum pname, GLint* param)
    {
        const char kFuncName[] = "glGetShaderiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetShaderiv(shader, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision)
    {
        const char kFuncName[] = "glGetShaderPrecisionFormat";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetShaderSource(GLint obj, GLsizei maxLength, GLsizei* length, GLchar* source)
    {
        const char kFuncName[] = "glGetShaderSource";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetShaderSource(obj, maxLength, length, source);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static const GLubyte*
    glGetString(GLenum name)
    {
        const char kFuncName[] = "glGetString";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fGetString(name);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static const GLubyte*
    glGetStringi(GLenum name, GLuint index)
    {
        const char kFuncName[] = "glGetStringi";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fGetStringi(name, index);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values)
    {
        const char kFuncName[] = "glGetSynciv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetSynciv(sync, pname, bufSize, length, values);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid* image)
    {
        const char kFuncName[] = "glGetTexImage";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetTexImage(target, level, format, type, image);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetTexLevelParameteriv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetTexLevelParameteriv(target, level, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params)
    {
        const char kFuncName[] = "glGetTexParameterfv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetTexParameterfv(target, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetTexParameteriv(GLenum target, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetTexParameteriv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetTexParameteriv(target, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei* size, GLenum* type, GLchar* name)
    {
        const char kFuncName[] = "glGetTransformFeedbackVarying";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static GLuint
    glGetUniformBlockIndex(GLuint program, const GLchar* uniformBlockName)
    {
        const char kFuncName[] = "glGetUniformBlockIndex";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fGetUniformBlockIndex(program, uniformBlockName);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glGetUniformfv(GLuint program, GLint location, GLfloat* params)
    {
        const char kFuncName[] = "glGetUniformfv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetUniformfv(program, location, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar* const* uniformNames, GLuint* uniformIndices)
    {
        const char kFuncName[] = "glGetUniformIndices";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetUniformiv(GLuint program, GLint location, GLint* params)
    {
        const char kFuncName[] = "glGetUniformiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetUniformiv(program, location, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static GLint
    glGetUniformLocation(GLint programObj, const GLchar* name)
    {
        const char kFuncName[] = "glGetUniformLocation";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fGetUniformLocation(programObj, name);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glGetUniformuiv(GLuint program, GLint location, GLuint* params)
    {
        const char kFuncName[] = "glGetUniformuiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetUniformuiv(program, location, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params)
    {
        const char kFuncName[] = "glGetVertexAttribfv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetVertexAttribfv(index, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetVertexAttribIiv(GLuint index, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetVertexAttribIiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetVertexAttribIiv(index, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint* params)
    {
        const char kFuncName[] = "glGetVertexAttribIuiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetVertexAttribIuiv(index, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params)
    {
        const char kFuncName[] = "glGetVertexAttribiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetVertexAttribiv(index, pname, params);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glGetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid** pointer)
    {
        const char kFuncName[] = "glGetVertexAttribPointerv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fGetVertexAttribPointerv(index, pname, pointer);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glHint(GLenum target, GLenum mode)
    {
        const char kFuncName[] = "glHint";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fHint(target, mode);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments)
    {
        const char kFuncName[] = "glInvalidateFramebuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fInvalidateFramebuffer(target, numAttachments, attachments);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height)
    {
        const char kFuncName[] = "glInvalidateSubFramebuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fInvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static realGLboolean
    glIsBuffer(GLuint buffer)
    {
        const char kFuncName[] = "glIsBuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsBuffer(buffer);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsEnabled(GLenum cap)
    {
        const char kFuncName[] = "glIsEnabled";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsEnabled(cap);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsFence(GLuint fence)
    {
        const char kFuncName[] = "glIsFence";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsFence(fence);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsFramebuffer(GLuint framebuffer)
    {
        const char kFuncName[] = "glIsFramebuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsFramebuffer(framebuffer);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsProgram(GLuint program)
    {
        const char kFuncName[] = "glIsProgram";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsProgram(program);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsQuery(GLuint id)
    {
        const char kFuncName[] = "glIsQuery";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsQuery(id);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsRenderbuffer(GLuint renderbuffer)
    {
        const char kFuncName[] = "glIsRenderbuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsRenderbuffer(renderbuffer);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsSampler(GLuint sampler)
    {
        const char kFuncName[] = "glIsSampler";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsSampler(sampler);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsShader(GLuint shader)
    {
        const char kFuncName[] = "glIsShader";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsShader(shader);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsSync(GLsync sync)
    {
        const char kFuncName[] = "glIsSync";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsSync(sync);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsTexture(GLuint texture)
    {
        const char kFuncName[] = "glIsTexture";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsTexture(texture);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsTransformFeedback(GLuint id)
    {
        const char kFuncName[] = "glIsTransformFeedback";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsTransformFeedback(id);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static realGLboolean
    glIsVertexArray(GLuint array)
    {
        const char kFuncName[] = "glIsVertexArray";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fIsVertexArray(array);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glLineWidth(GLfloat width)
    {
        const char kFuncName[] = "glLineWidth";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fLineWidth(width);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glLinkProgram(GLuint program)
    {
        const char kFuncName[] = "glLinkProgram";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fLinkProgram(program);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glLoadIdentity()
    {
        const char kFuncName[] = "glLoadIdentity";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fLoadIdentity();
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glLoadMatrixd(const GLdouble* matrix)
    {
        const char kFuncName[] = "glLoadMatrixd";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fLoadMatrixd(matrix);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glLoadMatrixf(const GLfloat* matrix)
    {
        const char kFuncName[] = "glLoadMatrixf";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fLoadMatrixf(matrix);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void*
    glMapBuffer(GLenum target, GLenum access)
    {
        const char kFuncName[] = "glMapBuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fMapBuffer(target, access);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void*
    glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
    {
        const char kFuncName[] = "glMapBufferRange";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fMapBufferRange(target, offset, length, access);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glMatrixMode(GLenum mode)
    {
        const char kFuncName[] = "glMatrixMode";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fMatrixMode(mode);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar* label)
    {
        const char kFuncName[] = "glObjectLabel";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fObjectLabel(identifier, name, length, label);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glObjectPtrLabel(const GLvoid* ptr, GLsizei length, const GLchar* label)
    {
        const char kFuncName[] = "glObjectPtrLabel";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fObjectPtrLabel(ptr, length, label);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glPauseTransformFeedback()
    {
        const char kFuncName[] = "glPauseTransformFeedback";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fPauseTransformFeedback();
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glPixelStorei(GLenum pname, GLint param)
    {
        const char kFuncName[] = "glPixelStorei";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fPixelStorei(pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glPointParameterf(GLenum pname, GLfloat param)
    {
        const char kFuncName[] = "glPointParameterf";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fPointParameterf(pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glPolygonOffset(GLfloat factor, GLfloat bias)
    {
        const char kFuncName[] = "glPolygonOffset";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fPolygonOffset(factor, bias);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glPopDebugGroup()
    {
        const char kFuncName[] = "glPopDebugGroup";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fPopDebugGroup();
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message)
    {
        const char kFuncName[] = "glPushDebugGroup";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fPushDebugGroup(source, id, length, message);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glQueryCounter(GLuint id, GLenum target)
    {
        const char kFuncName[] = "glQueryCounter";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fQueryCounter(id, target);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glReadBuffer(GLenum mode)
    {
        const char kFuncName[] = "glReadBuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fReadBuffer(mode);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid* pixels)
    {
        const char kFuncName[] = "glReadPixels";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fReadPixels(x, y, width, height, format, type, pixels);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glRenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height)
    {
        const char kFuncName[] = "glRenderbufferStorage";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fRenderbufferStorage(target, internalFormat, width, height);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height)
    {
        const char kFuncName[] = "glRenderbufferStorageMultisample";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fRenderbufferStorageMultisample(target, samples, internalFormat, width, height);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glResolveMultisampleFramebufferAPPLE()
    {
        const char kFuncName[] = "glResolveMultisampleFramebufferAPPLE";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fResolveMultisampleFramebufferAPPLE();
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glResumeTransformFeedback()
    {
        const char kFuncName[] = "glResumeTransformFeedback";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fResumeTransformFeedback();
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glSampleCoverage(GLclampf value, realGLboolean invert)
    {
        const char kFuncName[] = "glSampleCoverage";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fSampleCoverage(value, invert);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
    {
        const char kFuncName[] = "glSamplerParameterf";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fSamplerParameterf(sampler, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat* param)
    {
        const char kFuncName[] = "glSamplerParameterfv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fSamplerParameterfv(sampler, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
    {
        const char kFuncName[] = "glSamplerParameteri";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fSamplerParameteri(sampler, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint* param)
    {
        const char kFuncName[] = "glSamplerParameteriv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fSamplerParameteriv(sampler, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
    {
        const char kFuncName[] = "glScissor";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fScissor(x, y, width, height);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glSetFence(GLuint fence, GLenum condition)
    {
        const char kFuncName[] = "glSetFence";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fSetFence(fence, condition);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glShaderSource(GLuint shader, GLsizei count, const GLchar* const* strings, const GLint* lengths)
    {
        const char kFuncName[] = "glShaderSource";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fShaderSource(shader, count, strings, lengths);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glStencilFunc(GLenum func, GLint ref, GLuint mask)
    {
        const char kFuncName[] = "glStencilFunc";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fStencilFunc(func, ref, mask);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glStencilFuncSeparate(GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask)
    {
        const char kFuncName[] = "glStencilFuncSeparate";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fStencilFuncSeparate(frontfunc, backfunc, ref, mask);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glStencilMask(GLuint mask)
    {
        const char kFuncName[] = "glStencilMask";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fStencilMask(mask);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glStencilMaskSeparate(GLenum face, GLuint mask)
    {
        const char kFuncName[] = "glStencilMaskSeparate";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fStencilMaskSeparate(face, mask);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
    {
        const char kFuncName[] = "glStencilOp";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fStencilOp(fail, zfail, zpass);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
    {
        const char kFuncName[] = "glStencilOpSeparate";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fStencilOpSeparate(face, sfail, dpfail, dppass);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static realGLboolean
    glTestFence(GLuint fence)
    {
        const char kFuncName[] = "glTestFence";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fTestFence(fence);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glTexGenf(GLenum coord, GLenum pname, GLfloat param)
    {
        const char kFuncName[] = "glTexGenf";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexGenf(coord, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexGenfv(GLenum coord, GLenum pname, const GLfloat* param)
    {
        const char kFuncName[] = "glTexGenfv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexGenfv(coord, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexGeni(GLenum coord, GLenum pname, GLint param)
    {
        const char kFuncName[] = "glTexGeni";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexGeni(coord, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels)
    {
        const char kFuncName[] = "glTexImage2D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexImage3D(GLenum target, GLint level, GLenum internalFormat, GLenum width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid* pixels)
    {
        const char kFuncName[] = "glTexImage3D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexImage3D(target, level, internalFormat, width, height, depth, border, format, type, pixels);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexParameterf(GLenum target, GLenum pname, GLfloat param)
    {
        const char kFuncName[] = "glTexParameterf";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexParameterf(target, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexParameteri(GLenum target, GLenum pname, GLint param)
    {
        const char kFuncName[] = "glTexParameteri";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexParameteri(target, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexParameteriv(GLenum target, GLenum pname, const GLint* param)
    {
        const char kFuncName[] = "glTexParameteriv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexParameteriv(target, pname, param);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
    {
        const char kFuncName[] = "glTexStorage2D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexStorage2D(target, levels, internalformat, width, height);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
    {
        const char kFuncName[] = "glTexStorage3D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexStorage3D(target, levels, internalformat, width, height, depth);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)
    {
        const char kFuncName[] = "glTexSubImage2D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid* pixels)
    {
        const char kFuncName[] = "glTexSubImage3D";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTextureRangeAPPLE(GLenum target, GLsizei length, GLvoid* pointer)
    {
        const char kFuncName[] = "glTextureRangeAPPLE";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTextureRangeAPPLE(target, length, pointer);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar* const* varyings, GLenum bufferMode)
    {
        const char kFuncName[] = "glTransformFeedbackVaryings";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fTransformFeedbackVaryings(program, count, varyings, bufferMode);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform1f(GLint location, GLfloat v0)
    {
        const char kFuncName[] = "glUniform1f";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform1f(location, v0);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform1fv(GLint location, GLsizei count, const GLfloat* value)
    {
        const char kFuncName[] = "glUniform1fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform1fv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform1i(GLint location, GLint v0)
    {
        const char kFuncName[] = "glUniform1i";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform1i(location, v0);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform1iv(GLint location, GLsizei count, const GLint* value)
    {
        const char kFuncName[] = "glUniform1iv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform1iv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform1ui(GLint location, GLuint v0)
    {
        const char kFuncName[] = "glUniform1ui";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform1ui(location, v0);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform1uiv(GLint location, GLsizei count, const GLuint* value)
    {
        const char kFuncName[] = "glUniform1uiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform1uiv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform2f(GLint location, GLfloat v0, GLfloat v1)
    {
        const char kFuncName[] = "glUniform2f";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform2f(location, v0, v1);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform2fv(GLint location, GLsizei count, const GLfloat* value)
    {
        const char kFuncName[] = "glUniform2fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform2fv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform2i(GLint location, GLint v0, GLint v1)
    {
        const char kFuncName[] = "glUniform2i";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform2i(location, v0, v1);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform2iv(GLint location, GLsizei count, const GLint* value)
    {
        const char kFuncName[] = "glUniform2iv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform2iv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform2ui(GLint location, GLuint v0, GLuint v1)
    {
        const char kFuncName[] = "glUniform2ui";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform2ui(location, v0, v1);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform2uiv(GLint location, GLsizei count, const GLuint* value)
    {
        const char kFuncName[] = "glUniform2uiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform2uiv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
    {
        const char kFuncName[] = "glUniform3f";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform3f(location, v0, v1, v2);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform3fv(GLint location, GLsizei count, const GLfloat* value)
    {
        const char kFuncName[] = "glUniform3fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform3fv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform3i(GLint location, GLint v0, GLint v1, GLint v2)
    {
        const char kFuncName[] = "glUniform3i";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform3i(location, v0, v1, v2);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform3iv(GLint location, GLsizei count, const GLint* value)
    {
        const char kFuncName[] = "glUniform3iv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform3iv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
    {
        const char kFuncName[] = "glUniform3ui";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform3ui(location, v0, v1, v2);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform3uiv(GLint location, GLsizei count, const GLuint* value)
    {
        const char kFuncName[] = "glUniform3uiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform3uiv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
    {
        const char kFuncName[] = "glUniform4f";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform4f(location, v0, v1, v2, v3);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform4fv(GLint location, GLsizei count, const GLfloat* value)
    {
        const char kFuncName[] = "glUniform4fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform4fv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
    {
        const char kFuncName[] = "glUniform4i";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform4i(location, v0, v1, v2, v3);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform4iv(GLint location, GLsizei count, const GLint* value)
    {
        const char kFuncName[] = "glUniform4iv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform4iv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
    {
        const char kFuncName[] = "glUniform4ui";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform4ui(location, v0, v1, v2, v3);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniform4uiv(GLint location, GLsizei count, const GLuint* value)
    {
        const char kFuncName[] = "glUniform4uiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniform4uiv(location, count, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
    {
        const char kFuncName[] = "glUniformBlockBinding";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniformMatrix2fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value)
    {
        const char kFuncName[] = "glUniformMatrix2fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniformMatrix2fv(location, count, transpose, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniformMatrix2x3fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value)
    {
        const char kFuncName[] = "glUniformMatrix2x3fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniformMatrix2x3fv(location, count, transpose, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniformMatrix2x4fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value)
    {
        const char kFuncName[] = "glUniformMatrix2x4fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniformMatrix2x4fv(location, count, transpose, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniformMatrix3fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value)
    {
        const char kFuncName[] = "glUniformMatrix3fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniformMatrix3fv(location, count, transpose, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniformMatrix3x2fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value)
    {
        const char kFuncName[] = "glUniformMatrix3x2fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniformMatrix3x2fv(location, count, transpose, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniformMatrix3x4fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value)
    {
        const char kFuncName[] = "glUniformMatrix3x4fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniformMatrix3x4fv(location, count, transpose, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniformMatrix4fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value)
    {
        const char kFuncName[] = "glUniformMatrix4fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniformMatrix4fv(location, count, transpose, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniformMatrix4x2fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value)
    {
        const char kFuncName[] = "glUniformMatrix4x2fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniformMatrix4x2fv(location, count, transpose, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glUniformMatrix4x3fv(GLint location, GLsizei count, realGLboolean transpose, const GLfloat* value)
    {
        const char kFuncName[] = "glUniformMatrix4x3fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUniformMatrix4x3fv(location, count, transpose, value);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static realGLboolean
    glUnmapBuffer(GLenum target)
    {
        const char kFuncName[] = "glUnmapBuffer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        const auto ret = gl->mOriginalSymbols.fUnmapBuffer(target);
        gl->Debug_AfterGLCall(kFuncName, false);

        return ret;
    }

    static void
    glUseProgram(GLuint program)
    {
        const char kFuncName[] = "glUseProgram";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fUseProgram(program);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glValidateProgram(GLuint program)
    {
        const char kFuncName[] = "glValidateProgram";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fValidateProgram(program);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttrib1f(GLuint index, GLfloat x)
    {
        const char kFuncName[] = "glVertexAttrib1f";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttrib1f(index, x);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttrib1fv(GLuint index, const GLfloat* v)
    {
        const char kFuncName[] = "glVertexAttrib1fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttrib1fv(index, v);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y)
    {
        const char kFuncName[] = "glVertexAttrib2f";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttrib2f(index, x, y);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttrib2fv(GLuint index, const GLfloat* v)
    {
        const char kFuncName[] = "glVertexAttrib2fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttrib2fv(index, v);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)
    {
        const char kFuncName[] = "glVertexAttrib3f";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttrib3f(index, x, y, z);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttrib3fv(GLuint index, const GLfloat* v)
    {
        const char kFuncName[] = "glVertexAttrib3fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttrib3fv(index, v);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
    {
        const char kFuncName[] = "glVertexAttrib4f";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttrib4f(index, x, y, z, w);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttrib4fv(GLuint index, const GLfloat* v)
    {
        const char kFuncName[] = "glVertexAttrib4fv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttrib4fv(index, v);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttribDivisor(GLuint index, GLuint divisor)
    {
        const char kFuncName[] = "glVertexAttribDivisor";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttribDivisor(index, divisor);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
    {
        const char kFuncName[] = "glVertexAttribI4i";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttribI4i(index, x, y, z, w);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttribI4iv(GLuint index, const GLint* v)
    {
        const char kFuncName[] = "glVertexAttribI4iv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttribI4iv(index, v);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
    {
        const char kFuncName[] = "glVertexAttribI4ui";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttribI4ui(index, x, y, z, w);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttribI4uiv(GLuint index, const GLuint* v)
    {
        const char kFuncName[] = "glVertexAttribI4uiv";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttribI4uiv(index, v);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* ptr)
    {
        const char kFuncName[] = "glVertexAttribIPointer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttribIPointer(index, size, type, stride, ptr);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexAttribPointer(GLuint index, GLint size, GLenum type, realGLboolean normalized, GLsizei stride, const GLvoid* pointer)
    {
        const char kFuncName[] = "glVertexAttribPointer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexAttribPointer(index, size, type, normalized, stride, pointer);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
    {
        const char kFuncName[] = "glVertexPointer";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fVertexPointer(size, type, stride, pointer);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
    {
        const char kFuncName[] = "glViewport";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fViewport(x, y, width, height);
        gl->Debug_AfterGLCall(kFuncName, false);
    }

    static void
    glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
    {
        const char kFuncName[] = "glWaitSync";

        const auto gl = GLContext::ThreadCurrentContext(kFuncName);
        MOZ_ASSERT(gl);

        gl->Debug_BeforeGLCall(kFuncName, false);
        gl->mOriginalSymbols.fWaitSync(sync, flags, timeout);
        gl->Debug_AfterGLCall(kFuncName, false);
    }
};

void
GLContext::ShimDebugSymbols()
{
    mSymbols.fActiveTexture = GLContextDebugShims::glActiveTexture;
    mSymbols.fAttachShader = GLContextDebugShims::glAttachShader;
    mSymbols.fBeginQuery = GLContextDebugShims::glBeginQuery;
    mSymbols.fBeginTransformFeedback = GLContextDebugShims::glBeginTransformFeedback;
    mSymbols.fBindAttribLocation = GLContextDebugShims::glBindAttribLocation;
    mSymbols.fBindBuffer = GLContextDebugShims::glBindBuffer;
    mSymbols.fBindBufferBase = GLContextDebugShims::glBindBufferBase;
    mSymbols.fBindBufferOffset = GLContextDebugShims::glBindBufferOffset;
    mSymbols.fBindBufferRange = GLContextDebugShims::glBindBufferRange;
    mSymbols.fBindFramebuffer = GLContextDebugShims::glBindFramebuffer;
    mSymbols.fBindRenderbuffer = GLContextDebugShims::glBindRenderbuffer;
    mSymbols.fBindSampler = GLContextDebugShims::glBindSampler;
    mSymbols.fBindTexture = GLContextDebugShims::glBindTexture;
    mSymbols.fBindTransformFeedback = GLContextDebugShims::glBindTransformFeedback;
    mSymbols.fBindVertexArray = GLContextDebugShims::glBindVertexArray;
    mSymbols.fBlendColor = GLContextDebugShims::glBlendColor;
    mSymbols.fBlendEquation = GLContextDebugShims::glBlendEquation;
    mSymbols.fBlendEquationSeparate = GLContextDebugShims::glBlendEquationSeparate;
    mSymbols.fBlendFunc = GLContextDebugShims::glBlendFunc;
    mSymbols.fBlendFuncSeparate = GLContextDebugShims::glBlendFuncSeparate;
    mSymbols.fBlitFramebuffer = GLContextDebugShims::glBlitFramebuffer;
    mSymbols.fBufferData = GLContextDebugShims::glBufferData;
    mSymbols.fBufferSubData = GLContextDebugShims::glBufferSubData;
    mSymbols.fCheckFramebufferStatus = GLContextDebugShims::glCheckFramebufferStatus;
    mSymbols.fClear = GLContextDebugShims::glClear;
    mSymbols.fClearBufferfi = GLContextDebugShims::glClearBufferfi;
    mSymbols.fClearBufferfv = GLContextDebugShims::glClearBufferfv;
    mSymbols.fClearBufferiv = GLContextDebugShims::glClearBufferiv;
    mSymbols.fClearBufferuiv = GLContextDebugShims::glClearBufferuiv;
    mSymbols.fClearColor = GLContextDebugShims::glClearColor;
    mSymbols.fClearDepth = GLContextDebugShims::glClearDepth;
    mSymbols.fClearDepthf = GLContextDebugShims::glClearDepthf;
    mSymbols.fClearStencil = GLContextDebugShims::glClearStencil;
    mSymbols.fClientActiveTexture = GLContextDebugShims::glClientActiveTexture;
    mSymbols.fClientWaitSync = GLContextDebugShims::glClientWaitSync;
    mSymbols.fColorMask = GLContextDebugShims::glColorMask;
    mSymbols.fCompileShader = GLContextDebugShims::glCompileShader;
    mSymbols.fCompressedTexImage2D = GLContextDebugShims::glCompressedTexImage2D;
    mSymbols.fCompressedTexImage3D = GLContextDebugShims::glCompressedTexImage3D;
    mSymbols.fCompressedTexSubImage2D = GLContextDebugShims::glCompressedTexSubImage2D;
    mSymbols.fCompressedTexSubImage3D = GLContextDebugShims::glCompressedTexSubImage3D;
    mSymbols.fCopyBufferSubData = GLContextDebugShims::glCopyBufferSubData;
    mSymbols.fCopyTexImage2D = GLContextDebugShims::glCopyTexImage2D;
    mSymbols.fCopyTexSubImage2D = GLContextDebugShims::glCopyTexSubImage2D;
    mSymbols.fCopyTexSubImage3D = GLContextDebugShims::glCopyTexSubImage3D;
    mSymbols.fCreateProgram = GLContextDebugShims::glCreateProgram;
    mSymbols.fCreateShader = GLContextDebugShims::glCreateShader;
    mSymbols.fCullFace = GLContextDebugShims::glCullFace;
    mSymbols.fDebugMessageCallback = GLContextDebugShims::glDebugMessageCallback;
    mSymbols.fDebugMessageControl = GLContextDebugShims::glDebugMessageControl;
    mSymbols.fDebugMessageInsert = GLContextDebugShims::glDebugMessageInsert;
    mSymbols.fDeleteBuffers = GLContextDebugShims::glDeleteBuffers;
    mSymbols.fDeleteFences = GLContextDebugShims::glDeleteFences;
    mSymbols.fDeleteFramebuffers = GLContextDebugShims::glDeleteFramebuffers;
    mSymbols.fDeleteProgram = GLContextDebugShims::glDeleteProgram;
    mSymbols.fDeleteQueries = GLContextDebugShims::glDeleteQueries;
    mSymbols.fDeleteRenderbuffers = GLContextDebugShims::glDeleteRenderbuffers;
    mSymbols.fDeleteSamplers = GLContextDebugShims::glDeleteSamplers;
    mSymbols.fDeleteShader = GLContextDebugShims::glDeleteShader;
    mSymbols.fDeleteSync = GLContextDebugShims::glDeleteSync;
    mSymbols.fDeleteTextures = GLContextDebugShims::glDeleteTextures;
    mSymbols.fDeleteTransformFeedbacks = GLContextDebugShims::glDeleteTransformFeedbacks;
    mSymbols.fDeleteVertexArrays = GLContextDebugShims::glDeleteVertexArrays;
    mSymbols.fDepthFunc = GLContextDebugShims::glDepthFunc;
    mSymbols.fDepthMask = GLContextDebugShims::glDepthMask;
    mSymbols.fDepthRange = GLContextDebugShims::glDepthRange;
    mSymbols.fDepthRangef = GLContextDebugShims::glDepthRangef;
    mSymbols.fDetachShader = GLContextDebugShims::glDetachShader;
    mSymbols.fDisable = GLContextDebugShims::glDisable;
    mSymbols.fDisableClientState = GLContextDebugShims::glDisableClientState;
    mSymbols.fDisableVertexAttribArray = GLContextDebugShims::glDisableVertexAttribArray;
    mSymbols.fDrawArrays = GLContextDebugShims::glDrawArrays;
    mSymbols.fDrawArraysInstanced = GLContextDebugShims::glDrawArraysInstanced;
    mSymbols.fDrawBuffer = GLContextDebugShims::glDrawBuffer;
    mSymbols.fDrawBuffers = GLContextDebugShims::glDrawBuffers;
    mSymbols.fDrawElements = GLContextDebugShims::glDrawElements;
    mSymbols.fDrawElementsInstanced = GLContextDebugShims::glDrawElementsInstanced;
    mSymbols.fDrawRangeElements = GLContextDebugShims::glDrawRangeElements;
    mSymbols.fEGLImageTargetRenderbufferStorage = GLContextDebugShims::glEGLImageTargetRenderbufferStorage;
    mSymbols.fEGLImageTargetTexture2D = GLContextDebugShims::glEGLImageTargetTexture2D;
    mSymbols.fEnable = GLContextDebugShims::glEnable;
    mSymbols.fEnableClientState = GLContextDebugShims::glEnableClientState;
    mSymbols.fEnableVertexAttribArray = GLContextDebugShims::glEnableVertexAttribArray;
    mSymbols.fEndQuery = GLContextDebugShims::glEndQuery;
    mSymbols.fEndTransformFeedback = GLContextDebugShims::glEndTransformFeedback;
    mSymbols.fFenceSync = GLContextDebugShims::glFenceSync;
    mSymbols.fFinish = GLContextDebugShims::glFinish;
    mSymbols.fFinishFence = GLContextDebugShims::glFinishFence;
    mSymbols.fFlush = GLContextDebugShims::glFlush;
    mSymbols.fFlushMappedBufferRange = GLContextDebugShims::glFlushMappedBufferRange;
    mSymbols.fFramebufferRenderbuffer = GLContextDebugShims::glFramebufferRenderbuffer;
    mSymbols.fFramebufferTexture2D = GLContextDebugShims::glFramebufferTexture2D;
    mSymbols.fFramebufferTextureLayer = GLContextDebugShims::glFramebufferTextureLayer;
    mSymbols.fFrontFace = GLContextDebugShims::glFrontFace;
    mSymbols.fGenBuffers = GLContextDebugShims::glGenBuffers;
    mSymbols.fGenerateMipmap = GLContextDebugShims::glGenerateMipmap;
    mSymbols.fGenFences = GLContextDebugShims::glGenFences;
    mSymbols.fGenFramebuffers = GLContextDebugShims::glGenFramebuffers;
    mSymbols.fGenQueries = GLContextDebugShims::glGenQueries;
    mSymbols.fGenRenderbuffers = GLContextDebugShims::glGenRenderbuffers;
    mSymbols.fGenSamplers = GLContextDebugShims::glGenSamplers;
    mSymbols.fGenTextures = GLContextDebugShims::glGenTextures;
    mSymbols.fGenTransformFeedbacks = GLContextDebugShims::glGenTransformFeedbacks;
    mSymbols.fGenVertexArrays = GLContextDebugShims::glGenVertexArrays;
    mSymbols.fGetActiveAttrib = GLContextDebugShims::glGetActiveAttrib;
    mSymbols.fGetActiveUniform = GLContextDebugShims::glGetActiveUniform;
    mSymbols.fGetActiveUniformBlockiv = GLContextDebugShims::glGetActiveUniformBlockiv;
    mSymbols.fGetActiveUniformBlockName = GLContextDebugShims::glGetActiveUniformBlockName;
    mSymbols.fGetActiveUniformsiv = GLContextDebugShims::glGetActiveUniformsiv;
    mSymbols.fGetAttachedShaders = GLContextDebugShims::glGetAttachedShaders;
    mSymbols.fGetAttribLocation = GLContextDebugShims::glGetAttribLocation;
    mSymbols.fGetBooleanv = GLContextDebugShims::glGetBooleanv;
    mSymbols.fGetBufferParameteriv = GLContextDebugShims::glGetBufferParameteriv;
    mSymbols.fGetDebugMessageLog = GLContextDebugShims::glGetDebugMessageLog;
    mSymbols.fGetError = GLContextDebugShims::glGetError;
    mSymbols.fGetFenceiv = GLContextDebugShims::glGetFenceiv;
    mSymbols.fGetFloatv = GLContextDebugShims::glGetFloatv;
    mSymbols.fGetFragDataLocation = GLContextDebugShims::glGetFragDataLocation;
    mSymbols.fGetFramebufferAttachmentParameteriv = GLContextDebugShims::glGetFramebufferAttachmentParameteriv;
    mSymbols.fGetGraphicsResetStatus = GLContextDebugShims::glGetGraphicsResetStatus;
    mSymbols.fGetInteger64i_v = GLContextDebugShims::glGetInteger64i_v;
    mSymbols.fGetInteger64v = GLContextDebugShims::glGetInteger64v;
    mSymbols.fGetIntegeri_v = GLContextDebugShims::glGetIntegeri_v;
    mSymbols.fGetIntegerv = GLContextDebugShims::glGetIntegerv;
    mSymbols.fGetInternalformativ = GLContextDebugShims::glGetInternalformativ;
    mSymbols.fGetObjectLabel = GLContextDebugShims::glGetObjectLabel;
    mSymbols.fGetObjectPtrLabel = GLContextDebugShims::glGetObjectPtrLabel;
    mSymbols.fGetPointerv = GLContextDebugShims::glGetPointerv;
    mSymbols.fGetProgramInfoLog = GLContextDebugShims::glGetProgramInfoLog;
    mSymbols.fGetProgramiv = GLContextDebugShims::glGetProgramiv;
    mSymbols.fGetQueryiv = GLContextDebugShims::glGetQueryiv;
    mSymbols.fGetQueryObjecti64v = GLContextDebugShims::glGetQueryObjecti64v;
    mSymbols.fGetQueryObjectiv = GLContextDebugShims::glGetQueryObjectiv;
    mSymbols.fGetQueryObjectui64v = GLContextDebugShims::glGetQueryObjectui64v;
    mSymbols.fGetQueryObjectuiv = GLContextDebugShims::glGetQueryObjectuiv;
    mSymbols.fGetRenderbufferParameteriv = GLContextDebugShims::glGetRenderbufferParameteriv;
    mSymbols.fGetSamplerParameterfv = GLContextDebugShims::glGetSamplerParameterfv;
    mSymbols.fGetSamplerParameteriv = GLContextDebugShims::glGetSamplerParameteriv;
    mSymbols.fGetShaderInfoLog = GLContextDebugShims::glGetShaderInfoLog;
    mSymbols.fGetShaderiv = GLContextDebugShims::glGetShaderiv;
    mSymbols.fGetShaderPrecisionFormat = GLContextDebugShims::glGetShaderPrecisionFormat;
    mSymbols.fGetShaderSource = GLContextDebugShims::glGetShaderSource;
    mSymbols.fGetString = GLContextDebugShims::glGetString;
    mSymbols.fGetStringi = GLContextDebugShims::glGetStringi;
    mSymbols.fGetSynciv = GLContextDebugShims::glGetSynciv;
    mSymbols.fGetTexImage = GLContextDebugShims::glGetTexImage;
    mSymbols.fGetTexLevelParameteriv = GLContextDebugShims::glGetTexLevelParameteriv;
    mSymbols.fGetTexParameterfv = GLContextDebugShims::glGetTexParameterfv;
    mSymbols.fGetTexParameteriv = GLContextDebugShims::glGetTexParameteriv;
    mSymbols.fGetTransformFeedbackVarying = GLContextDebugShims::glGetTransformFeedbackVarying;
    mSymbols.fGetUniformBlockIndex = GLContextDebugShims::glGetUniformBlockIndex;
    mSymbols.fGetUniformfv = GLContextDebugShims::glGetUniformfv;
    mSymbols.fGetUniformIndices = GLContextDebugShims::glGetUniformIndices;
    mSymbols.fGetUniformiv = GLContextDebugShims::glGetUniformiv;
    mSymbols.fGetUniformLocation = GLContextDebugShims::glGetUniformLocation;
    mSymbols.fGetUniformuiv = GLContextDebugShims::glGetUniformuiv;
    mSymbols.fGetVertexAttribfv = GLContextDebugShims::glGetVertexAttribfv;
    mSymbols.fGetVertexAttribIiv = GLContextDebugShims::glGetVertexAttribIiv;
    mSymbols.fGetVertexAttribIuiv = GLContextDebugShims::glGetVertexAttribIuiv;
    mSymbols.fGetVertexAttribiv = GLContextDebugShims::glGetVertexAttribiv;
    mSymbols.fGetVertexAttribPointerv = GLContextDebugShims::glGetVertexAttribPointerv;
    mSymbols.fHint = GLContextDebugShims::glHint;
    mSymbols.fInvalidateFramebuffer = GLContextDebugShims::glInvalidateFramebuffer;
    mSymbols.fInvalidateSubFramebuffer = GLContextDebugShims::glInvalidateSubFramebuffer;
    mSymbols.fIsBuffer = GLContextDebugShims::glIsBuffer;
    mSymbols.fIsEnabled = GLContextDebugShims::glIsEnabled;
    mSymbols.fIsFence = GLContextDebugShims::glIsFence;
    mSymbols.fIsFramebuffer = GLContextDebugShims::glIsFramebuffer;
    mSymbols.fIsProgram = GLContextDebugShims::glIsProgram;
    mSymbols.fIsQuery = GLContextDebugShims::glIsQuery;
    mSymbols.fIsRenderbuffer = GLContextDebugShims::glIsRenderbuffer;
    mSymbols.fIsSampler = GLContextDebugShims::glIsSampler;
    mSymbols.fIsShader = GLContextDebugShims::glIsShader;
    mSymbols.fIsSync = GLContextDebugShims::glIsSync;
    mSymbols.fIsTexture = GLContextDebugShims::glIsTexture;
    mSymbols.fIsTransformFeedback = GLContextDebugShims::glIsTransformFeedback;
    mSymbols.fIsVertexArray = GLContextDebugShims::glIsVertexArray;
    mSymbols.fLineWidth = GLContextDebugShims::glLineWidth;
    mSymbols.fLinkProgram = GLContextDebugShims::glLinkProgram;
    mSymbols.fLoadIdentity = GLContextDebugShims::glLoadIdentity;
    mSymbols.fLoadMatrixd = GLContextDebugShims::glLoadMatrixd;
    mSymbols.fLoadMatrixf = GLContextDebugShims::glLoadMatrixf;
    mSymbols.fMapBuffer = GLContextDebugShims::glMapBuffer;
    mSymbols.fMapBufferRange = GLContextDebugShims::glMapBufferRange;
    mSymbols.fMatrixMode = GLContextDebugShims::glMatrixMode;
    mSymbols.fObjectLabel = GLContextDebugShims::glObjectLabel;
    mSymbols.fObjectPtrLabel = GLContextDebugShims::glObjectPtrLabel;
    mSymbols.fPauseTransformFeedback = GLContextDebugShims::glPauseTransformFeedback;
    mSymbols.fPixelStorei = GLContextDebugShims::glPixelStorei;
    mSymbols.fPointParameterf = GLContextDebugShims::glPointParameterf;
    mSymbols.fPolygonOffset = GLContextDebugShims::glPolygonOffset;
    mSymbols.fPopDebugGroup = GLContextDebugShims::glPopDebugGroup;
    mSymbols.fPushDebugGroup = GLContextDebugShims::glPushDebugGroup;
    mSymbols.fQueryCounter = GLContextDebugShims::glQueryCounter;
    mSymbols.fReadBuffer = GLContextDebugShims::glReadBuffer;
    mSymbols.fReadPixels = GLContextDebugShims::glReadPixels;
    mSymbols.fRenderbufferStorage = GLContextDebugShims::glRenderbufferStorage;
    mSymbols.fRenderbufferStorageMultisample = GLContextDebugShims::glRenderbufferStorageMultisample;
    mSymbols.fResolveMultisampleFramebufferAPPLE = GLContextDebugShims::glResolveMultisampleFramebufferAPPLE;
    mSymbols.fResumeTransformFeedback = GLContextDebugShims::glResumeTransformFeedback;
    mSymbols.fSampleCoverage = GLContextDebugShims::glSampleCoverage;
    mSymbols.fSamplerParameterf = GLContextDebugShims::glSamplerParameterf;
    mSymbols.fSamplerParameterfv = GLContextDebugShims::glSamplerParameterfv;
    mSymbols.fSamplerParameteri = GLContextDebugShims::glSamplerParameteri;
    mSymbols.fSamplerParameteriv = GLContextDebugShims::glSamplerParameteriv;
    mSymbols.fScissor = GLContextDebugShims::glScissor;
    mSymbols.fSetFence = GLContextDebugShims::glSetFence;
    mSymbols.fShaderSource = GLContextDebugShims::glShaderSource;
    mSymbols.fStencilFunc = GLContextDebugShims::glStencilFunc;
    mSymbols.fStencilFuncSeparate = GLContextDebugShims::glStencilFuncSeparate;
    mSymbols.fStencilMask = GLContextDebugShims::glStencilMask;
    mSymbols.fStencilMaskSeparate = GLContextDebugShims::glStencilMaskSeparate;
    mSymbols.fStencilOp = GLContextDebugShims::glStencilOp;
    mSymbols.fStencilOpSeparate = GLContextDebugShims::glStencilOpSeparate;
    mSymbols.fTestFence = GLContextDebugShims::glTestFence;
    mSymbols.fTexGenf = GLContextDebugShims::glTexGenf;
    mSymbols.fTexGenfv = GLContextDebugShims::glTexGenfv;
    mSymbols.fTexGeni = GLContextDebugShims::glTexGeni;
    mSymbols.fTexImage2D = GLContextDebugShims::glTexImage2D;
    mSymbols.fTexImage3D = GLContextDebugShims::glTexImage3D;
    mSymbols.fTexParameterf = GLContextDebugShims::glTexParameterf;
    mSymbols.fTexParameteri = GLContextDebugShims::glTexParameteri;
    mSymbols.fTexParameteriv = GLContextDebugShims::glTexParameteriv;
    mSymbols.fTexStorage2D = GLContextDebugShims::glTexStorage2D;
    mSymbols.fTexStorage3D = GLContextDebugShims::glTexStorage3D;
    mSymbols.fTexSubImage2D = GLContextDebugShims::glTexSubImage2D;
    mSymbols.fTexSubImage3D = GLContextDebugShims::glTexSubImage3D;
    mSymbols.fTextureRangeAPPLE = GLContextDebugShims::glTextureRangeAPPLE;
    mSymbols.fTransformFeedbackVaryings = GLContextDebugShims::glTransformFeedbackVaryings;
    mSymbols.fUniform1f = GLContextDebugShims::glUniform1f;
    mSymbols.fUniform1fv = GLContextDebugShims::glUniform1fv;
    mSymbols.fUniform1i = GLContextDebugShims::glUniform1i;
    mSymbols.fUniform1iv = GLContextDebugShims::glUniform1iv;
    mSymbols.fUniform1ui = GLContextDebugShims::glUniform1ui;
    mSymbols.fUniform1uiv = GLContextDebugShims::glUniform1uiv;
    mSymbols.fUniform2f = GLContextDebugShims::glUniform2f;
    mSymbols.fUniform2fv = GLContextDebugShims::glUniform2fv;
    mSymbols.fUniform2i = GLContextDebugShims::glUniform2i;
    mSymbols.fUniform2iv = GLContextDebugShims::glUniform2iv;
    mSymbols.fUniform2ui = GLContextDebugShims::glUniform2ui;
    mSymbols.fUniform2uiv = GLContextDebugShims::glUniform2uiv;
    mSymbols.fUniform3f = GLContextDebugShims::glUniform3f;
    mSymbols.fUniform3fv = GLContextDebugShims::glUniform3fv;
    mSymbols.fUniform3i = GLContextDebugShims::glUniform3i;
    mSymbols.fUniform3iv = GLContextDebugShims::glUniform3iv;
    mSymbols.fUniform3ui = GLContextDebugShims::glUniform3ui;
    mSymbols.fUniform3uiv = GLContextDebugShims::glUniform3uiv;
    mSymbols.fUniform4f = GLContextDebugShims::glUniform4f;
    mSymbols.fUniform4fv = GLContextDebugShims::glUniform4fv;
    mSymbols.fUniform4i = GLContextDebugShims::glUniform4i;
    mSymbols.fUniform4iv = GLContextDebugShims::glUniform4iv;
    mSymbols.fUniform4ui = GLContextDebugShims::glUniform4ui;
    mSymbols.fUniform4uiv = GLContextDebugShims::glUniform4uiv;
    mSymbols.fUniformBlockBinding = GLContextDebugShims::glUniformBlockBinding;
    mSymbols.fUniformMatrix2fv = GLContextDebugShims::glUniformMatrix2fv;
    mSymbols.fUniformMatrix2x3fv = GLContextDebugShims::glUniformMatrix2x3fv;
    mSymbols.fUniformMatrix2x4fv = GLContextDebugShims::glUniformMatrix2x4fv;
    mSymbols.fUniformMatrix3fv = GLContextDebugShims::glUniformMatrix3fv;
    mSymbols.fUniformMatrix3x2fv = GLContextDebugShims::glUniformMatrix3x2fv;
    mSymbols.fUniformMatrix3x4fv = GLContextDebugShims::glUniformMatrix3x4fv;
    mSymbols.fUniformMatrix4fv = GLContextDebugShims::glUniformMatrix4fv;
    mSymbols.fUniformMatrix4x2fv = GLContextDebugShims::glUniformMatrix4x2fv;
    mSymbols.fUniformMatrix4x3fv = GLContextDebugShims::glUniformMatrix4x3fv;
    mSymbols.fUnmapBuffer = GLContextDebugShims::glUnmapBuffer;
    mSymbols.fUseProgram = GLContextDebugShims::glUseProgram;
    mSymbols.fValidateProgram = GLContextDebugShims::glValidateProgram;
    mSymbols.fVertexAttrib1f = GLContextDebugShims::glVertexAttrib1f;
    mSymbols.fVertexAttrib1fv = GLContextDebugShims::glVertexAttrib1fv;
    mSymbols.fVertexAttrib2f = GLContextDebugShims::glVertexAttrib2f;
    mSymbols.fVertexAttrib2fv = GLContextDebugShims::glVertexAttrib2fv;
    mSymbols.fVertexAttrib3f = GLContextDebugShims::glVertexAttrib3f;
    mSymbols.fVertexAttrib3fv = GLContextDebugShims::glVertexAttrib3fv;
    mSymbols.fVertexAttrib4f = GLContextDebugShims::glVertexAttrib4f;
    mSymbols.fVertexAttrib4fv = GLContextDebugShims::glVertexAttrib4fv;
    mSymbols.fVertexAttribDivisor = GLContextDebugShims::glVertexAttribDivisor;
    mSymbols.fVertexAttribI4i = GLContextDebugShims::glVertexAttribI4i;
    mSymbols.fVertexAttribI4iv = GLContextDebugShims::glVertexAttribI4iv;
    mSymbols.fVertexAttribI4ui = GLContextDebugShims::glVertexAttribI4ui;
    mSymbols.fVertexAttribI4uiv = GLContextDebugShims::glVertexAttribI4uiv;
    mSymbols.fVertexAttribIPointer = GLContextDebugShims::glVertexAttribIPointer;
    mSymbols.fVertexAttribPointer = GLContextDebugShims::glVertexAttribPointer;
    mSymbols.fVertexPointer = GLContextDebugShims::glVertexPointer;
    mSymbols.fViewport = GLContextDebugShims::glViewport;
    mSymbols.fWaitSync = GLContextDebugShims::glWaitSync;
}

} // namespace gl
} // namespace mozilla
