#ifndef NEURAL_NET_H
#define NEURAL_NET_H

#include "physics_engine.h"
#include "vec_math.h"

typedef struct {
    int num_layers;
    int* layer_sizes;
    double** weights;
    double** biases;
    double* input_cache;
    double** layer_outputs;
    int max_input_size;
    int max_output_size;
} NeuralNetwork;

typedef struct {
    double* positions_prev;
    double* positions_curr;
    double* velocities;
    double* masses;
    int* types;
    int count;
    double time_delta;
} TrainingData;

NeuralNetwork* nn_create(int num_layers, int* layer_sizes);
void nn_destroy(NeuralNetwork* nn);
void nn_forward(NeuralNetwork* nn, double* input, double* output);
void nn_randomize(NeuralNetwork* nn, uint64_t seed);
int nn_save(NeuralNetwork* nn, const char* path);
NeuralNetwork* nn_load(const char* path);
void nn_train_particle_predictor(TrainingData* data, NeuralNetwork* nn, int epochs, double lr);

void nn_predict_trajectory(NeuralNetwork* nn,
    Particle* prev, Particle* curr, int count,
    double t, Particle* result);

typedef struct {
    int num_particles;
    int grid_width;
    int grid_height;
    double* density_map;
    double* velocity_field_x;
    double* velocity_field_y;
} FlowField;

FlowField* flow_create(int num_particles, int grid_w, int grid_h);
void flow_destroy(FlowField* ff);
void flow_compute(FlowField* ff, Particle* particles, int count);
void flow_smooth(FlowField* ff, int iterations);

#endif
