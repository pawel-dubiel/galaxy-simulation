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

    // 1. Generate Stars
    int r = rand() % 100;
    if (r < 60) sys.star_count = 1;
    else if (r < 90) sys.star_count = 2;
    else sys.star_count = 3;

    float total_mass = 0;
    float total_lum = 0;

    for (int i = 0; i < sys.star_count; i++) {
        Star *s = &sys.stars[i];
        sprintf(s->name, "Star-%c", 'A' + i);
        
        s->mass = rand_float(0.1, 5.0);
        if (i > 0) s->mass *= 0.5; // Companions smaller
        
        s->radius = powf(s->mass, 0.8);
        s->luminosity = powf(s->mass, 3.5);
        s->metallicity = rand_float(-0.5, 0.5);
        s->temp = 5778 * powf(s->mass, 0.5);
        
        // Color
        if (s->temp > 10000) s->color = 0x8888FFFF; 
        else if (s->temp > 6000) s->color = 0xFFFFFFFF;
        else if (s->temp > 4000) s->color = 0xFFFF00FF;
        else s->color = 0xFF4400FF;
        
        total_mass += s->mass;
        total_lum += s->luminosity;
    }

    // Position Stars (N-Body Setup)
    if (sys.star_count == 1) {
        sys.stars[0].x = 0; sys.stars[0].y = 0;
        sys.stars[0].vx = 0; sys.stars[0].vy = 0;
    } else if (sys.star_count == 2) {
        // Binary: Orbiting CoM
        Star *s1 = &sys.stars[0];
        Star *s2 = &sys.stars[1];
        float dist = 0.2; // 0.2 AU separation
        float v_circ = sqrtf(39.478 * (s1->mass + s2->mass) / dist); // G in AU^3/yr^2/Msol approx 4pi^2
        
        float r1 = dist * s2->mass / (s1->mass + s2->mass);
        float r2 = dist * s1->mass / (s1->mass + s2->mass);
        
        s1->x = -r1; s1->y = 0;
        s1->vx = 0; s1->vy = v_circ * (r1 / dist); // Velocity proportional to radius
        
        s2->x = r2; s2->y = 0;
        s2->vx = 0; s2->vy = -v_circ * (r2 / dist);
    } else {
        // Trinary: Binary + Distant Third
        // Simplified: Triangle rotating? Or chaotic.
        // Let's do stable Triangle for simplicity/visuals
        float dist = 0.3;
        float v = sqrtf(39.478 * total_mass / dist) * 0.5; // Rough approx
        
        sys.stars[0].x = dist; sys.stars[0].y = 0; 
        sys.stars[0].vx = 0; sys.stars[0].vy = v;
        
        sys.stars[1].x = -dist*0.5; sys.stars[1].y = dist*0.866;
        sys.stars[1].vx = -v*0.866; sys.stars[1].vy = -v*0.5;
        
        sys.stars[2].x = -dist*0.5; sys.stars[2].y = -dist*0.866;
        sys.stars[2].vx = v*0.866; sys.stars[2].vy = -v*0.5;
    }

    // 2. Generate Planets
    sys.planet_count = rand() % 10;
    float current_dist = 0.5 + sys.star_count * 0.2; // Push out for multi-star
    float frost_line = 2.7 * sqrtf(total_lum); 

    for (int i = 0; i < sys.planet_count; i++) {
        Planet *p = &sys.planets[i];
        
        current_dist *= rand_float(1.3, 1.8);
        
        // Physics Init (Pos/Vel)
        float angle = rand_float(0, 6.28);
        p->x = cos(angle) * current_dist;
        p->y = sin(angle) * current_dist;
        
        // Circular velocity: v = sqrt(GM/r)
        // Using CoM mass (total_mass)
        float v = sqrtf(39.478 * total_mass / current_dist);
        p->vx = -sin(angle) * v;
        p->vy = cos(angle) * v;
        
        // Details logic (simplified copy from before)
        bool is_outer = current_dist > frost_line * 0.8;
        if (is_outer && rand()%100 < 60) {
            p->mass = rand_float(10.0, 300.0);
            if (p->mass > 50.0) {
                p->type = TYPE_GAS_GIANT;
                p->radius = 11.0;
                p->density = 1.0;
                p->color = (rand()%2==0) ? 0xDDCCAAFF : 0xDDAA88FF; 
                sprintf(p->atmosphere, "H/He");
            } else {
                p->type = TYPE_ICE_GIANT;
                p->radius = 4.0;
                p->density = 1.5;
                p->color = 0xAABBFFFF;
                sprintf(p->atmosphere, "H/He/CH4");
            }
        } else {
            p->mass = rand_float(0.1, 5.0);
            if (current_dist < 0.1 * total_mass) {
                p->type = TYPE_MOLTEN;
                p->radius = powf(p->mass, 0.27);
                p->density = 5.0;
                p->color = 0xFF4400FF;
                sprintf(p->atmosphere, "Metal Vapor");
            } else if (is_outer) {
                p->type = TYPE_ICY_WORLD;
                p->radius = powf(p->mass, 0.3);
                p->density = 2.0;
                p->color = 0xEEEEFFFF;
                sprintf(p->atmosphere, "Thin N2");
            } else if (current_dist > 0.8 * sqrtf(total_lum) && current_dist < 1.5 * sqrtf(total_lum) && p->mass > 0.3) {
                p->type = TYPE_TERRESTRIAL;
                p->radius = powf(p->mass, 0.27);
                p->density = 5.5;
                p->color = 0x008800FF; 
                sprintf(p->atmosphere, "N2/O2");
            } else {
                p->type = TYPE_ROCKY;
                p->radius = powf(p->mass, 0.27);
                p->density = 4.0;
                p->color = 0x888888FF;
                sprintf(p->atmosphere, "Thin CO2");
            }
        }
        
        p->surface_temp = sys.stars[0].temp * sqrtf(sys.stars[0].radius / (2 * current_dist * 215)); // approx
        p->gravity = p->mass / powf(p->radius, 2);
        p->is_tidally_locked = (current_dist < 0.4 * total_mass);
        p->rotation_angle = rand_float(0, 6.28);
        
        // Rotation Period Calculation
        if (p->is_tidally_locked) {
            // Locked: Rotation = Orbit Period
            // T = sqrt(a^3 / M) * 365.25
            p->rotation_period = sqrtf(powf(current_dist, 3) / total_mass) * 365.25;
        } else if (p->type == TYPE_GAS_GIANT || p->type == TYPE_ICE_GIANT) {
            // Giants spin fast (Jupiter ~0.41 days)
            p->rotation_period = rand_float(0.3, 0.6); 
        } else {
            // Rocky Planets
            if (rand() % 10 < 8) {
                p->rotation_period = rand_float(0.5, 2.0); // Earth-like
            } else {
                p->rotation_period = rand_float(10.0, 243.0); // Slow rotator (Venus-like)
            }
        }
        
        sprintf(p->name, "P-%c", 'A' + i);
        
        // Moons (Simplified Visuals Only for N-Body)
        // We simulate them relative to planet visually in main loop for simplicity, 
        // or we could add them as N-bodies but that might be unstable with this timestep.
        // Let's stick to "visual orbit around planet position".
        p->moon_count = (p->type == TYPE_GAS_GIANT) ? (rand() % 4 + 1) : (rand()%100 < 30);
        for (int m=0; m<p->moon_count; m++) {
            Moon *mn = &p->moons[m];
            mn->orbit_dist = p->radius * (1.5 + rand_float(0.5, 2.0));
            mn->radius = p->radius * 0.25;
            // Using x,y for angle/speed relative storage to match struct
            mn->x = rand_float(0, 6.28); // angle
            mn->vx = rand_float(1.0, 3.0); // speed
            sprintf(mn->name, "M-%d", m+1);
        }
    }

    return sys;
}