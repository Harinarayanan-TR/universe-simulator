#ifndef PHYSICS_ENGINE_H
#define PHYSICS_ENGINE_H

#include <stdint.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define CLAMP(x,lo,hi) MIN(MAX((x),(lo)),(hi))

#define G 6.67430e-11
#define C_LIGHT 299792458.0
#define HBAR 1.054571817e-34
#define KB 1.380649e-23
#define SOLAR_MASS 1.98847e30
#define EARTH_MASS 5.9722e24
#define PARSEC 3.085677581e16
#define LIGHT_YEAR 9.461e15
#define YEAR_SECONDS 31557600.0
#define ELECTRON_CHARGE 1.602176634e-19
#define BOLTZMANN KB
#define STEFAN_BOLTZMANN 5.670374419e-8
#define PI 3.14159265358979323846
#define MAX_PARTICLES 100000
#define MAX_STARS 10000
#define MAX_GALAXIES 100
#define SOFTENING 1e-5
#define GRAVITY_DT_FACTOR 0.01

typedef enum {
    PARTICLE_QUARK_UP = 0,
    PARTICLE_QUARK_DOWN,
    PARTICLE_ELECTRON,
    PARTICLE_NEUTRINO,
    PARTICLE_PHOTON,
    PARTICLE_GLUON,
    PARTICLE_PROTON,
    PARTICLE_NEUTRON,
    PARTICLE_HYDROGEN,
    PARTICLE_HELIUM,
    PARTICLE_DEUTERIUM,
    PARTICLE_LITHIUM,
    PARTICLE_CARBON,
    PARTICLE_NITROGEN,
    PARTICLE_OXYGEN,
    PARTICLE_IRON,
    PARTICLE_DARK_MATTER,
    PARTICLE_BLACK_HOLE,
    PARTICLE_NEUTRON_STAR,
    PARTICLE_COSMIC_RAY,
    PARTICLE_NEBULA_GAS,
    PARTICLE_PLANETESIMAL,
    PARTICLE_TYPE_COUNT
} ParticleType;

typedef struct {
    double x, y, z;
    double vx, vy, vz;
    double mass;
    double charge;
    double temperature;
    double energy;
    ParticleType type;
    double formation_time;
    double lifetime;
    int bound_to;
    int active;
    double radius;
    double luminosity;
    double metallicity;
} Particle;

typedef struct {
    double age;
    double scale_factor;
    double hubble_constant;
    double omega_matter;
    double omega_dark_energy;
    double omega_radiation;
    double temperature_cmb;
    double expansion_rate;
} UniverseParams;

typedef struct {
    Particle* particles;
    int particle_count;
    int particle_capacity;
    double time_seconds;
    double time_years;
    UniverseParams universe;
    double time_multiplier;
    int paused;
    uint64_t seed;
    unsigned long long step_count;
    int epoch;
} SimulationState;

void sim_init(SimulationState* state, uint64_t seed);
void sim_destroy(SimulationState* state);
void sim_step(SimulationState* state, double dt_years);
int sim_add_particle(SimulationState* state, Particle p);
void sim_reset(SimulationState* state, uint64_t seed);
void sim_set_time_scale(SimulationState* state, double multiplier);
void sim_set_paused(SimulationState* state, int paused);

double sim_get_universe_age(SimulationState* state);
int sim_get_particle_count(SimulationState* state);
int sim_get_star_count(SimulationState* state);
int sim_get_galaxy_count(SimulationState* state);
double sim_get_scale_factor(SimulationState* state);
double sim_get_hubble(SimulationState* state);
int sim_get_epoch(SimulationState* state);

void compute_gravity(Particle* pi, Particle* pj, double* fx, double* fy, double* fz);
double compute_coulomb(Particle* pi, Particle* pj);
double fusion_cross_section(double temp, double coulomb_barrier);
int try_fusion(Particle* a, Particle* b);
void stellar_evolution(SimulationState* state, int idx, double dt);
void init_big_bang(SimulationState* state);
void expand_universe(SimulationState* state, double dt);
void propagate_photons(SimulationState* state, double dt);
void quantum_tunneling_probability(double mass, double energy, double barrier_height, double barrier_width, double* prob);
int particle_decay(SimulationState* state, int idx, double dt);
void nucleosynthesis(SimulationState* state, double dt);
void form_stars(SimulationState* state, double dt);
void form_planets(SimulationState* state, int star_idx, double dt);
void merge_galaxies(SimulationState* state, double dt);

#endif
