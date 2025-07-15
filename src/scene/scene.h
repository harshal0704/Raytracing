#ifndef SCENE_H
#define SCENE_H

#include "scene/object.h"
#include "scene/material.h"
#include <vector>
#include <fstream>

struct Scene {
    std::vector<Object>   objects;
    std::vector<Material> materials;
    color3 background_top;
    color3 background_bot;

    Scene() : background_top(0.48f, 0.64f, 0.88f), background_bot(0.98f, 0.93f, 0.85f) {}

    int addMaterial(const Material& m) { materials.push_back(m); return (int)materials.size()-1; }
    int addObject(const Object& o)     { objects.push_back(o);   return (int)objects.size()-1;   }

    bool hit(const ray& r, double t_min, double t_max, HitRecord& rec) const {
        HitRecord temp;
        bool hit_anything = false;
        double closest = t_max;
        for (int i = 0; i < (int)objects.size(); i++) {
            if (hit_object(objects[i], r, t_min, closest, temp)) {
                hit_anything = true;
                closest = temp.t;
                rec = temp;
            }
        }
        return hit_anything;
    }

    void save(const char* filename) const {
        std::ofstream f(filename, std::ios::binary);
        if (!f) return;
        int oc = (int)objects.size();
        f.write((char*)&oc, sizeof(int));
        for (auto& o : objects) {
            f.write((char*)&o.type,   sizeof(int));
            f.write((char*)&o.center.e, sizeof(double)*3);
            f.write((char*)&o.size.e,   sizeof(double)*3);
            if (o.type == OBJ_PLANE) f.write((char*)&o.normal.e, sizeof(double)*3);
            f.write((char*)&o.mat_index, sizeof(int));
        }
        int mc = (int)materials.size();
        f.write((char*)&mc, sizeof(int));
        for (auto& m : materials) {
            f.write((char*)&m.type,          sizeof(int));
            f.write((char*)&m.albedo.e,      sizeof(double)*3);
            f.write((char*)&m.fuzz,          sizeof(double));
            f.write((char*)&m.emit_color.e,  sizeof(double)*3);
            f.write((char*)&m.emit_strength, sizeof(double));
            f.write((char*)&m.ir,            sizeof(double));
        }
    }

    bool load(const char* filename) {
        std::ifstream f(filename, std::ios::binary);
        if (!f) return false;
        objects.clear(); materials.clear();
        int oc; f.read((char*)&oc, sizeof(int));
        for (int i = 0; i < oc; i++) {
            Object o;
            f.read((char*)&o.type,      sizeof(int));
            f.read((char*)&o.center.e,  sizeof(double)*3);
            f.read((char*)&o.size.e,    sizeof(double)*3);
            if (o.type == OBJ_PLANE) f.read((char*)&o.normal.e, sizeof(double)*3);
            f.read((char*)&o.mat_index, sizeof(int));
            o.selected = false;
            objects.push_back(o);
        }
        int mc; f.read((char*)&mc, sizeof(int));
        for (int i = 0; i < mc; i++) {
            Material m;
            f.read((char*)&m.type,          sizeof(int));
            f.read((char*)&m.albedo.e,      sizeof(double)*3);
            f.read((char*)&m.fuzz,          sizeof(double));
            f.read((char*)&m.emit_color.e,  sizeof(double)*3);
            f.read((char*)&m.emit_strength, sizeof(double));
            f.read((char*)&m.ir,            sizeof(double));
            materials.push_back(m);
        }
        return true;
    }

    Scene copy() const {
        Scene s; s.objects=objects; s.materials=materials;
        s.background_top=background_top; s.background_bot=background_bot; return s;
    }
    void restore(const Scene& s) {
        objects=s.objects; materials=s.materials;
        background_top=s.background_top; background_bot=s.background_bot;
    }

    color3 skyColor(const vec3& dir) const {
        double t = clamp01(0.5 * (dir.y() + 1.0));
        return (1.0-t)*background_bot + t*background_top;
    }

    // ── Default scene: well-lit showcase of all material types ─────────
    void setupDefault() {
        materials.clear(); objects.clear();

        // ── Materials ──────────────────────────────────────────────────
        // Diffuse / Lambertian
        int mat_checker = addMaterial(Material::checker(
            color3(0.92,0.92,0.92), color3(0.28,0.28,0.30), 1.2));
        int mat_red     = addMaterial(Material::lambertian(color3(0.88, 0.14, 0.14)));
        int mat_blue    = addMaterial(Material::lambertian(color3(0.10, 0.28, 0.90)));
        int mat_green   = addMaterial(Material::lambertian(color3(0.10, 0.78, 0.20)));
        int mat_orange  = addMaterial(Material::lambertian(color3(0.96, 0.50, 0.04)));
        int mat_purple  = addMaterial(Material::lambertian(color3(0.50, 0.08, 0.88)));
        // Metal
        int mat_gold    = addMaterial(Material::metal(color3(0.92, 0.76, 0.16), 0.03));
        int mat_mirror  = addMaterial(Material::metal(color3(0.97, 0.97, 1.00), 0.0));
        int mat_brushed = addMaterial(Material::metal(color3(0.66, 0.66, 0.70), 0.28));
        // Glass / Dielectric
        int mat_glass   = addMaterial(Material::dielectric(1.52));
        int mat_diamond = addMaterial(Material::dielectric(2.42));
        // Emissive lights — placed LOW and CLOSE for strong fill
        int mat_sun     = addMaterial(Material::makeEmissive(color3(1.00, 0.90, 0.65), 16.0));
        int mat_skyblue = addMaterial(Material::makeEmissive(color3(0.45, 0.78, 1.00), 13.0));
        int mat_ember   = addMaterial(Material::makeEmissive(color3(1.00, 0.38, 0.15), 13.0));

        // ── Floor ──────────────────────────────────────────────────────
        addObject(Object::makePlane(point3(0,-1.2,0), vec3(0,1,0), mat_checker));

        // ── Front row (closest to camera) ──────────────────────────────
        // Central glass sphere — focal hero
        addObject(Object::makeSphere(point3( 0.0, -0.25, 0.0), 0.95, mat_glass));
        // Gold sphere, left
        addObject(Object::makeSphere(point3(-2.4, -0.50, 0.4), 0.70, mat_gold));
        // Mirror sphere, right
        addObject(Object::makeSphere(point3( 2.4, -0.50, 0.4), 0.70, mat_mirror));

        // ── Middle row ─────────────────────────────────────────────────
        // Blue tall box left
        addObject(Object::makeBox(point3(-1.5, -0.30, 2.5), vec3(0.85, 1.8, 0.85), mat_blue));
        // Green box right
        addObject(Object::makeBox(point3( 1.5, -0.40, 2.5), vec3(0.85, 1.6, 0.85), mat_green));
        // Brushed metal cylinder, center back
        addObject(Object::makeCylinder(point3(0.0, -0.50, 3.0), 0.50, 1.4, mat_brushed));

        // ── Far flanks ─────────────────────────────────────────────────
        // Red sphere far left
        addObject(Object::makeSphere(point3(-3.5, -0.72, 2.0), 0.48, mat_red));
        // Orange cylinder far right
        addObject(Object::makeCylinder(point3( 3.5, -0.72, 2.0), 0.38, 0.96, mat_orange));
        // Purple sphere, back-center
        addObject(Object::makeSphere(point3( 0.0, -0.80, 4.6), 0.40, mat_purple));

        // Tiny diamond floating on top of glass sphere — detail gem
        addObject(Object::makeSphere(point3( 0.45, 0.78, -0.22), 0.21, mat_diamond));

        // ── Area lights: LOW (y≈1.6) so they actually illuminate objects ─
        addObject(Object::makeSphere(point3(-2.0,  1.6, -0.8), 0.42, mat_sun));     // warm left
        addObject(Object::makeSphere(point3( 2.0,  1.6, -0.8), 0.38, mat_skyblue)); // cool right
        addObject(Object::makeSphere(point3( 0.0,  1.8,  3.2), 0.32, mat_ember));   // ember back
    }
};

#endif
