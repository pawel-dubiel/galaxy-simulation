#ifndef GALAXY_H
#define GALAXY_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "math3d.h"

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 900
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

// Data Structures
typedef struct {
    char name[32];
    float mass;
    float radius;
    float temp;
    float luminosity; 
    float metallicity; 
    Uint32 color; 
    
    // Physics (3D)
    Vec3 pos; // AU
    Vec3 vel; // AU/year
} Star;

typedef enum {
    TYPE_ROCKY,
    TYPE_TERRESTRIAL,
    TYPE_GAS_GIANT,
    TYPE_ICE_GIANT,
    TYPE_MOLTEN,
    TYPE_ICY_WORLD
} PlanetType;

typedef struct {
    char name[32];
    float radius;
    float orbit_dist; 
    float orbit_angle; 
    float orbit_speed;
    // Physics (Relative 3D)
    Vec3 pos;   
    Vec3 vel;
} Moon;

typedef struct {
    char name[32];
    float mass;
    float radius;
    Uint32 color;
    PlanetType type;
    
    // Physics (3D)
    Vec3 pos; 
    Vec3 vel; 
    
    // Orbital Parameters (Keplerian fallback)
    float orbit_radius;
    float orbit_angle;
    float orbit_speed;
    float eccentricity;
    float inclination;
    
    // Details
    float density; 
    float surface_temp; 
    char atmosphere[32];
    float albedo;
    float greenhouse; 
    bool is_tidally_locked;
    float gravity; 
    float rotation_period; 
    float rotation_angle;
    
    // Resources
    float resources[8];
    
    int moon_count;
    Moon moons[4];
} Planet;

typedef struct {
    int star_id;             // Unique star identifier
    unsigned int seed;
    int star_count;
    Star stars[3];
    int planet_count;
    Planet planets[16];
} StarSystem;

// Galaxy-scale structures for rotating Milky Way
typedef struct {
    unsigned int seed;        // Seed for system generation
    float orbit_radius;       // Distance from galactic center (kLY)
    float orbit_angle;        // Current angle in orbit (radians)
    float orbit_speed;        // Angular velocity (rad/year)
    float z_offset;           // Vertical offset from galactic plane (kLY)
    Uint32 color;            // Star color for rendering
    int star_id;             // Unique star identifier for system generation
} GalaxyStar;

typedef struct {
    GalaxyStar *stars;       // Dynamic array of galaxy stars
    int star_count;          // Total number of stars
    float time;              // Galaxy simulation time (years)
} Galaxy;

// Spatial Grid for Click Detection Optimization
#define SPATIAL_GRID_SIZE 64
#define SPATIAL_CELL_CAPACITY 32

typedef struct {
    int star_indices[SPATIAL_CELL_CAPACITY];
    int count;
} SpatialCell;

typedef struct {
    SpatialCell cells[SPATIAL_GRID_SIZE][SPATIAL_GRID_SIZE];
    float cell_size;
} SpatialGrid;


// Ship (6-DOF)
typedef struct {
    Vec3 pos;
    Vec3 vel;
    Mat3 rot;      
    float speed;   
    float throttle; 
} Ship;

// Game State
typedef enum {
    VIEW_GALAXY,
    VIEW_COCKPIT, // 3D
    VIEW_TACTICAL // Top-down
} ViewMode;

typedef struct {
    ViewMode mode;
    
    // Galaxy View Camera
    Vec3 map_cam_pos;
    float map_zoom;
    float map_tilt;        // Camera tilt angle (0 = top-down, PI/2 = edge-on)
    
    // Galaxy Simulation
    Galaxy galaxy;
    
    // System View / Player
    Ship player;
    
    StarSystem current_system;
    bool running;
    bool debug;
    bool paused;
    float time_speed;
    
    int selected_planet_idx; 
    bool selected_star;
    int centered_planet_idx;
    
    // Rendering
    SDL_Texture *body_texture;
    
    // Cache
    StarSystem *visited_systems;
    int visited_count;
    int visited_capacity;
    
    // Spatial Grid for Click Detection (updated each frame)
    SpatialGrid click_grid;
} GameState;

// Trig Lookup Tables (for galaxy rotation optimization)
#define TRIG_TABLE_SIZE 4096
extern float g_sin_table[TRIG_TABLE_SIZE];
extern float g_cos_table[TRIG_TABLE_SIZE];

void init_trig_tables(void);
float fast_sin(float angle);
float fast_cos(float angle);

// Functions
unsigned int get_seed_from_id(int star_id);
StarSystem generate_system(int star_id);
float rand_float(float min, float max); 
void set_forced_star_count(int count);
void init_galaxy(Galaxy *galaxy, int star_count);


#endif