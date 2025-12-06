
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
    sys.star.luminosity = powf(sys.star.mass, 3.5);
    sys.star.metallicity = rand_float(-0.5, 0.5);
    sys.star.temp = 5778 * powf(sys.star.mass, 0.5); 
    
    // Star Color (Approx)
    if (sys.star.temp > 10000) sys.star.color = 0x8888FFFF; // Blue
    else if (sys.star.temp > 6000) sys.star.color = 0xFFFFFFFF; // White
    else if (sys.star.temp > 4000) sys.star.color = 0xFFFF00FF; // Yellow
    else sys.star.color = 0xFF4400FF; // Red

    // Planets
    sys.planet_count = rand() % 10;
    float current_dist = rand_float(0.4, 1.0);
    
    // Frost line approx (where volatiles freeze)
    float frost_line = 2.7 * sqrtf(sys.star.luminosity); // Fixed from Python logic

    for (int i = 0; i < sys.planet_count; i++) {
        Planet *p = &sys.planets[i];
        
        current_dist *= rand_float(1.3, 1.8); // Titius-Bode spacing
        
        p->orbit_dist = current_dist;
        p->orbit_angle = rand_float(0, 6.28);
        p->orbit_speed = sqrtf(sys.star.mass / powf(p->orbit_dist, 3)); 
        
        // Determine Type & Mass
        bool is_outer = p->orbit_dist > frost_line * 0.8;
        
        if (is_outer && rand()%100 < 60) {
            // Giant
            p->mass = rand_float(10.0, 300.0);
            if (p->mass > 50.0) {
                p->type = TYPE_GAS_GIANT;
                p->radius = 11.0 * powf(p->mass/318.0, -0.1); // Degeneracy
                p->density = rand_float(0.7, 1.5);
                p->color = (rand()%2==0) ? 0xDDCCAAFF : 0xDDAA88FF; 
                sprintf(p->atmosphere, "H/He");
                p->albedo = 0.5;
                p->greenhouse = 20;
            } else {
                p->type = TYPE_ICE_GIANT;
                p->radius = 4.0 * powf(p->mass/17.0, 0.5);
                p->density = rand_float(1.0, 2.0);
                p->color = 0xAABBFFFF;
                sprintf(p->atmosphere, "H/He/CH4");
                p->albedo = 0.6;
                p->greenhouse = 10;
            }
        } else {
            // Rocky / Small
            p->mass = rand_float(0.1, 5.0);
            
            if (p->orbit_dist < 0.1 * sys.star.mass) {
                p->type = TYPE_MOLTEN;
                p->radius = powf(p->mass, 0.27);
                p->density = rand_float(4.5, 6.0);
                p->color = 0xFF4400FF;
                p->albedo = 0.1;
                sprintf(p->atmosphere, "Metal Vapor");
                p->greenhouse = 0;
            } else if (is_outer) {
                p->type = TYPE_ICY_WORLD;
                p->radius = powf(p->mass, 0.3); // Less dense
                p->density = rand_float(1.5, 2.5);
                p->color = 0xEEEEFFFF;
                p->albedo = 0.7;
                sprintf(p->atmosphere, "Thin N2");
                p->greenhouse = 5;
            } else if (p->orbit_dist > 0.8 * sqrtf(sys.star.luminosity) && p->orbit_dist < 1.5 * sqrtf(sys.star.luminosity) && p->mass > 0.3) {
                p->type = TYPE_TERRESTRIAL;
                p->radius = powf(p->mass, 0.27);
                p->density = rand_float(5.0, 5.8);
                p->color = 0x008800FF; // Green
                p->albedo = 0.3;
                sprintf(p->atmosphere, "N2/O2");
                p->greenhouse = 33;
            } else {
                p->type = TYPE_ROCKY;
                p->radius = powf(p->mass, 0.27);
                p->density = rand_float(3.5, 5.5);
                p->color = 0x888888FF;
                p->albedo = 0.2;
                sprintf(p->atmosphere, "Thin CO2");
                p->greenhouse = 5;
            }
        }
        
        // Gravity
        p->gravity = p->mass / powf(p->radius, 2);
        
        // Temperature Calculation
        // T = T_star * sqrt(R_star / 2D) * (1-A)^0.25
        // D in AU needs conversion to Star Radii units? 
        // Simplified: T ~ T_star * sqrt(R_star_sr / 2 * D_au * 215)
        float dist_sr = p->orbit_dist * 215.0; // AU to Solar Radii approx
        float t_eq = sys.star.temp * sqrtf(sys.star.radius / (2 * dist_sr));
        
        // Greenhouse refinement
        if (p->type == TYPE_ROCKY && p->mass > 0.5 && t_eq > 250 && t_eq < 400) {
             // Venus runaway chance
             if (rand()%2 == 0) {
                 sprintf(p->atmosphere, "Thick CO2");
                 p->greenhouse = 450;
                 p->albedo = 0.75;
             }
        }
        
        p->surface_temp = t_eq + p->greenhouse;
        
        // Tidal Locking
        p->is_tidally_locked = (p->orbit_dist < 0.4 * sys.star.mass);
        
        // Rotation Period (Day Length)
        float orbital_period_days = sqrtf(powf(p->orbit_dist, 3) / sys.star.mass) * 365.25;
        
        if (p->is_tidally_locked) {
            p->rotation_period = orbital_period_days;
        } else if (p->type == TYPE_GAS_GIANT || p->type == TYPE_ICE_GIANT) {
            p->rotation_period = rand_float(0.3, 0.6); // Fast spin (Jupiter ~0.4d)
        } else {
            // Rocky planets
            if (rand() % 10 < 8) {
                p->rotation_period = rand_float(0.5, 2.0); // Earth-like
            } else {
                p->rotation_period = rand_float(10.0, 100.0); // Slow rotator
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
