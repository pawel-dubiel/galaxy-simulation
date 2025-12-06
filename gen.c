
#include "galaxy.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

// Deterministic Hash for "Infinite" Galaxy
unsigned int get_seed(int x, int y) {
    unsigned int seed = (x * 73856093) ^ (y * 19349663);
    return seed;
}

bool star_exists(int x, int y) {
    unsigned int seed = get_seed(x, y);
    srand(seed);
    return (rand() % 100) < 5; // 5% chance
}

float rand_float(float min, float max) {
    return min + (float)rand() / (float)(RAND_MAX / (max - min));
}

StarSystem generate_system(int x, int y) {
    StarSystem sys;
    sys.x = x;
    sys.y = y;
    sys.seed = get_seed(x, y);
    srand(sys.seed);

    // Star
    sprintf(sys.star.name, "Star-%d-%d", x, y);
    sys.star.mass = rand_float(0.1, 5.0);
    sys.star.radius = powf(sys.star.mass, 0.8);
    sys.star.temp = 5778 * powf(sys.star.mass, 0.5); // Simplified
    
    // Star Color (Approx)
    if (sys.star.temp > 10000) sys.star.color = 0x8888FFFF; // Blue
    else if (sys.star.temp > 6000) sys.star.color = 0xFFFFFFFF; // White
    else if (sys.star.temp > 4000) sys.star.color = 0xFFFF00FF; // Yellow
    else sys.star.color = 0xFF4400FF; // Red

    // Planets
    sys.planet_count = rand() % 10;
    float current_dist = rand_float(0.4, 1.0);
    
    // Frost line approx (where volatiles freeze)
    float frost_line = 2.7 * powf(sys.star.mass, 2.0);

    for (int i = 0; i < sys.planet_count; i++) {
        Planet *p = &sys.planets[i];
        
        current_dist *= rand_float(1.3, 1.8); // Titius-Bode spacing
        
        p->orbit_dist = current_dist;
        p->orbit_angle = rand_float(0, 6.28);
        p->orbit_speed = sqrtf(sys.star.mass / powf(p->orbit_dist, 3)); 
        
        // Determine Type & Mass
        bool is_outer = p->orbit_dist > frost_line;
        
        if (is_outer && rand()%100 < 60) {
            // Giant
            p->mass = rand_float(10.0, 300.0);
            if (p->mass > 50.0) {
                p->type = TYPE_GAS_GIANT;
                p->radius = 10.0;
                p->color = (rand()%2==0) ? 0xDDCCAAFF : 0xDDAA88FF; 
            } else {
                p->type = TYPE_ICE_GIANT;
                p->radius = 7.0;
                p->color = 0xAABBFFFF;
            }
        } else {
            // Rocky / Small
            p->mass = rand_float(0.1, 5.0);
            p->radius = 3.0;
            
            if (p->orbit_dist < 0.3 * sys.star.mass) {
                p->type = TYPE_MOLTEN;
                p->color = 0xFF4400FF;
            } else if (is_outer) {
                p->type = TYPE_ICY_WORLD;
                p->color = 0xEEEEFFFF;
            } else if (p->orbit_dist > 0.8 * sys.star.mass && p->orbit_dist < 1.5 * sys.star.mass) {
                p->type = TYPE_TERRESTRIAL;
                p->color = 0x008800FF; // Green
            } else {
                p->type = TYPE_ROCKY;
                p->color = 0x888888FF;
            }
        }
        
        sprintf(p->name, "P-%c", 'A' + i);
        
        // Moons
        p->moon_count = 0;
        if (p->type != TYPE_MOLTEN) { // Too hot for moons?
            if (rand() % 100 < 30 || p->type == TYPE_GAS_GIANT) {
                p->moon_count = (p->type == TYPE_GAS_GIANT) ? (rand() % 4 + 1) : 1;
                for (int m=0; m<p->moon_count; m++) {
                    Moon *mn = &p->moons[m];
                    // Reduced distance: 1.3x to 2.0x planet radius (was 2.0x to 3.5x)
                    mn->orbit_dist = p->radius * (1.3 + rand_float(0.2, 0.7)); 
                    mn->radius = p->radius * 0.25;
                    mn->orbit_angle = rand_float(0, 6.28);
                    mn->orbit_speed = rand_float(1.0, 3.0);
                    sprintf(mn->name, "M-%d", m+1);
                }
            }
        }
    }

    return sys;
}
