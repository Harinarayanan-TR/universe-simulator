#include "renderer_3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static unsigned int make_color(int r, int g, int b, int a) {
    return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

static unsigned int blend_over(unsigned int dst, unsigned int src) {
    int sa = (src >> 24) & 0xFF;
    if (sa == 255) return src;
    if (sa == 0) return dst;
    int sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
    int da = (dst >> 24) & 0xFF, dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    int a = sa + (da * (255 - sa)) / 255;
    int r = (sr * sa + dr * da * (255 - sa) / 255) / a;
    int g = (sg * sa + dg * da * (255 - sa) / 255) / a;
    int b = (sb * sa + db * da * (255 - sa) / 255) / a;
    return make_color(r, g, b, a);
}

static void draw_pixel(Renderer3D* r, int x, int y, float depth, unsigned int color) {
    if (x < 0 || x >= r->width || y < 0 || y >= r->height) return;
    int idx = y * r->width + x;
    if (depth < r->zbuffer[idx]) {
        r->framebuffer[idx] = blend_over(r->framebuffer[idx], color);
        r->zbuffer[idx] = depth;
    }
}

static void draw_circle(Renderer3D* r, int cx, int cy, float depth, int radius, unsigned int color, int glow) {
    int rsq = radius * radius;
    int start_x = CLAMP(cx - radius - glow, 0, r->width - 1);
    int end_x = CLAMP(cx + radius + glow + 1, 0, r->width);
    int start_y = CLAMP(cy - radius - glow, 0, r->height - 1);
    int end_y = CLAMP(cy + radius + glow + 1, 0, r->height);
    int sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            int dx = x - cx, dy = y - cy;
            int dist2 = dx * dx + dy * dy;
            if (glow > 0 && dist2 <= (radius + glow) * (radius + glow)) {
                float dist = sqrtf(dist2);
                float alpha = 1.0f - CLAMP((dist - radius) / glow, 0.0f, 1.0f);
                alpha *= alpha * 0.4f;
                int a = (int)(alpha * 255);
                if (a > 0) {
                    unsigned int c = make_color(sr, sg, sb, a);
                    draw_pixel(r, x, y, depth, c);
                }
            }
            if (dist2 <= rsq) {
                float dist = sqrtf(dist2);
                float edge = 1.0f - CLAMP(dist / radius, 0.0f, 1.0f);
                int a = (int)(edge * 255);
                unsigned int c = make_color(
                    CLAMP(sr * edge + 50 * (1 - edge), 0, 255),
                    CLAMP(sg * edge + 50 * (1 - edge), 0, 255),
                    CLAMP(sb * edge + 50 * (1 - edge), 0, 255), a);
                draw_pixel(r, x, y, depth, c);
            }
        }
    }
}

static void draw_halo(Renderer3D* r, int cx, int cy, float depth, int radius, unsigned int color) {
    int sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    int start_x = CLAMP(cx - radius, 0, r->width - 1);
    int end_x = CLAMP(cx + radius + 1, 0, r->width);
    int start_y = CLAMP(cy - radius, 0, r->height - 1);
    int end_y = CLAMP(cy + radius + 1, 0, r->height);
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            int dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist > radius) continue;
            float alpha = powf(1.0f - dist / radius, 3.0f) * 0.5f;
            int a = (int)(alpha * 255);
            if (a > 0) {
                unsigned int c = make_color(sr, sg, sb, a);
                int idx = y * r->width + x;
                if (depth < r->zbuffer[idx]) {
                    r->framebuffer[idx] = blend_over(r->framebuffer[idx], c);
                }
            }
        }
    }
}

static void draw_line_2d(Renderer3D* r, int x0, int y0, int x1, int y1, float depth, unsigned int color) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;
    while (1) {
        draw_pixel(r, x, y, depth, color);
        if (x == x1 && y == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

static unsigned int spectral_class_color(double temp) {
    if (temp > 30000)  return make_color(155, 176, 255, 255);
    if (temp > 10000)  return make_color(170, 191, 255, 255);
    if (temp > 7500)   return make_color(202, 215, 255, 255);
    if (temp > 6000)   return make_color(248, 247, 255, 255);
    if (temp > 5200)   return make_color(255, 244, 234, 255);
    if (temp > 3700)   return make_color(255, 210, 161, 255);
    if (temp > 2400)   return make_color(255, 160, 100, 255);
    return make_color(255, 100, 60, 255);
}

static unsigned int spectral_glow_color(double temp) {
    if (temp > 30000)  return make_color(100, 120, 200, 180);
    if (temp > 10000)  return make_color(120, 140, 200, 180);
    if (temp > 7500)   return make_color(150, 170, 200, 180);
    if (temp > 6000)   return make_color(200, 200, 220, 180);
    if (temp > 5200)   return make_color(220, 210, 200, 180);
    if (temp > 3700)   return make_color(220, 180, 150, 180);
    if (temp > 2400)   return make_color(200, 130, 90, 180);
    return make_color(180, 80, 50, 180);
}

static void render_star( Renderer3D* r, int sx, int sy, float depth, double mass, double temp, double luminosity) {
    double size = fmax(2.0, log10(mass + 1) * 3 + 3);
    unsigned int color = spectral_class_color(temp);
    unsigned int glow_col = spectral_glow_color(temp);

    if (luminosity > 1e20) {
        int halo_r = (int)(size * 12);
        draw_halo(r, sx, sy, depth, halo_r, glow_col);

        int outer_glow = (int)(size * 4);
        draw_circle(r, sx, sy, depth, outer_glow, color, (int)size);
    }

    draw_circle(r, sx, sy, depth, (int)size, color, (int)(size * 2));
}

static void render_accretion_disk(Renderer3D* r, int cx, int cy, float depth, double mass, vec3 view_dir) {
    (void)view_dir;
    double base_radius = log10(mass) * 2;
    double inner_r = fmax(3, base_radius * 0.3);
    double outer_r = fmax(10, base_radius * 2);

    double tilt = 0.4;
    double cos_tilt = cos(tilt);

    for (int r2 = (int)inner_r; r2 <= (int)outer_r; r2++) {
        double radius_ratio = (r2 - inner_r) / (outer_r - inner_r);
        int segments = (int)(PI * r2 * 2);
        if (segments < 8) segments = 8;
        for (int i = 0; i < segments; i++) {
            double angle = 2.0 * PI * i / segments;
            double dx = r2 * cos(angle);
            double dy = r2 * sin(angle) * cos_tilt;
            int px = cx + (int)dx;
            int py = cy + (int)dy;
            if (px < 0 || px >= r->width || py < 0 || py >= r->height) continue;

            double orb_speed = 1.0 / sqrt(inner_r + r2 * 0.1);
            double doppler = 1.0 + sin(angle) * orb_speed * 0.2;

            int r_col = (int)(180 * doppler * (1 - radius_ratio));
            int g_col = (int)(120 * doppler * (1 - radius_ratio * 0.5));
            int b_col = (int)(80 * (1 - radius_ratio * 0.3));

            int alpha = (int)(60 * (1 - radius_ratio) + 20);
            unsigned int col = make_color(CLAMP(r_col, 20, 255), CLAMP(g_col, 10, 200), CLAMP(b_col, 5, 150), CLAMP(alpha, 10, 120));
            draw_pixel(r, px, py, depth, col);
        }
    }
    unsigned int shadow = make_color(0, 0, 0, 220);
    draw_circle(r, cx, cy, depth, (int)inner_r, shadow, 0);
}

static void render_gravitational_lensing(Renderer3D* r, int cx, int cy, float depth, double mass) {
    double strength = log10(mass) * 0.5;
    int ring_r = (int)(fmax(4, strength * 3));
    int einstein_r = ring_r * 2;

    for (int y = -einstein_r; y <= einstein_r; y++) {
        for (int x = -einstein_r; x <= einstein_r; x++) {
            int px = cx + x, py = cy + y;
            if (px < 0 || px >= r->width || py < 0 || py >= r->height) continue;
            float dist = sqrtf(x * x + y * y);
            if (dist < 2 || dist > einstein_r) continue;

            float bend = strength / (dist + 1);
            int src_x = cx + (int)(x * (1 + bend * 3));
            int src_y = cy + (int)(y * (1 + bend * 3));
            if (src_x < 0 || src_x >= r->width || src_y < 0 || src_y >= r->height) continue;

            unsigned int src_pixel = r->framebuffer[src_y * r->width + src_x];
            int sr = (src_pixel >> 16) & 0xFF, sg = (src_pixel >> 8) & 0xFF, sb = src_pixel & 0xFF;
            float ring_factor = fabs(dist - ring_r) / ring_r;
            int alpha = (int)((1.0 - ring_factor) * 60);
            if (alpha > 5) {
                unsigned int lensed = make_color(
                    CLAMP(sr + alpha, 0, 255),
                    CLAMP(sg + alpha, 0, 255),
                    CLAMP(sb + alpha, 0, 255), 255);
                r->framebuffer[py * r->width + px] = blend_over(r->framebuffer[py * r->width + px], lensed);
            }
        }
    }
}

static void render_particle_trail(Renderer3D* r, int sx, int sy, float depth,
                                    double* trail_x, double* trail_y, double* trail_z,
                                    int trail_len, unsigned int color) {
    int sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    for (int t = 0; t < trail_len - 1; t++) {
        if (trail_x[t] == 0 && trail_y[t] == 0) continue;
        if (trail_x[t+1] == 0 && trail_y[t+1] == 0) continue;
        double tsx, tsy, td;
        if (!project_to_screen(vec3_new(trail_x[t], trail_y[t], trail_z[t]), r->vp,
                               r->width, r->height, &tsx, &tsy, &td)) continue;
        double tsx2, tsy2, td2;
        if (!project_to_screen(vec3_new(trail_x[t+1], trail_y[t+1], trail_z[t+1]), r->vp,
                               r->width, r->height, &tsx2, &tsy2, &td2)) continue;
        int alpha = (int)((200.0 / trail_len) * (t + 1));
        unsigned int trail_col = make_color(sr, sg, sb, CLAMP(alpha, 5, 80));
        draw_line_2d(r, (int)tsx, (int)tsy, (int)tsx2, (int)tsy2, depth, trail_col);
    }
}

static void quantize_color(double temp, unsigned int* color) {
    int r, g, b;
         if (temp < 1e3)  { r = 150; g = 0;   b = 50; }
    else if (temp < 3e3)  { r = 200; g = 50;  b = 0; }
    else if (temp < 5e3)  { r = 255; g = 150; b = 50; }
    else if (temp < 8e3)  { r = 255; g = 220; b = 100; }
    else if (temp < 1e4)  { r = 255; g = 255; b = 150; }
    else if (temp < 3e4)  { r = 200; g = 200; b = 255; }
    else if (temp < 1e5)  { r = 150; g = 150; b = 255; }
    else if (temp < 1e6)  { r = 100; g = 100; b = 255; }
    else if (temp < 1e7)  { r = 50;  g = 50;  b = 255; }
    else                  { r = 200; g = 200; b = 255; }
    *color = make_color(CLAMP(r,0,255), CLAMP(g,0,255), CLAMP(b,0,255), 255);
}

static void render_nebula(Renderer3D* r, SimulationState* sim) {
    if (!r->flow) return;

    double min_x = 1e30, max_x = -1e30, min_y = 1e30, max_y = -1e30, min_z = 1e30, max_z = -1e30;
    for (int i = 0; i < sim->particle_count; i++) {
        if (!sim->particles[i].active) continue;
        if (sim->particles[i].x < min_x) min_x = sim->particles[i].x;
        if (sim->particles[i].x > max_x) max_x = sim->particles[i].x;
        if (sim->particles[i].y < min_y) min_y = sim->particles[i].y;
        if (sim->particles[i].y > max_y) max_y = sim->particles[i].y;
        if (sim->particles[i].z < min_z) min_z = sim->particles[i].z;
        if (sim->particles[i].z > max_z) max_z = sim->particles[i].z;
    }
    double range_x = max_x - min_x; if (range_x < 1) range_x = 1;
    double range_y = max_y - min_y; if (range_y < 1) range_y = 1;
    double range_z = max_z - min_z; if (range_z < 1) range_z = 1;

    int res = r->nebula_resolution;
    for (int iz = 1; iz < res - 1; iz++) {
        for (int iy = 1; iy < res - 1; iy++) {
            for (int ix = 1; ix < res - 1; ix++) {
                int idx = (iz * res + iy) * res + ix;
                if (r->nebula_density[idx] < 1e-30) continue;

                double wx = min_x + (double)ix / (res - 1) * range_x;
                double wy = min_y + (double)iy / (res - 1) * range_y;
                double wz = min_z + (double)iz / (res - 1) * range_z;

                double sx, sy, depth;
                if (!project_to_screen(vec3_new(wx, wy, wz), r->vp,
                                       r->width, r->height, &sx, &sy, &depth)) continue;

                double density = r->nebula_density[idx];
                int alpha = (int)CLAMP(density * 100, 5, 60);
                unsigned int color = make_color(120, 80, 180, alpha);
                draw_pixel(r, (int)sx, (int)sy, depth, color);
            }
        }
    }
}

static void render_flow_field(Renderer3D* r, SimulationState* sim) {
    if (!r->flow || r->view_mode != RENDER_MODE_FLOW) return;
    int gw = r->flow->grid_width;
    int gh = r->flow->grid_height;

    double min_x = 1e30, max_x = -1e30, min_y = 1e30, max_y = -1e30;
    for (int i = 0; i < sim->particle_count; i++) {
        if (!sim->particles[i].active) continue;
        if (sim->particles[i].x < min_x) min_x = sim->particles[i].x;
        if (sim->particles[i].x > max_x) max_x = sim->particles[i].x;
        if (sim->particles[i].y < min_y) min_y = sim->particles[i].y;
        if (sim->particles[i].y > max_y) max_y = sim->particles[i].y;
    }
    double range_x = max_x - min_x; if (range_x < 1) range_x = 1;
    double range_y = max_y - min_y; if (range_y < 1) range_y = 1;

    for (int gy = 0; gy < gh; gy += 2) {
        for (int gx = 0; gx < gw; gx += 2) {
            int idx = gy * gw + gx;
            if (r->flow->density_map[idx] < 1e-20) continue;

            double wx = min_x + (double)gx / (gw - 1) * range_x;
            double wy = min_y + (double)gy / (gh - 1) * range_y;
            double wz = 0;

            double sx, sy, depth;
            if (!project_to_screen(vec3_new(wx, wy, wz), r->vp,
                                   r->width, r->height, &sx, &sy, &depth)) continue;

            double vx2 = wx + r->flow->velocity_field_x[idx] * 0.1;
            double vy2 = wy + r->flow->velocity_field_y[idx] * 0.1;
            double sx2, sy2, depth2;
            if (!project_to_screen(vec3_new(vx2, vy2, wz), r->vp,
                                   r->width, r->height, &sx2, &sy2, &depth2)) continue;

            double speed = sqrt(r->flow->velocity_field_x[idx] * r->flow->velocity_field_x[idx] +
                                r->flow->velocity_field_y[idx] * r->flow->velocity_field_y[idx]);
            int alpha = (int)CLAMP(speed * 100, 10, 80);
            unsigned int col = make_color(100, 180, 255, alpha);
            draw_line_2d(r, (int)sx, (int)sy, (int)sx2, (int)sy2, depth, col);
        }
    }
}

void renderer_set_nn(Renderer3D* r, NeuralNetwork* nn) {
    r->nn = nn;
}

void renderer_init(Renderer3D* r, int w, int h) {
    r->width = w;
    r->height = h;
    r->framebuffer = (unsigned int*)calloc(w * h, sizeof(unsigned int));
    r->zbuffer = (float*)malloc(w * h * sizeof(float));
    r->flags = (int*)calloc(w * h, sizeof(int));
    for (int i = 0; i < w * h; i++) r->zbuffer[i] = 1e30f;
    r->fov = 60.0 * M_PI / 180.0;
    r->near_plane = 1.0;
    r->far_plane = 1e30;
    r->orbit_radius = 1e20;
    r->orbit_theta = 0.0;
    r->orbit_phi = M_PI * 0.3;
    r->view_mode = 0;
    r->show_grid = 0;
    r->auto_rotate = 0.1;
    r->time_seconds = 0;
    r->frame_count = 0;
    r->nn = NULL;
    r->flow = NULL;
    r->trail_count = 0;
    r->trail_head = 0;
    r->nebula_resolution = 16;
    r->nebula_grid = NULL;
    r->nebula_density = NULL;

    memset(r->trail_x, 0, sizeof(r->trail_x));
    memset(r->trail_y, 0, sizeof(r->trail_y));
    memset(r->trail_z, 0, sizeof(r->trail_z));

    r->camera_pos = vec3_new(0, 0, r->orbit_radius);
    r->camera_target = vec3_new(0, 0, 0);
    renderer_set_camera_orbit(r, r->orbit_theta, r->orbit_phi, r->orbit_radius);
}

void renderer_destroy(Renderer3D* r) {
    free(r->framebuffer);
    free(r->zbuffer);
    free(r->flags);
    if (r->flow) { flow_destroy(r->flow); r->flow = NULL; }
    if (r->trail_active) free(r->trail_active);
    for (int i = 0; i < MAX_TRAIL_LENGTH; i++) {
        free(r->trail_x[i]); r->trail_x[i] = NULL;
        free(r->trail_y[i]); r->trail_y[i] = NULL;
        free(r->trail_z[i]); r->trail_z[i] = NULL;
    }
    free(r->nebula_grid);
    free(r->nebula_density);
    r->framebuffer = NULL;
    r->zbuffer = NULL;
    r->flags = NULL;
}

void renderer_clear(Renderer3D* r) {
    memset(r->framebuffer, 0, r->width * r->height * sizeof(unsigned int));
    for (int i = 0; i < r->width * r->height; i++) r->zbuffer[i] = 1e30f;
}

void renderer_set_camera_orbit(Renderer3D* r, double theta, double phi, double radius) {
    r->orbit_theta = theta;
    r->orbit_phi = phi;
    r->orbit_radius = fmax(radius, 1.0);
    r->camera_pos.x = r->camera_target.x + r->orbit_radius * sin(r->orbit_phi) * cos(r->orbit_theta);
    r->camera_pos.y = r->camera_target.y + r->orbit_radius * cos(r->orbit_phi);
    r->camera_pos.z = r->camera_target.z + r->orbit_radius * sin(r->orbit_phi) * sin(r->orbit_theta);
}

void renderer_set_camera_target(Renderer3D* r, vec3 target) {
    r->camera_target = target;
}

void renderer_set_view_mode(Renderer3D* r, int mode) {
    r->view_mode = mode;
}

void renderer_resize(Renderer3D* r, int w, int h) {
    free(r->framebuffer);
    free(r->zbuffer);
    free(r->flags);
    r->width = w;
    r->height = h;
    r->framebuffer = (unsigned int*)calloc(w * h, sizeof(unsigned int));
    r->zbuffer = (float*)malloc(w * h * sizeof(float));
    r->flags = (int*)calloc(w * h, sizeof(int));
    for (int i = 0; i < w * h; i++) r->zbuffer[i] = 1e30f;
}

void renderer_update(Renderer3D* r, SimulationState* sim, double dt) {
    r->time_seconds += dt;
    r->frame_count++;
    r->auto_rotate += dt * 0.01;

    double age = sim->universe.age;
    double log_age = age > 0 ? log10(age + 1) : 0;
    double auto_radius = 1e18 * pow(10, log_age * 0.3);
    auto_radius = CLAMP(auto_radius, 1e10, 1e25);
    r->camera_target = vec3_new(0, 0, 0);
    renderer_set_camera_orbit(r, r->orbit_theta + dt * 0.02, r->orbit_phi, auto_radius);

    if (!r->flow) {
        r->flow = flow_create(sim->particle_count, 64, 64);
    }
    if (r->flow) {
        flow_compute(r->flow, sim->particles, sim->particle_count);
        flow_smooth(r->flow, 2);
    }

    int n = sim->particle_count;
    if (r->trail_count != n) {
        for (int i = 0; i < MAX_TRAIL_LENGTH; i++) {
            free(r->trail_x[i]); r->trail_x[i] = (double*)calloc(n, sizeof(double));
            free(r->trail_y[i]); r->trail_y[i] = (double*)calloc(n, sizeof(double));
            free(r->trail_z[i]); r->trail_z[i] = (double*)calloc(n, sizeof(double));
        }
        free(r->trail_active);
        r->trail_active = (int*)calloc(n, sizeof(int));
        r->trail_count = n;
        r->trail_head = 0;
    }

    if (n > 0) {
        r->trail_head = (r->trail_head + 1) % MAX_TRAIL_LENGTH;
        double* tx = r->trail_x[r->trail_head];
        double* ty = r->trail_y[r->trail_head];
        double* tz = r->trail_z[r->trail_head];
        for (int i = 0; i < n; i++) {
            tx[i] = sim->particles[i].x;
            ty[i] = sim->particles[i].y;
            tz[i] = sim->particles[i].z;
            r->trail_active[i] = sim->particles[i].active;
        }
    }

    if (!r->nebula_density) {
        int res = r->nebula_resolution;
        r->nebula_density = (double*)calloc(res * res * res, sizeof(double));
        r->nebula_grid = (vec3*)calloc(res * res * res, sizeof(vec3));
    }
    if (r->nebula_density && n > 0) {
        int res = r->nebula_resolution;
        memset(r->nebula_density, 0, res * res * res * sizeof(double));

        double min_x = 1e30, max_x = -1e30, min_y = 1e30, max_y = -1e30, min_z = 1e30, max_z = -1e30;
        for (int i = 0; i < n; i++) {
            if (!sim->particles[i].active) continue;
            if (sim->particles[i].x < min_x) min_x = sim->particles[i].x;
            if (sim->particles[i].x > max_x) max_x = sim->particles[i].x;
            if (sim->particles[i].y < min_y) min_y = sim->particles[i].y;
            if (sim->particles[i].y > max_y) max_y = sim->particles[i].y;
            if (sim->particles[i].z < min_z) min_z = sim->particles[i].z;
            if (sim->particles[i].z > max_z) max_z = sim->particles[i].z;
        }
        double range_x = max_x - min_x; if (range_x < 1) range_x = 1;
        double range_y = max_y - min_y; if (range_y < 1) range_y = 1;
        double range_z = max_z - min_z; if (range_z < 1) range_z = 1;

        for (int i = 0; i < n; i++) {
            if (!sim->particles[i].active) continue;
            int ix = (int)((sim->particles[i].x - min_x) / range_x * (res - 1));
            int iy = (int)((sim->particles[i].y - min_y) / range_y * (res - 1));
            int iz = (int)((sim->particles[i].z - min_z) / range_z * (res - 1));
            ix = CLAMP(ix, 0, res - 1);
            iy = CLAMP(iy, 0, res - 1);
            iz = CLAMP(iz, 0, res - 1);
            int idx = (iz * res + iy) * res + ix;
            r->nebula_density[idx] += sim->particles[i].mass;
        }
        double max_d = 1e-30;
        for (int i = 0; i < res * res * res; i++) {
            if (r->nebula_density[i] > max_d) max_d = r->nebula_density[i];
        }
        if (max_d > 0) {
            for (int i = 0; i < res * res * res; i++) r->nebula_density[i] /= max_d;
        }
    }
}

static void render_particles(Renderer3D* r, SimulationState* sim) {
    mat4 proj = mat4_perspective(r->fov, (double)r->width / r->height, r->near_plane, r->far_plane);
    mat4 view = mat4_lookat(r->camera_pos, r->camera_target, vec3_new(0, 1, 0));
    r->vp = mat4_mul(proj, view);

    int n = sim->particle_count;
    vec3 view_dir = vec3_norm(vec3_sub(r->camera_target, r->camera_pos));

    for (int i = 0; i < n; i++) {
        Particle* p = &sim->particles[i];
        if (!p->active) continue;

        if (r->view_mode == RENDER_MODE_STARS) {
            if (p->type != PARTICLE_HYDROGEN && p->type != PARTICLE_HELIUM &&
                p->type != PARTICLE_CARBON && p->type != PARTICLE_OXYGEN &&
                p->type != PARTICLE_IRON && p->type != PARTICLE_BLACK_HOLE &&
                p->type != PARTICLE_NEUTRON_STAR) continue;
        }
        if (r->view_mode == RENDER_MODE_DARK_MATTER && p->type != PARTICLE_DARK_MATTER) continue;
        if (r->view_mode == RENDER_MODE_PHOTONS && p->type != PARTICLE_PHOTON) continue;

        double sx, sy, depth;
        if (!project_to_screen(vec3_new(p->x, p->y, p->z), r->vp,
                               r->width, r->height, &sx, &sy, &depth))
            continue;

        unsigned int color;
        double temp = p->temperature;

        switch (p->type) {
            case PARTICLE_HYDROGEN:
            case PARTICLE_HELIUM:
            case PARTICLE_CARBON:
            case PARTICLE_OXYGEN:
            case PARTICLE_IRON:
            case PARTICLE_DEUTERIUM:
            case PARTICLE_LITHIUM:
            case PARTICLE_NITROGEN: {
                if (r->nn && p->luminosity > 1e20) {
                    double input[7] = {p->x, p->y, p->z, p->vx, p->vy, p->vz, p->mass};
                    double nn_offset[3];
                    nn_forward(r->nn, input, nn_offset);
                    render_star(r, (int)sx, (int)sy, depth, p->mass, temp, p->luminosity);

                    if (r->trail_count > 0 && r->trail_x[0] != NULL) {
                        int head = r->trail_head;
                        int trail_start = (head + 1) % MAX_TRAIL_LENGTH;
                        int trail_len = 0;
                        double* trail_x_t = (double*)malloc(MAX_TRAIL_LENGTH * sizeof(double));
                        double* trail_y_t = (double*)malloc(MAX_TRAIL_LENGTH * sizeof(double));
                        double* trail_z_t = (double*)malloc(MAX_TRAIL_LENGTH * sizeof(double));
                        if (trail_x_t && trail_y_t && trail_z_t) {
                            for (int t = 0; t < MAX_TRAIL_LENGTH; t++) {
                                int idx = (trail_start + t) % MAX_TRAIL_LENGTH;
                                trail_x_t[t] = r->trail_x[idx][i];
                                trail_y_t[t] = r->trail_y[idx][i];
                                trail_z_t[t] = r->trail_z[idx][i];
                            }
                            trail_len = MAX_TRAIL_LENGTH;
                            unsigned int trail_color = spectral_glow_color(temp);
                            render_particle_trail(r, (int)sx, (int)sy, depth,
                                                  trail_x_t, trail_y_t, trail_z_t,
                                                  trail_len, trail_color);
                            free(trail_x_t); free(trail_y_t); free(trail_z_t);
                        }
                    }
                } else {
                    render_star(r, (int)sx, (int)sy, depth, p->mass, temp, p->luminosity);
                }
                break;
            }

            case PARTICLE_BLACK_HOLE: {
                render_gravitational_lensing(r, (int)sx, (int)sy, depth, p->mass);
                render_accretion_disk(r, (int)sx, (int)sy, depth, p->mass, view_dir);
                draw_circle(r, (int)sx, (int)sy, depth, 3, make_color(0, 0, 0, 255), 8);
                break;
            }

            case PARTICLE_NEUTRON_STAR:
                color = make_color(180, 180, 255, 255);
                draw_halo(r, (int)sx, (int)sy, depth, 15, make_color(80, 80, 255, 60));
                draw_circle(r, (int)sx, (int)sy, depth, 4, color, 6);
                break;

            case PARTICLE_PHOTON:
                color = make_color(255, 255, 200, 200);
                draw_pixel(r, (int)sx, (int)sy, depth, color);
                break;

            case PARTICLE_DARK_MATTER:
                color = make_color(80, 80, 120, 40);
                draw_circle(r, (int)sx, (int)sy, depth, 3, color, 2);
                break;

            case PARTICLE_NEBULA_GAS:
                quantize_color(temp, &color);
                color = (color & 0x00FFFFFF) | (60 << 24);
                draw_circle(r, (int)sx, (int)sy, depth, 4, color, 3);
                break;

            case PARTICLE_ELECTRON:
            case PARTICLE_PROTON:
            case PARTICLE_NEUTRON:
            case PARTICLE_QUARK_UP:
            case PARTICLE_QUARK_DOWN:
                quantize_color(temp, &color);
                draw_circle(r, (int)sx, (int)sy, depth, 2, color, 1);
                break;

            case PARTICLE_PLANETESIMAL:
                quantize_color(temp, &color);
                draw_circle(r, (int)sx, (int)sy, depth, 3, color, 1);
                break;

            default:
                color = make_color(150, 150, 150, 200);
                draw_circle(r, (int)sx, (int)sy, depth, 2, color, 0);
                break;
        }
    }
}

static void render_background(Renderer3D* r, SimulationState* sim) {
    unsigned int* fb = r->framebuffer;
    int w = r->width, h = r->height;

    double age_ratio = CLAMP(sim->universe.age / 1.38e10, 0.0, 1.0);
    int bg_r = (int)(5 * (1 - age_ratio));
    int bg_g = (int)(5 * (1 - age_ratio) + 10 * age_ratio);
    int bg_b = (int)(15 * (1 - age_ratio) + 5 * age_ratio);
    unsigned int bg = make_color(bg_r, bg_g, bg_b, 255);
    for (int i = 0; i < w * h; i++) fb[i] = bg;

    uint64_t seed = 42;
    for (int i = 0; i < 400; i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        int sx = (int)(seed % w);
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        int sy = (int)(seed % h);
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        int bright = 30 + (int)(seed % 70);
        fb[sy * w + sx] = make_color(bright, bright, bright, 255);
    }

    seed = 42;
    for (int i = 0; i < 50; i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        int sx = (int)(seed % w);
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        int sy = (int)(seed % h);
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        int bright = 120 + (int)(seed % 80);
        int radius = 1 + (int)(seed % 2);
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int px = sx + dx, py = sy + dy;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    fb[py * w + px] = make_color(bright, bright, bright, 255);
                }
            }
        }
    }

    int cx = w / 2, cy = h / 2;
    unsigned int grid_color = make_color(40, 40, 60, 80);
    float grid_depth = 0.5f;
    for (int i = -20; i <= 20; i++) {
        int gx = cx + i * 30;
        if (gx >= 0 && gx < w) {
            draw_pixel(r, gx, 0, grid_depth, grid_color);
            draw_pixel(r, gx, h - 1, grid_depth, grid_color);
        }
        int gy = cy + i * 30;
        if (gy >= 0 && gy < h) {
            draw_pixel(r, 0, gy, grid_depth, grid_color);
            draw_pixel(r, w - 1, gy, grid_depth, grid_color);
        }
    }
}

void renderer_render(Renderer3D* r, SimulationState* sim) {
    renderer_clear(r);
    render_background(r, sim);
    render_particles(r, sim);
    render_flow_field(r, sim);
    render_nebula(r, sim);
}
