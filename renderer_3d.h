#ifndef RENDERER_3D_H
#define RENDERER_3D_H

#include "physics_engine.h"
#include "neural_net.h"
#include "vec_math.h"

#define RENDER_MODE_PARTICLES 0
#define RENDER_MODE_STARS 1
#define RENDER_MODE_DENSITY 2
#define RENDER_MODE_TEMPERATURE 3
#define RENDER_MODE_DARK_MATTER 4
#define RENDER_MODE_PHOTONS 5
#define RENDER_MODE_FLOW 6

#define MAX_TRAIL_LENGTH 32

typedef struct {
    int width, height;
    unsigned int* framebuffer;
    float* zbuffer;
    int* flags;
    mat4 projection;
    mat4 view;
    mat4 vp;
    vec3 camera_pos;
    vec3 camera_target;
    double fov;
    double near_plane, far_plane;
    double orbit_radius;
    double orbit_theta;
    double orbit_phi;
    int view_mode;
    int show_grid;
    double auto_rotate;
    double time_seconds;
    int frame_count;

    FlowField* flow;
    NeuralNetwork* nn;
    double* trail_x[MAX_TRAIL_LENGTH];
    double* trail_y[MAX_TRAIL_LENGTH];
    double* trail_z[MAX_TRAIL_LENGTH];
    int trail_count;
    int trail_head;
    int* trail_active;

    vec3* nebula_grid;
    double* nebula_density;
    int nebula_resolution;
} Renderer3D;

void renderer_init(Renderer3D* r, int w, int h);
void renderer_destroy(Renderer3D* r);
void renderer_clear(Renderer3D* r);
void renderer_set_camera_orbit(Renderer3D* r, double theta, double phi, double radius);
void renderer_set_camera_target(Renderer3D* r, vec3 target);
void renderer_set_view_mode(Renderer3D* r, int mode);
void renderer_set_nn(Renderer3D* r, NeuralNetwork* nn);
void renderer_resize(Renderer3D* r, int w, int h);
void renderer_update(Renderer3D* r, SimulationState* sim, double dt);
void renderer_render(Renderer3D* r, SimulationState* sim);

static inline int renderer_get_pixel(Renderer3D* r, int x, int y) {
    if (x < 0 || x >= r->width || y < 0 || y >= r->height) return 0;
    return r->framebuffer[y * r->width + x];
}

#endif
