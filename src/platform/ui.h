#ifndef UI_H
#define UI_H

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "render/tracer.h"
#include "scene/scene.h"
#include "scene/object.h"

static HFONT g_font_sm = 0;
static HFONT g_font_md = 0;
static GLuint g_font_base_sm = 0;
static GLuint g_font_base_md = 0;

// ── Color palette ──────────────────────────────────────────────────────
// Dark bg:     #0D0F14  -> 0.051, 0.059, 0.078
// Panel bg:    #141720  -> 0.079, 0.090, 0.125
// Panel edge:  #252C42  -> 0.145, 0.173, 0.259
// Accent blue: #4A9EFF  -> 0.290, 0.620, 1.000
// Accent gold: #FFB347  -> 1.000, 0.702, 0.278
// Selected:    #FFE566  -> 1.000, 0.898, 0.400
// Green ok:    #3DCC7E  -> 0.239, 0.800, 0.494
// Text bright: #D8E0F0  -> 0.847, 0.878, 0.941
// Text muted:  #6B7593  -> 0.420, 0.459, 0.576
// Red warn:    #FF5555  -> 1.000, 0.333, 0.333

// Mode accent colors (per RenderMode)
static const float MODE_COLORS[RMODE_COUNT][3] = {
    {0.290f, 0.620f, 1.000f},  // PATH_TRACE  - blue
    {0.380f, 0.900f, 0.550f},  // NORMALS     - green
    {1.000f, 0.702f, 0.278f},  // ALBEDO      - gold
    {0.350f, 0.700f, 0.950f},  // DEPTH       - cyan
    {1.000f, 0.898f, 0.400f},  // SHADOW      - yellow
    {0.720f, 0.380f, 1.000f},  // AO          - purple
    {1.000f, 0.450f, 0.200f},  // FRESNEL     - orange
    {0.220f, 0.900f, 0.780f},  // WIREFRAME   - teal
};

static const char* EDIT_MODE_NAMES[] = {
    "VIEW", "MOVE", "SCALE",
    "ADD SPHERE", "ADD BOX", "ADD PLANE", "ADD CYL"
};

struct UI {
    int screen_w, screen_h;
    int fps, samples;
    int obj_count, selected_obj;
    bool show_help;
    int  edit_mode;
    int  thread_count;
    int  render_mode;

    UI()
        : screen_w(640), screen_h(480), fps(0), samples(0),
          obj_count(0), selected_obj(-1), show_help(false),
          edit_mode(0), thread_count(1), render_mode(0) {}

    // ── Init fonts ──────────────────────────────────────────────────────
    void init(int w, int h) {
        screen_w = w; screen_h = h;
        g_font_base_sm = glGenLists(96);
        g_font_sm = CreateFontA(
            -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Consolas");
        HDC hdc = wglGetCurrentDC();
        HFONT old = (HFONT)SelectObject(hdc, g_font_sm);
        wglUseFontBitmaps(hdc, 32, 96, g_font_base_sm);
        SelectObject(hdc, old);

        g_font_base_md = glGenLists(96);
        g_font_md = CreateFontA(
            -16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Consolas");
        old = (HFONT)SelectObject(hdc, g_font_md);
        wglUseFontBitmaps(hdc, 32, 96, g_font_base_md);
        SelectObject(hdc, old);
    }

    void shutdown() {
        if (g_font_base_sm) { glDeleteLists(g_font_base_sm, 96); g_font_base_sm=0; }
        if (g_font_base_md) { glDeleteLists(g_font_base_md, 96); g_font_base_md=0; }
        if (g_font_sm) { DeleteObject(g_font_sm); g_font_sm=0; }
        if (g_font_md) { DeleteObject(g_font_md); g_font_md=0; }
    }

    // ── Drawing helpers ─────────────────────────────────────────────────
    void drawTextSm(int x, int y, float r, float g, float b, const char* fmt, ...) const {
        char buf[512]; va_list ap;
        va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
        glPushAttrib(GL_LIST_BIT);
        glRasterPos2i(x, y);
        glColor3f(r, g, b);
        glListBase(g_font_base_sm - 32);
        glCallLists((int)strlen(buf), GL_UNSIGNED_BYTE, buf);
        glPopAttrib();
    }

    void drawTextMd(int x, int y, float r, float g, float b, const char* fmt, ...) const {
        char buf[512]; va_list ap;
        va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
        glPushAttrib(GL_LIST_BIT);
        glRasterPos2i(x, y);
        glColor3f(r, g, b);
        glListBase(g_font_base_md - 32);
        glCallLists((int)strlen(buf), GL_UNSIGNED_BYTE, buf);
        glPopAttrib();
    }

    // Filled rect with alpha-blended color
    void fillRect(int x, int y, int w, int h, float r, float g, float b, float a=1.0f) const {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(r, g, b, a);
        glBegin(GL_QUADS);
        glVertex2i(x,   y);   glVertex2i(x+w, y);
        glVertex2i(x+w, y+h); glVertex2i(x,   y+h);
        glEnd();
        glDisable(GL_BLEND);
    }

    // Outlined rect border
    void strokeRect(int x, int y, int w, int h, float r, float g, float b, float a=1.0f) const {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(r, g, b, a);
        glBegin(GL_LINE_LOOP);
        glVertex2i(x,   y);   glVertex2i(x+w, y);
        glVertex2i(x+w, y+h); glVertex2i(x,   y+h);
        glEnd();
        glDisable(GL_BLEND);
    }

    // Horizontal divider line
    void hLine(int x, int y, int w, float r, float g, float b, float a=0.5f) const {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(r,g,b,a);
        glBegin(GL_LINES);
        glVertex2i(x, y); glVertex2i(x+w, y);
        glEnd();
        glDisable(GL_BLEND);
    }

    // Small color swatch
    void colorSwatch(int x, int y, float r, float g, float b) const {
        fillRect(x, y-10, 14, 12, r, g, b);
        strokeRect(x, y-10, 14, 12, 1,1,1, 0.25f);
    }

    // ── Main render ─────────────────────────────────────────────────────
    void render(const struct Editor* editor) const;
};

#include "editor/editor.h"

inline void UI::render(const Editor* editor) const {
    // Setup 2D ortho projection
    glMatrixMode(GL_PROJECTION);
    glPushMatrix(); glLoadIdentity();
    glOrtho(0, screen_w, screen_h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    const int HEADER_H  = 48;
    const int FOOTER_H  = 44;
    const int PANEL_W   = 226;
    const int PANEL_H   = 200;

    // ── HEADER BAR ─────────────────────────────────────────────────────
    // Background
    fillRect(0, 0, screen_w, HEADER_H, 0.051f, 0.059f, 0.090f, 0.92f);
    // Bottom border accent
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.145f, 0.173f, 0.320f, 0.9f);
    glBegin(GL_LINES);
    glVertex2i(0, HEADER_H); glVertex2i(screen_w, HEADER_H);
    glEnd();
    glDisable(GL_BLEND);

    // Accent strip top
    fillRect(0, 0, screen_w, 2, 0.290f, 0.620f, 1.0f, 0.9f);

    // Title
    drawTextMd(14, 28, 0.847f, 0.878f, 0.941f, "Ray Tracing Showcase");

    // Separator
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.24f, 0.30f, 0.45f, 0.7f);
    glBegin(GL_LINES);
    glVertex2i(230, 8); glVertex2i(230, HEADER_H-8);
    glEnd();
    glDisable(GL_BLEND);

    // FPS badge
    {
        int bx = 244;
        float fc = fps >= 30 ? 0.239f : 0.9f;
        float gc = fps >= 30 ? 0.800f : 0.4f;
        float bc = fps >= 30 ? 0.494f : 0.2f;
        fillRect(bx, 10, 72, 24, fc*0.14f, gc*0.1f, bc*0.1f, 0.85f);
        strokeRect(bx, 10, 72, 24, fc, gc, bc, 0.55f);
        drawTextSm(bx+6, 26, fc, gc, bc, "FPS  %3d", fps);
    }
    // SPP badge
    {
        int bx = 324;
        fillRect(bx, 10, 80, 24, 0.05f, 0.08f, 0.17f, 0.85f);
        strokeRect(bx, 10, 80, 24, 0.290f, 0.620f, 1.0f, 0.45f);
        drawTextSm(bx+6, 26, 0.290f, 0.620f, 1.0f, "SPP %4d", samples);
    }
    // Threads badge
    {
        int bx = 412;
        fillRect(bx, 10, 80, 24, 0.05f, 0.08f, 0.10f, 0.85f);
        strokeRect(bx, 10, 80, 24, 0.420f, 0.459f, 0.576f, 0.4f);
        drawTextSm(bx+6, 26, 0.420f, 0.459f, 0.576f, "THR  %2d", thread_count);
    }
    // Objects badge
    {
        int bx = 500;
        fillRect(bx, 10, 80, 24, 0.05f, 0.08f, 0.10f, 0.85f);
        strokeRect(bx, 10, 80, 24, 0.420f, 0.459f, 0.576f, 0.4f);
        drawTextSm(bx+6, 26, 0.420f, 0.459f, 0.576f, "OBJ  %2d", obj_count);
    }

    // Edit mode indicator (top-left below header)
    {
        int mi = edit_mode < 7 ? edit_mode : 0;
        const char* mn = EDIT_MODE_NAMES[mi];
        float r = 0.290f, g2 = 0.620f, b = 1.0f;
        if (edit_mode == MODE_MOVE)  { r=1.0f;  g2=0.702f; b=0.278f; }
        if (edit_mode == MODE_SCALE) { r=0.720f; g2=0.380f; b=1.0f;  }
        if (edit_mode >= MODE_ADD_SPHERE) { r=0.239f; g2=0.800f; b=0.494f; }
        int tw = (int)strlen(mn)*8 + 18;
        fillRect(12, HEADER_H+8, tw, 22, r*0.1f, g2*0.1f, b*0.1f, 0.88f);
        strokeRect(12, HEADER_H+8, tw, 22, r, g2, b, 0.7f);
        drawTextSm(22, HEADER_H+23, r, g2, b, "%s", mn);
    }

    // ── PROPERTIES PANEL ───────────────────────────────────────────────
    if (editor && selected_obj >= 0 && selected_obj < obj_count &&
        selected_obj < (int)editor->scene->objects.size()) {
        const Object&   obj = editor->scene->objects[selected_obj];
        const Material& mat = editor->scene->materials[obj.mat_index];

        int px = screen_w - PANEL_W - 12;
        int py = HEADER_H + 12;

        // Panel background
        fillRect(px, py, PANEL_W, PANEL_H, 0.079f, 0.090f, 0.125f, 0.92f);
        strokeRect(px, py, PANEL_W, PANEL_H, 0.145f, 0.173f, 0.320f, 0.7f);
        // Left accent bar
        fillRect(px, py, 3, PANEL_H, 0.290f, 0.620f, 1.0f, 0.9f);

        // Header
        fillRect(px+3, py, PANEL_W-3, 26, 0.12f, 0.15f, 0.22f, 0.95f);
        drawTextSm(px+12, py+17, 0.290f, 0.620f, 1.0f, "PROPERTIES");

        hLine(px+3, py+26, PANEL_W-3, 0.24f, 0.30f, 0.45f, 0.7f);

        const char* type_str[] = { "Sphere", "Box", "Plane", "Cylinder" };
        const char* mat_str[]  = { "Lambertian", "Metal", "Emissive", "Dielectric", "Checker" };
        const char* ts = (obj.type>=0 && obj.type<4) ? type_str[obj.type] : "Unknown";
        const char* ms = (mat.type>=0 && mat.type<5) ? mat_str[mat.type]  : "Unknown";

        int row = py + 40;
        drawTextSm(px+12, row, 0.420f, 0.459f, 0.576f, "Type");
        drawTextSm(px+80, row, 0.847f, 0.878f, 0.941f, "%s", ts);  row += 18;

        drawTextSm(px+12, row, 0.420f, 0.459f, 0.576f, "Pos");
        drawTextSm(px+80, row, 0.847f, 0.878f, 0.941f,
            "%.2f %.2f %.2f", obj.center.x(), obj.center.y(), obj.center.z());  row += 18;

        if (obj.type == OBJ_SPHERE || obj.type == OBJ_CYLINDER) {
            drawTextSm(px+12, row, 0.420f, 0.459f, 0.576f, "Radius");
            drawTextSm(px+80, row, 0.847f, 0.878f, 0.941f, "%.2f", obj.size.x()); row += 18;
        } else if (obj.type == OBJ_BOX) {
            drawTextSm(px+12, row, 0.420f, 0.459f, 0.576f, "Size");
            drawTextSm(px+80, row, 0.847f, 0.878f, 0.941f,
                "%.2f %.2f %.2f", obj.size.x(), obj.size.y(), obj.size.z()); row += 18;
        }
        hLine(px+12, row, PANEL_W-24, 0.24f, 0.30f, 0.45f, 0.45f); row += 10;

        drawTextSm(px+12, row, 0.420f, 0.459f, 0.576f, "Mat");
        drawTextSm(px+80, row, 1.0f, 0.702f, 0.278f, "%s", ms); row += 18;

        // Color swatch
        drawTextSm(px+12, row, 0.420f, 0.459f, 0.576f, "Color");
        colorSwatch(px+80, row, (float)mat.albedo.x(), (float)mat.albedo.y(), (float)mat.albedo.z());
        drawTextSm(px+98, row, 0.847f, 0.878f, 0.941f,
            "%.2f %.2f %.2f", mat.albedo.x(), mat.albedo.y(), mat.albedo.z()); row += 18;

        if (mat.type == MAT_METAL) {
            drawTextSm(px+12, row, 0.420f, 0.459f, 0.576f, "Fuzz");
            drawTextSm(px+80, row, 0.900f, 0.900f, 0.600f, "%.2f", mat.fuzz); row += 18;
        } else if (mat.type == MAT_DIELECTRIC) {
            drawTextSm(px+12, row, 0.420f, 0.459f, 0.576f, "IOR");
            drawTextSm(px+80, row, 0.500f, 0.900f, 0.900f, "%.2f", mat.ir); row += 18;
        } else if (mat.type == MAT_EMISSIVE) {
            drawTextSm(px+12, row, 0.420f, 0.459f, 0.576f, "Emit");
            drawTextSm(px+80, row, 1.0f, 0.800f, 0.350f, "x%.1f", mat.emit_strength); row += 18;
        }
        hLine(px+12, row, PANEL_W-24, 0.24f, 0.30f, 0.45f, 0.35f); row += 8;
        drawTextSm(px+12, row, 0.28f, 0.35f, 0.48f, "T:mat  +/-:prop  C/V/X/G:color");
    }

    // ── BOTTOM RENDER MODE BAR ─────────────────────────────────────────
    int bar_y = screen_h - FOOTER_H;
    fillRect(0, bar_y, screen_w, FOOTER_H, 0.051f, 0.059f, 0.090f, 0.94f);
    // Top border
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.145f, 0.173f, 0.320f, 0.8f);
    glBegin(GL_LINES);
    glVertex2i(0, bar_y); glVertex2i(screen_w, bar_y);
    glEnd();
    glDisable(GL_BLEND);

    // Label
    drawTextSm(12, bar_y + 14, 0.420f, 0.459f, 0.576f, "MODE");

    // Mode tabs
    int tab_start_x = 60;
    int tab_w = 90, tab_h = 30, tab_gap = 4;
    for (int m = 0; m < RMODE_COUNT; m++) {
        int tx = tab_start_x + m * (tab_w + tab_gap);
        int ty = bar_y + 7;
        bool active = (m == render_mode);
        float mr = MODE_COLORS[m][0];
        float mg = MODE_COLORS[m][1];
        float mb = MODE_COLORS[m][2];
        if (active) {
            fillRect(tx, ty, tab_w, tab_h, mr*0.22f, mg*0.18f, mb*0.18f, 0.95f);
            strokeRect(tx, ty, tab_w, tab_h, mr, mg, mb, 0.9f);
            // Top indicator line
            fillRect(tx, ty, tab_w, 2, mr, mg, mb, 0.95f);
            char label[32];
            snprintf(label, sizeof(label), "F%d %s", m+1, RMODE_NAMES[m]);
            drawTextSm(tx+6, ty+19, mr, mg, mb, "%s", label);
        } else {
            fillRect(tx, ty, tab_w, tab_h, 0.09f, 0.10f, 0.14f, 0.8f);
            strokeRect(tx, ty, tab_w, tab_h, 0.20f, 0.23f, 0.33f, 0.5f);
            char label[32];
            snprintf(label, sizeof(label), "F%d %s", m+1, RMODE_NAMES[m]);
            drawTextSm(tx+6, ty+19, 0.35f, 0.40f, 0.52f, "%s", label);
        }
    }

    // ── HELP PANEL (toggle H) ──────────────────────────────────────────
    if (show_help) {
        int hx = 14, hy = HEADER_H + 44, hw = 290, hh = 192;
        fillRect(hx, hy, hw, hh, 0.079f, 0.090f, 0.125f, 0.92f);
        strokeRect(hx, hy, hw, hh, 0.145f, 0.173f, 0.320f, 0.7f);
        fillRect(hx, hy, 3, hh, 1.0f, 0.702f, 0.278f, 0.9f);
        fillRect(hx+3, hy, hw-3, 24, 0.13f, 0.15f, 0.22f, 0.95f);
        drawTextSm(hx+12, hy+16, 1.0f, 0.702f, 0.278f, "CONTROLS");
        hLine(hx+3, hy+24, hw-3, 0.24f, 0.30f, 0.45f, 0.7f);

        struct HelpItem { const char* key; const char* desc; };
        static const HelpItem help[] = {
            {"LMB Drag", "Orbit camera"},
            {"RMB Drag", "Pan camera"},
            {"Scroll",   "Zoom"},
            {"Click",    "Select object"},
            {"M / S",    "Move / Scale mode"},
            {"1/2/3/4",  "Add Sphere/Box/Plane/Cyl"},
            {"Del",      "Delete selected"},
            {"T",        "Cycle material"},
            {"Arrows",   "Move selected (X/Y)"},
            {"PgUp/Dn",  "Move selected (Z)"},
            {"R",        "Reset camera"},
            {"Ctrl+Z/Y", "Undo / Redo"},
        };
        int row = hy + 38;
        for (auto& item : help) {
            drawTextSm(hx+12, row, 0.290f, 0.620f, 1.0f, "%-10s", item.key);
            drawTextSm(hx+110, row, 0.700f, 0.740f, 0.820f, "%s", item.desc);
            row += 14;
        }
        drawTextSm(hx+12, row+2, 0.35f, 0.40f, 0.50f, "H: toggle this panel");
    } else {
        // Small hint
        drawTextSm(14, HEADER_H + 46, 0.28f, 0.33f, 0.45f, "H: Controls");
    }

    // Restore state
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
}

#endif
