#ifndef MATERIAL_H
#define MATERIAL_H

#include "core/math.h"
#include "core/ray.h"

enum MatType {
    MAT_LAMBERTIAN,
    MAT_METAL,
    MAT_EMISSIVE,
    MAT_DIELECTRIC,
    MAT_CHECKER  // checkered procedural
};

struct Material {
    MatType type;
    color3  albedo;
    color3  albedo2;   // second color for checker
    double  fuzz;
    color3  emit_color;
    double  emit_strength;
    double  ir;
    double  checker_scale;

    Material()
        : type(MAT_LAMBERTIAN), albedo(0.8, 0.3, 0.3), albedo2(0.1, 0.1, 0.1),
          fuzz(0.0), emit_color(1,1,1), emit_strength(0.0), ir(1.5), checker_scale(2.0) {}

    static Material lambertian(const color3& a) {
        Material m; m.type = MAT_LAMBERTIAN; m.albedo = a; return m;
    }
    static Material metal(const color3& a, double f) {
        Material m; m.type = MAT_METAL; m.albedo = a; m.fuzz = f < 1.0 ? f : 1.0; return m;
    }
    static Material makeEmissive(const color3& e, double strength) {
        Material m; m.type = MAT_EMISSIVE; m.emit_color = e;
        m.emit_strength = strength; m.albedo = e; return m;
    }
    static Material dielectric(double index_of_refraction) {
        Material m; m.type = MAT_DIELECTRIC;
        m.albedo = color3(1.0, 1.0, 1.0); m.ir = index_of_refraction; return m;
    }
    static Material checker(const color3& a, const color3& b, double scale = 2.0) {
        Material m; m.type = MAT_CHECKER;
        m.albedo = a; m.albedo2 = b; m.checker_scale = scale; return m;
    }
};

struct HitRecord {
    point3 p;
    vec3   normal;
    double t;
    bool   front_face;
    int    mat_index;
    void set_face_normal(const ray& r, const vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

struct ScatterResult {
    bool   scattered;
    ray    scattered_ray;
    color3 attenuation;
};

inline color3 material_albedo(const Material& mat, const point3& p) {
    if (mat.type == MAT_CHECKER) {
        double s = mat.checker_scale;
        int ix = (int)std::floor(p.x() * s);
        int iy = (int)std::floor(p.y() * s);
        int iz = (int)std::floor(p.z() * s);
        bool even = ((ix + iy + iz) & 1) == 0;
        return even ? mat.albedo : mat.albedo2;
    }
    return mat.albedo;
}

inline bool material_scatter(const Material& mat, const ray& r_in,
                              const HitRecord& rec, ScatterResult& sr) {
    if (mat.type == MAT_LAMBERTIAN || mat.type == MAT_CHECKER) {
        vec3 scatter_dir = rec.normal + random_unit_vector();
        if (scatter_dir.near_zero()) scatter_dir = rec.normal;
        sr.scattered_ray = ray(rec.p, unit_vector(scatter_dir));
        sr.attenuation   = material_albedo(mat, rec.p);
        sr.scattered      = true;
        return true;
    }
    if (mat.type == MAT_METAL) {
        vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
        vec3 fuzz_dir  = mat.fuzz * random_in_unit_sphere();
        sr.scattered_ray = ray(rec.p, unit_vector(reflected + fuzz_dir));
        sr.attenuation   = mat.albedo;
        sr.scattered      = dot(sr.scattered_ray.direction(), rec.normal) > 0;
        return sr.scattered;
    }
    if (mat.type == MAT_DIELECTRIC) {
        double refraction_ratio = rec.front_face ? (1.0 / mat.ir) : mat.ir;
        vec3 unit_dir = unit_vector(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_dir, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        vec3 direction;
        if (cannot_refract || schlick(cos_theta, refraction_ratio) > random01())
            direction = reflect(unit_dir, rec.normal);
        else
            direction = refract(unit_dir, rec.normal, refraction_ratio);
        sr.scattered_ray = ray(rec.p, unit_vector(direction));
        sr.attenuation   = color3(1.0, 1.0, 1.0);
        sr.scattered      = true;
        return true;
    }
    sr.scattered = false;
    return false;
}

#endif
