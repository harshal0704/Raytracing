#ifndef CAMERA_H
#define CAMERA_H

#include "core/math.h"
#include "core/ray.h"

struct Camera {
    point3 target;
    double distance, yaw, pitch, fov;
    int    width, height;
    double aperture, focus_dist;

    point3 position;
    vec3   forward, right, up;

    Camera()
        : target(0,0,0), distance(7.5),
          yaw(30.0), pitch(20.0), fov(55.0),
          width(640), height(480), aperture(0.0), focus_dist(7.5)
    { update(); }

    void update() {
        double yr = deg2rad(yaw), pr = deg2rad(pitch);
        double cp = std::cos(pr);
        forward = unit_vector(vec3(-cp*std::cos(yr), -std::sin(pr), -cp*std::sin(yr)));
        position = target - distance * forward;
        vec3 world_up(0,1,0);
        right = unit_vector(cross(forward, world_up));
        up    = cross(right, forward);
    }

    void setDOF(double ap, double fd) { aperture=ap; focus_dist=fd; }

    ray get_ray(double u, double v) const {
        double aspect = (double)width / (double)height;
        double half_h = std::tan(deg2rad(fov)*0.5);
        double half_w = aspect * half_h;
        vec3 focus_pt = position + forward*focus_dist + (u*half_w)*right + (v*half_h)*up;
        if (aperture > 0.0) {
            vec3 rd = random_in_unit_disk() * (aperture*0.5);
            vec3 lens_offset = rd.x()*right + rd.y()*up;
            vec3 origin = position + lens_offset;
            return ray(origin, unit_vector(focus_pt - origin));
        }
        vec3 rd = forward + (u*half_w)*right + (v*half_h)*up;
        return ray(position, unit_vector(rd));
    }

    void orbit(double dx, double dy) {
        yaw+=dx; pitch+=dy;
        if (pitch>89.0) pitch=89.0;
        if (pitch<-89.0) pitch=-89.0;
        update();
    }
    void zoom(double d) {
        distance -= d;
        if (distance<0.5) distance=0.5;
        if (distance>120.0) distance=120.0;
        update();
    }
    void pan(double dx, double dy) { target += dx*right + dy*up; update(); }

    vec3 screen_to_world_dir(int sx, int sy) const {
        double u = (2.0*(double)sx/(double)width-1.0);
        double v = -(2.0*(double)sy/(double)height-1.0);
        double aspect = (double)width/(double)height;
        double half_h = std::tan(deg2rad(fov)*0.5);
        double half_w = aspect*half_h;
        return unit_vector(forward + (u*half_w)*right + (v*half_h)*up);
    }
    ray screen_ray(int sx, int sy) const { return ray(position, screen_to_world_dir(sx,sy)); }

    double world_to_screen_x(const point3& wp) const {
        vec3 d = wp-position;
        double f = dot(d, forward); if (f<=0) return -9999;
        double aspect=(double)width/(double)height;
        double half_h=std::tan(deg2rad(fov)*0.5), half_w=aspect*half_h;
        return (dot(d,right)/(f*half_w)+1.0)*0.5*(double)width;
    }
    double world_to_screen_y(const point3& wp) const {
        vec3 d = wp-position;
        double f = dot(d, forward); if (f<=0) return -9999;
        double half_h=std::tan(deg2rad(fov)*0.5);
        return (1.0-(dot(d,up)/(f*half_h)+1.0)*0.5)*(double)height;
    }
};

#endif
