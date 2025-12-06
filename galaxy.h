
#ifndef GALAXY_H
#define GALAXY_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 900

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
    Uint32 color; // 0xRRGGBBAA
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
    float orbit_dist; // Relative to planet
    float orbit_angle;
    float orbit_speed;
} Moon;

typedef struct {
    char name[32];
    float mass;
    float radius;
    float orbit_dist; // AU
    float orbit_angle; // Radians
    float orbit_speed; // Rad/sec
    Uint32 color;
    PlanetType type;
    int moon_count;
    Moon moons[4]; // Max 4 moons for simplicity in C
} Planet;

typedef struct {
    int x, y; // Galaxy Coordinates
    unsigned int seed;
    Star star;
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
} GameState;

// Functions
unsigned int get_seed(int x, int y);
bool star_exists(int x, int y);
StarSystem generate_system(int x, int y);
float rand_float(float min, float max); // Shared helper

#endif
