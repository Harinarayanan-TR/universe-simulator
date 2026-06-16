#include "physics_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static unsigned long long xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static double random_double(uint64_t* state) {
    return (double)xorshift64(state) / (double)UINT64_MAX;
}

static double random_range(uint64_t* state, double lo, double hi) {
    return lo + random_double(state) * (hi - lo);
}

static double gaussian_random(uint64_t* state) {
    double u1 = random_double(state);
    double u2 = random_double(state);
    if (u1 < 1e-15) u1 = 1e-15;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
}


int sim_get_particle_count(SimulationState* state) {
    return state->particle_count;
}

double sim_get_universe_age(SimulationState* state) {
    return state->universe.age;
}

int sim_get_star_count(SimulationState* state) {
    int count = 0;
    for (int i = 0; i < state->particle_count; i++) {
        Particle* p = &state->particles[i];
        if (p->active && (p->type == PARTICLE_HYDROGEN || p->type == PARTICLE_HELIUM ||
            p->type == PARTICLE_CARBON || p->type == PARTICLE_OXYGEN) &&
            p->mass > 0.1 * SOLAR_MASS && p->luminosity > 1e20) {
            count++;
        }
    }
    return count;
}

int sim_get_galaxy_count(SimulationState* state) {
    int count = 0;
    for (int i = 0; i < state->particle_count; i++) {
        Particle* p = &state->particles[i];
        if (p->active && p->type == PARTICLE_DARK_MATTER && p->mass > 1e12 * SOLAR_MASS) {
            count++;
        }
    }
    return count;
}

double sim_get_scale_factor(SimulationState* state) {
    return state->universe.scale_factor;
}

double sim_get_hubble(SimulationState* state) {
    return state->universe.hubble_constant;
}

int sim_get_epoch(SimulationState* state) {
    return state->epoch;
}

void sim_set_time_scale(SimulationState* state, double multiplier) {
    state->time_multiplier = CLAMP(multiplier, 0.0, 1e12);
}

void sim_set_paused(SimulationState* state, int paused) {
    state->paused = paused;
}

int sim_add_particle(SimulationState* state, Particle p) {
    if (state->particle_count >= state->particle_capacity) {
        int new_cap = state->particle_capacity == 0 ? 1024 : state->particle_capacity * 2;
        if (new_cap > 1000000) new_cap = 1000000;
        Particle* new_particles = realloc(state->particles, new_cap * sizeof(Particle));
        if (!new_particles) return -1;
        state->particles = new_particles;
        state->particle_capacity = new_cap;
    }
    state->particles[state->particle_count++] = p;
    return state->particle_count - 1;
}

void compute_gravity(Particle* pi, Particle* pj, double* fx, double* fy, double* fz) {
    double dx = pj->x - pi->x;
    double dy = pj->y - pi->y;
    double dz = pj->z - pi->z;
    double r2 = dx*dx + dy*dy + dz*dz + SOFTENING;
    double r = sqrt(r2);
    double inv_r3 = 1.0 / (r2 * r);
    double f = G * pi->mass * pj->mass * inv_r3;
    *fx = f * dx;
    *fy = f * dy;
    *fz = f * dz;
}

double compute_coulomb(Particle* pi, Particle* pj) {
    double dx = pj->x - pi->x;
    double dy = pj->y - pi->y;
    double dz = pj->z - pi->z;
    double r2 = dx*dx + dy*dy + dz*dz + SOFTENING;
    double r = sqrt(r2);
    double k = 8.987551787e9;
    return k * pi->charge * pj->charge / (r * r);
}

static int charges_opposite(Particle* a, Particle* b) {
    return (a->charge > 0 && b->charge < 0) || (a->charge < 0 && b->charge > 0);
}

static double coulomb_barrier(Particle* a, Particle* b) {
    double r_nuclear = 1.2e-15 * (pow(a->mass / (1.67e-27), 1.0/3.0) +
                                   pow(b->mass / (1.67e-27), 1.0/3.0));
    double k = 8.987551787e9;
    return k * a->charge * b->charge * ELECTRON_CHARGE * ELECTRON_CHARGE / r_nuclear;
}

double fusion_cross_section(double temp, double barrier_energy) {
    if (temp < 1e6) return 0.0;
    double kT = KB * temp;
    double gamow = sqrt(barrier_energy / kT);
    double cross_section = exp(-gamow) / (temp * temp * 1e10);
    return cross_section;
}

int try_fusion(Particle* a, Particle* b) {
    if (a->type != PARTICLE_HYDROGEN && a->type != PARTICLE_DEUTERIUM &&
        a->type != PARTICLE_HELIUM) return 0;
    if (b->type != PARTICLE_HYDROGEN && b->type != PARTICLE_DEUTERIUM &&
        b->type != PARTICLE_HELIUM) return 0;
    if (a->temperature < 1e7 || b->temperature < 1e7) return 0;

    double barrier = coulomb_barrier(a, b) / ELECTRON_CHARGE;
    double avg_temp = (a->temperature + b->temperature) * 0.5;
    double cs = fusion_cross_section(avg_temp, barrier);
    if (cs < 1e-50) return 0;

    double density = 1e5;
    double rate = cs * density * 1e7;
    if (rate > random_double(&(uint64_t){rand()})) {
        if (a->type == PARTICLE_HYDROGEN && b->type == PARTICLE_HYDROGEN) {
            a->type = PARTICLE_DEUTERIUM;
            a->mass = 3.34e-27;
            b->type = PARTICLE_PHOTON;
            b->mass = 0;
            b->charge = 0;
            b->energy = 5.493e-13;
            return 1;
        } else if ((a->type == PARTICLE_DEUTERIUM && b->type == PARTICLE_HYDROGEN) ||
                   (a->type == PARTICLE_HYDROGEN && b->type == PARTICLE_DEUTERIUM)) {
            a->type = PARTICLE_HELIUM;
            a->mass = 6.64e-27;
            a->charge = 2 * ELECTRON_CHARGE;
            b->type = PARTICLE_PHOTON;
            b->mass = 0;
            b->charge = 0;
            b->energy = 1.63e-12;
            return 1;
        } else if (a->type == PARTICLE_DEUTERIUM && b->type == PARTICLE_DEUTERIUM) {
            a->type = PARTICLE_HELIUM;
            a->mass = 6.64e-27;
            b->type = PARTICLE_NEUTRON;
            b->mass = 1.675e-27;
            b->charge = 0;
            return 1;
        }
    }
    return 0;
}

void quantum_tunneling_probability(double mass, double energy, double barrier_height,
                                    double barrier_width, double* prob) {
    if (energy >= barrier_height) {
        *prob = 1.0;
        return;
    }
    double kappa = sqrt(2.0 * mass * (barrier_height - energy)) / HBAR;
    if (kappa * barrier_width > 100) {
        *prob = 0.0;
        return;
    }
    *prob = exp(-2.0 * kappa * barrier_width);
}

int particle_decay(SimulationState* state, int idx, double dt) {
    Particle* p = &state->particles[idx];
    if (!p->active) return 0;

    if (p->type == PARTICLE_NEUTRON) {
        double mean_life = 880.2;
        double decay_prob = dt / mean_life;
        if (decay_prob > random_double(&state->seed)) {
            p->type = PARTICLE_PROTON;
            p->charge = ELECTRON_CHARGE;
            p->mass = 1.6726e-27;
            int e_idx = sim_add_particle(state, *p);
            if (e_idx >= 0) {
                state->particles[e_idx].type = PARTICLE_ELECTRON;
                state->particles[e_idx].charge = -ELECTRON_CHARGE;
                state->particles[e_idx].mass = 9.11e-31;
                state->particles[e_idx].energy = 7.82e-14;
            }
            return 1;
        }
    }

    if (p->type == PARTICLE_NEUTRINO) {
        double mean_life = 1e12 * YEAR_SECONDS;
        double decay_prob = dt / mean_life;
        if (decay_prob > random_double(&state->seed)) {
            p->type = PARTICLE_PHOTON;
            p->mass = 0;
            p->charge = 0;
            return 1;
        }
    }

    return 0;
}

void stellar_evolution(SimulationState* state, int idx, double dt) {
    Particle* p = &state->particles[idx];
    if (!p->active) return;
    if (p->mass < 0.08 * SOLAR_MASS) return;

    double age_years = state->universe.age - p->formation_time;

    if (p->type == PARTICLE_NEBULA_GAS && p->mass > 0.1 * SOLAR_MASS) {
        if (age_years > 5e7) {
            p->type = PARTICLE_HYDROGEN;
            p->luminosity = 3.8e26 * pow(p->mass / SOLAR_MASS, 3.5);
            p->temperature = 1.5e7;
            p->radius = 6.96e8 * pow(p->mass / SOLAR_MASS, 0.8);
        }
        return;
    }

    if (p->luminosity > 1e20) {
        double main_sequence_life = 1e10 * SOLAR_MASS / p->mass;

        if (age_years < main_sequence_life) {
            p->temperature = 1.5e7;
            p->luminosity = 3.8e26 * pow(p->mass / SOLAR_MASS, 3.5);
            p->radius = 6.96e8 * pow(p->mass / SOLAR_MASS, 0.8);
            return;
        }

        double post_main_seq_age = age_years - main_sequence_life;

        if (p->mass < 8.0 * SOLAR_MASS) {
            double giant_life = 1e9 * pow(SOLAR_MASS / p->mass, 2);
            if (post_main_seq_age < giant_life) {
                p->temperature = 1.0e8;
                p->luminosity = 1e3 * 3.8e26 * pow(p->mass / SOLAR_MASS, 3.5);
                p->radius = 1e2 * 6.96e8 * pow(p->mass / SOLAR_MASS, 0.8);
                p->type = PARTICLE_CARBON;
                return;
            }
            if (p->mass < 1.4 * SOLAR_MASS) {
                p->type = PARTICLE_IRON;
                p->radius = 6.37e6;
                p->luminosity = 0;
                p->temperature = 1e7;
                p->mass *= 0.6;
                return;
            }
            p->type = PARTICLE_NEUTRON_STAR;
            p->radius = 1e4;
            p->luminosity = 0;
            p->mass *= 0.3;
            p->temperature = 1e12;
            return;
        }
        {
            double supernova_time = 1e7 * pow(SOLAR_MASS / p->mass, 2);
            if (post_main_seq_age > supernova_time) {
                for (int j = 0; j < 10; j++) {
                    Particle ejecta = *p;
                    ejecta.mass = p->mass / 20.0;
                    ejecta.type = PARTICLE_NEBULA_GAS;
                    ejecta.vx = p->vx + random_range(&state->seed, -1e6, 1e6);
                    ejecta.vy = p->vy + random_range(&state->seed, -1e6, 1e6);
                    ejecta.vz = p->vz + random_range(&state->seed, -1e6, 1e6);
                    ejecta.active = 1;
                    ejecta.luminosity = 0;
                    sim_add_particle(state, ejecta);
                }

                if (p->mass > 3.0 * SOLAR_MASS) {
                    p->type = PARTICLE_BLACK_HOLE;
                    p->radius = 2.0 * G * p->mass / (C_LIGHT * C_LIGHT);
                    p->luminosity = 0;
                    p->temperature = 0;
                } else {
                    p->type = PARTICLE_NEUTRON_STAR;
                    p->radius = 1e4;
                    p->luminosity = 0;
                    p->mass *= 0.3;
                    p->temperature = 1e12;
                }
            }
        }
    }
}

static int is_star(Particle* p) {
    return p->active && p->luminosity > 1e20;
}

void form_stars(SimulationState* state, double dt) {
    if (state->universe.age < 1.5e8) return;

    for (int i = 0; i < state->particle_count; i++) {
        Particle* p = &state->particles[i];
        if (!p->active || p->type != PARTICLE_NEBULA_GAS) continue;
        if (p->mass < 0.5 * SOLAR_MASS) continue;

        double density = 0;
        int nearby = 0;
        for (int j = 0; j < state->particle_count; j++) {
            if (i == j || !state->particles[j].active) continue;
            double dx = state->particles[j].x - p->x;
            double dy = state->particles[j].y - p->y;
            double dz = state->particles[j].z - p->z;
            double r = sqrt(dx*dx + dy*dy + dz*dz);
            if (r < 1e18) { // within ~100 ly
                density += state->particles[j].mass;
                nearby++;
            }
        }

        double critical_density = 1e-18;
        if (density > critical_density && nearby > 10) {
            double prob = dt / 1e7;
            if (prob > random_double(&state->seed)) {
                p->formation_time = state->universe.age;
                p->type = PARTICLE_HYDROGEN;
                p->luminosity = 3.8e26 * pow(p->mass / SOLAR_MASS, 3.5);
                p->temperature = 1.5e7;
                p->radius = 6.96e8 * pow(p->mass / SOLAR_MASS, 0.8);
            }
        }
    }
}

void form_planets(SimulationState* state, int star_idx, double dt) {
    Particle* star = &state->particles[star_idx];
    if (!is_star(star)) return;
    if (star->mass < 0.3 * SOLAR_MASS || star->mass > 3.0 * SOLAR_MASS) return;

    double star_age = state->universe.age - star->formation_time;
    if (star_age > 1e8 || star_age < 1e6) return;

    for (int i = 0; i < state->particle_count; i++) {
        if (state->particles[i].active && state->particles[i].type == PARTICLE_PLANETESIMAL &&
            state->particles[i].bound_to == star_idx) {
            Particle* planetesimal = &state->particles[i];
            planetesimal->mass += dt * 1e10;
            if (planetesimal->mass > EARTH_MASS * 0.01) {
                planetesimal->type = PARTICLE_OXYGEN;
                planetesimal->radius = 6.37e6 * pow(planetesimal->mass / EARTH_MASS, 1.0/3.0);
            }
        }
    }
}

void nucleosynthesis(SimulationState* state, double dt) {
    if (state->universe.age > 1e4) return;

    for (int i = 0; i < state->particle_count; i++) {
        Particle* p = &state->particles[i];
        if (!p->active) continue;
        if (p->temperature < 1e8) continue;

        double kT = KB * p->temperature;
        double prob = dt * exp(-7.0e9 / (kT * 1.6e-19));

        if (p->type == PARTICLE_PROTON && prob > random_double(&state->seed)) {
            if (random_double(&state->seed) < 0.75) {
                p->type = PARTICLE_HYDROGEN;
                p->mass = 1.67e-27;
            } else {
                p->type = PARTICLE_HELIUM;
                p->mass = 6.64e-27;
                p->charge = 2 * ELECTRON_CHARGE;
            }
        }
    }
}

void propagate_photons(SimulationState* state, double dt) {
    double dt_seconds = dt * YEAR_SECONDS;

    for (int i = 0; i < state->particle_count; i++) {
        Particle* p = &state->particles[i];
        if (!p->active || p->type != PARTICLE_PHOTON) continue;

        double speed = C_LIGHT;
        p->x += p->vx / speed * speed * dt_seconds;
        p->y += p->vy / speed * speed * dt_seconds;
        p->z += p->vz / speed * speed * dt_seconds;

        p->energy /= (1.0 + state->universe.expansion_rate * dt);

        if (p->energy < 1e-30) {
            p->active = 0;
        }
    }
}

void expand_universe(SimulationState* state, double dt) {
    double H0 = 2.36e-18;
    double H = H0 * sqrt(state->universe.omega_matter / (state->universe.scale_factor *
                          state->universe.scale_factor * state->universe.scale_factor) +
                         state->universe.omega_radiation / (state->universe.scale_factor *
                          state->universe.scale_factor * state->universe.scale_factor *
                          state->universe.scale_factor) +
                         state->universe.omega_dark_energy);

    state->universe.expansion_rate = H;
    state->universe.scale_factor *= (1.0 + H * dt * YEAR_SECONDS);
    state->universe.hubble_constant = H * PARSEC / 1000.0;

    state->universe.temperature_cmb = 2.725 / state->universe.scale_factor;

    double scale = state->universe.scale_factor;
    state->universe.omega_matter = 0.3089 / (scale * scale * scale) /
        (0.3089 / (scale * scale * scale) + 0.6911 + 9e-5 / (scale * scale * scale * scale));
    state->universe.omega_dark_energy = 0.6911 /
        (0.3089 / (scale * scale * scale) + 0.6911 + 9e-5 / (scale * scale * scale * scale));

    if (state->universe.scale_factor < 1e-5) {
        state->epoch = 0;
    } else if (state->universe.age < 1e-12) {
        state->epoch = 1;
    } else if (state->universe.age < 1e-6) {
        state->epoch = 2;
    } else if (state->universe.age < 1.0) {
        state->epoch = 3;
    } else if (state->universe.age < 3.8e5) {
        state->epoch = 4;
    } else if (state->universe.age < 1.5e8) {
        state->epoch = 5;
    } else if (state->universe.age < 5e8) {
        state->epoch = 6;
    } else if (state->universe.age < 9e9) {
        state->epoch = 7;
    } else if (state->universe.age < 1.38e10) {
        state->epoch = 8;
    } else {
        state->epoch = 9;
    }

    for (int i = 0; i < state->particle_count; i++) {
        Particle* p = &state->particles[i];
        if (!p->active) continue;
        if (p->type != PARTICLE_PHOTON) {
            double expansion_factor = H * dt * YEAR_SECONDS;
            p->x *= (1.0 + expansion_factor);
            p->y *= (1.0 + expansion_factor);
            p->z *= (1.0 + expansion_factor);
        }
    }
}

void init_big_bang(SimulationState* state) {
    state->particle_count = 0;
    state->time_years = 0;
    state->time_seconds = 0;
    state->step_count = 0;
    state->epoch = 0;

    state->universe.age = 0;
    state->universe.scale_factor = 1e-10;
    state->universe.hubble_constant = 67.4;
    state->universe.omega_matter = 0.3089;
    state->universe.omega_dark_energy = 0.6911;
    state->universe.omega_radiation = 9e-5;
    state->universe.temperature_cmb = 2.725;
    state->universe.expansion_rate = 0;

    uint64_t rng = state->seed;

    int num_quarks = 300;
    int num_electrons = 150;
    int num_photons = 300;
    int num_dark_matter = 200;
    int num_neutrinos = 100;

    double init_temp = 1e12;
    double init_radius = 1e-10;

    for (int i = 0; i < num_quarks; i++) {
        Particle p = {0};
        p.x = random_range(&rng, -init_radius, init_radius);
        p.y = random_range(&rng, -init_radius, init_radius);
        p.z = random_range(&rng, -init_radius, init_radius);
        p.vx = gaussian_random(&rng) * 1e7;
        p.vy = gaussian_random(&rng) * 1e7;
        p.vz = gaussian_random(&rng) * 1e7;
        p.mass = 5.0e-30;
        p.charge = (random_double(&rng) > 0.5 ? 2.0/3.0 : -1.0/3.0) * ELECTRON_CHARGE;
        p.temperature = init_temp;
        p.energy = 1.5 * KB * init_temp;
        p.type = random_double(&rng) > 0.5 ? PARTICLE_QUARK_UP : PARTICLE_QUARK_DOWN;
        p.formation_time = 0;
        p.lifetime = 1e30;
        p.bound_to = -1;
        p.active = 1;
        p.radius = 1e-18;
        p.luminosity = 0;
        p.metallicity = 0;
        sim_add_particle(state, p);
    }

    for (int i = 0; i < num_electrons; i++) {
        Particle p = {0};
        p.x = random_range(&rng, -init_radius, init_radius);
        p.y = random_range(&rng, -init_radius, init_radius);
        p.z = random_range(&rng, -init_radius, init_radius);
        p.vx = gaussian_random(&rng) * 1e7;
        p.vy = gaussian_random(&rng) * 1e7;
        p.vz = gaussian_random(&rng) * 1e7;
        p.mass = 9.11e-31;
        p.charge = -ELECTRON_CHARGE;
        p.temperature = init_temp;
        p.energy = 1.5 * KB * init_temp;
        p.type = PARTICLE_ELECTRON;
        p.formation_time = 0;
        p.lifetime = 1e30;
        p.bound_to = -1;
        p.active = 1;
        p.radius = 1e-22;
        p.luminosity = 0;
        p.metallicity = 0;
        sim_add_particle(state, p);
    }

    for (int i = 0; i < num_photons; i++) {
        Particle p = {0};
        p.x = random_range(&rng, -init_radius, init_radius);
        p.y = random_range(&rng, -init_radius, init_radius);
        p.z = random_range(&rng, -init_radius, init_radius);
        double theta = random_range(&rng, 0, 2*PI);
        double phi = acos(random_range(&rng, -1, 1));
        p.vx = C_LIGHT * sin(phi) * cos(theta);
        p.vy = C_LIGHT * sin(phi) * sin(theta);
        p.vz = C_LIGHT * cos(phi);
        p.mass = 0;
        p.charge = 0;
        p.temperature = init_temp;
        p.energy = KB * init_temp;
        p.type = PARTICLE_PHOTON;
        p.formation_time = 0;
        p.lifetime = 1e30;
        p.bound_to = -1;
        p.active = 1;
        p.radius = 0;
        p.luminosity = 0;
        p.metallicity = 0;
        sim_add_particle(state, p);
    }

    for (int i = 0; i < num_dark_matter; i++) {
        Particle p = {0};
        p.x = random_range(&rng, -init_radius, init_radius);
        p.y = random_range(&rng, -init_radius, init_radius);
        p.z = random_range(&rng, -init_radius, init_radius);
        p.vx = gaussian_random(&rng) * 1e5;
        p.vy = gaussian_random(&rng) * 1e5;
        p.vz = gaussian_random(&rng) * 1e5;
        p.mass = 1e-25;
        p.charge = 0;
        p.temperature = 0;
        p.energy = 0;
        p.type = PARTICLE_DARK_MATTER;
        p.formation_time = 0;
        p.lifetime = 1e30;
        p.bound_to = -1;
        p.active = 1;
        p.radius = 0;
        p.luminosity = 0;
        p.metallicity = 0;
        sim_add_particle(state, p);
    }

    for (int i = 0; i < num_neutrinos; i++) {
        Particle p = {0};
        p.x = random_range(&rng, -init_radius, init_radius);
        p.y = random_range(&rng, -init_radius, init_radius);
        p.z = random_range(&rng, -init_radius, init_radius);
        double theta = random_range(&rng, 0, 2*PI);
        double phi = acos(random_range(&rng, -1, 1));
        p.vx = C_LIGHT * sin(phi) * cos(theta);
        p.vy = C_LIGHT * sin(phi) * sin(theta);
        p.vz = C_LIGHT * cos(phi);
        p.mass = 1e-37;
        p.charge = 0;
        p.temperature = init_temp;
        p.energy = KB * init_temp;
        p.type = PARTICLE_NEUTRINO;
        p.formation_time = 0;
        p.lifetime = 1e30;
        p.bound_to = -1;
        p.active = 1;
        p.radius = 0;
        p.luminosity = 0;
        p.metallicity = 0;
        sim_add_particle(state, p);
    }

    state->seed = rng;
}

static void apply_forces(SimulationState* state, double dt) {
    int n = state->particle_count;
    if (n < 2) return;
    double dt_seconds = dt * YEAR_SECONDS;

    if (state->universe.age < 1e6) return;

    if (n > 5000) return;

    double* ax = calloc(n, sizeof(double));
    double* ay = calloc(n, sizeof(double));
    double* az = calloc(n, sizeof(double));

    if (!ax || !ay || !az) {
        free(ax); free(ay); free(az);
        return;
    }

    double gravity_cutoff = 1e20;

    for (int i = 0; i < n; i++) {
        if (!state->particles[i].active) continue;
        Particle* pi = &state->particles[i];

        for (int j = i + 1; j < n; j++) {
            if (!state->particles[j].active) continue;
            Particle* pj = &state->particles[j];

            double dx = pj->x - pi->x;
            double dy = pj->y - pi->y;
            double dz = pj->z - pi->z;
            double dist2 = dx*dx + dy*dy + dz*dz;

            if (dist2 > gravity_cutoff * gravity_cutoff) continue;

            double dist = sqrt(dist2);
            double inv_mi = 1.0 / pi->mass;
            double inv_mj = 1.0 / pj->mass;

            double fx, fy, fz;
            compute_gravity(pi, pj, &fx, &fy, &fz);

            ax[i] += fx * inv_mi;
            ay[i] += fy * inv_mi;
            az[i] += fz * inv_mi;
            ax[j] -= fx * inv_mj;
            ay[j] -= fy * inv_mj;
            az[j] -= fz * inv_mj;

            if (pi->charge != 0 && pj->charge != 0 && dist > SOFTENING) {
                double coulomb_force = compute_coulomb(pi, pj);
                double f_em = coulomb_force / dist;
                ax[i] += f_em * dx * inv_mi;
                ay[i] += f_em * dy * inv_mi;
                az[i] += f_em * dz * inv_mi;
                ax[j] -= f_em * dx * inv_mj;
                ay[j] -= f_em * dy * inv_mj;
                az[j] -= f_em * dz * inv_mj;
            }

            if (dist < 1e-14 && pi->type <= PARTICLE_NEUTRON && pj->type <= PARTICLE_NEUTRON) {
                double r0 = 1.2e-15;
                double g_strong = 1.0;
                double yukawa = g_strong * exp(-dist / r0) / (dist * dist);
                ax[i] += yukawa * dx * inv_mi;
                ay[i] += yukawa * dy * inv_mi;
                az[i] += yukawa * dz * inv_mi;
                ax[j] -= yukawa * dx * inv_mj;
                ay[j] -= yukawa * dy * inv_mj;
                az[j] -= yukawa * dz * inv_mj;

                if (!charges_opposite(pi, pj)) {
                    try_fusion(pi, pj);
                }
            }
        }
    }

    for (int i = 0; i < n; i++) {
        if (!state->particles[i].active) continue;
        Particle* p = &state->particles[i];

        p->vx += ax[i] * dt_seconds * 0.5;
        p->vy += ay[i] * dt_seconds * 0.5;
        p->vz += az[i] * dt_seconds * 0.5;

        p->x += p->vx * dt_seconds;
        p->y += p->vy * dt_seconds;
        p->z += p->vz * dt_seconds;

        p->vx += ax[i] * dt_seconds * 0.5;
        p->vy += ay[i] * dt_seconds * 0.5;
        p->vz += az[i] * dt_seconds * 0.5;
    }

    free(ax); free(ay); free(az);
}

static void merge_particles(SimulationState* state) {
    int n = state->particle_count;
    int merged = 0;

    for (int i = 0; i < n; i++) {
        if (!state->particles[i].active) continue;
        Particle* pi = &state->particles[i];

        for (int j = i + 1; j < n; j++) {
            if (!state->particles[j].active) continue;
            Particle* pj = &state->particles[j];

            double dx = pj->x - pi->x;
            double dy = pj->y - pi->y;
            double dz = pj->z - pi->z;
            double dist = sqrt(dx*dx + dy*dy + dz*dz);

            if (pi->type == PARTICLE_BLACK_HOLE || pj->type == PARTICLE_BLACK_HOLE) {
                double bh_schwarzschild = 2.0 * G * (pi->type == PARTICLE_BLACK_HOLE ? pi->mass : pj->mass) / (C_LIGHT * C_LIGHT);
                if (dist < bh_schwarzschild * 10) {
                    if (pi->type == PARTICLE_BLACK_HOLE) {
                        pi->mass += pj->mass;
                        pj->active = 0;
                    } else {
                        pj->mass += pi->mass;
                        pi->active = 0;
                    }
                    merged++;
                    continue;
                }
            }

            if (dist < (pi->radius + pj->radius) * 0.5 && pi->type != PARTICLE_PHOTON && pj->type != PARTICLE_PHOTON) {
                double total_mass = pi->mass + pj->mass;
                pi->x = (pi->x * pi->mass + pj->x * pj->mass) / total_mass;
                pi->y = (pi->y * pi->mass + pj->y * pj->mass) / total_mass;
                pi->z = (pi->z * pi->mass + pj->z * pj->mass) / total_mass;
                pi->vx = (pi->vx * pi->mass + pj->vx * pj->mass) / total_mass;
                pi->vy = (pi->vy * pi->mass + pj->vy * pj->mass) / total_mass;
                pi->vz = (pi->vz * pi->mass + pj->vz * pj->mass) / total_mass;
                pi->mass = total_mass;
                pi->radius = pow(pi->radius*pi->radius*pi->radius + pj->radius*pj->radius*pj->radius, 1.0/3.0);
                pj->active = 0;
                merged++;
            }
        }
    }

    if (merged > 0) {
        int write_idx = 0;
        for (int read_idx = 0; read_idx < n; read_idx++) {
            if (state->particles[read_idx].active) {
                if (write_idx != read_idx) {
                    state->particles[write_idx] = state->particles[read_idx];
                }
                write_idx++;
            }
        }
        state->particle_count = write_idx;
    }
}

void merge_galaxies(SimulationState* state, double dt) {
    if (state->universe.age < 1e9) return;

    for (int i = 0; i < state->particle_count; i++) {
        Particle* pi = &state->particles[i];
        if (!pi->active) continue;
        if (pi->mass < 1e10 * SOLAR_MASS) continue;

        for (int j = i + 1; j < state->particle_count; j++) {
            Particle* pj = &state->particles[j];
            if (!pj->active) continue;
            if (pj->mass < 1e10 * SOLAR_MASS) continue;

            double dx = pj->x - pi->x;
            double dy = pj->y - pi->y;
            double dz = pj->z - pi->z;
            double dist = sqrt(dx*dx + dy*dy + dz*dz);

            if (dist < 1e22) {
                double rel_v = sqrt((pj->vx - pi->vx)*(pj->vx - pi->vx) +
                                    (pj->vy - pi->vy)*(pj->vy - pi->vy) +
                                    (pj->vz - pi->vz)*(pj->vz - pi->vz));
                double escape_v = sqrt(2.0 * G * (pi->mass + pj->mass) / dist);

                if (rel_v < escape_v * 1.5) {
                    pi->mass += pj->mass * dt * 0.01;
                    pi->x += pj->x * pj->mass / (pi->mass + pj->mass) * dt * 0.01;
                    pi->y += pj->y * pj->mass / (pi->mass + pj->mass) * dt * 0.01;
                    pi->z += pj->z * pj->mass / (pi->mass + pj->mass) * dt * 0.01;
                }
            }
        }
    }
}

void sim_destroy(SimulationState* state) {
    free(state->particles);
    state->particles = NULL;
    state->particle_count = 0;
    state->particle_capacity = 0;
}

void sim_init(SimulationState* state, uint64_t seed) {
    memset(state, 0, sizeof(SimulationState));
    state->seed = seed;
    state->time_multiplier = 1.0;
    state->paused = 0;

    init_big_bang(state);
}

void sim_step(SimulationState* state, double dt_years) {
    if (state->paused) return;

    state->universe.age += dt_years;
    state->time_years = state->universe.age;
    state->time_seconds = state->universe.age * YEAR_SECONDS;
    state->step_count++;

    expand_universe(state, dt_years);

    apply_forces(state, dt_years);

    merge_particles(state);

    propagate_photons(state, dt_years);

    for (int i = 0; i < state->particle_count; i++) {
        particle_decay(state, i, dt_years);
        stellar_evolution(state, i, dt_years);
    }

    nucleosynthesis(state, dt_years);
    form_stars(state, dt_years);

    if (state->universe.age > 1e9) {
        for (int i = 0; i < state->particle_count; i++) {
            if (is_star(&state->particles[i])) {
                form_planets(state, i, dt_years);
            }
        }
    }

    merge_galaxies(state, dt_years);
}

void sim_reset(SimulationState* state, uint64_t seed) {
    sim_destroy(state);
    sim_init(state, seed);
}
