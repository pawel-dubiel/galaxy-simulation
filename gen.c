
#include "galaxy.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

unsigned int get_seed(int x, int y) {
    unsigned int seed = (x * 73856093) ^ (y * 19349663);
    return seed;
}

bool star_exists(int x, int y) {
    unsigned int seed = get_seed(x, y);
    srand(seed);
    return (rand() % 100) < 5; 
}

float rand_float(float min, float max) {
    return min + (float)rand() / (float)(RAND_MAX / (max - min));
}

static int g_forced_star_count = 0;
void set_forced_star_count(int count) {
    g_forced_star_count = count;
}

void generate_resources(Planet *p, float metallicity) {
    for(int i=0; i<8; i++) p->resources[i] = 0.0f;
    float metal_boost = powf(10.0, metallicity);
    float weights[8] = {0};
    
    if (p->type == TYPE_GAS_GIANT) {
        weights[4] = 75.0; weights[5] = 24.0; weights[3] = 1.0 * metal_boost; 
    } else if (p->type == TYPE_ICE_GIANT) {
        weights[3] = 50.0; weights[4] = 15.0; weights[5] = 5.0; weights[1] = 5.0; 
    } else if (p->type == TYPE_MOLTEN) {
        weights[0] = 40.0; weights[1] = 30.0; weights[2] = 10.0; weights[6] = 0.1 * metal_boost; 
    } else if (p->type == TYPE_TERRESTRIAL || p->type == TYPE_ROCKY) {
        weights[1] = 30.0; weights[0] = 25.0; weights[3] = (p->type == TYPE_TERRESTRIAL) ? 5.0 : 0.1;
        weights[2] = 5.0; weights[7] = 0.05 * metal_boost; weights[6] = 0.01 * metal_boost; 
    } else { 
        weights[3] = 70.0; weights[1] = 10.0; 
    }
    
    float total = 0;
    for(int i=0; i<8; i++) {
        if (weights[i] > 0) weights[i] *= rand_float(0.8, 1.2);
        total += weights[i];
    }
    if (total > 0) for(int i=0; i<8; i++) p->resources[i] = (weights[i] / total) * 100.0;
}

StarSystem generate_system(int x, int y) {
    StarSystem sys;
    sys.x = x; sys.y = y;
    sys.seed = get_seed(x, y);
    srand(sys.seed);

    if (g_forced_star_count > 0) sys.star_count = g_forced_star_count;
    else {
        int r = rand() % 100;
        sys.star_count = (r < 60) ? 1 : ((r < 90) ? 2 : 3);
    }

    float total_mass = 0;
    float total_lum = 0;

    for (int i = 0; i < sys.star_count; i++) {
        Star *s = &sys.stars[i];
        sprintf(s->name, "Star-%c", 'A' + i);
        s->mass = rand_float(0.1, 5.0);
        if (i > 0) s->mass *= 0.5; 
        s->radius = powf(s->mass, 0.8);
        s->luminosity = powf(s->mass, 3.5);
        s->metallicity = rand_float(-0.5, 0.5);
        s->temp = 5778 * powf(s->mass, 0.5);
        if (s->temp > 10000) s->color = 0x8888FFFF; 
        else if (s->temp > 6000) s->color = 0xFFFFFFFF;
        else if (s->temp > 4000) s->color = 0xFFFF00FF;
        else s->color = 0xFF4400FF;
        total_mass += s->mass;
        total_lum += s->luminosity;
    }

    if (sys.star_count == 1) {
        sys.stars[0].pos = (Vec3){0,0,0};
        sys.stars[0].vel = (Vec3){0,0,0};
    } else if (sys.star_count == 2) {
        Star *s1 = &sys.stars[0]; Star *s2 = &sys.stars[1];
        float dist = 0.2; 
        float v_circ = sqrtf(39.478 * (s1->mass + s2->mass) / dist);
        float r1 = dist * s2->mass / (s1->mass + s2->mass);
        float r2 = dist * s1->mass / (s1->mass + s2->mass);
        s1->pos = (Vec3){-r1,0,0}; s1->vel = (Vec3){0, v_circ * (r1 / dist), 0};
        s2->pos = (Vec3){r2,0,0}; s2->vel = (Vec3){0, -v_circ * (r2 / dist), 0};
    } else {
        float dist = 0.3;
        float v = sqrtf(39.478 * total_mass / dist) * 0.5; 
        sys.stars[0].pos = (Vec3){dist,0,0}; sys.stars[0].vel = (Vec3){0,v,0};
        sys.stars[1].pos = (Vec3){-dist*0.5, dist*0.866,0}; sys.stars[1].vel = (Vec3){-v*0.866, -v*0.5,0};
        sys.stars[2].pos = (Vec3){-dist*0.5,-dist*0.866,0}; sys.stars[2].vel = (Vec3){v*0.866, -v*0.5,0};
    }

    sys.planet_count = rand() % 10;
    float current_dist = 0.5 + sys.star_count * 0.2; 
    float frost_line = 2.7 * sqrtf(total_lum); 

    for (int i = 0; i < sys.planet_count; i++) {
        Planet *p = &sys.planets[i];
        current_dist *= rand_float(1.3, 1.8);
        
        float angle = rand_float(0, 6.28);
        p->pos = (Vec3){cos(angle) * current_dist, sin(angle) * current_dist, 0};
        
        float v = sqrtf(39.478 * total_mass / current_dist);
        p->vel = (Vec3){-sin(angle) * v, cos(angle) * v, 0};
        p->rotation_angle = rand_float(0, 6.28);
        
        bool is_outer = current_dist > frost_line * 0.8;
        if (is_outer && rand()%100 < 60) {
            p->mass = rand_float(10.0, 300.0);
            if (p->mass > 50.0) {
                p->type = TYPE_GAS_GIANT; p->radius = 11.0; p->color = (rand()%2==0) ? 0xDDCCAAFF : 0xDDAA88FF; sprintf(p->atmosphere, "H/He");
            } else {
                p->type = TYPE_ICE_GIANT; p->radius = 4.0; p->color = 0xAABBFFFF; sprintf(p->atmosphere, "H/He/CH4");
            }
        } else {
            p->mass = rand_float(0.1, 5.0);
            if (current_dist < 0.1 * total_mass) {
                p->type = TYPE_MOLTEN; p->radius = powf(p->mass, 0.27); p->color = 0xFF4400FF; sprintf(p->atmosphere, "Metal Vapor");
            } else if (is_outer) {
                p->type = TYPE_ICY_WORLD; p->radius = powf(p->mass, 0.3); p->color = 0xEEEEFFFF; sprintf(p->atmosphere, "Thin N2");
            } else if (current_dist > 0.8 * sqrtf(total_lum) && current_dist < 1.5 * sqrtf(total_lum) && p->mass > 0.3) {
                p->type = TYPE_TERRESTRIAL; p->radius = powf(p->mass, 0.27); p->color = 0x008800FF; sprintf(p->atmosphere, "N2/O2");
            } else {
                p->type = TYPE_ROCKY; p->radius = powf(p->mass, 0.27); p->color = 0x888888FF; sprintf(p->atmosphere, "Thin CO2");
            }
        }
        
        generate_resources(p, sys.stars[0].metallicity);
        
        float w_sum = 0, vol_sum = 0; 
        float dens_map[8] = {7.8, 3.3, 8.9, 1.0, 0.1, 0.15, 19.3, 6.0};
        for (int r=0; r<8; r++) {
            float pct = p->resources[r];
            if (pct > 0) { w_sum += pct; vol_sum += pct / dens_map[r]; }
        }
        float uncompressed_density = (vol_sum > 0) ? (w_sum / vol_sum) : 3.0;
        
        if (p->type == TYPE_GAS_GIANT || p->type == TYPE_ICE_GIANT) {
            if (p->mass > 100) p->density = 1.33 * powf(p->mass/318.0, 0.3); 
            else p->density = 0.7 * powf(p->mass/95.0, 0.1);
        } else {
            float compression = 1.0 + 0.4 * log10f(MAX(0.1, p->mass)); 
            p->density = uncompressed_density * compression;
        }
        p->radius = powf(p->mass / (p->density / 5.51), 0.333);
        
        p->gravity = p->mass / powf(p->radius, 2);
        p->surface_temp = sys.stars[0].temp * sqrtf(sys.stars[0].radius / (2 * current_dist * 215));
        p->is_tidally_locked = (current_dist < 0.4 * total_mass);
        
        if (p->is_tidally_locked) {
            p->rotation_period = sqrtf(powf(current_dist, 3) / total_mass) * 365.25;
        } else if (p->type == TYPE_GAS_GIANT || p->type == TYPE_ICE_GIANT) {
            p->rotation_period = rand_float(0.3, 0.6); 
        } else {
            if (rand() % 10 < 8) {
                p->rotation_period = rand_float(0.5, 2.0); 
            } else {
                p->rotation_period = rand_float(10.0, 243.0); 
            }
        }
        
        sprintf(p->name, "P-%c", 'A' + i);
        
        p->moon_count = (p->type == TYPE_GAS_GIANT) ? (rand() % 4 + 1) : (rand()%100 < 30);
        for (int m=0; m<p->moon_count; m++) {
            Moon *mn = &p->moons[m];
            mn->orbit_dist = p->radius * (1.5 + rand_float(0.5, 2.0));
            mn->radius = p->radius * 0.25;
            mn->pos = (Vec3){0,0,0}; 
            mn->vel = (Vec3){0,0,0}; 
            sprintf(mn->name, "M-%d", m+1);
        }
    }

    return sys;
}
