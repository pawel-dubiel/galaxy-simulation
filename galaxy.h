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
    int x, y; 
    unsigned int seed;
    int star_count;
    Star stars[3];
    int planet_count;
    Planet planets[16];
} StarSystem;

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
    
    // Cache
    StarSystem *visited_systems;
    int visited_count;
    int visited_capacity;
} GameState;

// Functions
unsigned int get_seed(int x, int y);
bool star_exists(int x, int y);
StarSystem generate_system(int x, int y);
float rand_float(float min, float max); 
void set_forced_star_count(int count);

#endif