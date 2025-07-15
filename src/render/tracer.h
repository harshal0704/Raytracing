#ifndef TRACER_H
#define TRACER_H

#include "core/math.h"
#include "core/ray.h"
#include "scene/scene.h"
#include "render/camera.h"
#include <vector>
#include <windows.h>
#include <process.h>
#include <cstdint>

// ── Ray Tracing Modes ──────────────────────────────────────────────────
enum RenderMode {
    RMODE_PATH_TRACE = 0,   // F1: Full path tracing (default)
    RMODE_NORMALS,           // F2: Surface normal colours
    RMODE_ALBEDO,            // F3: Raw material albedo, no lighting
    RMODE_DEPTH,             // F4: Distance from camera (fog)
    RMODE_SHADOW,            // F5: Direct shadow only (1 bounce)
    RMODE_AO,                // F6: Ambient Occlusion
    RMODE_FRESNEL,           // F7: Schlick reflectance at surface
    RMODE_WIREFRAME,         // F8: Edge detection via surface curvature
    RMODE_COUNT
};

static const char* RMODE_NAMES[RMODE_COUNT] = {
    "Path Trace", "Normals", "Albedo", "Depth",
    "Shadow",     "Amb Occ", "Fresnel","Wireframe"
};

// Keyboard F1–F8 → RMODE_PATH_TRACE … RMODE_WIREFRAME
inline RenderMode fkey_to_mode(int vk) {
    if (vk >= VK_F1 && vk <= VK_F8) return (RenderMode)(vk - VK_F1);
    return RMODE_COUNT; // unrecognized
}

// ── Tracer ────────────────────────────────────────────────────────────
struct Tracer {
    float*   accumulation;
    int      width, height;
    int      samples_per_pixel;
    int      max_bounces;
    volatile LONG scanline_row;
    bool     needs_reset;
    float    exposure;
    int      num_threads;
    RenderMode render_mode;

    const Scene*  scene_ptr;
    const Camera* cam_ptr;

    Tracer()
        : accumulation(nullptr), width(0), height(0),
          samples_per_pixel(1), max_bounces(5),
          scanline_row(0), needs_reset(true), exposure(2.5f),
          num_threads(1), render_mode(RMODE_PATH_TRACE),
          scene_ptr(nullptr), cam_ptr(nullptr) {}

    void init(int w, int h) {
        width=w; height=h;
        if (accumulation) delete[] accumulation;
        accumulation = new float[w*h*3]();
        SYSTEM_INFO si; GetSystemInfo(&si);
        num_threads = (int)si.dwNumberOfProcessors;
        if (num_threads < 1) num_threads = 2;
        if (num_threads > 16) num_threads = 16;
        reset();
    }
    void reset() {
        if (accumulation) for (int i=0; i<width*height*3; i++) accumulation[i]=0.0f;
        scanline_row = 0;
        samples_per_pixel = 1;
        needs_reset = false;
    }
    void free() { if (accumulation) { delete[] accumulation; accumulation=nullptr; } }

    // ── Per-mode shading ───────────────────────────────────────────────

    // Full path tracing with ambient fill so low-SPP frames are never pitch black
    color3 shade_pathTrace(const ray& r, int depth) {
        if (depth <= 0) return color3(0,0,0);
        HitRecord rec;
        if (scene_ptr->hit(r, 0.001, INF, rec)) {
            const Material& mat = scene_ptr->materials[rec.mat_index];
            if (mat.type == MAT_EMISSIVE) return mat.emit_color * mat.emit_strength;
            ScatterResult sr;
            if (material_scatter(mat, r, rec, sr)) {
                color3 scattered = shade_pathTrace(sr.scattered_ray, depth-1);
                color3 result = sr.attenuation * scattered;
                // Ambient fill: prevents pure black at low SPP
                color3 amb = 0.04 * scene_ptr->skyColor(unit_vector(rec.normal));
                result = result + sr.attenuation * amb;
                // explicit shadow for lambertian/checker
                if (mat.type == MAT_LAMBERTIAN || mat.type == MAT_CHECKER) {
                    double shadow = 1.0;
                    for (auto& obj : scene_ptr->objects) {
                        if (obj.mat_index < 0 || obj.mat_index >= (int)scene_ptr->materials.size()) continue;
                        const Material& lm = scene_ptr->materials[obj.mat_index];
                        if (lm.type == MAT_EMISSIVE && lm.emit_strength > 1.0) {
                            vec3 light_dir = unit_vector(obj.center - rec.p);
                            double light_dist = (obj.center - rec.p).length();
                            ray shadow_ray(rec.p + rec.normal*0.001, light_dir);
                            HitRecord sr2;
                            if (scene_ptr->hit(shadow_ray, 0.001, light_dist, sr2)) {
                                double dist = (sr2.p - obj.center).length();
                                if (dist < obj.size.x()*1.5) shadow *= 0.35;
                            }
                        }
                    }
                    result = result * shadow;
                }
                return result;
            }
            return color3(0,0,0);
        }
        return scene_ptr->skyColor(unit_vector(r.direction()));
    }

    // Normal visualization
    color3 shade_normals(const ray& r) {
        HitRecord rec;
        if (scene_ptr->hit(r, 0.001, INF, rec)) {
            vec3 n = rec.normal;
            return color3(n.x()*0.5+0.5, n.y()*0.5+0.5, n.z()*0.5+0.5);
        }
        return scene_ptr->skyColor(unit_vector(r.direction()));
    }

    // Albedo (raw color, no lighting)
    color3 shade_albedo(const ray& r) {
        HitRecord rec;
        if (scene_ptr->hit(r, 0.001, INF, rec)) {
            const Material& mat = scene_ptr->materials[rec.mat_index];
            if (mat.type == MAT_EMISSIVE) return mat.emit_color;
            return material_albedo(mat, rec.p);
        }
        return color3(0.08, 0.08, 0.12);
    }

    // Depth (normalized distance fog)
    color3 shade_depth(const ray& r) {
        HitRecord rec;
        if (scene_ptr->hit(r, 0.001, INF, rec)) {
            double near_d = 0.5, far_d = 18.0;
            double t = clamp01(1.0-(rec.t - near_d)/(far_d-near_d));
            // Map to a cool blue-to-white gradient
            return (1.0-t)*color3(0.04,0.06,0.18) + t*color3(0.85,0.92,1.0);
        }
        return color3(0.02, 0.03, 0.08);
    }

    // Direct shadow only
    color3 shade_shadow(const ray& r) {
        HitRecord rec;
        if (!scene_ptr->hit(r, 0.001, INF, rec)) {
            return scene_ptr->skyColor(unit_vector(r.direction()));
        }
        const Material& mat = scene_ptr->materials[rec.mat_index];
        if (mat.type == MAT_EMISSIVE) return color3(1.0,1.0,0.8);

        color3 base = material_albedo(mat, rec.p);
        double brightness = 0.05;

        for (auto& obj : scene_ptr->objects) {
            if (obj.mat_index < 0 || obj.mat_index >= (int)scene_ptr->materials.size()) continue;
            const Material& lm = scene_ptr->materials[obj.mat_index];
            if (lm.type != MAT_EMISSIVE) continue;
            vec3 to_light = obj.center - rec.p;
            double dist = to_light.length();
            vec3 light_dir = unit_vector(to_light);
            double ndotl = clamp01(dot(rec.normal, light_dir));
            ray shadow_ray(rec.p + rec.normal*0.001, light_dir);
            HitRecord sr2;
            double shadow = 1.0;
            if (scene_ptr->hit(shadow_ray, 0.001, dist*0.98, sr2)) shadow = 0.0;
            brightness += ndotl * shadow * lm.emit_strength * 0.08 / (dist*dist + 0.5);
        }
        return base * clamp01(brightness);
    }

    // Ambient occlusion
    color3 shade_ao(const ray& r) {
        HitRecord rec;
        if (!scene_ptr->hit(r, 0.001, INF, rec)) return color3(1,1,1);
        const int N = 12;
        double unblocked = 0;
        for (int i = 0; i < N; i++) {
            vec3 dir = rec.normal + random_unit_vector();
            if (dir.near_zero()) dir = rec.normal;
            dir = unit_vector(dir);
            if (dot(dir, rec.normal) < 0) dir = -dir;
            ray ao_ray(rec.p + rec.normal*0.001, dir);
            HitRecord tmp;
            if (!scene_ptr->hit(ao_ray, 0.001, 1.5, tmp)) unblocked += 1.0;
        }
        double ao = unblocked / (double)N;
        // tint: dark blue-grey in occluded areas
        return (1.0-ao)*color3(0.04,0.06,0.14) + ao*color3(0.92,0.95,1.0);
    }

    // Fresnel / reflectance visualization
    color3 shade_fresnel(const ray& r) {
        HitRecord rec;
        if (!scene_ptr->hit(r, 0.001, INF, rec)) {
            return scene_ptr->skyColor(unit_vector(r.direction()));
        }
        const Material& mat = scene_ptr->materials[rec.mat_index];
        double ref_idx = (mat.type == MAT_DIELECTRIC) ? mat.ir : 1.5;
        vec3 unit_dir = unit_vector(r.direction());
        double cos_theta = clamp01(dot(-unit_dir, rec.normal));
        double f = schlick(cos_theta, ref_idx > 1.0 ? ref_idx : 1.5);
        // Gold highlights on interior, cyan on exterior
        color3 edge_col = color3(1.0, 0.85, 0.2);
        color3 face_col = color3(0.1, 0.3, 0.7);
        return (1.0-f)*face_col + f*edge_col;
    }

    // Edge/wireframe via surface normal divergence
    color3 shade_wireframe(const ray& r) {
        HitRecord rec;
        if (!scene_ptr->hit(r, 0.001, INF, rec)) {
            return color3(0.05,0.07,0.12);
        }
        const double eps = 0.004;
        auto dx = [&](vec3 offset) {
            ray r2(r.orig, unit_vector(r.dir + offset));
            HitRecord rec2;
            if (scene_ptr->hit(r2, 0.001, INF, rec2)) return rec2.normal;
            return rec.normal;
        };
        vec3 nx = dx(cam_ptr->right * eps);
        vec3 ny = dx(cam_ptr->up    * eps);
        double edge = (rec.normal - nx).length() + (rec.normal - ny).length();
        double e = clamp01(edge * 18.0);
        // Edge = bright accent, face = dark with light tint
        color3 edge_color = color3(0.25, 0.85, 1.0);
        color3 face_color = color3(0.05, 0.07, 0.10);
        return (1.0-e)*face_color + e*edge_color;
    }

    // Dispatch per mode
    color3 shade(const ray& r, int depth, XorShift64&) {
        switch(render_mode) {
            case RMODE_NORMALS:   return shade_normals(r);
            case RMODE_ALBEDO:    return shade_albedo(r);
            case RMODE_DEPTH:     return shade_depth(r);
            case RMODE_SHADOW:    return shade_shadow(r);
            case RMODE_AO:        return shade_ao(r);
            case RMODE_FRESNEL:   return shade_fresnel(r);
            case RMODE_WIREFRAME: return shade_wireframe(r);
            default:              return shade_pathTrace(r, depth);
        }
    }

    // ── Multithreaded render ────────────────────────────────────────────
    static unsigned __stdcall renderThreadProc(void* param) {
        Tracer* tracer = (Tracer*)param;
        unsigned int thread_seed = 123456789U + (unsigned)GetCurrentThreadId() * 0x9E3779B9U;
        XorShift64 rng((uint64_t)thread_seed);
        while (true) {
            LONG row = InterlockedIncrement(&tracer->scanline_row) - 1;
            if (row >= tracer->height) break;
            for (int i = 0; i < tracer->width; i++) {
                double u = (2.0*((double)i + rng.random01()-0.5)/(double)tracer->width - 1.0);
                double v = -(2.0*((double)(tracer->height-1-row)+rng.random01()-0.5)/(double)tracer->height - 1.0);
                ray r = tracer->cam_ptr->get_ray(u, v);
                color3 col = tracer->shade(r, tracer->max_bounces, rng);
                int idx = (row * tracer->width + i) * 3;
                tracer->accumulation[idx+0] += (float)col.x();
                tracer->accumulation[idx+1] += (float)col.y();
                tracer->accumulation[idx+2] += (float)col.z();
            }
        }
        return 0;
    }

    void renderFrame(const Scene& scene, const Camera& cam) {
        if (!accumulation) return;
        scanline_row = 0;
        scene_ptr = &scene;
        cam_ptr   = &cam;
        HANDLE* threads = new HANDLE[num_threads];
        for (int t=0; t<num_threads; t++)
            threads[t] = (HANDLE)_beginthreadex(NULL, 0, renderThreadProc, this, 0, NULL);
        WaitForMultipleObjects(num_threads, threads, TRUE, INFINITE);
        for (int t=0; t<num_threads; t++) CloseHandle(threads[t]);
        delete[] threads;
        samples_per_pixel++;
    }

    void getPixels(unsigned char* pixels) const {
        if (!accumulation) return;
        float scale = 1.0f / (float)(samples_per_pixel > 0 ? samples_per_pixel : 1);
        bool is_single_sample_mode = (render_mode != RMODE_PATH_TRACE);
        for (int i = 0; i < width*height; i++) {
            color3 col(
                accumulation[i*3+0] * scale * exposure,
                accumulation[i*3+1] * scale * exposure,
                accumulation[i*3+2] * scale * exposure
            );
            if (!is_single_sample_mode) {
                col = color3(clamp01(col.x()*0.95), clamp01(col.y()*0.95), clamp01(col.z()*0.95));
                col = tonemap_aces(col);
            }
            double inv_gamma = 1.0/2.2;
            pixels[i*3+0] = (unsigned char)(pow(clamp01(col.x()), inv_gamma)*255.99);
            pixels[i*3+1] = (unsigned char)(pow(clamp01(col.y()), inv_gamma)*255.99);
            pixels[i*3+2] = (unsigned char)(pow(clamp01(col.z()), inv_gamma)*255.99);
        }
    }
};

#endif
