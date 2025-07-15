#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cstring>

#include "core/math.h"
#include "scene/scene.h"
#include "render/camera.h"
#include "render/tracer.h"
#include "editor/editor.h"
#include "platform/ui.h"

static bool   g_running    = true;
static Scene  g_scene;
static Camera g_cam;
static Tracer g_tracer;
static Editor g_editor;
static UI     g_ui;
static GLuint g_texture    = 0;
static int    g_width      = 1024;
static int    g_height     = 640;
static DWORD  g_last_tick  = 0;
static int    g_frame_count = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:   g_running=false; PostQuitMessage(0); return 0;
        case WM_DESTROY: g_running=false; PostQuitMessage(0); return 0;
        case WM_SIZE:
            g_width  = LOWORD(lParam); g_height = HIWORD(lParam);
            if (g_height < 1) g_height = 1;
            g_cam.width=g_width; g_cam.height=g_height;
            g_tracer.free(); g_tracer.init(g_width, g_height);
            glViewport(0,0,g_width,g_height);
            g_ui.screen_w=g_width; g_ui.screen_h=g_height;
            return 0;
        case WM_LBUTTONDOWN: g_editor.on_lmb_down(LOWORD(lParam),HIWORD(lParam)); return 0;
        case WM_LBUTTONUP:
            g_editor.on_lmb_up(LOWORD(lParam),HIWORD(lParam));
            g_tracer.needs_reset=true; return 0;
        case WM_RBUTTONDOWN: g_editor.on_rmb_down(LOWORD(lParam),HIWORD(lParam)); return 0;
        case WM_RBUTTONUP:   g_editor.on_rmb_up(LOWORD(lParam),HIWORD(lParam));   return 0;
        case WM_MOUSEMOVE:
            g_editor.on_mouse_move(LOWORD(lParam),HIWORD(lParam));
            if (g_editor.lmb_down || g_editor.rmb_down) g_tracer.needs_reset=true;
            return 0;
        case WM_MOUSEWHEEL:
            g_editor.on_scroll(GET_WHEEL_DELTA_WPARAM(wParam)/WHEEL_DELTA);
            g_tracer.needs_reset=true; return 0;
        case WM_KEYDOWN:
            if (wParam=='Q') { g_running=false; PostQuitMessage(0); }
            g_editor.on_key((int)wParam);
            return 0;
        case WM_KEYUP: g_editor.on_key_up((int)wParam); return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void initGL() {
    glClearColor(0.051f, 0.059f, 0.090f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
}

void renderFrame(unsigned char* pixels) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw ray-traced image as fullscreen quad
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_width, g_height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(-1,1,-1,1,-1,1);
    glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    // V coords are flipped: tracer stores row-0 at top, GL texture (0,0) is bottom-left
    glBegin(GL_QUADS);
    glTexCoord2f(0,1); glVertex2f(-1, 1);   // screen top-left    = texture top-left
    glTexCoord2f(1,1); glVertex2f( 1, 1);   // screen top-right   = texture top-right
    glTexCoord2f(1,0); glVertex2f( 1,-1);   // screen bottom-right = texture bottom-right
    glTexCoord2f(0,0); glVertex2f(-1,-1);   // screen bottom-left  = texture bottom-left
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);

    // 3D gizmo overlay
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(g_cam.fov, (double)g_width/(double)g_height, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(
        g_cam.position.x(), g_cam.position.y(), g_cam.position.z(),
        g_cam.target.x(),   g_cam.target.y(),   g_cam.target.z(),
        g_cam.up.x(),       g_cam.up.y(),        g_cam.up.z()
    );

    if (g_editor.show_gizmos) g_editor.drawGizmos();

    // 2D UI overlay
    glEnable(GL_TEXTURE_2D);
    g_ui.render(&g_editor);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSA wc = {};
    wc.style       = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance   = hInstance;
    wc.hCursor     = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon       = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = "RaytracingShowcase";
    RegisterClassA(&wc);

    RECT rc = {0,0,g_width,g_height};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(0, "RaytracingShowcase",
        "Ray Tracing Showcase  |  F1-F8: Switch Mode  |  H: Controls",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right-rc.left, rc.bottom-rc.top,
        NULL, NULL, hInstance, NULL);

    HDC hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);
    HGLRC hglrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hglrc);

    initGL();

    g_scene.setupDefault();
    g_cam.update();
    g_cam.setDOF(0.0, 7.0);
    g_editor.init(&g_scene, &g_cam, &g_tracer);
    g_tracer.init(g_width, g_height);
    g_ui.init(g_width, g_height);
    g_ui.thread_count = g_tracer.num_threads;

    unsigned char* pixels = new unsigned char[g_width * g_height * 3];
    memset(pixels, 0, g_width * g_height * 3);

    g_last_tick   = GetTickCount();
    g_frame_count = 0;

    MSG msg;
    while (g_running) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_running=false; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!g_running) break;

        if (g_tracer.needs_reset) g_tracer.reset();

        g_tracer.renderFrame(g_scene, g_cam);
        g_tracer.getPixels(pixels);

        renderFrame(pixels);
        SwapBuffers(hdc);

        g_frame_count++;
        DWORD now = GetTickCount();
        if (now - g_last_tick >= 1000) {
            g_ui.fps = g_frame_count;
            g_frame_count = 0;
            g_last_tick = now;
        }

        g_ui.samples       = g_tracer.samples_per_pixel;
        g_ui.obj_count     = (int)g_scene.objects.size();
        g_ui.selected_obj  = g_editor.selected;
        g_ui.edit_mode     = (int)g_editor.mode;
        g_ui.show_help     = g_editor.show_help;
        g_ui.thread_count  = g_tracer.num_threads;
        g_ui.render_mode   = (int)g_tracer.render_mode;

        Sleep(1);
    }

    delete[] pixels;
    g_tracer.free();
    g_ui.shutdown();
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    return 0;
}
