
#ifndef GALAXY_H
#define GALAXY_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 900
#define MAX(x, y) (((x) > (y)) ? (x) : (y)) // Shared macro

// Math
typedef struct {
    float x, y;
} Vec2;

// Data Structures
typedef struct {
    char name[32];
    float mass;
    float radius;
    float temp;
    float luminosity; 
    float metallicity; 
    Uint32 color;
    // Physics
    float x, y;   // AU
    float vx, vy; // AU/year
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
    float orbit_dist; // Relative visual distance
    float x, y;   // Angle/Speed in simulation or actual coords
    float vx, vy;
} Moon;

typedef struct {
    char name[32];
    float mass;
    float radius;
    Uint32 color;
    PlanetType type;
    
    // Physics
    float x, y;   // AU
    float vx, vy; // AU/year
    
    // Details
    float density; 
    float surface_temp; 
    char atmosphere[32];
    float albedo;
    float greenhouse; 
    bool is_tidally_locked;
    float gravity; 
    float rotation_period; 
    float rotation_angle; // Current axial rotation
    
    // Resources (Percentages 0.0 - 100.0)
    // 0:Iron, 1:Silicon, 2:Nickel, 3:Water, 4:Hydrogen, 5:Helium, 6:Gold, 7:RareEarths
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

// Game State
typedef enum {
    VIEW_GALAXY,
    VIEW_SYSTEM
} ViewMode;

typedef struct {
    ViewMode mode;
    Vec2 camera_pos; // Current camera position (System or Galaxy)
    Vec2 galaxy_camera_pos; // Saved galaxy position
    float zoom;
    float galaxy_zoom; // Saved galaxy zoom
    StarSystem current_system;
    bool running;
    bool debug;
    bool paused;
    float time_speed;
    int selected_planet_idx; // -1 for none
    bool selected_star;
    int centered_planet_idx; // -1 for none (Star centered)
    Vec2 system_center_offset; // Offset to center view on a body
    
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
void set_forced_star_count(int count); // New

#endif
