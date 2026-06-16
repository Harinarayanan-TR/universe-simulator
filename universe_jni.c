#include "physics_engine.h"
#include "renderer_3d.h"
#include "neural_net.h"
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SimulationState* g_state = NULL;
static Renderer3D* g_renderer = NULL;
static NeuralNetwork* g_nn = NULL;

static void ensure_nn() {
    if (!g_nn) {
        g_nn = nn_load("nn_weights.bin");
        if (!g_nn) {
            int layers[] = {7, 32, 3};
            g_nn = nn_create(3, layers);
            nn_randomize(g_nn, 42);
        }
    }
}

JNIEXPORT void JNICALL Java_UniverseSimulator_initSimulation(JNIEnv* env, jobject obj, jlong seed) {
    if (g_state) {
        sim_destroy(g_state);
        free(g_state);
    }
    g_state = (SimulationState*)malloc(sizeof(SimulationState));
    if (g_state) {
        memset(g_state, 0, sizeof(SimulationState));
        sim_init(g_state, (uint64_t)seed);
    }
    ensure_nn();
    if (g_renderer) renderer_set_nn(g_renderer, g_nn);
}

JNIEXPORT void JNICALL Java_UniverseSimulator_stepSimulation(JNIEnv* env, jobject obj, jdouble dtYears) {
    if (g_state) sim_step(g_state, dtYears);
}

JNIEXPORT void JNICALL Java_UniverseSimulator_resetSimulation(JNIEnv* env, jobject obj, jlong seed) {
    if (g_state) {
        sim_destroy(g_state);
        sim_init(g_state, (uint64_t)seed);
    }
}

JNIEXPORT void JNICALL Java_UniverseSimulator_setTimeScale(JNIEnv* env, jobject obj, jdouble multiplier) {
    if (g_state) sim_set_time_scale(g_state, multiplier);
}

JNIEXPORT void JNICALL Java_UniverseSimulator_setPaused(JNIEnv* env, jobject obj, jboolean paused) {
    if (g_state) sim_set_paused(g_state, paused ? 1 : 0);
}

JNIEXPORT jdouble JNICALL Java_UniverseSimulator_getUniverseAge(JNIEnv* env, jobject obj) {
    return g_state ? g_state->universe.age : 0.0;
}

JNIEXPORT jint JNICALL Java_UniverseSimulator_getParticleCount(JNIEnv* env, jobject obj) {
    return g_state ? (jint)g_state->particle_count : 0;
}

JNIEXPORT jint JNICALL Java_UniverseSimulator_getStarCount(JNIEnv* env, jobject obj) {
    return g_state ? (jint)sim_get_star_count(g_state) : 0;
}

JNIEXPORT jint JNICALL Java_UniverseSimulator_getGalaxyCount(JNIEnv* env, jobject obj) {
    return g_state ? (jint)sim_get_galaxy_count(g_state) : 0;
}

JNIEXPORT jdouble JNICALL Java_UniverseSimulator_getScaleFactor(JNIEnv* env, jobject obj) {
    return g_state ? g_state->universe.scale_factor : 0.0;
}

JNIEXPORT jdouble JNICALL Java_UniverseSimulator_getHubbleConstant(JNIEnv* env, jobject obj) {
    return g_state ? g_state->universe.hubble_constant : 0.0;
}

JNIEXPORT jint JNICALL Java_UniverseSimulator_getEpoch(JNIEnv* env, jobject obj) {
    return g_state ? (jint)g_state->epoch : 0;
}

JNIEXPORT jdoubleArray JNICALL Java_UniverseSimulator_getParticleData(JNIEnv* env, jobject obj) {
    if (!g_state || g_state->particle_count == 0) return NULL;
    int n = g_state->particle_count;
    int active = 0;
    for (int i = 0; i < n; i++) if (g_state->particles[i].active) active++;
    jdoubleArray result = (*env)->NewDoubleArray(env, active * 7);
    if (!result) return NULL;
    jdouble* buf = (*env)->GetDoubleArrayElements(env, result, NULL);
    if (!buf) return NULL;
    int idx = 0;
    for (int i = 0; i < n; i++) {
        Particle* p = &g_state->particles[i];
        if (!p->active) continue;
        buf[idx++] = p->x;
        buf[idx++] = p->y;
        buf[idx++] = p->z;
        buf[idx++] = p->mass;
        buf[idx++] = (jdouble)p->type;
        buf[idx++] = p->temperature;
        buf[idx++] = p->radius;
    }
    (*env)->ReleaseDoubleArrayElements(env, result, buf, 0);
    return result;
}

JNIEXPORT jdoubleArray JNICALL Java_UniverseSimulator_getUniverseParams(JNIEnv* env, jobject obj) {
    if (!g_state) return NULL;
    jdoubleArray result = (*env)->NewDoubleArray(env, 6);
    if (!result) return NULL;
    jdouble* buf = (*env)->GetDoubleArrayElements(env, result, NULL);
    if (!buf) return NULL;
    buf[0] = g_state->universe.age;
    buf[1] = g_state->universe.scale_factor;
    buf[2] = g_state->universe.hubble_constant;
    buf[3] = g_state->universe.temperature_cmb;
    buf[4] = g_state->universe.expansion_rate;
    buf[5] = g_state->epoch;
    (*env)->ReleaseDoubleArrayElements(env, result, buf, 0);
    return result;
}

JNIEXPORT void JNICALL Java_UniverseSimulator_initRenderer(JNIEnv* env, jobject obj, jint width, jint height) {
    if (g_renderer) {
        renderer_destroy(g_renderer);
        free(g_renderer);
    }
    g_renderer = (Renderer3D*)malloc(sizeof(Renderer3D));
    if (g_renderer) {
        memset(g_renderer, 0, sizeof(Renderer3D));
        renderer_init(g_renderer, (int)width, (int)height);
        ensure_nn();
        renderer_set_nn(g_renderer, g_nn);
    }
}

JNIEXPORT void JNICALL Java_UniverseSimulator_resizeRenderer(JNIEnv* env, jobject obj, jint width, jint height) {
    if (g_renderer) renderer_resize(g_renderer, (int)width, (int)height);
}

JNIEXPORT void JNICALL Java_UniverseSimulator_setCameraOrbit(JNIEnv* env, jobject obj, jdouble theta, jdouble phi, jdouble radius) {
    if (g_renderer) renderer_set_camera_orbit(g_renderer, theta, phi, radius);
}

JNIEXPORT void JNICALL Java_UniverseSimulator_setRenderMode(JNIEnv* env, jobject obj, jint mode) {
    if (g_renderer) renderer_set_view_mode(g_renderer, (int)mode);
}

JNIEXPORT void JNICALL Java_UniverseSimulator_renderFrame(JNIEnv* env, jobject obj, jintArray pixelBuffer, jdouble dt) {
    if (!g_state || !g_renderer) return;
    if (g_renderer) {
        renderer_update(g_renderer, g_state, dt);
        renderer_render(g_renderer, g_state);
    }
    jint* pixels = (*env)->GetIntArrayElements(env, pixelBuffer, NULL);
    if (!pixels) return;
    int len = (*env)->GetArrayLength(env, pixelBuffer);
    int fb_size = g_renderer->width * g_renderer->height;
    int copy_size = len < fb_size ? len : fb_size;
    memcpy(pixels, g_renderer->framebuffer, copy_size * sizeof(jint));
    (*env)->ReleaseIntArrayElements(env, pixelBuffer, pixels, 0);
}

JNIEXPORT jdoubleArray JNICALL Java_UniverseSimulator_getPhotonData(JNIEnv* env, jobject obj) {
    if (!g_state || g_state->particle_count == 0) return NULL;
    int n = g_state->particle_count;
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (g_state->particles[i].active && g_state->particles[i].type == PARTICLE_PHOTON) count++;
    }
    if (count == 0) return NULL;
    jdoubleArray result = (*env)->NewDoubleArray(env, count * 4);
    if (!result) return NULL;
    jdouble* buf = (*env)->GetDoubleArrayElements(env, result, NULL);
    if (!buf) return NULL;
    int idx = 0;
    for (int i = 0; i < n; i++) {
        Particle* p = &g_state->particles[i];
        if (!p->active || p->type != PARTICLE_PHOTON) continue;
        buf[idx++] = p->x;
        buf[idx++] = p->y;
        buf[idx++] = p->z;
        buf[idx++] = p->energy;
    }
    (*env)->ReleaseDoubleArrayElements(env, result, buf, 0);
    return result;
}

JNIEXPORT jdouble JNICALL Java_UniverseSimulator_nnPredictTrajectory(JNIEnv* env, jobject obj,
    jdoubleArray input, jdoubleArray output) {
    ensure_nn();
    if (!g_nn) return -1;
    jdouble* in = (*env)->GetDoubleArrayElements(env, input, NULL);
    jdouble* out = (*env)->GetDoubleArrayElements(env, output, NULL);
    if (!in || !out) {
        if (in) (*env)->ReleaseDoubleArrayElements(env, input, in, 0);
        if (out) (*env)->ReleaseDoubleArrayElements(env, output, out, 0);
        return -1;
    }
    nn_forward(g_nn, in, out);
    (*env)->ReleaseDoubleArrayElements(env, input, in, 0);
    (*env)->ReleaseDoubleArrayElements(env, output, out, 0);
    return 0;
}
