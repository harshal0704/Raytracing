#ifndef EDITOR_H
#define EDITOR_H

#include "core/math.h"
#include "scene/scene.h"
#include "render/camera.h"
#include "render/tracer.h"
#include <windows.h>
#include <GL/gl.h>
#include <vector>

enum EditMode {
    MODE_VIEW, MODE_MOVE, MODE_SCALE,
    MODE_ADD_SPHERE, MODE_ADD_BOX, MODE_ADD_PLANE, MODE_ADD_CYLINDER
};

struct Editor {
    Scene*   scene;
    Camera*  cam;
    Tracer*  tracer;
    int      selected;
    EditMode mode;
    bool     wireframe;
    bool     show_gizmos;  // G: toggle wireframe gizmos
    bool     show_help;    // H: toggle help panel
    bool     lmb_down, rmb_down;
    int      drag_start_x, drag_start_y;
    int      last_mouse_x, last_mouse_y;
    bool     ctrl_down;

    std::vector<Scene> undo_stack;
    std::vector<Scene> redo_stack;
    static const int MAX_UNDO = 20;

    Editor()
        : scene(nullptr), cam(nullptr), tracer(nullptr), selected(-1), mode(MODE_VIEW),
          wireframe(false), show_gizmos(false), show_help(false),
          lmb_down(false), rmb_down(false),
          drag_start_x(0), drag_start_y(0),
          last_mouse_x(0), last_mouse_y(0), ctrl_down(false) {}

    void init(Scene* s, Camera* c, Tracer* t) { scene=s; cam=c; tracer=t; }

    void pushUndo() {
        if ((int)undo_stack.size() >= MAX_UNDO) undo_stack.erase(undo_stack.begin());
        undo_stack.push_back(scene->copy());
        redo_stack.clear();
    }
    void undo() {
        if (undo_stack.empty()) return;
        redo_stack.push_back(scene->copy());
        scene->restore(undo_stack.back()); undo_stack.pop_back();
        if (selected >= (int)scene->objects.size()) selected = -1;
    }
    void redo() {
        if (redo_stack.empty()) return;
        undo_stack.push_back(scene->copy());
        scene->restore(redo_stack.back()); redo_stack.pop_back();
        if (selected >= (int)scene->objects.size()) selected = -1;
    }

    void saveScene(const char* path) { if (scene) scene->save(path); }
    bool loadScene(const char* path) {
        if (!scene) return false;
        bool ok = scene->load(path);
        if (ok) { selected=-1; undo_stack.clear(); redo_stack.clear(); }
        return ok;
    }

    int pick_object(int mx, int my) {
        if (!scene || !cam) return -1;
        ray r = cam->screen_ray(mx, my);
        HitRecord rec; double best_t=INF; int best=-1;
        for (int i=0; i<(int)scene->objects.size(); i++) {
            HitRecord tmp;
            if (hit_object(scene->objects[i], r, 0.001, best_t, tmp) && tmp.t < best_t) {
                best_t=tmp.t; best=i;
            }
        }
        return best;
    }

    void on_lmb_down(int mx, int my) {
        lmb_down=true; drag_start_x=mx; drag_start_y=my;
        last_mouse_x=mx; last_mouse_y=my;
        if (mode==MODE_MOVE && selected>=0) pushUndo();
    }
    void on_lmb_up(int mx, int my) {
        lmb_down=false;
        if (abs(mx-drag_start_x)<4 && abs(my-drag_start_y)<4) {
            int hit = pick_object(mx, my);
            if (mode >= MODE_ADD_SPHERE && hit == -1) {
                ray r = cam->screen_ray(mx, my);
                double t = -dot(r.origin(), vec3(0,1,0)) / dot(r.direction(), vec3(0,1,0));
                if (t > 0) {
                    point3 pos = r.at(t);
                    int mat_idx = scene->addMaterial(Material::lambertian(
                        color3(random01()*0.7+0.3, random01()*0.7+0.3, random01()*0.7+0.3)));
                    pushUndo();
                    switch(mode) {
                        case MODE_ADD_SPHERE: scene->addObject(Object::makeSphere(pos,0.6,mat_idx)); break;
                        case MODE_ADD_BOX:    scene->addObject(Object::makeBox(pos,vec3(1,1,1),mat_idx)); break;
                        case MODE_ADD_PLANE:  scene->addObject(Object::makePlane(pos,vec3(0,1,0),mat_idx)); break;
                        case MODE_ADD_CYLINDER: scene->addObject(Object::makeCylinder(pos,0.5,1.5,mat_idx)); break;
                        default: break;
                    }
                }
                mode = MODE_VIEW;
            } else {
                selected = hit;
                for (int i=0; i<(int)scene->objects.size(); i++) scene->objects[i].selected=(i==selected);
            }
        }
    }
    void on_rmb_down(int mx, int my) { rmb_down=true; last_mouse_x=mx; last_mouse_y=my; }
    void on_rmb_up(int, int)         { rmb_down=false; }

    void on_mouse_move(int mx, int my) {
        int dx=mx-last_mouse_x, dy=my-last_mouse_y;
        last_mouse_x=mx; last_mouse_y=my;
        if (!cam) return;
        if (lmb_down && mode==MODE_VIEW) cam->orbit(dx*0.3, dy*0.3);
        if (rmb_down) cam->pan(-dx*0.01, dy*0.01);
        if (lmb_down && mode==MODE_MOVE && selected>=0 && selected<(int)scene->objects.size()) {
            double move_scale = 0.01 * cam->distance;
            vec3 move = dx*cam->right*move_scale - dy*cam->up*move_scale;
            scene->objects[selected].center = scene->objects[selected].center + move;
        }
    }
    void on_scroll(int delta) { if (cam) cam->zoom(delta*0.5); }

    void cycleMaterial() {
        if (selected<0 || selected>=(int)scene->objects.size()) return;
        Object&   obj = scene->objects[selected];
        if (obj.mat_index<0 || obj.mat_index>=(int)scene->materials.size()) return;
        Material& mat = scene->materials[obj.mat_index];
        pushUndo();
        switch(mat.type) {
            case MAT_LAMBERTIAN: mat.type=MAT_METAL; break;
            case MAT_METAL:      mat.type=MAT_DIELECTRIC; mat.ir=1.5; mat.albedo=color3(1,1,1); break;
            case MAT_DIELECTRIC: mat.type=MAT_EMISSIVE; mat.emit_color=color3(1,0.8,0.5); mat.emit_strength=3.0; break;
            case MAT_EMISSIVE:   mat.type=MAT_LAMBERTIAN; mat.emit_strength=0.0; mat.albedo=color3(0.8,0.3,0.3); break;
            default: mat.type=MAT_LAMBERTIAN; break;
        }
    }

    void on_key(int vk) {
        if (!scene || !cam) return;
        if (ctrl_down) {
            if (vk=='Z') { undo(); if(tracer) tracer->needs_reset=true; return; }
            if (vk=='Y') { redo(); if(tracer) tracer->needs_reset=true; return; }
            if (vk=='S') { saveScene("scene.dat"); return; }
            if (vk=='L') { loadScene("scene.dat"); if(tracer) tracer->needs_reset=true; return; }
        }
        // Render mode switching F1–F8
        if (tracer) {
            RenderMode rm = fkey_to_mode(vk);
            if (rm != RMODE_COUNT) { tracer->render_mode=rm; tracer->needs_reset=true; return; }
        }
        switch(vk) {
            case VK_CONTROL: ctrl_down=true; break;
            case 'M': mode=MODE_MOVE; break;
            case 'S': if(!ctrl_down) mode=MODE_SCALE; break;
            case '1': mode=MODE_ADD_SPHERE; break;
            case '2': mode=MODE_ADD_BOX; break;
            case '3': mode=MODE_ADD_PLANE; break;
            case '4': mode=MODE_ADD_CYLINDER; break;
            case 'G': show_gizmos = !show_gizmos; break;
            case 'H': show_help   = !show_help; break;
            case 'R':
                cam->target=vec3(0,0,0); cam->distance=7; cam->yaw=35; cam->pitch=22; cam->update();
                if(tracer) tracer->needs_reset=true; break;
            case VK_ESCAPE:
                selected=-1; mode=MODE_VIEW;
                for (auto& o:scene->objects) o.selected=false;
                break;
            case VK_DELETE:
                if (selected>=0 && selected<(int)scene->objects.size()) {
                    pushUndo();
                    scene->objects.erase(scene->objects.begin()+selected);
                    selected=-1; if(tracer) tracer->needs_reset=true;
                }
                break;
            case 'T': cycleMaterial(); if(tracer) tracer->needs_reset=true; break;
        }
        if (selected>=0 && selected<(int)scene->objects.size()) {
            Object& obj = scene->objects[selected];
            double step=0.2;
            if (mode==MODE_MOVE) {
                if (vk==VK_LEFT)  { pushUndo(); obj.center=obj.center-cam->right*step; }
                if (vk==VK_RIGHT) { pushUndo(); obj.center=obj.center+cam->right*step; }
                if (vk==VK_UP)    { pushUndo(); obj.center=obj.center+cam->up*step; }
                if (vk==VK_DOWN)  { pushUndo(); obj.center=obj.center-cam->up*step; }
            }
            if (vk==VK_PRIOR) { pushUndo(); obj.center=obj.center+vec3(0,step,0); }
            if (vk==VK_NEXT)  { pushUndo(); obj.center=obj.center-vec3(0,step,0); }
            if (obj.mat_index>=0 && obj.mat_index<(int)scene->materials.size()) {
                Material& mat=scene->materials[obj.mat_index];
                if (vk==VK_OEM_PLUS || vk==VK_ADD) {
                    pushUndo();
                    if (mat.type==MAT_METAL) mat.fuzz=clamp(mat.fuzz+0.05,0.0,1.0);
                    if (mat.type==MAT_DIELECTRIC) mat.ir=clamp(mat.ir+0.1,1.0,3.0);
                    if (mat.type==MAT_EMISSIVE) mat.emit_strength=clamp(mat.emit_strength+0.5,0.0,20.0);
                }
                if (vk==VK_OEM_MINUS || vk==VK_SUBTRACT) {
                    pushUndo();
                    if (mat.type==MAT_METAL) mat.fuzz=clamp(mat.fuzz-0.05,0.0,1.0);
                    if (mat.type==MAT_DIELECTRIC) mat.ir=clamp(mat.ir-0.1,1.0,3.0);
                    if (mat.type==MAT_EMISSIVE) mat.emit_strength=clamp(mat.emit_strength-0.5,0.0,20.0);
                }
                if (vk=='C') { pushUndo(); mat.albedo=color3(0.85,0.15,0.15); }
                if (vk=='V') { pushUndo(); mat.albedo=color3(0.15,0.80,0.25); }
                if (vk=='X') { pushUndo(); mat.albedo=color3(0.15,0.25,0.90); }
                if (vk=='G') { pushUndo(); mat.albedo=color3(0.90,0.65,0.15); }
            }
            if (mode==MODE_SCALE) {
                double s=1.1;
                if (vk==VK_UP)   { pushUndo(); obj.size=obj.size*s; }
                if (vk==VK_DOWN) { pushUndo(); obj.size=obj.size*(1.0/s); }
            }
            if(tracer) tracer->needs_reset=true;
        }
    }
    void on_key_up(int vk) { if (vk==VK_CONTROL) ctrl_down=false; }

    // OpenGL wireframe gizmos
    void drawWireframeBox(const point3& c, const vec3& s) {
        double hx=s.x()*0.5, hy=s.y()*0.5, hz=s.z()*0.5;
        point3 corners[8] = {
            c+vec3(-hx,-hy,-hz), c+vec3(hx,-hy,-hz), c+vec3(hx,hy,-hz), c+vec3(-hx,hy,-hz),
            c+vec3(-hx,-hy, hz), c+vec3(hx,-hy, hz), c+vec3(hx,hy, hz), c+vec3(-hx,hy, hz)
        };
        glBegin(GL_LINES);
        for (int i=0; i<4; i++) {
            int j=(i+1)%4;
            glVertex3d(corners[i].x(),corners[i].y(),corners[i].z());
            glVertex3d(corners[j].x(),corners[j].y(),corners[j].z());
            glVertex3d(corners[i+4].x(),corners[i+4].y(),corners[i+4].z());
            glVertex3d(corners[j+4].x(),corners[j+4].y(),corners[j+4].z());
            glVertex3d(corners[i].x(),corners[i].y(),corners[i].z());
            glVertex3d(corners[i+4].x(),corners[i+4].y(),corners[i+4].z());
        }
        glEnd();
    }
    void drawWireframeSphere(const point3& c, double r) {
        const int seg=24;
        for (int axis=0; axis<3; axis++) {
            glBegin(GL_LINE_LOOP);
            for (int i=0; i<seg; i++) {
                double a = 2.0*PI*i/seg;
                double x=0,y=0,z=0;
                if (axis==0) { y=r*cos(a); z=r*sin(a); }
                else if(axis==1) { x=r*cos(a); z=r*sin(a); }
                else { x=r*cos(a); y=r*sin(a); }
                glVertex3d(c.x()+x, c.y()+y, c.z()+z);
            }
            glEnd();
        }
    }
    void drawWireframeCylinder(const point3& c, double radius, double height) {
        const int seg=20;
        double hh=height*0.5;
        for (int cap=0; cap<2; cap++) {
            double hy = cap==0 ? hh : -hh;
            glBegin(GL_LINE_LOOP);
            for (int i=0; i<seg; i++) {
                double a=2.0*PI*i/seg;
                glVertex3d(c.x()+radius*cos(a), c.y()+hy, c.z()+radius*sin(a));
            }
            glEnd();
        }
        glBegin(GL_LINES);
        for (int i=0; i<seg; i+=5) {
            double a=2.0*PI*i/seg;
            glVertex3d(c.x()+radius*cos(a), c.y()+hh,  c.z()+radius*sin(a));
            glVertex3d(c.x()+radius*cos(a), c.y()-hh,  c.z()+radius*sin(a));
        }
        glEnd();
    }

    void drawGizmos() {
        if (!scene) return;
        for (int i=0; i<(int)scene->objects.size(); i++) {
            const Object& obj = scene->objects[i];
            if (obj.selected) { glColor3f(1.0f,0.95f,0.20f); glLineWidth(2.5f); }
            else              { glColor3f(0.55f,0.60f,0.65f); glLineWidth(0.8f); }
            switch(obj.type) {
                case OBJ_SPHERE:   drawWireframeSphere(obj.center, obj.size.x()); break;
                case OBJ_BOX:      drawWireframeBox(obj.center, obj.size); break;
                case OBJ_CYLINDER: drawWireframeCylinder(obj.center, obj.size.x(), obj.size.y()); break;
                case OBJ_PLANE: {
                    glBegin(GL_LINES);
                    double s=2.5;
                    vec3 n=obj.normal;
                    vec3 t1 = fabs(n.y())<0.9 ? cross(n,vec3(0,1,0)) : cross(n,vec3(1,0,0));
                    t1=unit_vector(t1); vec3 t2=cross(n,t1);
                    for (int l=-4; l<=4; l++) {
                        point3 p1=obj.center+s*l*t1;
                        point3 p2=obj.center+s*l*t2;
                        glVertex3d(p1.x()-s*4*t2.x(),p1.y()-s*4*t2.y(),p1.z()-s*4*t2.z());
                        glVertex3d(p1.x()+s*4*t2.x(),p1.y()+s*4*t2.y(),p1.z()+s*4*t2.z());
                        glVertex3d(p2.x()-s*4*t1.x(),p2.y()-s*4*t1.y(),p2.z()-s*4*t1.z());
                        glVertex3d(p2.x()+s*4*t1.x(),p2.y()+s*4*t1.y(),p2.z()+s*4*t1.z());
                    }
                    glEnd();
                    break;
                }
            }
        }
        glLineWidth(1.0f);
    }
};

#endif
