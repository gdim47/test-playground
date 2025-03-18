/*
 * Some tests for OpenGL functions
 *
 * Copyright (C) 2007-2008 Roderick Colenbrander
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <windows.h>
#include <wingdi.h>
#include "wine/test.h"
#include "wine/wgl.h"

#define MAX_FORMATS 256

/* WGL_ARB_create_context */
static HGLRC (WINAPI *pwglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext, const int *attribList);

/* WGL_ARB_extensions_string */
static const char* (WINAPI *pwglGetExtensionsStringARB)(HDC);
static int (WINAPI *pwglReleasePbufferDCARB)(HPBUFFERARB, HDC);

/* WGL_ARB_make_current_read */
static BOOL (WINAPI *pwglMakeContextCurrentARB)(HDC hdraw, HDC hread, HGLRC hglrc);
static HDC (WINAPI *pwglGetCurrentReadDCARB)(void);

/* WGL_ARB_pixel_format */
static BOOL (WINAPI *pwglChoosePixelFormatARB)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);
static BOOL (WINAPI *pwglGetPixelFormatAttribivARB)(HDC, int, int, UINT, const int *, int *);

/* WGL_ARB_pbuffer */
static HPBUFFERARB (WINAPI *pwglCreatePbufferARB)(HDC, int, int, int, const int *);
static HDC (WINAPI *pwglGetPbufferDCARB)(HPBUFFERARB);

/* WGL_EXT_swap_control */
static BOOL (WINAPI *pwglSwapIntervalEXT)(int interval);
static int (WINAPI *pwglGetSwapIntervalEXT)(void);

/* GL_ARB_debug_output */
static void (WINAPI *pglDebugMessageCallbackARB)(void *, void *);
static void (WINAPI *pglDebugMessageControlARB)(GLenum, GLenum, GLenum, GLsizei, const GLuint *, GLboolean);
static void (WINAPI *pglDebugMessageInsertARB)(GLenum, GLenum, GLuint, GLenum, GLsizei, const char *);

static const char* wgl_extensions = NULL;

static void init_functions(void)
{
#define GET_PROC(func) \
    p ## func = (void*)wglGetProcAddress(#func); \
    if(!p ## func) \
      trace("wglGetProcAddress(%s) failed\n", #func);

    /* WGL_ARB_create_context */
    GET_PROC(wglCreateContextAttribsARB);

    /* WGL_ARB_extensions_string */
    GET_PROC(wglGetExtensionsStringARB)

    /* WGL_ARB_make_current_read */
    GET_PROC(wglMakeContextCurrentARB);
    GET_PROC(wglGetCurrentReadDCARB);

    /* WGL_ARB_pixel_format */
    GET_PROC(wglChoosePixelFormatARB)
    GET_PROC(wglGetPixelFormatAttribivARB)

    /* WGL_ARB_pbuffer */
    GET_PROC(wglCreatePbufferARB)
    GET_PROC(wglGetPbufferDCARB)
    GET_PROC(wglReleasePbufferDCARB)

    /* WGL_EXT_swap_control */
    GET_PROC(wglSwapIntervalEXT)
    GET_PROC(wglGetSwapIntervalEXT)

    /* GL_ARB_debug_output */
    GET_PROC(glDebugMessageCallbackARB)
    GET_PROC(glDebugMessageControlARB)
    GET_PROC(glDebugMessageInsertARB)

#undef GET_PROC
}

static BOOL gl_extension_supported(const char *extensions, const char *extension_string)
{
    size_t ext_str_len = strlen(extension_string);

    while (*extensions)
    {
        const char *start;
        size_t len;

        while (isspace(*extensions))
            ++extensions;
        start = extensions;
        while (!isspace(*extensions) && *extensions)
            ++extensions;

        len = extensions - start;
        if (!len)
            continue;

        if (len == ext_str_len && !memcmp(start, extension_string, ext_str_len))
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void test_pbuffers(HDC hdc)
{
    const int iAttribList[] = { WGL_DRAW_TO_PBUFFER_ARB, 1, /* Request pbuffer support */
                                0 };
    int iFormats[MAX_FORMATS];
    unsigned int nOnscreenFormats;
    unsigned int nFormats;
    int i, res;
    int iPixelFormat = 0;

    nOnscreenFormats = DescribePixelFormat(hdc, 0, 0, NULL);

    /* When you want to render to a pbuffer you need to call wglGetPbufferDCARB which
     * returns a 'magic' HDC which you can then pass to wglMakeCurrent to switch rendering
     * to the pbuffer. Below some tests are performed on what happens if you use standard WGL calls
     * on this 'magic' HDC for both a pixelformat that support onscreen and offscreen rendering
     * and a pixelformat that's only available for offscreen rendering (this means that only
     * wglChoosePixelFormatARB and friends know about the format.
     *
     * The first thing we need are pixelformats with pbuffer capabilities.
     */
    res = pwglChoosePixelFormatARB(hdc, iAttribList, NULL, MAX_FORMATS, iFormats, &nFormats);
    if(res <= 0)
    {
        skip("No pbuffer compatible formats found while WGL_ARB_pbuffer is supported\n");
        return;
    }
    trace("nOnscreenFormats: %d\n", nOnscreenFormats);
    trace("Total number of pbuffer capable pixelformats: %d\n", nFormats);

    /* Try to select an onscreen pixelformat out of the list */
    for(i=0; i < nFormats; i++)
    {
        /* Check if the format is onscreen, if it is choose it */
        if(iFormats[i] <= nOnscreenFormats)
        {
            iPixelFormat = iFormats[i];
            trace("Selected iPixelFormat=%d\n", iPixelFormat);
            break;
        }
    }

    /* A video driver supports a large number of onscreen and offscreen pixelformats.
     * The traditional WGL calls only see a subset of the whole pixelformat list. First
     * of all they only see the onscreen formats (the offscreen formats are at the end of the
     * pixelformat list) and second extended pixelformat capabilities are hidden from the
     * standard WGL calls. Only functions that depend on WGL_ARB_pixel_format can see them.
     *
     * Below we check if the pixelformat is also supported onscreen.
     */
    if(iPixelFormat != 0)
    {
        HDC pbuffer_hdc;
        int attrib = 0;
        HPBUFFERARB pbuffer = pwglCreatePbufferARB(hdc, iPixelFormat, 640 /* width */, 480 /* height */, &attrib);
        if(!pbuffer)
            skip("Pbuffer creation failed!\n");

        /* Test the pixelformat returned by GetPixelFormat on a pbuffer as the behavior is not clear */
        pbuffer_hdc = pwglGetPbufferDCARB(pbuffer);
        res = GetPixelFormat(pbuffer_hdc);
        ok(res == iPixelFormat, "Unexpected iPixelFormat=%d returned by GetPixelFormat for format %d\n", res, iPixelFormat);
        trace("iPixelFormat returned by GetPixelFormat: %d\n", res);
        trace("PixelFormat from wglChoosePixelFormatARB: %d\n", iPixelFormat);

        pwglReleasePbufferDCARB(pbuffer, pbuffer_hdc);
    }
    else skip("Pbuffer test for onscreen pixelformat skipped as no onscreen format with pbuffer capabilities have been found\n");

    /* Search for a real offscreen format */
    for(i=0, iPixelFormat=0; i<nFormats; i++)
    {
        if(iFormats[i] > nOnscreenFormats)
        {
            iPixelFormat = iFormats[i];
            trace("Selected iPixelFormat: %d\n", iPixelFormat);
            break;
        }
    }

    if(iPixelFormat != 0)
    {
        HDC pbuffer_hdc;
        HPBUFFERARB pbuffer = pwglCreatePbufferARB(hdc, iPixelFormat, 640 /* width */, 480 /* height */, NULL);
        if(pbuffer)
        {
            /* Test the pixelformat returned by GetPixelFormat on a pbuffer as the behavior is not clear */
            pbuffer_hdc = pwglGetPbufferDCARB(pbuffer);
            res = GetPixelFormat(pbuffer_hdc);

            ok(res == 1, "Unexpected iPixelFormat=%d (1 expected) returned by GetPixelFormat for offscreen format %d\n", res, iPixelFormat);
            trace("iPixelFormat returned by GetPixelFormat: %d\n", res);
            trace("PixelFormat from wglChoosePixelFormatARB: %d\n", iPixelFormat);
            pwglReleasePbufferDCARB(pbuffer, hdc);
        }
        else skip("Pbuffer creation failed!\n");
    }
    else skip("Pbuffer test for offscreen pixelformat skipped as no offscreen-only format with pbuffer capabilities has been found\n");
}

static int test_pfd(const PIXELFORMATDESCRIPTOR *pfd, PIXELFORMATDESCRIPTOR *fmt)
{
    int pf;
    HDC hdc;
    HWND hwnd;

    hwnd = CreateWindowA("static", "Title", WS_OVERLAPPEDWINDOW, 10, 10, 200, 200, NULL, NULL,
            NULL, NULL);
    if (!hwnd)
        return 0;

    hdc = GetDC( hwnd );
    pf = ChoosePixelFormat( hdc, pfd );
    if (pf && fmt)
    {
        INT ret;
        memset(fmt, 0, sizeof(*fmt));
        ret = DescribePixelFormat( hdc, pf, sizeof(*fmt), fmt );
        ok(ret, "DescribePixelFormat failed with error: %lu\n", GetLastError());
    }
    ReleaseDC( hwnd, hdc );
    DestroyWindow( hwnd );

    return pf;
}

static void test_choosepixelformat(void)
{
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     /* version */
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL,
        PFD_TYPE_RGBA,
        0,                     /* color depth */
        0, 0, 0, 0, 0, 0,      /* color bits */
        0,                     /* alpha buffer */
        0,                     /* shift bit */
        0,                     /* accumulation buffer */
        0, 0, 0, 0,            /* accum bits */
        0,                     /* z-buffer */
        0,                     /* stencil buffer */
        0,                     /* auxiliary buffer */
        PFD_MAIN_PLANE,        /* main layer */
        0,                     /* reserved */
        0, 0, 0                /* layer masks */
    };
    PIXELFORMATDESCRIPTOR ret_fmt;

    ok( test_pfd(&pfd, NULL), "Simple pfd failed\n" );
    pfd.dwFlags |= PFD_DOUBLEBUFFER_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_DOUBLEBUFFER_DONTCARE failed\n" );
    pfd.dwFlags |= PFD_STEREO_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_DOUBLEBUFFER_DONTCARE|PFD_STEREO_DONTCARE failed\n" );
    pfd.dwFlags &= ~PFD_DOUBLEBUFFER_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_STEREO_DONTCARE failed\n" );
    pfd.dwFlags &= ~PFD_STEREO_DONTCARE;
    pfd.iPixelType = 32;
    ok( test_pfd(&pfd, &ret_fmt), "Invalid pixel format 32 failed\n" );
    ok( ret_fmt.iPixelType == PFD_TYPE_RGBA, "Expected pixel type PFD_TYPE_RGBA, got %d\n", ret_fmt.iPixelType );
    pfd.iPixelType = 33;
    ok( test_pfd(&pfd, &ret_fmt), "Invalid pixel format 33 failed\n" );
    ok( ret_fmt.iPixelType == PFD_TYPE_RGBA, "Expected pixel type PFD_TYPE_RGBA, got %d\n", ret_fmt.iPixelType );
    pfd.iPixelType = 15;
    ok( test_pfd(&pfd, &ret_fmt), "Invalid pixel format 15 failed\n" );
    ok( ret_fmt.iPixelType == PFD_TYPE_RGBA, "Expected pixel type PFD_TYPE_RGBA, got %d\n", ret_fmt.iPixelType );
    pfd.iPixelType = PFD_TYPE_RGBA;

    pfd.cColorBits = 32;
    ok( test_pfd(&pfd, &ret_fmt), "Simple pfd failed\n" );
    ok( ret_fmt.cColorBits == 32, "Got %u.\n", ret_fmt.cColorBits );
    ok( !ret_fmt.cBlueShift, "Got %u.\n", ret_fmt.cBlueShift );
    ok( ret_fmt.cBlueBits == 8, "Got %u.\n", ret_fmt.cBlueBits );
    ok( ret_fmt.cRedBits == 8, "Got %u.\n", ret_fmt.cRedBits );
    ok( ret_fmt.cGreenBits == 8, "Got %u.\n", ret_fmt.cGreenBits );
    ok( ret_fmt.cGreenShift == 8, "Got %u.\n", ret_fmt.cGreenShift );
    ok( ret_fmt.cRedShift == 16, "Got %u.\n", ret_fmt.cRedShift );
    ok( !ret_fmt.cAlphaBits || ret_fmt.cAlphaBits == 8, "Got %u.\n", ret_fmt.cAlphaBits );
    if (ret_fmt.cAlphaBits)
        ok( ret_fmt.cAlphaShift == 24, "Got %u.\n", ret_fmt.cAlphaShift );
    else
        ok( !ret_fmt.cAlphaShift, "Got %u.\n", ret_fmt.cAlphaShift );
    ok( ret_fmt.cDepthBits, "Got %u.\n", ret_fmt.cDepthBits );

    pfd.dwFlags |= PFD_DOUBLEBUFFER_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_DOUBLEBUFFER_DONTCARE failed\n" );
    pfd.dwFlags |= PFD_STEREO_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_DOUBLEBUFFER_DONTCARE|PFD_STEREO_DONTCARE failed\n" );
    pfd.dwFlags &= ~PFD_DOUBLEBUFFER_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_STEREO_DONTCARE failed\n" );
    pfd.dwFlags &= ~PFD_STEREO_DONTCARE;
    pfd.cColorBits = 0;

    pfd.cAlphaBits = 8;
    ok( test_pfd(&pfd, NULL), "Simple pfd failed\n" );
    pfd.dwFlags |= PFD_DOUBLEBUFFER_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_DOUBLEBUFFER_DONTCARE failed\n" );
    pfd.dwFlags |= PFD_STEREO_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_DOUBLEBUFFER_DONTCARE|PFD_STEREO_DONTCARE failed\n" );
    pfd.dwFlags &= ~PFD_DOUBLEBUFFER_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_STEREO_DONTCARE failed\n" );
    pfd.dwFlags &= ~PFD_STEREO_DONTCARE;
    pfd.cAlphaBits = 0;

    pfd.cStencilBits = 8;
    ok( test_pfd(&pfd, NULL), "Simple pfd failed\n" );
    pfd.dwFlags |= PFD_DOUBLEBUFFER_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_DOUBLEBUFFER_DONTCARE failed\n" );
    pfd.dwFlags |= PFD_STEREO_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_DOUBLEBUFFER_DONTCARE|PFD_STEREO_DONTCARE failed\n" );
    pfd.dwFlags &= ~PFD_DOUBLEBUFFER_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_STEREO_DONTCARE failed\n" );
    pfd.dwFlags &= ~PFD_STEREO_DONTCARE;
    pfd.cStencilBits = 0;

    pfd.cAuxBuffers = 1;
    ok( test_pfd(&pfd, NULL), "Simple pfd failed\n" );
    pfd.dwFlags |= PFD_DOUBLEBUFFER_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_DOUBLEBUFFER_DONTCARE failed\n" );
    pfd.dwFlags |= PFD_STEREO_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_DOUBLEBUFFER_DONTCARE|PFD_STEREO_DONTCARE failed\n" );
    pfd.dwFlags &= ~PFD_DOUBLEBUFFER_DONTCARE;
    ok( test_pfd(&pfd, NULL), "PFD_STEREO_DONTCARE failed\n" );
    pfd.dwFlags &= ~PFD_STEREO_DONTCARE;
    pfd.cAuxBuffers = 0;

    pfd.dwFlags |= PFD_DEPTH_DONTCARE;
    pfd.cDepthBits = 24;
    ok( test_pfd(&pfd, &ret_fmt), "PFD_DEPTH_DONTCARE failed.\n" );
    ok( !ret_fmt.cDepthBits, "Got unexpected cDepthBits %u.\n", ret_fmt.cDepthBits );
    pfd.cStencilBits = 8;
    ok( test_pfd(&pfd, &ret_fmt), "PFD_DEPTH_DONTCARE, depth 24, stencil 8 failed.\n" );
    ok( !ret_fmt.cDepthBits || ret_fmt.cDepthBits == 24, "Got unexpected cDepthBits %u.\n", ret_fmt.cDepthBits );
    ok( ret_fmt.cStencilBits == 8, "Got unexpected cStencilBits %u.\n", ret_fmt.cStencilBits );
    pfd.cDepthBits = 0;
    pfd.cStencilBits = 0;
    pfd.dwFlags &= ~PFD_DEPTH_DONTCARE;

    pfd.cDepthBits = 16;
    ok( test_pfd(&pfd, &ret_fmt), "depth 16 failed.\n" );
    ok( ret_fmt.cDepthBits >= 16, "Got unexpected cDepthBits %u.\n", ret_fmt.cDepthBits );
    pfd.cDepthBits = 0;

    pfd.cDepthBits = 16;
    pfd.cStencilBits = 8;
    ok( test_pfd(&pfd, &ret_fmt), "depth 16, stencil 8 failed.\n" );
    ok( ret_fmt.cDepthBits >= 16, "Got unexpected cDepthBits %u.\n", ret_fmt.cDepthBits );
    ok( ret_fmt.cStencilBits == 8, "Got unexpected cStencilBits %u.\n", ret_fmt.cStencilBits );
    pfd.cDepthBits = 0;
    pfd.cStencilBits = 0;

    pfd.cDepthBits = 8;
    pfd.cStencilBits = 8;
    ok( test_pfd(&pfd, &ret_fmt), "depth 8, stencil 8 failed.\n" );
    ok( ret_fmt.cDepthBits >= 8, "Got unexpected cDepthBits %u.\n", ret_fmt.cDepthBits );
    ok( ret_fmt.cStencilBits == 8, "Got unexpected cStencilBits %u.\n", ret_fmt.cStencilBits );
    pfd.cDepthBits = 0;
    pfd.cStencilBits = 0;

    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    ok( test_pfd(&pfd, &ret_fmt), "depth 24, stencil 8 failed.\n" );
    ok( ret_fmt.cDepthBits >= 24, "Got unexpected cDepthBits %u.\n", ret_fmt.cDepthBits );
    ok( ret_fmt.cStencilBits == 8, "Got unexpected cStencilBits %u.\n", ret_fmt.cStencilBits );
    pfd.cDepthBits = 0;
    pfd.cStencilBits = 0;

    pfd.cDepthBits = 32;
    pfd.cStencilBits = 8;
    ok( test_pfd(&pfd, &ret_fmt), "depth 32, stencil 8 failed.\n" );
    ok( ret_fmt.cDepthBits >= 24, "Got unexpected cDepthBits %u.\n", ret_fmt.cDepthBits );
    ok( ret_fmt.cStencilBits == 8, "Got unexpected cStencilBits %u.\n", ret_fmt.cStencilBits );
    pfd.cDepthBits = 0;
    pfd.cStencilBits = 0;

    pfd.cStencilBits = 8;
    ok( test_pfd(&pfd, &ret_fmt), "depth 32, stencil 8 failed.\n" );
    ok( ret_fmt.cStencilBits == 8, "Got unexpected cStencilBits %u.\n", ret_fmt.cStencilBits );
    pfd.cStencilBits = 0;

    pfd.cDepthBits = 1;
    pfd.cStencilBits = 8;
    ok( test_pfd(&pfd, &ret_fmt), "depth 32, stencil 8 failed.\n" );
    ok( ret_fmt.cStencilBits == 8, "Got unexpected cStencilBits %u.\n", ret_fmt.cStencilBits );
    pfd.cStencilBits = 0;
    pfd.cDepthBits = 0;
}

static void test_choosepixelformat_flag_is_ignored_when_unset(DWORD flag)
{
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     /* version */
        flag,
        PFD_TYPE_RGBA,
        0,                     /* color depth */
        0, 0, 0, 0, 0, 0,      /* color bits */
        0,                     /* alpha buffer */
        0,                     /* shift bit */
        0,                     /* accumulation buffer */
        0, 0, 0, 0,            /* accum bits */
        0,                     /* z-buffer */
        0,                     /* stencil buffer */
        0,                     /* auxiliary buffer */
        PFD_MAIN_PLANE,        /* main layer */
        0,                     /* reserved */
        0, 0, 0                /* layer masks */
    };
    PIXELFORMATDESCRIPTOR ret_fmt;
    int set_idx;
    int clear_idx;

    set_idx = test_pfd(&pfd, &ret_fmt);
    if (set_idx > 0)
    {
        ok( ret_fmt.dwFlags & flag, "flag 0x%08lu not set\n", flag );
        /* now search for that pixel format with the flag cleared: */
        pfd = ret_fmt;
        pfd.dwFlags &= ~flag;
        clear_idx = test_pfd(&pfd, &ret_fmt);
        ok( set_idx == clear_idx, "flag 0x%08lu matched different pixel formats when set vs cleared\n", flag );
        ok( ret_fmt.dwFlags & flag, "flag 0x%08lu not still set\n", flag );
    } else skip( "couldn't find a pixel format with flag 0x%08lu\n", flag );
}

static void WINAPI gl_debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                             GLsizei length, const GLchar *message, const void *userParam)
{
    DWORD *count = (DWORD *)userParam;
    (*count)++;
}

static void test_debug_message_callback(void)
{
    static const char testmsg[] = "Hello World";
    DWORD count;

    if (!pglDebugMessageCallbackARB)
    {
        skip("glDebugMessageCallbackARB not supported\n");
        return;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    pglDebugMessageCallbackARB(gl_debug_message_callback, &count);
    pglDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

    count = 0;
    pglDebugMessageInsertARB(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0x42424242,
                             GL_DEBUG_SEVERITY_LOW, sizeof(testmsg), testmsg);
    ok(count == 1, "expected count == 1, got %lu\n", count);

    glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDisable(GL_DEBUG_OUTPUT);
}

static void test_setpixelformat(HDC winhdc)
{
    int res = 0;
    int nCfgs;
    int pf;
    int i;
    HWND hwnd;
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     /* version */
        PFD_DRAW_TO_WINDOW |
        PFD_SUPPORT_OPENGL |
        PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,                    /* 24-bit color depth */
        0, 0, 0, 0, 0, 0,      /* color bits */
        0,                     /* alpha buffer */
        0,                     /* shift bit */
        0,                     /* accumulation buffer */
        0, 0, 0, 0,            /* accum bits */
        32,                    /* z-buffer */
        0,                     /* stencil buffer */
        0,                     /* auxiliary buffer */
        PFD_MAIN_PLANE,        /* main layer */
        0,                     /* reserved */
        0, 0, 0                /* layer masks */
    };

    HDC hdc = GetDC(0);
    ok(hdc != 0, "GetDC(0) failed!\n");

    /* This should pass even on the main device context */
    pf = ChoosePixelFormat(hdc, &pfd);
    ok(pf != 0, "ChoosePixelFormat failed on main device context\n");

    /* SetPixelFormat on the main device context 'X root window' should fail,
     * but some broken drivers allow it
     */
    res = SetPixelFormat(hdc, pf, &pfd);
    trace("SetPixelFormat on main device context %s\n", res ? "succeeded" : "failed");

    /* Setting the same format that was set on the HDC is allowed; other
       formats fail */
    nCfgs = DescribePixelFormat(winhdc, 0, 0, NULL);
    pf = GetPixelFormat(winhdc);
    for(i = 1;i <= nCfgs;i++)
    {
        int res = SetPixelFormat(winhdc, i, NULL);
        if(i == pf) ok(res, "Failed to set the same pixel format\n");
        else ok(!res, "Unexpectedly set an alternate pixel format\n");
    }

    hwnd = CreateWindowA("static", "Title", WS_OVERLAPPEDWINDOW, 10, 10, 200, 200, NULL, NULL,
            NULL, NULL);
    ok(hwnd != NULL, "err: %ld\n", GetLastError());
    if (hwnd)
    {
        HDC hdc = GetDC( hwnd );
        pf = ChoosePixelFormat( hdc, &pfd );
        ok( pf != 0, "ChoosePixelFormat failed\n" );
        res = SetPixelFormat( hdc, pf, &pfd );
        ok( res != 0, "SetPixelFormat failed\n" );
        i = GetPixelFormat( hdc );
        ok( i == pf, "GetPixelFormat returned wrong format %d/%d\n", i, pf );
        ReleaseDC( hwnd, hdc );
        hdc = GetWindowDC( hwnd );
        i = GetPixelFormat( hdc );
        ok( i == pf, "GetPixelFormat returned wrong format %d/%d\n", i, pf );
        ReleaseDC( hwnd, hdc );
        DestroyWindow( hwnd );
        /* check various calls with invalid hdc */
        SetLastError( 0xdeadbeef );
        i = GetPixelFormat( hdc );
        ok( i == 0, "GetPixelFormat succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_PIXEL_FORMAT, "wrong error %lu\n", GetLastError() );
        SetLastError( 0xdeadbeef );
        res = SetPixelFormat( hdc, pf, &pfd );
        ok( !res, "SetPixelFormat succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_HANDLE, "wrong error %lu\n", GetLastError() );
        SetLastError( 0xdeadbeef );
        res = DescribePixelFormat( hdc, 0, 0, NULL );
        ok( !res, "DescribePixelFormat succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_HANDLE, "wrong error %lu\n", GetLastError() );
        SetLastError( 0xdeadbeef );
        pf = ChoosePixelFormat( hdc, &pfd );
        ok( !pf, "ChoosePixelFormat succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_HANDLE, "wrong error %lu\n", GetLastError() );
        SetLastError( 0xdeadbeef );
        res = SwapBuffers( hdc );
        ok( !res, "SwapBuffers succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_HANDLE, "wrong error %lu\n", GetLastError() );
        SetLastError( 0xdeadbeef );
        ok( !wglCreateContext( hdc ), "CreateContext succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_HANDLE, "wrong error %lu\n", GetLastError() );
    }

    hwnd = CreateWindowA("static", "Title", WS_OVERLAPPEDWINDOW, 10, 10, 200, 200, NULL, NULL,
            NULL, NULL);
    ok(hwnd != NULL, "err: %ld\n", GetLastError());
    if (hwnd)
    {
        HDC hdc = GetWindowDC( hwnd );
        pf = ChoosePixelFormat( hdc, &pfd );
        ok( pf != 0, "ChoosePixelFormat failed\n" );
        res = SetPixelFormat( hdc, pf, &pfd );
        ok( res != 0, "SetPixelFormat failed\n" );
        i = GetPixelFormat( hdc );
        ok( i == pf, "GetPixelFormat returned wrong format %d/%d\n", i, pf );
        ReleaseDC( hwnd, hdc );
        DestroyWindow( hwnd );
    }
}

static void test_sharelists(HDC winhdc)
{
    BOOL res, nvidia, amd, source_current, source_sharing, dest_current, dest_sharing;
    HGLRC source, dest, other;

    res = wglShareLists(NULL, NULL);
    ok(!res, "Sharing display lists for no contexts passed!\n");

    nvidia = !!strstr((const char*)glGetString(GL_VENDOR), "NVIDIA");
    amd = strstr((const char*)glGetString(GL_VENDOR), "AMD") ||
          strstr((const char*)glGetString(GL_VENDOR), "ATI");

    for (source_current = FALSE; source_current <= TRUE; source_current++)
    {
        for (source_sharing = FALSE; source_sharing <= TRUE; source_sharing++)
        {
            for (dest_current = FALSE; dest_current <= TRUE; dest_current++)
            {
                for (dest_sharing = FALSE; dest_sharing <= TRUE; dest_sharing++)
                {
                    winetest_push_context("source_current=%d source_sharing=%d dest_current=%d dest_sharing=%d",
                                          source_current, source_sharing, dest_current, dest_sharing);

                    source = wglCreateContext(winhdc);
                    ok(!!source, "Create source context failed\n");
                    dest = wglCreateContext(winhdc);
                    ok(!!dest, "Create dest context failed\n");
                    other = wglCreateContext(winhdc);
                    ok(!!other, "Create other context failed\n");

                    if (source_current)
                    {
                        res = wglMakeCurrent(winhdc, source);
                        ok(res, "Make source current failed\n");
                    }
                    if (source_sharing)
                    {
                        res = wglShareLists(other, source);
                        ok(res, "Sharing of display lists from other to source failed\n");
                    }
                    if (dest_current)
                    {
                        res = wglMakeCurrent(winhdc, dest);
                        ok(res, "Make dest current failed\n");
                    }
                    if (dest_sharing)
                    {
                        res = wglShareLists(other, dest);
                        todo_wine_if(source_sharing && dest_current)
                        ok(res, "Sharing of display lists from other to dest failed\n");
                    }

                    res = wglShareLists(source, dest);
                    todo_wine_if((source_current || source_sharing) && (dest_current || dest_sharing))
                    ok(res || broken(nvidia && !source_sharing && dest_sharing),
                       "Sharing of display lists from source to dest failed\n");

                    if (source_current || dest_current)
                    {
                        res = wglMakeCurrent(NULL, NULL);
                        ok(res, "Make none current failed\n");
                    }
                    res = wglDeleteContext(source);
                    ok(res, "Delete source context failed\n");
                    res = wglDeleteContext(dest);
                    ok(res, "Delete dest context failed\n");
                    if (!strcmp(winetest_platform, "wine") || !amd || source_sharing || !dest_sharing)
                    {
                        /* If source_sharing=FALSE and dest_sharing=TRUE, wglShareLists succeeds on AMD, but
                         * sometimes wglDeleteContext crashes afterwards. On Wine, both functions should always
                         * succeed in this case. */
                        res = wglDeleteContext(other);
                        ok(res, "Delete other context failed\n");
                    }

                    winetest_pop_context();
                }
            }
        }
    }
}

static void test_makecurrent(HDC winhdc)
{
    BOOL ret;
    HGLRC hglrc;

    hglrc = wglCreateContext(winhdc);
    ok( hglrc != 0, "wglCreateContext failed\n" );

    ret = wglMakeCurrent( winhdc, hglrc );
    ok( ret, "wglMakeCurrent failed\n" );

    ok( wglGetCurrentContext() == hglrc, "wrong context\n" );

    /* set the same context again */
    ret = wglMakeCurrent( winhdc, hglrc );
    ok( ret, "wglMakeCurrent failed\n" );

    /* check wglMakeCurrent(x, y) after another call to wglMakeCurrent(x, y) */
    ret = wglMakeCurrent( winhdc, NULL );
    ok( ret, "wglMakeCurrent failed\n" );

    ret = wglMakeCurrent( winhdc, NULL );
    ok( ret, "wglMakeCurrent failed\n" );

    SetLastError( 0xdeadbeef );
    ret = wglMakeCurrent( NULL, NULL );
    ok( !ret || broken(ret) /* nt4 */, "wglMakeCurrent succeeded\n" );
    if (!ret) ok( GetLastError() == ERROR_INVALID_HANDLE,
                  "Expected ERROR_INVALID_HANDLE, got error=%lx\n", GetLastError() );

    ret = wglMakeCurrent( winhdc, NULL );
    ok( ret, "wglMakeCurrent failed\n" );

    ret = wglMakeCurrent( winhdc, hglrc );
    ok( ret, "wglMakeCurrent failed\n" );

    ret = wglMakeCurrent( NULL, NULL );
    ok( ret, "wglMakeCurrent failed\n" );

    ok( wglGetCurrentContext() == NULL, "wrong context\n" );

    SetLastError( 0xdeadbeef );
    ret = wglMakeCurrent( NULL, NULL );
    ok( !ret || broken(ret) /* nt4 */, "wglMakeCurrent succeeded\n" );
    if (!ret) ok( GetLastError() == ERROR_INVALID_HANDLE,
                  "Expected ERROR_INVALID_HANDLE, got error=%lx\n", GetLastError() );

    ret = wglMakeCurrent( winhdc, hglrc );
    ok( ret, "wglMakeCurrent failed\n" );
}

static void test_colorbits(HDC hdc)
{
    const int iAttribList[] = { WGL_COLOR_BITS_ARB, WGL_RED_BITS_ARB, WGL_GREEN_BITS_ARB,
                                WGL_BLUE_BITS_ARB, WGL_ALPHA_BITS_ARB, WGL_BLUE_SHIFT_ARB, WGL_GREEN_SHIFT_ARB,
                                WGL_RED_SHIFT_ARB, WGL_ALPHA_SHIFT_ARB, };
    int iAttribRet[ARRAY_SIZE(iAttribList)];
    const int iAttribs[] = { WGL_ALPHA_BITS_ARB, 1, 0 };
    unsigned int nFormats;
    BOOL res;
    int iPixelFormat = 0;

    if (!pwglChoosePixelFormatARB)
    {
        win_skip("wglChoosePixelFormatARB is not available\n");
        return;
    }

    /* We need a pixel format with at least one bit of alpha */
    res = pwglChoosePixelFormatARB(hdc, iAttribs, NULL, 1, &iPixelFormat, &nFormats);
    if(res == FALSE || nFormats == 0)
    {
        skip("No suitable pixel formats found\n");
        return;
    }

    res = pwglGetPixelFormatAttribivARB(hdc, iPixelFormat, 0, ARRAY_SIZE(iAttribList), iAttribList,
            iAttribRet);
    if(res == FALSE)
    {
        skip("wglGetPixelFormatAttribivARB failed\n");
        return;
    }
    ok(!iAttribRet[5], "got %d.\n", iAttribRet[5]);
    ok(iAttribRet[6] == iAttribRet[3], "got %d.\n", iAttribRet[6]);
    ok(iAttribRet[7] == iAttribRet[6] + iAttribRet[2], "got %d.\n", iAttribRet[7]);
    ok(iAttribRet[8] == iAttribRet[7] + iAttribRet[1], "got %d.\n", iAttribRet[8]);

    iAttribRet[1] += iAttribRet[2]+iAttribRet[3]+iAttribRet[4];
    ok(iAttribRet[0] == iAttribRet[1], "WGL_COLOR_BITS_ARB (%d) does not equal R+G+B+A (%d)!\n",
                                       iAttribRet[0], iAttribRet[1]);
}

static void test_gdi_dbuf(HDC hdc)
{
    const int iAttribList[] = { WGL_SUPPORT_GDI_ARB, WGL_DOUBLE_BUFFER_ARB };
    int iAttribRet[ARRAY_SIZE(iAttribList)];
    unsigned int nFormats;
    int iPixelFormat;
    BOOL res;

    if (!pwglGetPixelFormatAttribivARB)
    {
        win_skip("wglGetPixelFormatAttribivARB is not available\n");
        return;
    }

    nFormats = DescribePixelFormat(hdc, 0, 0, NULL);
    for(iPixelFormat = 1;iPixelFormat <= nFormats;iPixelFormat++)
    {
        res = pwglGetPixelFormatAttribivARB(hdc, iPixelFormat, 0, ARRAY_SIZE(iAttribList),
                iAttribList, iAttribRet);
        ok(res!=FALSE, "wglGetPixelFormatAttribivARB failed for pixel format %d\n", iPixelFormat);
        if(res == FALSE)
            continue;

        ok(!(iAttribRet[0] && iAttribRet[1]), "GDI support and double buffering on pixel format %d\n", iPixelFormat);
    }
}

static void test_acceleration(HDC hdc)
{
    const int iAttribList[] = { WGL_ACCELERATION_ARB };
    int iAttribRet[ARRAY_SIZE(iAttribList)];
    unsigned int nFormats;
    int iPixelFormat;
    int res;
    PIXELFORMATDESCRIPTOR pfd;

    if (!pwglGetPixelFormatAttribivARB)
    {
        win_skip("wglGetPixelFormatAttribivARB is not available\n");
        return;
    }

    nFormats = DescribePixelFormat(hdc, 0, 0, NULL);
    for(iPixelFormat = 1; iPixelFormat <= nFormats; iPixelFormat++)
    {
        res = pwglGetPixelFormatAttribivARB(hdc, iPixelFormat, 0, ARRAY_SIZE(iAttribList),
                iAttribList, iAttribRet);
        ok(res!=FALSE, "wglGetPixelFormatAttribivARB failed for pixel format %d\n", iPixelFormat);
        if(res == FALSE)
            continue;

        memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
        DescribePixelFormat(hdc, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

        switch(iAttribRet[0])
        {
            case WGL_NO_ACCELERATION_ARB:
                ok( (pfd.dwFlags & (PFD_GENERIC_FORMAT | PFD_GENERIC_ACCELERATED)) == PFD_GENERIC_FORMAT , "Expected only PFD_GENERIC_FORMAT to be set for WGL_NO_ACCELERATION_ARB!: iPixelFormat=%d, dwFlags=%lx!\n", iPixelFormat, pfd.dwFlags);
                break;
            case WGL_GENERIC_ACCELERATION_ARB:
                ok( (pfd.dwFlags & (PFD_GENERIC_FORMAT | PFD_GENERIC_ACCELERATED)) == (PFD_GENERIC_FORMAT | PFD_GENERIC_ACCELERATED), "Expected both PFD_GENERIC_FORMAT and PFD_GENERIC_ACCELERATION to be set for WGL_GENERIC_ACCELERATION_ARB: iPixelFormat=%d, dwFlags=%lx!\n", iPixelFormat, pfd.dwFlags);
                break;
            case WGL_FULL_ACCELERATION_ARB:
                ok( (pfd.dwFlags & (PFD_GENERIC_FORMAT | PFD_GENERIC_ACCELERATED)) == 0, "Expected no PFD_GENERIC_FORMAT/_ACCELERATION to be set for WGL_FULL_ACCELERATION_ARB: iPixelFormat=%d, dwFlags=%lx!\n", iPixelFormat, pfd.dwFlags);
                break;
        }
    }
}

static void test_bitmap_rendering( BOOL use_dib )
{
    PIXELFORMATDESCRIPTOR pfd;
    int i, ret, bpp, iPixelFormat=0;
    unsigned int nFormats;
    HGLRC hglrc, hglrc2;
    BITMAPINFO biDst;
    HBITMAP bmpDst, oldDst, bmp2;
    HDC hdcDst, hdcScreen;
    UINT *dstBuffer = NULL;

    hdcScreen = CreateCompatibleDC(0);
    hdcDst = CreateCompatibleDC(0);

    if (use_dib)
    {
        bpp = 32;
        memset(&biDst, 0, sizeof(BITMAPINFO));
        biDst.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        biDst.bmiHeader.biWidth = 4;
        biDst.bmiHeader.biHeight = -4;
        biDst.bmiHeader.biPlanes = 1;
        biDst.bmiHeader.biBitCount = 32;
        biDst.bmiHeader.biCompression = BI_RGB;

        bmpDst = CreateDIBSection(0, &biDst, DIB_RGB_COLORS, (void**)&dstBuffer, NULL, 0);

        biDst.bmiHeader.biWidth = 12;
        biDst.bmiHeader.biHeight = -12;
        biDst.bmiHeader.biBitCount = 16;
        bmp2 = CreateDIBSection(0, &biDst, DIB_RGB_COLORS, NULL, NULL, 0);
    }
    else
    {
        bpp = GetDeviceCaps( hdcScreen, BITSPIXEL );
        bmpDst = CreateBitmap( 4, 4, 1, bpp, NULL );
        bmp2 = CreateBitmap( 12, 12, 1, bpp, NULL );
    }

    oldDst = SelectObject(hdcDst, bmpDst);

    trace( "testing on %s\n", use_dib ? "DIB" : "DDB" );

    /* Pick a pixel format by hand because ChoosePixelFormat is unreliable */
    nFormats = DescribePixelFormat(hdcDst, 0, 0, NULL);
    for(i=1; i<=nFormats; i++)
    {
        memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
        DescribePixelFormat(hdcDst, i, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

        if((pfd.dwFlags & PFD_DRAW_TO_BITMAP) &&
           (pfd.dwFlags & PFD_SUPPORT_OPENGL) &&
           (pfd.cColorBits == bpp) &&
           (pfd.cAlphaBits == 8) )
        {
            iPixelFormat = i;
            break;
        }
    }

    if(!iPixelFormat)
    {
        skip("Unable to find a suitable pixel format\n");
    }
    else
    {
        ret = SetPixelFormat(hdcDst, iPixelFormat, &pfd);
        ok( ret, "SetPixelFormat failed\n" );
        ret = GetPixelFormat( hdcDst );
        ok( ret == iPixelFormat, "GetPixelFormat returned %d/%d\n", ret, iPixelFormat );
        ret = SetPixelFormat(hdcDst, iPixelFormat + 1, &pfd);
        ok( !ret, "SetPixelFormat succeeded\n" );
        hglrc = wglCreateContext(hdcDst);
        ok(hglrc != NULL, "Unable to create a context\n");

        if(hglrc)
        {
            GLint viewport[4];
            wglMakeCurrent(hdcDst, hglrc);
            hglrc2 = wglCreateContext(hdcDst);
            ok(hglrc2 != NULL, "Unable to create a context\n");

            /* Note this is RGBA but we read ARGB back */
            glClearColor((float)0x22/0xff, (float)0x33/0xff, (float)0x44/0xff, (float)0x11/0xff);
            glClear(GL_COLOR_BUFFER_BIT);
            glGetIntegerv( GL_VIEWPORT, viewport );
            glFinish();

            ok( viewport[0] == 0 && viewport[1] == 0 && viewport[2] == 4 && viewport[3] == 4,
                "wrong viewport %d,%d,%d,%d\n", viewport[0], viewport[1], viewport[2], viewport[3] );
            /* Note apparently the alpha channel is not supported by the software renderer (bitmap only works using software) */
            if (dstBuffer)
                for (i = 0; i < 16; i++)
                    ok(dstBuffer[i] == 0x223344 || dstBuffer[i] == 0x11223344, "Received color=%x at %u\n",
                       dstBuffer[i], i);

            SelectObject(hdcDst, bmp2);
            ret = GetPixelFormat( hdcDst );
            ok( ret == iPixelFormat, "GetPixelFormat returned %d/%d\n", ret, iPixelFormat );
            ret = SetPixelFormat(hdcDst, iPixelFormat + 1, &pfd);
            ok( !ret, "SetPixelFormat succeeded\n" );

            /* context still uses the old pixel format and viewport */
            glClearColor((float)0x44/0xff, (float)0x33/0xff, (float)0x22/0xff, (float)0x11/0xff);
            glClear(GL_COLOR_BUFFER_BIT);
            glFinish();
            glGetIntegerv( GL_VIEWPORT, viewport );
            ok( viewport[0] == 0 && viewport[1] == 0 && viewport[2] == 4 && viewport[3] == 4,
                "wrong viewport %d,%d,%d,%d\n", viewport[0], viewport[1], viewport[2], viewport[3] );

            wglMakeCurrent(NULL, NULL);
            wglMakeCurrent(hdcDst, hglrc);
            glClearColor((float)0x44/0xff, (float)0x55/0xff, (float)0x66/0xff, (float)0x11/0xff);
            glClear(GL_COLOR_BUFFER_BIT);
            glFinish();
            glGetIntegerv( GL_VIEWPORT, viewport );
            ok( viewport[0] == 0 && viewport[1] == 0 && viewport[2] == 4 && viewport[3] == 4,
                "wrong viewport %d,%d,%d,%d\n", viewport[0], viewport[1], viewport[2], viewport[3] );

            wglMakeCurrent(hdcDst, hglrc2);
            glGetIntegerv( GL_VIEWPORT, viewport );
            ok( viewport[0] == 0 && viewport[1] == 0 && viewport[2] == 12 && viewport[3] == 12,
                "wrong viewport %d,%d,%d,%d\n", viewport[0], viewport[1], viewport[2], viewport[3] );

            wglMakeCurrent(hdcDst, hglrc);
            glGetIntegerv( GL_VIEWPORT, viewport );
            ok( viewport[0] == 0 && viewport[1] == 0 && viewport[2] == 4 && viewport[3] == 4,
                "wrong viewport %d,%d,%d,%d\n", viewport[0], viewport[1], viewport[2], viewport[3] );

            SelectObject(hdcDst, bmpDst);
            ret = GetPixelFormat( hdcDst );
            ok( ret == iPixelFormat, "GetPixelFormat returned %d/%d\n", ret, iPixelFormat );
            ret = SetPixelFormat(hdcDst, iPixelFormat + 1, &pfd);
            ok( !ret, "SetPixelFormat succeeded\n" );
            wglMakeCurrent(hdcDst, hglrc2);
            glGetIntegerv( GL_VIEWPORT, viewport );
            ok( viewport[0] == 0 && viewport[1] == 0 && viewport[2] == 12 && viewport[3] == 12,
                "wrong viewport %d,%d,%d,%d\n", viewport[0], viewport[1], viewport[2], viewport[3] );

            wglDeleteContext(hglrc2);
            wglDeleteContext(hglrc);
        }
    }

    SelectObject(hdcDst, oldDst);
    DeleteObject(bmp2);
    DeleteObject(bmpDst);
    DeleteDC(hdcDst);
    DeleteDC(hdcScreen);
}

struct wgl_thread_param
{
    HANDLE test_finished;
    HWND hwnd;
    HGLRC hglrc;
    BOOL make_current;
    BOOL make_current_error;
    BOOL deleted;
    DWORD deleted_error;
};

static DWORD WINAPI wgl_thread(void *param)
{
    struct wgl_thread_param *p = param;
    HDC hdc = GetDC( p->hwnd );

    ok(!glGetString(GL_RENDERER) && !glGetString(GL_VERSION) && !glGetString(GL_VENDOR),
       "Expected NULL string when no active context is set\n");

    SetLastError(0xdeadbeef);
    p->make_current = wglMakeCurrent(hdc, p->hglrc);
    p->make_current_error = GetLastError();
    p->deleted = wglDeleteContext(p->hglrc);
    p->deleted_error = GetLastError();
    ReleaseDC( p->hwnd, hdc );
    SetEvent(p->test_finished);
    return 0;
}

static void test_deletecontext(HWND hwnd, HDC hdc)
{
    struct wgl_thread_param thread_params;
    HGLRC hglrc = wglCreateContext(hdc);
    HANDLE thread_handle;
    BOOL res;
    DWORD tid;

    SetLastError(0xdeadbeef);
    res = wglDeleteContext(NULL);
    ok(res == FALSE, "wglDeleteContext succeeded\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE, "Expected last error to be ERROR_INVALID_HANDLE, got %lu\n", GetLastError());

    if(!hglrc)
    {
        skip("wglCreateContext failed!\n");
        return;
    }

    res = wglMakeCurrent(hdc, hglrc);
    if(!res)
    {
        skip("wglMakeCurrent failed!\n");
        return;
    }

    /* WGL doesn't allow you to delete a context from a different thread than the one in which it is current.
     * This differs from GLX which does allow it but it delays actual deletion until the context becomes not current.
     */
    thread_params.hglrc = hglrc;
    thread_params.hwnd  = hwnd;
    thread_params.test_finished = CreateEventW(NULL, FALSE, FALSE, NULL);
    thread_handle = CreateThread(NULL, 0, wgl_thread, &thread_params, 0, &tid);
    ok(!!thread_handle, "Failed to create thread, last error %#lx.\n", GetLastError());
    if(thread_handle)
    {
        WaitForSingleObject(thread_handle, INFINITE);
        ok(!thread_params.make_current, "Attempt to make WGL context from another thread passed\n");
        ok(thread_params.make_current_error == ERROR_BUSY, "Expected last error to be ERROR_BUSY, got %u\n", thread_params.make_current_error);
        ok(!thread_params.deleted, "Attempt to delete WGL context from another thread passed\n");
        ok(thread_params.deleted_error == ERROR_BUSY, "Expected last error to be ERROR_BUSY, got %lu\n", thread_params.deleted_error);
    }
    CloseHandle(thread_params.test_finished);

    res = wglDeleteContext(hglrc);
    ok(res == TRUE, "wglDeleteContext failed\n");

    /* Attempting to delete the same context twice should fail. */
    SetLastError(0xdeadbeef);
    res = wglDeleteContext(hglrc);
    ok(res == FALSE, "wglDeleteContext succeeded\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE, "Expected last error to be ERROR_INVALID_HANDLE, got %lu\n", GetLastError());

    /* WGL makes a context not current when deleting it. This differs from GLX behavior where
     * deletion takes place when the thread becomes not current. */
    hglrc = wglGetCurrentContext();
    ok(hglrc == NULL, "A WGL context is active while none was expected\n");
}


static void test_getprocaddress(HDC hdc)
{
    const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
    PROC func = NULL;
    HGLRC ctx = wglGetCurrentContext();

    if (!extensions)
    {
        skip("skipping wglGetProcAddress tests because no GL extensions supported\n");
        return;
    }

    /* Core GL 1.0/1.1 functions should not be loadable through wglGetProcAddress.
     * Try to load the function with and without a context.
     */
    func = wglGetProcAddress("glEnable");
    ok(func == NULL, "Lookup of function glEnable with a context passed, expected a failure\n");
    wglMakeCurrent(hdc, NULL);
    func = wglGetProcAddress("glEnable");
    ok(func == NULL, "Lookup of function glEnable without a context passed, expected a failure\n");
    wglMakeCurrent(hdc, ctx);

    /* The goal of the test will be to test behavior of wglGetProcAddress when
     * no WGL context is active. Before the test we pick an extension (GL_ARB_multitexture)
     * which any GL >=1.2.1 implementation supports. Unfortunately the GDI renderer doesn't
     * support it. There aren't any extensions we can use for this test which are supported by
     * both GDI and real drivers.
     * Note GDI only has GL_EXT_bgra, GL_EXT_paletted_texture and GL_WIN_swap_hint.
     */
    if (!gl_extension_supported(extensions, "GL_ARB_multitexture"))
    {
        skip("skipping test because lack of GL_ARB_multitexture support\n");
        return;
    }

    func = wglGetProcAddress("glActiveTextureARB");
    ok(func != NULL, "Unable to lookup glActiveTextureARB, last error %#lx\n", GetLastError());

    /* Temporarily disable the context, so we can see that we can't retrieve functions now. */
    wglMakeCurrent(hdc, NULL);
    func = wglGetProcAddress("glActiveTextureARB");
    ok(func == NULL, "Function lookup without a context passed, expected a failure; last error %#lx\n", GetLastError());
    wglMakeCurrent(hdc, ctx);
}

static void test_make_current_read(HDC hdc)
{
    int res;
    HDC hread;
    HGLRC hglrc = wglCreateContext(hdc);

    if(!hglrc)
    {
        skip("wglCreateContext failed!\n");
        return;
    }

    res = wglMakeCurrent(hdc, hglrc);
    if(!res)
    {
        skip("wglMakeCurrent failed!\n");
        return;
    }

    /* Test what wglGetCurrentReadDCARB does for wglMakeCurrent as the spec doesn't mention it */
    hread = pwglGetCurrentReadDCARB();
    trace("hread %p, hdc %p\n", hread, hdc);
    ok(hread == hdc, "wglGetCurrentReadDCARB failed for standard wglMakeCurrent\n");

    pwglMakeContextCurrentARB(hdc, hdc, hglrc);
    hread = pwglGetCurrentReadDCARB();
    ok(hread == hdc, "wglGetCurrentReadDCARB failed for wglMakeContextCurrent\n");
}

static void test_dc(HWND hwnd, HDC hdc)
{
    int pf1, pf2;
    HDC hdc2;

    /* Get another DC and make sure it has the same pixel format */
    hdc2 = GetDC(hwnd);
    if(hdc != hdc2)
    {
        pf1 = GetPixelFormat(hdc);
        pf2 = GetPixelFormat(hdc2);
        ok(pf1 == pf2, "Second DC does not have the same format (%d != %d)\n", pf1, pf2);
    }
    else
        skip("Could not get a different DC for the window\n");

    if(hdc2)
    {
        ReleaseDC(hwnd, hdc2);
        hdc2 = NULL;
    }
}

/* Nvidia converts win32 error codes to (0xc007 << 16) | win32_error_code */
#define NVIDIA_HRESULT_FROM_WIN32(x) (HRESULT_FROM_WIN32(x) | 0x40000000)
static void test_opengl3(HDC hdc)
{
    /* Try to create a context compatible with OpenGL 1.x; 1.0-2.1 is allowed */
    {
        HGLRC gl3Ctx;
        int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 1, 0};

        gl3Ctx = pwglCreateContextAttribsARB(hdc, 0, attribs);
        ok(gl3Ctx != 0, "pwglCreateContextAttribsARB for a 1.x context failed!\n");
        wglDeleteContext(gl3Ctx);
    }

    /* Try to pass an invalid HDC */
    {
        HGLRC gl3Ctx;
        DWORD error;
        SetLastError(0xdeadbeef);
        gl3Ctx = pwglCreateContextAttribsARB((HDC)0xdeadbeef, 0, 0);
        ok(gl3Ctx == 0, "pwglCreateContextAttribsARB using an invalid HDC passed\n");
        error = GetLastError();
        ok(error == ERROR_DC_NOT_FOUND || error == ERROR_INVALID_HANDLE ||
           broken(error == ERROR_DS_GENERIC_ERROR) ||
           broken(error == NVIDIA_HRESULT_FROM_WIN32(ERROR_INVALID_DATA)), /* Nvidia Vista + Win7 */
           "Expected ERROR_DC_NOT_FOUND, got error=%lx\n", error);
        wglDeleteContext(gl3Ctx);
    }

    /* Try to pass an invalid shareList */
    {
        HGLRC gl3Ctx;
        DWORD error;
        SetLastError(0xdeadbeef);
        gl3Ctx = pwglCreateContextAttribsARB(hdc, (HGLRC)0xdeadbeef, 0);
        ok(gl3Ctx == 0, "pwglCreateContextAttribsARB using an invalid shareList passed\n");
        error = GetLastError();
        /* The Nvidia implementation seems to return hresults instead of win32 error codes */
        ok(error == ERROR_INVALID_OPERATION || error == ERROR_INVALID_DATA ||
           error == NVIDIA_HRESULT_FROM_WIN32(ERROR_INVALID_OPERATION), "Expected ERROR_INVALID_OPERATION, got error=%lx\n", error);
        wglDeleteContext(gl3Ctx);
    }

    /* Try to create an OpenGL 3.0 context */
    {
        int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 0, 0};
        HGLRC gl3Ctx = pwglCreateContextAttribsARB(hdc, 0, attribs);

        if(gl3Ctx == NULL)
        {
            skip("Skipping the rest of the WGL_ARB_create_context test due to lack of OpenGL 3.0\n");
            return;
        }

        wglDeleteContext(gl3Ctx);
    }

    /* Test matching an OpenGL 3.0 context with an older one, OpenGL 3.0 should allow it until the new object model is introduced in a future revision */
    {
        HGLRC glCtx = wglCreateContext(hdc);

        int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 0, 0};
        int attribs_future[] = {WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 0, 0};

        HGLRC gl3Ctx = pwglCreateContextAttribsARB(hdc, glCtx, attribs);
        ok(gl3Ctx != NULL, "Sharing of a display list between OpenGL 3.0 and OpenGL 1.x/2.x failed!\n");
        if(gl3Ctx)
            wglDeleteContext(gl3Ctx);

        gl3Ctx = pwglCreateContextAttribsARB(hdc, glCtx, attribs_future);
        ok(gl3Ctx != NULL, "Sharing of a display list between a forward compatible OpenGL 3.0 context and OpenGL 1.x/2.x failed!\n");
        if(gl3Ctx)
            wglDeleteContext(gl3Ctx);

        if(glCtx)
            wglDeleteContext(glCtx);
    }

    /* Try to create an OpenGL 3.0 context and test windowless rendering */
    {
        HGLRC gl3Ctx;
        int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 0, 0};
        BOOL res;

        gl3Ctx = pwglCreateContextAttribsARB(hdc, 0, attribs);
        ok(gl3Ctx != 0, "pwglCreateContextAttribsARB for a 3.0 context failed!\n");

        /* OpenGL 3.0 allows offscreen rendering WITHOUT a drawable
         * Neither AMD or Nvidia support it at this point. The WGL_ARB_create_context specs also say that
         * it is hard because drivers use the HDC to enter the display driver and it sounds like they don't
         * expect drivers to ever offer it.
         */
        res = wglMakeCurrent(0, gl3Ctx);
        ok(res == FALSE, "Wow, OpenGL 3.0 windowless rendering passed while it was expected not to!\n");
        if(res)
            wglMakeCurrent(0, 0);

        if(gl3Ctx)
            wglDeleteContext(gl3Ctx);
    }
}

static void test_minimized(void)
{
    PIXELFORMATDESCRIPTOR pf_desc =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     /* version */
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,                    /* 24-bit color depth */
        0, 0, 0, 0, 0, 0,      /* color bits */
        0,                     /* alpha buffer */
        0,                     /* shift bit */
        0,                     /* accumulation buffer */
        0, 0, 0, 0,            /* accum bits */
        32,                    /* z-buffer */
        0,                     /* stencil buffer */
        0,                     /* auxiliary buffer */
        PFD_MAIN_PLANE,        /* main layer */
        0,                     /* reserved */
        0, 0, 0                /* layer masks */
    };
    int pixel_format;
    HWND window;
    LONG style;
    HGLRC ctx;
    BOOL ret;
    HDC dc;

    window = CreateWindowA("static", "opengl32_test",
            WS_POPUP | WS_MINIMIZE, 0, 0, 640, 480, 0, 0, 0, 0);
    ok(!!window, "Failed to create window, last error %#lx.\n", GetLastError());

    dc = GetDC(window);
    ok(!!dc, "Failed to get DC.\n");

    pixel_format = ChoosePixelFormat(dc, &pf_desc);
    if (!pixel_format)
    {
        win_skip("Failed to find pixel format.\n");
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return;
    }

    ret = SetPixelFormat(dc, pixel_format, &pf_desc);
    ok(ret, "Failed to set pixel format, last error %#lx.\n", GetLastError());

    style = GetWindowLongA(window, GWL_STYLE);
    ok(style & WS_MINIMIZE, "Window should be minimized, got style %#lx.\n", style);

    ctx = wglCreateContext(dc);
    ok(!!ctx, "Failed to create GL context, last error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(dc, ctx);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());

    style = GetWindowLongA(window, GWL_STYLE);
    ok(style & WS_MINIMIZE, "window should be minimized, got style %#lx.\n", style);

    ret = wglMakeCurrent(NULL, NULL);
    ok(ret, "Failed to clear current context, last error %#lx.\n", GetLastError());

    ret = wglDeleteContext(ctx);
    ok(ret, "Failed to delete GL context, last error %#lx.\n", GetLastError());

    ReleaseDC(window, dc);
    DestroyWindow(window);
}

static void test_window_dc(void)
{
    PIXELFORMATDESCRIPTOR pf_desc =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     /* version */
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,                    /* 24-bit color depth */
        0, 0, 0, 0, 0, 0,      /* color bits */
        0,                     /* alpha buffer */
        0,                     /* shift bit */
        0,                     /* accumulation buffer */
        0, 0, 0, 0,            /* accum bits */
        32,                    /* z-buffer */
        0,                     /* stencil buffer */
        0,                     /* auxiliary buffer */
        PFD_MAIN_PLANE,        /* main layer */
        0,                     /* reserved */
        0, 0, 0                /* layer masks */
    };
    int pixel_format;
    HWND window;
    RECT vp, r;
    HGLRC ctx;
    BOOL ret;
    HDC dc;

    window = CreateWindowA("static", "opengl32_test",
            WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0, 0, 0);
    ok(!!window, "Failed to create window, last error %#lx.\n", GetLastError());

    ShowWindow(window, SW_SHOW);

    dc = GetWindowDC(window);
    ok(!!dc, "Failed to get DC.\n");

    pixel_format = ChoosePixelFormat(dc, &pf_desc);
    if (!pixel_format)
    {
        win_skip("Failed to find pixel format.\n");
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return;
    }

    ret = SetPixelFormat(dc, pixel_format, &pf_desc);
    ok(ret, "Failed to set pixel format, last error %#lx.\n", GetLastError());

    ctx = wglCreateContext(dc);
    ok(!!ctx, "Failed to create GL context, last error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(dc, ctx);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());

    GetClientRect(window, &r);
    glGetIntegerv(GL_VIEWPORT, (GLint *)&vp);
    ok(EqualRect(&r, &vp), "Viewport not equal to client rect.\n");

    ret = wglMakeCurrent(NULL, NULL);
    ok(ret, "Failed to clear current context, last error %#lx.\n", GetLastError());

    ret = wglDeleteContext(ctx);
    ok(ret, "Failed to delete GL context, last error %#lx.\n", GetLastError());

    ReleaseDC(window, dc);
    DestroyWindow(window);
}

static void test_message_window(void)
{
    PIXELFORMATDESCRIPTOR pf_desc =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     /* version */
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,                    /* 24-bit color depth */
        0, 0, 0, 0, 0, 0,      /* color bits */
        0,                     /* alpha buffer */
        0,                     /* shift bit */
        0,                     /* accumulation buffer */
        0, 0, 0, 0,            /* accum bits */
        32,                    /* z-buffer */
        0,                     /* stencil buffer */
        0,                     /* auxiliary buffer */
        PFD_MAIN_PLANE,        /* main layer */
        0,                     /* reserved */
        0, 0, 0                /* layer masks */
    };
    int pixel_format;
    HWND window;
    RECT vp, r;
    HGLRC ctx;
    BOOL ret;
    HDC dc;
    GLenum glerr;

    window = CreateWindowA("static", "opengl32_test",
                           WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, HWND_MESSAGE, 0, 0, 0);
    if (!window)
    {
        win_skip( "HWND_MESSAGE not supported\n" );
        return;
    }
    dc = GetDC(window);
    ok(!!dc, "Failed to get DC.\n");

    pixel_format = ChoosePixelFormat(dc, &pf_desc);
    if (!pixel_format)
    {
        win_skip("Failed to find pixel format.\n");
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return;
    }

    ret = SetPixelFormat(dc, pixel_format, &pf_desc);
    ok(ret, "Failed to set pixel format, last error %#lx.\n", GetLastError());

    ctx = wglCreateContext(dc);
    ok(!!ctx, "Failed to create GL context, last error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(dc, ctx);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());

    GetClientRect(window, &r);
    glGetIntegerv(GL_VIEWPORT, (GLint *)&vp);
    ok(EqualRect(&r, &vp), "Viewport not equal to client rect.\n");

    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    glerr = glGetError();
    ok(glerr == GL_NO_ERROR, "Failed glClear, error %#x.\n", glerr);
    ret = SwapBuffers(dc);
    ok(ret, "Failed SwapBuffers, error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(NULL, NULL);
    ok(ret, "Failed to clear current context, last error %#lx.\n", GetLastError());

    ret = wglDeleteContext(ctx);
    ok(ret, "Failed to delete GL context, last error %#lx.\n", GetLastError());

    ReleaseDC(window, dc);
    DestroyWindow(window);
}

static void test_destroy(HDC oldhdc)
{
    PIXELFORMATDESCRIPTOR pf_desc =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     /* version */
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,                    /* 24-bit color depth */
        0, 0, 0, 0, 0, 0,      /* color bits */
        0,                     /* alpha buffer */
        0,                     /* shift bit */
        0,                     /* accumulation buffer */
        0, 0, 0, 0,            /* accum bits */
        32,                    /* z-buffer */
        0,                     /* stencil buffer */
        0,                     /* auxiliary buffer */
        PFD_MAIN_PLANE,        /* main layer */
        0,                     /* reserved */
        0, 0, 0                /* layer masks */
    };
    int pixel_format;
    HWND window;
    HGLRC ctx;
    BOOL ret;
    HDC dc;
    GLenum glerr;
    DWORD err;
    HGLRC oldctx = wglGetCurrentContext();

    ok(!!oldctx, "Expected to find a valid current context.\n");

    window = CreateWindowA("static", "opengl32_test",
            WS_POPUP, 0, 0, 640, 480, 0, 0, 0, 0);
    ok(!!window, "Failed to create window, last error %#lx.\n", GetLastError());

    dc = GetDC(window);
    ok(!!dc, "Failed to get DC.\n");

    pixel_format = ChoosePixelFormat(dc, &pf_desc);
    if (!pixel_format)
    {
        win_skip("Failed to find pixel format.\n");
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return;
    }

    ret = SetPixelFormat(dc, pixel_format, &pf_desc);
    ok(ret, "Failed to set pixel format, last error %#lx.\n", GetLastError());

    ctx = wglCreateContext(dc);
    ok(!!ctx, "Failed to create GL context, last error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(dc, ctx);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());

    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    glerr = glGetError();
    ok(glerr == GL_NO_ERROR, "Failed glClear, error %#x.\n", glerr);
    ret = SwapBuffers(dc);
    ok(ret, "Failed SwapBuffers, error %#lx.\n", GetLastError());

    ret = DestroyWindow(window);
    ok(ret, "Failed to destroy window, last error %#lx.\n", GetLastError());

    ok(wglGetCurrentContext() == ctx, "Wrong current context.\n");

    SetLastError(0xdeadbeef);
    ret = wglMakeCurrent(dc, ctx);
    err = GetLastError();
    ok(!ret && err == ERROR_INVALID_HANDLE,
            "Unexpected behavior when making context current, ret %d, last error %#lx.\n", ret, err);
    SetLastError(0xdeadbeef);
    ret = SwapBuffers(dc);
    err = GetLastError();
    ok(!ret && err == ERROR_INVALID_HANDLE, "Unexpected behavior with SwapBuffer, last error %#lx.\n", err);

    ok(wglGetCurrentContext() == ctx, "Wrong current context.\n");

    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    glerr = glGetError();
    ok(glerr == GL_NO_ERROR, "Failed glClear, error %#x.\n", glerr);
    SetLastError(0xdeadbeef);
    ret = SwapBuffers(dc);
    err = GetLastError();
    ok(!ret && err == ERROR_INVALID_HANDLE, "Unexpected behavior with SwapBuffer, last error %#lx.\n", err);

    ret = wglMakeCurrent(NULL, NULL);
    ok(ret, "Failed to clear current context, last error %#lx.\n", GetLastError());

    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    glerr = glGetError();
    ok(glerr == GL_INVALID_OPERATION, "Failed glClear, error %#x.\n", glerr);
    SetLastError(0xdeadbeef);
    ret = SwapBuffers(dc);
    err = GetLastError();
    ok(!ret && err == ERROR_INVALID_HANDLE, "Unexpected behavior with SwapBuffer, last error %#lx.\n", err);

    SetLastError(0xdeadbeef);
    ret = wglMakeCurrent(dc, ctx);
    err = GetLastError();
    ok(!ret && err == ERROR_INVALID_HANDLE,
            "Unexpected behavior when making context current, ret %d, last error %#lx.\n", ret, err);

    ok(wglGetCurrentContext() == NULL, "Wrong current context.\n");

    ret = wglMakeCurrent(oldhdc, oldctx);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());
    ok(wglGetCurrentContext() == oldctx, "Wrong current context.\n");

    SetLastError(0xdeadbeef);
    ret = wglMakeCurrent(dc, ctx);
    err = GetLastError();
    ok(!ret && err == ERROR_INVALID_HANDLE,
            "Unexpected behavior when making context current, ret %d, last error %#lx.\n", ret, err);

    ok(wglGetCurrentContext() == oldctx, "Wrong current context.\n");

    ret = wglDeleteContext(ctx);
    ok(ret, "Failed to delete GL context, last error %#lx.\n", GetLastError());

    ReleaseDC(window, dc);

    ret = wglMakeCurrent(oldhdc, oldctx);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());
}

static void test_destroy_read(HDC oldhdc)
{
    PIXELFORMATDESCRIPTOR pf_desc =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     /* version */
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,                    /* 24-bit color depth */
        0, 0, 0, 0, 0, 0,      /* color bits */
        0,                     /* alpha buffer */
        0,                     /* shift bit */
        0,                     /* accumulation buffer */
        0, 0, 0, 0,            /* accum bits */
        32,                    /* z-buffer */
        0,                     /* stencil buffer */
        0,                     /* auxiliary buffer */
        PFD_MAIN_PLANE,        /* main layer */
        0,                     /* reserved */
        0, 0, 0                /* layer masks */
    };
    int pixel_format;
    HWND draw_window, read_window;
    HGLRC ctx;
    BOOL ret;
    HDC read_dc, draw_dc;
    GLenum glerr;
    DWORD err;
    HGLRC oldctx = wglGetCurrentContext();

    ok(!!oldctx, "Expected to find a valid current context\n");

    draw_window = CreateWindowA("static", "opengl32_test",
            WS_POPUP, 0, 0, 640, 480, 0, 0, 0, 0);
    ok(!!draw_window, "Failed to create window, last error %#lx.\n", GetLastError());

    draw_dc = GetDC(draw_window);
    ok(!!draw_dc, "Failed to get DC.\n");

    pixel_format = ChoosePixelFormat(draw_dc, &pf_desc);
    if (!pixel_format)
    {
        win_skip("Failed to find pixel format.\n");
        ReleaseDC(draw_window, draw_dc);
        DestroyWindow(draw_window);
        return;
    }

    ret = SetPixelFormat(draw_dc, pixel_format, &pf_desc);
    ok(ret, "Failed to set pixel format, last error %#lx.\n", GetLastError());

    read_window = CreateWindowA("static", "opengl32_test",
            WS_POPUP, 0, 0, 640, 480, 0, 0, 0, 0);
    ok(!!read_window, "Failed to create window, last error %#lx.\n", GetLastError());

    read_dc = GetDC(read_window);
    ok(!!draw_dc, "Failed to get DC.\n");

    pixel_format = ChoosePixelFormat(read_dc, &pf_desc);
    if (!pixel_format)
    {
        win_skip("Failed to find pixel format.\n");
        ReleaseDC(read_window, read_dc);
        DestroyWindow(read_window);
        ReleaseDC(draw_window, draw_dc);
        DestroyWindow(draw_window);
        return;
    }

    ret = SetPixelFormat(read_dc, pixel_format, &pf_desc);
    ok(ret, "Failed to set pixel format, last error %#lx.\n", GetLastError());

    ctx = wglCreateContext(draw_dc);
    ok(!!ctx, "Failed to create GL context, last error %#lx.\n", GetLastError());

    ret = pwglMakeContextCurrentARB(draw_dc, read_dc, ctx);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());

    glCopyPixels(0, 0, 640, 480, GL_COLOR);
    glFinish();
    glerr = glGetError();
    ok(glerr == GL_NO_ERROR, "Failed glCopyPixel, error %#x.\n", glerr);
    ret = SwapBuffers(draw_dc);
    ok(ret, "Failed SwapBuffers, error %#lx.\n", GetLastError());

    ret = DestroyWindow(read_window);
    ok(ret, "Failed to destroy window, last error %#lx.\n", GetLastError());

    ok(wglGetCurrentContext() == ctx, "Wrong current context.\n");

    if (0) /* Crashes on AMD on Windows */
    {
        glCopyPixels(0, 0, 640, 480, GL_COLOR);
        glFinish();
        glerr = glGetError();
        ok(glerr == GL_NO_ERROR, "Failed glCopyPixel, error %#x.\n", glerr);
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    glerr = glGetError();
    ok(glerr == GL_NO_ERROR, "Failed glClear, error %#x.\n", glerr);
    ret = SwapBuffers(draw_dc);
    ok(ret, "Failed SwapBuffers, error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(NULL, NULL);
    ok(ret, "Failed to clear current context, last error %#lx.\n", GetLastError());

    if (0) /* This crashes with Nvidia drivers on Windows. */
    {
        SetLastError(0xdeadbeef);
        ret = pwglMakeContextCurrentARB(draw_dc, read_dc, ctx);
        err = GetLastError();
        ok(!ret && err == ERROR_INVALID_HANDLE,
                "Unexpected behavior when making context current, ret %d, last error %#lx.\n", ret, err);
    }

    ret = DestroyWindow(draw_window);
    ok(ret, "Failed to destroy window, last error %#lx.\n", GetLastError());

    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    glerr = glGetError();
    ok(glerr == GL_INVALID_OPERATION, "Failed glClear, error %#x.\n", glerr);
    SetLastError(0xdeadbeef);
    ret = SwapBuffers(draw_dc);
    err = GetLastError();
    ok(!ret && err == ERROR_INVALID_HANDLE, "Unexpected behavior with SwapBuffer, last error %#lx.\n", err);

    SetLastError(0xdeadbeef);
    ret = pwglMakeContextCurrentARB(draw_dc, read_dc, ctx);
    err = GetLastError();
    ok(!ret && (err == ERROR_INVALID_HANDLE || err == 0xc0070006),
            "Unexpected behavior when making context current, ret %d, last error %#lx.\n", ret, err);

    ok(wglGetCurrentContext() == NULL, "Wrong current context.\n");

    wglMakeCurrent(NULL, NULL);

    wglMakeCurrent(oldhdc, oldctx);
    ok(wglGetCurrentContext() == oldctx, "Wrong current context.\n");

    SetLastError(0xdeadbeef);
    ret = pwglMakeContextCurrentARB(draw_dc, read_dc, ctx);
    err = GetLastError();
    ok(!ret && (err == ERROR_INVALID_HANDLE || err == 0xc0070006),
            "Unexpected behavior when making context current, last error %#lx.\n", err);

    ok(wglGetCurrentContext() == oldctx, "Wrong current context.\n");

    ret = wglDeleteContext(ctx);
    ok(ret, "Failed to delete GL context, last error %#lx.\n", GetLastError());

    ReleaseDC(read_window, read_dc);
    ReleaseDC(draw_window, draw_dc);

    wglMakeCurrent(oldhdc, oldctx);
}

static void test_swap_control(HDC oldhdc)
{
    PIXELFORMATDESCRIPTOR pf_desc =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     /* version */
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,                    /* 24-bit color depth */
        0, 0, 0, 0, 0, 0,      /* color bits */
        0,                     /* alpha buffer */
        0,                     /* shift bit */
        0,                     /* accumulation buffer */
        0, 0, 0, 0,            /* accum bits */
        32,                    /* z-buffer */
        0,                     /* stencil buffer */
        0,                     /* auxiliary buffer */
        PFD_MAIN_PLANE,        /* main layer */
        0,                     /* reserved */
        0, 0, 0                /* layer masks */
    };
    int pixel_format;
    HWND window1, window2, old_parent;
    HGLRC ctx1, ctx2, oldctx;
    BOOL ret;
    HDC dc1, dc2;
    int interval;

    oldctx = wglGetCurrentContext();
    ok(!!oldctx, "Expected to find a valid current context.\n");

    window1 = CreateWindowA("static", "opengl32_test",
            WS_POPUP, 0, 0, 640, 480, 0, 0, 0, 0);
    ok(!!window1, "Failed to create window1, last error %#lx.\n", GetLastError());

    dc1 = GetDC(window1);
    ok(!!dc1, "Failed to get DC.\n");

    pixel_format = ChoosePixelFormat(dc1, &pf_desc);
    if (!pixel_format)
    {
        win_skip("Failed to find pixel format.\n");
        ReleaseDC(window1, dc1);
        DestroyWindow(window1);
        return;
    }

    ret = SetPixelFormat(dc1, pixel_format, &pf_desc);
    ok(ret, "Failed to set pixel format, last error %#lx.\n", GetLastError());

    ctx1 = wglCreateContext(dc1);
    ok(!!ctx1, "Failed to create GL context, last error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(dc1, ctx1);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());

    interval = pwglGetSwapIntervalEXT();
    ok(interval == 1, "Expected default swap interval 1, got %d\n", interval);

    ret = pwglSwapIntervalEXT(0);
    ok(ret, "Failed to set swap interval to 0, last error %#lx.\n", GetLastError());

    interval = pwglGetSwapIntervalEXT();
    ok(interval == 0, "Expected swap interval 0, got %d\n", interval);

    /* Check what interval we get on a second context on the same drawable.*/
    ctx2 = wglCreateContext(dc1);
    ok(!!ctx2, "Failed to create GL context, last error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(dc1, ctx2);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());

    interval = pwglGetSwapIntervalEXT();
    ok(interval == 0, "Expected swap interval 0, got %d\n", interval);

    /* A second window is created to see whether its swap interval was affected
     * by previous calls.
     */
    window2 = CreateWindowA("static", "opengl32_test",
            WS_POPUP, 0, 0, 640, 480, 0, 0, 0, 0);
    ok(!!window2, "Failed to create window2, last error %#lx.\n", GetLastError());

    dc2 = GetDC(window2);
    ok(!!dc2, "Failed to get DC.\n");

    ret = SetPixelFormat(dc2, pixel_format, &pf_desc);
    ok(ret, "Failed to set pixel format, last error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(dc2, ctx1);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());

    /* Since the second window lacks the swap interval, this proves that the interval
     * is not global or shared among contexts.
     */
    interval = pwglGetSwapIntervalEXT();
    ok(interval == 1, "Expected default swap interval 1, got %d\n", interval);

    /* Test if setting the parent of a window resets the swap interval. */
    ret = wglMakeCurrent(dc1, ctx1);
    ok(ret, "Failed to make context current, last error %#lx.\n", GetLastError());

    old_parent = SetParent(window1, window2);
    ok(!!old_parent, "Failed to make window1 a child of window2, last error %#lx.\n", GetLastError());

    interval = pwglGetSwapIntervalEXT();
    ok(interval == 0, "Expected swap interval 0, got %d\n", interval);

    ret = wglDeleteContext(ctx1);
    ok(ret, "Failed to delete GL context, last error %#lx.\n", GetLastError());
    ret = wglDeleteContext(ctx2);
    ok(ret, "Failed to delete GL context, last error %#lx.\n", GetLastError());

    ReleaseDC(window1, dc1);
    DestroyWindow(window1);
    ReleaseDC(window2, dc2);
    DestroyWindow(window2);

    wglMakeCurrent(oldhdc, oldctx);
}

static void test_wglChoosePixelFormatARB(HDC hdc)
{
    static int attrib_list[] =
    {
        WGL_DRAW_TO_WINDOW_ARB, 1,
        WGL_SUPPORT_OPENGL_ARB, 1,
        0
    };
    static int attrib_list_flags[] =
    {
        WGL_DRAW_TO_WINDOW_ARB, 1,
        WGL_SUPPORT_OPENGL_ARB, 1,
        WGL_SUPPORT_GDI_ARB, 1,
        0
    };

    PIXELFORMATDESCRIPTOR fmt, last_fmt;
    BYTE depth, last_depth;
    UINT format_count;
    int formats[1024];
    unsigned int i;
    int res;

    if (!pwglChoosePixelFormatARB)
    {
        skip("wglChoosePixelFormatARB is not available\n");
        return;
    }

    format_count = 0;
    res = pwglChoosePixelFormatARB(hdc, attrib_list, NULL, ARRAY_SIZE(formats), formats, &format_count);
    ok(res, "Got unexpected result %d.\n", res);

    memset(&last_fmt, 0, sizeof(last_fmt));
    last_depth = 0;

    for (i = 0; i < format_count; ++i)
    {
        memset(&fmt, 0, sizeof(fmt));
        if (!DescribePixelFormat(hdc, formats[i], sizeof(fmt), &fmt)
                || (fmt.dwFlags & PFD_GENERIC_FORMAT))
        {
            memset(&fmt, 0, sizeof(fmt));
            continue;
        }

        depth = fmt.cDepthBits;
        fmt.cDepthBits = 0;
        fmt.cStencilBits = 0;

        if (memcmp(&fmt, &last_fmt, sizeof(fmt)))
        {
            last_fmt = fmt;
            last_depth = depth;
        }
        else
        {
            ok(last_depth <= depth, "Got unexpected depth %u, last_depth %u, i %u, format %u.\n",
                    depth, last_depth, i, formats[i]);
        }
    }

    format_count = 0;
    res = pwglChoosePixelFormatARB(hdc, attrib_list_flags, NULL, ARRAY_SIZE(formats), formats, &format_count);
    ok(res, "Got unexpected result %d.\n", res);

    for (i = 0; i < format_count; ++i)
    {
        PIXELFORMATDESCRIPTOR format = {0};
        BOOL ret;

        winetest_push_context("%u", i);

        ret = DescribePixelFormat(hdc, formats[i], sizeof(format), &format);
        ok(ret, "DescribePixelFormat failed, error %lu\n", GetLastError());

        ok(format.dwFlags & PFD_DRAW_TO_WINDOW, "got dwFlags %#lx\n", format.dwFlags);
        ok(format.dwFlags & PFD_SUPPORT_OPENGL, "got dwFlags %#lx\n", format.dwFlags);
        ok(format.dwFlags & PFD_SUPPORT_GDI, "got dwFlags %#lx\n", format.dwFlags);

        winetest_pop_context();
    }
}

static void test_copy_context(HDC hdc)
{
    HGLRC ctx, ctx2, old_ctx;
    BOOL ret;

    old_ctx = wglGetCurrentContext();
    ok(!!old_ctx, "wglGetCurrentContext failed, last error %#lx.\n", GetLastError());

    ctx = wglCreateContext(hdc);
    ok(!!ctx, "Failed to create GL context, last error %#lx.\n", GetLastError());
    ret = wglMakeCurrent(hdc, ctx);
    ok(ret, "wglMakeCurrent failed, last error %#lx.\n", GetLastError());
    ctx2 = wglCreateContext(hdc);
    ok(!!ctx2, "Failed to create GL context, last error %#lx.\n", GetLastError());

    ret = wglCopyContext(ctx, ctx2, GL_ALL_ATTRIB_BITS);
    todo_wine
    ok(ret, "Failed to copy GL context, last error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(NULL, NULL);
    ok(ret, "wglMakeCurrent failed, last error %#lx.\n", GetLastError());
    ret = wglDeleteContext(ctx2);
    ok(ret, "Failed to delete GL context, last error %#lx.\n", GetLastError());
    ret = wglDeleteContext(ctx);
    ok(ret, "Failed to delete GL context, last error %#lx.\n", GetLastError());

    ret = wglMakeCurrent(hdc, old_ctx);
    ok(ret, "wglMakeCurrent failed, last error %#lx.\n", GetLastError());
}

static void test_child_window(HWND hwnd, PIXELFORMATDESCRIPTOR *pfd)
{
    int pixel_format;
    DWORD t1, t;
    HGLRC hglrc;
    HWND child;
    HDC hdc;
    int res;

    child = CreateWindowA("static", "Title", WS_CHILDWINDOW | WS_VISIBLE, 50, 50, 100, 100, hwnd, NULL, NULL, NULL);
    ok(!!child, "got error %lu.\n", GetLastError());

    hdc = GetDC(child);
    pixel_format = ChoosePixelFormat(hdc, pfd);
    res = SetPixelFormat(hdc, pixel_format, pfd);
    ok(res, "got error %lu.\n", GetLastError());

    hglrc = wglCreateContext(hdc);
    ok(!!hglrc, "got error %lu.\n", GetLastError());

    /* Test SwapBuffers with NULL context. */

    glDrawBuffer(GL_BACK);

    /* Currently blit happening for child window in winex11 may not be updated with the latest GL frame
     * even on glXWaitForSbcOML() path. So simulate continuous present for the test purpose. */
    trace("Child window rectangle should turn from red to green now.\n");
    t1 = GetTickCount();
    while ((t = GetTickCount()) - t1 < 3000)
    {
        res = wglMakeCurrent(hdc, hglrc);
        ok(res, "got error %lu.\n", GetLastError());
        if (t - t1 > 1500)
            glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        else
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        res = wglMakeCurrent(NULL, NULL);
        ok(res, "got error %lu.\n", GetLastError());
        SwapBuffers(hdc);
    }

    res = wglDeleteContext(hglrc);
    ok(res, "got error %lu.\n", GetLastError());

    ReleaseDC(child, hdc);
    DestroyWindow(child);
}

START_TEST(opengl)
{
    HWND hwnd;
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                     /* version */
        PFD_DRAW_TO_WINDOW |
        PFD_SUPPORT_OPENGL |
        PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,                    /* 24-bit color depth */
        0, 0, 0, 0, 0, 0,      /* color bits */
        0,                     /* alpha buffer */
        0,                     /* shift bit */
        0,                     /* accumulation buffer */
        0, 0, 0, 0,            /* accum bits */
        32,                    /* z-buffer */
        0,                     /* stencil buffer */
        0,                     /* auxiliary buffer */
        PFD_MAIN_PLANE,        /* main layer */
        0,                     /* reserved */
        0, 0, 0                /* layer masks */
    };

    hwnd = CreateWindowA("static", "Title", WS_OVERLAPPEDWINDOW, 10, 10, 200, 200, NULL, NULL,
            NULL, NULL);
    ok(hwnd != NULL, "err: %ld\n", GetLastError());
    if (hwnd)
    {
        HDC hdc;
        int iPixelFormat, res;
        HGLRC hglrc;
        DWORD error;
        ShowWindow(hwnd, SW_SHOW);

        hdc = GetDC(hwnd);

        iPixelFormat = ChoosePixelFormat(hdc, &pfd);
        if(iPixelFormat == 0)
        {
            /* This should never happen as ChoosePixelFormat always returns a closest match, but currently this fails in Wine if we don't have glX */
            win_skip("Unable to find pixel format.\n");
            goto cleanup;
        }

        /* We shouldn't be able to create a context from a hdc which doesn't have a pixel format set */
        hglrc = wglCreateContext(hdc);
        ok(hglrc == NULL, "wglCreateContext should fail when no pixel format has been set, but it passed\n");
        error = GetLastError();
        ok(error == ERROR_INVALID_PIXEL_FORMAT, "expected ERROR_INVALID_PIXEL_FORMAT for wglCreateContext without a pixelformat set, but received %#lx\n", error);

        res = SetPixelFormat(hdc, iPixelFormat, &pfd);
        ok(res, "SetPixelformat failed: %lx\n", GetLastError());

        test_bitmap_rendering( TRUE );
        test_bitmap_rendering( FALSE );
        test_minimized();
        test_window_dc();
        test_message_window();
        test_dc(hwnd, hdc);

        ok(!glGetString(GL_RENDERER) && !glGetString(GL_VERSION) && !glGetString(GL_VENDOR),
           "Expected NULL string when no active context is set\n");
        hglrc = wglCreateContext(hdc);
        res = wglMakeCurrent(hdc, hglrc);
        ok(res, "wglMakeCurrent failed!\n");
        if(res)
        {
            trace("OpenGL renderer: %s\n", glGetString(GL_RENDERER));
            trace("OpenGL driver version: %s\n", glGetString(GL_VERSION));
            trace("OpenGL vendor: %s\n", glGetString(GL_VENDOR));
        }
        else
        {
            skip("Skipping OpenGL tests without a current context\n");
            return;
        }

        /* Initialisation of WGL functions depends on an implicit WGL context. For this reason we can't load them before making
         * any WGL call :( On Wine this would work but not on real Windows because there can be different implementations (software, ICD, MCD).
         */
        init_functions();
        test_getprocaddress(hdc);
        test_deletecontext(hwnd, hdc);
        test_makecurrent(hdc);
        test_copy_context(hdc);

        /* The lack of wglGetExtensionsStringARB in general means broken software rendering or the lack of decent OpenGL support, skip tests in such cases */
        if (!pwglGetExtensionsStringARB)
        {
            win_skip("wglGetExtensionsStringARB is not available\n");
            return;
        }

        test_choosepixelformat();
        test_choosepixelformat_flag_is_ignored_when_unset(PFD_DRAW_TO_WINDOW);
        test_choosepixelformat_flag_is_ignored_when_unset(PFD_DRAW_TO_BITMAP);
        test_choosepixelformat_flag_is_ignored_when_unset(PFD_SUPPORT_GDI);
        test_choosepixelformat_flag_is_ignored_when_unset(PFD_SUPPORT_OPENGL);
        test_wglChoosePixelFormatARB(hdc);
        test_debug_message_callback();
        test_setpixelformat(hdc);
        test_destroy(hdc);
        test_sharelists(hdc);
        test_colorbits(hdc);
        test_gdi_dbuf(hdc);
        test_acceleration(hdc);

        wgl_extensions = pwglGetExtensionsStringARB(hdc);
        if(wgl_extensions == NULL) skip("Skipping opengl32 tests because this OpenGL implementation doesn't support WGL extensions!\n");

        if(strstr(wgl_extensions, "WGL_ARB_create_context"))
            test_opengl3(hdc);

        if(strstr(wgl_extensions, "WGL_ARB_make_current_read"))
        {
            test_make_current_read(hdc);
            test_destroy_read(hdc);
        }
        else
            skip("WGL_ARB_make_current_read not supported, skipping test\n");

        if(strstr(wgl_extensions, "WGL_ARB_pbuffer"))
            test_pbuffers(hdc);
        else
            skip("WGL_ARB_pbuffer not supported, skipping pbuffer test\n");

        if(strstr(wgl_extensions, "WGL_EXT_swap_control"))
            test_swap_control(hdc);
        else
            skip("WGL_EXT_swap_control not supported, skipping test\n");

        if (winetest_interactive)
            test_child_window(hwnd, &pfd);

cleanup:
        ReleaseDC(hwnd, hdc);
        DestroyWindow(hwnd);
    }
}
