#include "neural_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static double activation(double x) {
    return tanh(x);
}

static uint64_t rng_state = 12345;
static double rand_uniform() {
    rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(rng_state >> 11) / (double)(1ULL << 53);
}

NeuralNetwork* nn_create(int num_layers, int* layer_sizes) {
    NeuralNetwork* nn = (NeuralNetwork*)calloc(1, sizeof(NeuralNetwork));
    nn->num_layers = num_layers;
    nn->layer_sizes = (int*)malloc(num_layers * sizeof(int));
    memcpy(nn->layer_sizes, layer_sizes, num_layers * sizeof(int));

    nn->weights = (double**)malloc((num_layers - 1) * sizeof(double*));
    nn->biases = (double**)malloc((num_layers - 1) * sizeof(double*));

    for (int l = 0; l < num_layers - 1; l++) {
        int rows = layer_sizes[l + 1];
        int cols = layer_sizes[l];
        nn->weights[l] = (double*)calloc(rows * cols, sizeof(double));
        nn->biases[l] = (double*)calloc(rows, sizeof(double));
    }

    int max_in = 0, max_out = 0;
    for (int l = 0; l < num_layers - 1; l++) {
        if (layer_sizes[l] > max_in) max_in = layer_sizes[l];
        if (layer_sizes[l + 1] > max_out) max_out = layer_sizes[l + 1];
    }
    nn->max_input_size = max_in;
    nn->max_output_size = max_out;
    nn->input_cache = (double*)calloc(max_in, sizeof(double));

    nn->layer_outputs = (double**)malloc(num_layers * sizeof(double*));
    for (int l = 0; l < num_layers; l++) {
        nn->layer_outputs[l] = (double*)calloc(layer_sizes[l], sizeof(double));
    }

    return nn;
}

void nn_destroy(NeuralNetwork* nn) {
    if (!nn) return;
    for (int l = 0; l < nn->num_layers - 1; l++) {
        free(nn->weights[l]);
        free(nn->biases[l]);
    }
    free(nn->weights);
    free(nn->biases);
    free(nn->layer_sizes);
    free(nn->input_cache);
    for (int l = 0; l < nn->num_layers; l++) {
        free(nn->layer_outputs[l]);
    }
    free(nn->layer_outputs);
    free(nn);
}

void nn_randomize(NeuralNetwork* nn, uint64_t seed) {
    rng_state = seed;
    for (int l = 0; l < nn->num_layers - 1; l++) {
        int rows = nn->layer_sizes[l + 1];
        int cols = nn->layer_sizes[l];
        double scale = sqrt(2.0 / cols);
        for (int i = 0; i < rows * cols; i++) {
            nn->weights[l][i] = rand_uniform() * 2 * scale - scale;
        }
        for (int i = 0; i < rows; i++) {
            nn->biases[l][i] = rand_uniform() * 0.1 - 0.05;
        }
    }
}

void nn_forward(NeuralNetwork* nn, double* input, double* output) {
    memcpy(nn->layer_outputs[0], input, nn->layer_sizes[0] * sizeof(double));

    for (int l = 0; l < nn->num_layers - 1; l++) {
        int rows = nn->layer_sizes[l + 1];
        int cols = nn->layer_sizes[l];

        for (int i = 0; i < rows; i++) {
            double sum = nn->biases[l][i];
            for (int j = 0; j < cols; j++) {
                sum += nn->weights[l][i * cols + j] * nn->layer_outputs[l][j];
            }
            if (l < nn->num_layers - 2) {
                nn->layer_outputs[l + 1][i] = activation(sum);
            } else {
                nn->layer_outputs[l + 1][i] = tanh(sum);
            }
        }
    }

    memcpy(output, nn->layer_outputs[nn->num_layers - 1],
           nn->layer_sizes[nn->num_layers - 1] * sizeof(double));
}

int nn_save(NeuralNetwork* nn, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;

    fwrite(&nn->num_layers, sizeof(int), 1, f);
    fwrite(nn->layer_sizes, sizeof(int), nn->num_layers, f);

    for (int l = 0; l < nn->num_layers - 1; l++) {
        int rows = nn->layer_sizes[l + 1];
        int cols = nn->layer_sizes[l];
        fwrite(nn->weights[l], sizeof(double), rows * cols, f);
        fwrite(nn->biases[l], sizeof(double), rows, f);
    }

    fclose(f);
    return 1;
}

NeuralNetwork* nn_load(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    int num_layers;
    fread(&num_layers, sizeof(int), 1, f);

    int* layer_sizes = (int*)malloc(num_layers * sizeof(int));
    fread(layer_sizes, sizeof(int), num_layers, f);

    NeuralNetwork* nn = nn_create(num_layers, layer_sizes);
    free(layer_sizes);

    for (int l = 0; l < nn->num_layers - 1; l++) {
        int rows = nn->layer_sizes[l + 1];
        int cols = nn->layer_sizes[l];
        fread(nn->weights[l], sizeof(double), rows * cols, f);
        fread(nn->biases[l], sizeof(double), rows, f);
    }

    fclose(f);
    return nn;
}

void nn_train_particle_predictor(TrainingData* data, NeuralNetwork* nn, int epochs, double lr) {
    int n = data->count;
    int input_size = 7;

    double* dW = (double*)calloc(nn->layer_sizes[1] * nn->layer_sizes[0], sizeof(double));
    double* db = (double*)calloc(nn->layer_sizes[1], sizeof(double));

    for (int epoch = 0; epoch < epochs; epoch++) {
        double total_loss = 0;

        memset(dW, 0, nn->layer_sizes[1] * nn->layer_sizes[0] * sizeof(double));
        memset(db, 0, nn->layer_sizes[1] * sizeof(double));

        for (int i = 0; i < n; i++) {
            double input[7];
            input[0] = data->positions_prev[i * 3];
            input[1] = data->positions_prev[i * 3 + 1];
            input[2] = data->positions_prev[i * 3 + 2];
            input[3] = data->velocities[i * 3];
            input[4] = data->velocities[i * 3 + 1];
            input[5] = data->velocities[i * 3 + 2];
            input[6] = data->masses[i];

            double target[3];
            target[0] = data->positions_curr[i * 3];
            target[1] = data->positions_curr[i * 3 + 1];
            target[2] = data->positions_curr[i * 3 + 2];

            nn_forward(nn, input, nn->input_cache);

            for (int j = 0; j < 3; j++) {
                double error = nn->input_cache[j] - target[j];
                total_loss += error * error;
                double grad = 2.0 * error;
                db[j] += grad;
                for (int k = 0; k < input_size; k++) {
                    dW[j * input_size + k] += grad * input[k];
                }
            }
        }

        for (int j = 0; j < nn->layer_sizes[1]; j++) {
            for (int k = 0; k < nn->layer_sizes[0]; k++) {
                nn->weights[0][j * nn->layer_sizes[0] + k] -= lr * dW[j * nn->layer_sizes[0] + k] / n;
            }
            nn->biases[0][j] -= lr * db[j] / n;
        }

        if (epoch % 100 == 0) {
            printf("  Epoch %d/%d, Loss: %.6e\n", epoch, epochs, total_loss / n);
        }
    }

    free(dW);
    free(db);
}

void nn_predict_trajectory(NeuralNetwork* nn,
    Particle* prev, Particle* curr, int count,
    double t, Particle* result) {
    for (int i = 0; i < count; i++) {
        double input[7];
        input[0] = prev[i].x;
        input[1] = prev[i].y;
        input[2] = prev[i].z;
        input[3] = curr[i].vx;
        input[4] = curr[i].vy;
        input[5] = curr[i].vz;
        input[6] = curr[i].mass;

        nn_forward(nn, input, nn->input_cache);

        result[i].x = prev[i].x + (curr[i].x - prev[i].x) * t;
        result[i].y = prev[i].y + (curr[i].y - prev[i].y) * t;
        result[i].z = prev[i].z + (curr[i].z - prev[i].z) * t;

        result[i].type = curr[i].type;
        result[i].mass = curr[i].mass;
        result[i].temperature = curr[i].temperature;
        result[i].radius = curr[i].radius;
        result[i].luminosity = curr[i].luminosity;
        result[i].active = curr[i].active;
    }
}

FlowField* flow_create(int num_particles, int grid_w, int grid_h) {
    FlowField* ff = (FlowField*)calloc(1, sizeof(FlowField));
    ff->num_particles = num_particles;
    ff->grid_width = grid_w;
    ff->grid_height = grid_h;
    ff->density_map = (double*)calloc(grid_w * grid_h, sizeof(double));
    ff->velocity_field_x = (double*)calloc(grid_w * grid_h, sizeof(double));
    ff->velocity_field_y = (double*)calloc(grid_w * grid_h, sizeof(double));
    return ff;
}

void flow_destroy(FlowField* ff) {
    if (!ff) return;
    free(ff->density_map);
    free(ff->velocity_field_x);
    free(ff->velocity_field_y);
    free(ff);
}

void flow_compute(FlowField* ff, Particle* particles, int count) {
    int gw = ff->grid_width, gh = ff->grid_height;
    memset(ff->density_map, 0, gw * gh * sizeof(double));
    memset(ff->velocity_field_x, 0, gw * gh * sizeof(double));
    memset(ff->velocity_field_y, 0, gw * gh * sizeof(double));
    double* weight = (double*)calloc(gw * gh, sizeof(double));

    double min_x = 1e30, max_x = -1e30, min_y = 1e30, max_y = -1e30;
    for (int i = 0; i < count; i++) {
        if (particles[i].x < min_x) min_x = particles[i].x;
        if (particles[i].x > max_x) max_x = particles[i].x;
        if (particles[i].y < min_y) min_y = particles[i].y;
        if (particles[i].y > max_y) max_y = particles[i].y;
    }
    double range_x = max_x - min_x;
    double range_y = max_y - min_y;
    if (range_x < 1) range_x = 1;
    if (range_y < 1) range_y = 1;

    for (int i = 0; i < count; i++) {
        Particle* p = &particles[i];
        if (!p->active) continue;
        int gx = (int)((p->x - min_x) / range_x * (gw - 1));
        int gy = (int)((p->y - min_y) / range_y * (gh - 1));
        gx = CLAMP(gx, 0, gw - 1);
        gy = CLAMP(gy, 0, gh - 1);
        int idx = gy * gw + gx;
        ff->density_map[idx] += p->mass;
        ff->velocity_field_x[idx] += p->vx;
        ff->velocity_field_y[idx] += p->vy;
        weight[idx] += 1.0;
    }

    for (int i = 0; i < gw * gh; i++) {
        if (weight[i] > 0) {
            ff->velocity_field_x[i] /= weight[i];
            ff->velocity_field_y[i] /= weight[i];
        }
    }

    free(weight);
}

void flow_smooth(FlowField* ff, int iterations) {
    int gw = ff->grid_width, gh = ff->grid_height;
    double* temp = (double*)calloc(gw * gh, sizeof(double));

    for (int iter = 0; iter < iterations; iter++) {
        memcpy(temp, ff->density_map, gw * gh * sizeof(double));
        for (int y = 1; y < gh - 1; y++) {
            for (int x = 1; x < gw - 1; x++) {
                int idx = y * gw + x;
                temp[idx] = (ff->density_map[idx] * 0.5 +
                            (ff->density_map[(y-1)*gw + x] +
                             ff->density_map[(y+1)*gw + x] +
                             ff->density_map[y*gw + x-1] +
                             ff->density_map[y*gw + x+1]) * 0.125);
            }
        }
        memcpy(ff->density_map, temp, gw * gh * sizeof(double));
    }

    free(temp);
}
