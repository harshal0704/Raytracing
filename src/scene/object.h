#ifndef OBJECT_H
#define OBJECT_H

#include "core/math.h"
#include "core/ray.h"
#include "scene/material.h"
#include <vector>
#include <cmath>

enum ObjType {
    OBJ_SPHERE,
    OBJ_BOX,
    OBJ_PLANE,
    OBJ_CYLINDER
};

struct Object {
    ObjType type;
    point3  center;
    vec3    size;
    vec3    normal;
    int     mat_index;
    bool    selected;

    Object() : type(OBJ_SPHERE), mat_index(0), selected(false) {}

    static Object makeSphere(const point3& c, double r, int mat) {
        Object o; o.type=OBJ_SPHERE; o.center=c; o.size=vec3(r,r,r); o.mat_index=mat; return o;
    }
    static Object makeBox(const point3& c, const vec3& s, int mat) {
        Object o; o.type=OBJ_BOX; o.center=c; o.size=s; o.mat_index=mat; return o;
    }
    static Object makePlane(const point3& c, const vec3& n, int mat) {
        Object o; o.type=OBJ_PLANE; o.center=c; o.normal=unit_vector(n); o.mat_index=mat; return o;
    }
    static Object makeCylinder(const point3& c, double radius, double height, int mat) {
        Object o; o.type=OBJ_CYLINDER; o.center=c; o.size=vec3(radius,height,0); o.mat_index=mat; return o;
    }
};

inline bool hit_sphere(const Object& obj, const ray& r, double t_min, double t_max, HitRecord& rec) {
    double radius = obj.size.x();
    vec3 oc = r.origin() - obj.center;
    double a = r.direction().length_squared();
    double half_b = dot(oc, r.direction());
    double c_val = oc.length_squared() - radius*radius;
    double disc = half_b*half_b - a*c_val;
    if (disc < 0) return false;
    double sqrtd = std::sqrt(disc);
    double root = (-half_b - sqrtd) / a;
    if (root < t_min || root > t_max) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || root > t_max) return false;
    }
    rec.t = root;
    rec.p = r.at(rec.t);
    vec3 outward_normal = (rec.p - obj.center) / radius;
    rec.set_face_normal(r, outward_normal);
    rec.mat_index = obj.mat_index;
    return true;
}

inline bool hit_box(const Object& obj, const ray& r, double t_min, double t_max, HitRecord& rec) {
    vec3 half = obj.size * 0.5;
    vec3 bmin = obj.center - half;
    vec3 bmax = obj.center + half;
    double tmin_val = t_min, tmax_val = t_max;
    vec3 inv_d(1.0/r.direction().x(), 1.0/r.direction().y(), 1.0/r.direction().z());
    for (int i = 0; i < 3; i++) {
        double t0 = (bmin[i] - r.origin()[i]) * inv_d[i];
        double t1 = (bmax[i] - r.origin()[i]) * inv_d[i];
        if (inv_d[i] < 0.0) { double tmp=t0; t0=t1; t1=tmp; }
        if (t0 > tmin_val) tmin_val = t0;
        if (t1 < tmax_val) tmax_val = t1;
        if (tmax_val <= tmin_val) return false;
    }
    rec.t = tmin_val;
    rec.p = r.at(rec.t);
    vec3 d = (rec.p - obj.center);
    double ax = std::fabs(d.x()), ay = std::fabs(d.y()), az = std::fabs(d.z());
    vec3 outward_normal(0,1,0);
    if (ax >= ay && ax >= az) outward_normal = vec3(d.x()>0?1.0:-1.0, 0, 0);
    else if (ay >= ax && ay >= az) outward_normal = vec3(0, d.y()>0?1.0:-1.0, 0);
    else outward_normal = vec3(0, 0, d.z()>0?1.0:-1.0);
    rec.set_face_normal(r, outward_normal);
    rec.mat_index = obj.mat_index;
    return true;
}

inline bool hit_plane(const Object& obj, const ray& r, double t_min, double t_max, HitRecord& rec) {
    double denom = dot(obj.normal, r.direction());
    if (std::fabs(denom) < EPSILON) return false;
    double t = dot(obj.center - r.origin(), obj.normal) / denom;
    if (t < t_min || t > t_max) return false;
    rec.t = t;
    rec.p = r.at(t);
    rec.set_face_normal(r, obj.normal);
    rec.mat_index = obj.mat_index;
    return true;
}

inline bool hit_cylinder(const Object& obj, const ray& r, double t_min, double t_max, HitRecord& rec) {
    double radius = obj.size.x();
    double half_h = obj.size.y() * 0.5;
    vec3 oc = r.origin() - obj.center;
    double a = r.direction().x()*r.direction().x() + r.direction().z()*r.direction().z();
    if (a < EPSILON) return false;
    double half_b = oc.x()*r.direction().x() + oc.z()*r.direction().z();
    double c_val = oc.x()*oc.x() + oc.z()*oc.z() - radius*radius;
    double disc = half_b*half_b - a*c_val;
    if (disc < 0) return false;
    double sqrtd = std::sqrt(disc);
    bool hit_any = false;
    double closest = t_max;
    double roots[2] = { (-half_b - sqrtd)/a, (-half_b + sqrtd)/a };
    for (int i = 0; i < 2 && !hit_any; i++) {
        double root = roots[i];
        if (root >= t_min && root <= t_max) {
            double y = r.at(root).y();
            if (y >= obj.center.y()-half_h && y <= obj.center.y()+half_h) {
                closest = root; hit_any = true;
            }
        }
    }
    if (!hit_any) return false;
    rec.t = closest;
    rec.p = r.at(closest);
    vec3 outward_normal((rec.p.x()-obj.center.x())/radius, 0, (rec.p.z()-obj.center.z())/radius);
    rec.set_face_normal(r, outward_normal);
    rec.mat_index = obj.mat_index;
    return true;
}

inline bool hit_object(const Object& obj, const ray& r, double t_min, double t_max, HitRecord& rec) {
    switch (obj.type) {
        case OBJ_SPHERE:   return hit_sphere(obj, r, t_min, t_max, rec);
        case OBJ_BOX:      return hit_box(obj, r, t_min, t_max, rec);
        case OBJ_PLANE:    return hit_plane(obj, r, t_min, t_max, rec);
        case OBJ_CYLINDER: return hit_cylinder(obj, r, t_min, t_max, rec);
    }
    return false;
}

#endif
