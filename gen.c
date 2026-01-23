
#include "galaxy.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

unsigned int get_seed_from_id(int star_id) {
    // Use a good hash to generate reproducible seed from star_id
    unsigned int seed = (star_id * 2654435761u) ^ (star_id * 73856093);
    return seed;
}

float rand_float(float min, float max) {
    return min + (float)rand() / (float)(RAND_MAX / (max - min));
}

static int g_forced_star_count = 0;
void set_forced_star_count(int count) {
    g_forced_star_count = count;
}

// Trig Lookup Tables for fast galaxy rotation
float g_sin_table[TRIG_TABLE_SIZE];
float g_cos_table[TRIG_TABLE_SIZE];

void init_trig_tables(void) {
    for (int i = 0; i < TRIG_TABLE_SIZE; i++) {
        float angle = (float)i / TRIG_TABLE_SIZE * 6.28318530718f;
        g_sin_table[i] = sinf(angle);
        g_cos_table[i] = cosf(angle);
    }
}

float fast_sin(float angle) {
    // Normalize to [0, 2π)
    const float TWO_PI = 6.28318530718f;
    angle = fmodf(angle, TWO_PI);
    if (angle < 0) angle += TWO_PI;
    int idx = (int)(angle / TWO_PI * TRIG_TABLE_SIZE) % TRIG_TABLE_SIZE;
    return g_sin_table[idx];
}

float fast_cos(float angle) {
    const float TWO_PI = 6.28318530718f;
    angle = fmodf(angle, TWO_PI);
    if (angle < 0) angle += TWO_PI;
    int idx = (int)(angle / TWO_PI * TRIG_TABLE_SIZE) % TRIG_TABLE_SIZE;
    return g_cos_table[idx];
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

StarSystem generate_system(int star_id) {
    StarSystem sys;
    sys.star_id = star_id;
    sys.seed = get_seed_from_id(star_id);
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
        
        p->orbit_radius = current_dist;
        p->orbit_angle = angle;
        p->orbit_speed = v / current_dist; // Angular velocity for Keplerian orbit
        p->eccentricity = rand_float(0.00, 0.20); 
        p->inclination = rand_float(-0.15, 0.15);
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

// Initialize a realistic Milky Way galaxy
void init_galaxy(Galaxy *galaxy, int star_count) {
    galaxy->star_count = star_count;
    galaxy->stars = (GalaxyStar*)malloc(sizeof(GalaxyStar) * star_count);
    galaxy->time = 0.0f;
    
    // Milky Way parameters (accurate to current understanding)
    const float GALAXY_RADIUS = 50.0f;      // ~50 kLY radius
    const float BAR_LENGTH = 13.0f;         // Central bar ~13 kLY half-length
    const float BAR_WIDTH = 2.5f;           // Bar width
    const float BULGE_RADIUS = 2.0f;        // Nuclear bulge ~2 kLY
    const float DISK_THICKNESS = 1.0f;      // Thin disk ~1 kLY (exaggerated for visibility)
    const float SUN_DISTANCE = 26.0f;       // Sun at ~26 kLY from center
    const float V_FLAT = 220.0f;            // Rotation velocity km/s
    const float BAR_ANGLE = 0.45f;          // Angle of the bar
    
    // Spiral arm parameters (4 main arms)
    // Arms start from ends of the bar
    const float ARM_TIGHTNESS = 0.22f;      // Pitch angle ~12 degrees
    
    srand(42);
    
    // Distribution:
    // 15% Bulge (Dense, Rugby Ball)
    // 10% Bar (Elongated)
    // 40% Spiral Arms (Thin, Young)
    // 15% Thin Disk (Background)
    // 10% Thick Disk (Older, thicker)
    // 10% Halo (Split 7% Inner, 3% Outer)
    
    int bulge_count = star_count * 15 / 100;
    int bar_count = star_count * 10 / 100;
    int arm_count = star_count * 40 / 100;
    int thin_disk_count = star_count * 15 / 100;
    int thick_disk_count = star_count * 10 / 100;
    // Remainder is halo
    
    int idx = 0;
    
    // ===== NUCLEAR BULGE (Prolate / Rugby Ball Shape) =====
    // Structurally aligned with the bar, but thicker
    for (int i = 0; i < bulge_count && idx < star_count; i++, idx++) {
        GalaxyStar *s = &galaxy->stars[idx];
        
        // Generate in local aligned coordinates
        // We want a prolate spheroid: Long axis X (Bar), Short axis Y/Z
        
        float u = rand_float(0.001, 1.0);
        float d_base = -logf(u) * 0.4f; // Exponential density falloff
        if (d_base > 2.0) d_base = rand_float(0.1, 2.0); // Clamp tails
        
        // Random direction on sphere
        float theta_rand = rand_float(0, 6.2831853);
        float phi_rand = acosf(rand_float(-1, 1));
        
        // Unit sphere coordinates
        float ux = sinf(phi_rand) * cosf(theta_rand);
        float uy = sinf(phi_rand) * sinf(theta_rand);
        float uz = cosf(phi_rand);
        
        // Stretch to creating rugby ball shape
        // Elongate along bar axis (X in local frame)
        float lx = ux * d_base * BULGE_RADIUS * 2.5f; // Longer
        float ly = uy * d_base * BULGE_RADIUS * 1.2f; // Wider than sphere
        float lz = uz * d_base * BULGE_RADIUS * 1.0f; // Tall
        
        // Rotate by BAR_ANGLE to world space
        float x = lx * cosf(BAR_ANGLE) - ly * sinf(BAR_ANGLE);
        float y = lx * sinf(BAR_ANGLE) + ly * cosf(BAR_ANGLE);
        
        float r = sqrtf(x*x + y*y);
        float theta = atan2f(y, x);
        
        s->orbit_radius = r;
        s->orbit_angle = theta;
        s->z_offset = lz;
        
        s->orbit_speed = V_FLAT * (r / BULGE_RADIUS) * 0.5 / (r * 3.086e16) * 3.154e7;
        
        // BULGE: Old, metal-rich giants. High surface brightness.
        // Extremely bright core, yellow-white.
        // Brightness decays with distance from center (3D)
        float dist_3d = sqrtf(lx*lx + ly*ly + lz*lz);
        int base_bright = 230 + rand() % 25;
        float r_factor = 1.0f - (dist_3d / (BULGE_RADIUS * 2.0f)) * 0.5f; 
        if (r_factor < 0.2) r_factor = 0.2;
        int final_bright = (int)(base_bright * r_factor);
        if (final_bright > 255) final_bright = 255;
        
        // Color: Warm Yellow/White (4000K - 5000K)
        // R=255, G=200-240, B=100-180
        s->color = (final_bright << 24) | ((final_bright - 20) << 16) | ((final_bright - 80) << 8) | 0xFF;
        
        s->star_id = idx;
        s->seed = get_seed_from_id(s->star_id);
    }
    
    // ===== CENTRAL BAR (Old Disk/Bulge mix, elongated) =====
    for (int i = 0; i < bar_count && idx < star_count; i++, idx++) {
        GalaxyStar *s = &galaxy->stars[idx];
        
        // float bar_angle = 0.45f; // Use constant
        float u = rand_float(0.001, 1.0);
        float bar_x = -BAR_LENGTH * 0.7f * logf(u) * (rand() % 2 == 0 ? 1 : -1);
        if (fabsf(bar_x) > BAR_LENGTH) bar_x = rand_float(-BAR_LENGTH, BAR_LENGTH) * 0.8f;
        
        float width_scale = 1.0f - powf(fabsf(bar_x) / BAR_LENGTH, 2.0f) * 0.7f;
        float bar_y = rand_float(-1.0, 1.0) * BAR_WIDTH * width_scale;
        
        bar_x += rand_float(-0.5, 0.5);
        bar_y += rand_float(-0.3, 0.3);
        
        float x = bar_x * cosf(BAR_ANGLE) - bar_y * sinf(BAR_ANGLE);
        float y = bar_x * sinf(BAR_ANGLE) + bar_y * cosf(BAR_ANGLE);
        
        float r = sqrtf(x*x + y*y);
        float theta = atan2f(y, x);
        if (r < 0.1) r = 0.1;
        
        s->orbit_radius = r;
        s->orbit_angle = theta;
        
        // Thicken the bar vertically to blend with the rugby-ball bulge
        // Peanut shape: thicker at ends? Or just thick general?
        // Let's make it fairly thick to match the "squashed sphere/rugby" vibe
        s->z_offset = rand_float(-0.8, 0.8) * width_scale;
        
        float pattern_speed = V_FLAT * 0.4 / (BAR_LENGTH * 3.086e16) * 3.154e7;
        s->orbit_speed = pattern_speed + (V_FLAT / (r * 3.086e16) * 3.154e7 - pattern_speed) * 0.3;
        
        // BAR: Similar to bulge but transitioning to disk.
        // Good brightness, warm colors.
        int brightness = 200 + rand() % 55;
        // slightly less yellow than bulge
        s->color = (brightness << 24) | ((brightness - 30) << 16) | ((brightness - 60) << 8) | 0xFF;
        
        s->star_id = idx;
        s->seed = get_seed_from_id(s->star_id);
    }
    
    // ===== SPIRAL ARMS (Young Population I, star formation) =====
    const char* arm_names[] = {"Norma", "Scutum", "Perseus", "Carina"};
    (void)arm_names;
    
    for (int i = 0; i < arm_count && idx < star_count; i++, idx++) {
        GalaxyStar *s = &galaxy->stars[idx];
        
        int arm = rand() % 4;
        // float bar_angle = 0.45f; // Use constant
        float arm_start;
        if (arm == 0) arm_start = BAR_ANGLE;
        else if (arm == 1) arm_start = BAR_ANGLE + 1.5707963f;
        else if (arm == 2) arm_start = BAR_ANGLE + 3.14159f;
        else arm_start = BAR_ANGLE + 4.712389f;
        
        float min_r = BAR_LENGTH * 0.5f;
        float r = min_r + powf(rand_float(0, 1), 0.5) * (GALAXY_RADIUS - min_r);
        
        float spiral_theta = logf(r / min_r) / ARM_TIGHTNESS + arm_start;
        
        float arm_width = 0.3f + (r / GALAXY_RADIUS) * 0.5f;
        float arm_scatter = rand_float(-arm_width, arm_width);
        
        s->orbit_radius = r;
        s->orbit_angle = spiral_theta + arm_scatter;
        s->z_offset = rand_float(-DISK_THICKNESS/2, DISK_THICKNESS/2);
        
        s->orbit_speed = V_FLAT / (r * 3.086e16) * 3.154e7;
        
        // ARMS: Young, hot stars (O/B associations).
        // Some VERY bright blue/white stars, others average.
        // We want accurate "clumping" of brightness if possible, but random is okay for now.
        
        if (rand() % 20 == 0) {
            // Supergiant / O-Star Cluster (Very Bright, Blue)
            int b = 240 + rand() % 15;
            s->color = ((b - 30) << 24) | ((b - 10) << 16) | (b << 8) | 0xFF;
        } else if (rand() % 5 == 0) {
           // Bright Main Sequence (A/F stars, White)
            int b = 220 + rand() % 35;
            s->color = (b << 24) | (b << 16) | (b << 8) | 0xFF; // Pure white
        } else {
            // Background arm population (B-F stars, Blue-White)
            int b = 180 + rand() % 50;
            s->color = ((b - 40) << 24) | ((b - 10) << 16) | (b << 8) | 0xFF;
        }
        
        s->star_id = idx;
        s->seed = get_seed_from_id(s->star_id);
    }
    
    // ===== THIN DISK (Background Population I) =====
    for (int i = 0; i < thin_disk_count && idx < star_count; i++, idx++) {
        GalaxyStar *s = &galaxy->stars[idx];
        
        // Exponential disk
        float u = rand_float(0.01, 1.0);
        float r = -15.0f * logf(u);
        if (r > GALAXY_RADIUS) r = rand_float(BAR_LENGTH, GALAXY_RADIUS);
        if (r < BAR_LENGTH) r = rand_float(BAR_LENGTH, GALAXY_RADIUS * 0.8);
        
        float theta = rand_float(0, 6.28318530718);
        
        s->orbit_radius = r;
        s->orbit_angle = theta;
        s->z_offset = rand_float(-DISK_THICKNESS/2, DISK_THICKNESS/2);
        
        s->orbit_speed = V_FLAT / (r * 3.086e16) * 3.154e7;
        
        // DISK: Dimmer background stars.
        // Apply radial brightness gradient (brighter near center)
        float radial_boost = 1.0f + (1.0f - (r / GALAXY_RADIUS)) * 0.5f;
        
        int base = 100 + rand() % 100;
        int brightness = (int)(base * radial_boost);
        if (brightness > 255) brightness = 255;
        
        // Slightly yellow/orange (Sun-like)
        s->color = (brightness << 24) | ((brightness - 20) << 16) | ((brightness - 40) << 8) | 0xFF;
        
        s->star_id = idx;
        s->seed = get_seed_from_id(s->star_id);
    }
    
    // ===== THICK DISK (Old Population II, Thicker, Dimmer) =====
    for (int i = 0; i < thick_disk_count && idx < star_count; i++, idx++) {
        GalaxyStar *s = &galaxy->stars[idx];
        
        // Similar radial distribution to Thin Disk, maybe slightly more enhancing the center
        float u = rand_float(0.01, 1.0);
        float r = -12.0f * logf(u); // Scale length slightly shorter? Or similar.
        if (r > GALAXY_RADIUS) r = rand_float(BAR_LENGTH, GALAXY_RADIUS);
        if (r < BAR_LENGTH) r = rand_float(BAR_LENGTH, GALAXY_RADIUS * 0.8);
        
        float theta = rand_float(0, 6.28318530718);
        
        s->orbit_radius = r;
        s->orbit_angle = theta;
        
        // Much thicker! 1kLY -> 3-4kLY scale
        s->z_offset = rand_float(-3.0, 3.0) * powf(rand_float(0, 1), 0.7);
        
        s->orbit_speed = V_FLAT / (r * 3.086e16) * 3.154e7 * 0.9; // Slightly slower lag
        
        // Dimmer, Orange/Red
        int base = 80 + rand() % 80;
        // Radial gradient
        float radial_boost = 1.0f + (1.0f - (r / GALAXY_RADIUS)) * 0.4f;
         int brightness = (int)(base * radial_boost);
        if (brightness > 255) brightness = 255;
        
        // Stronger Orange/Red tint for old population
        s->color = (brightness << 24) | ((brightness - 30) << 16) | ((brightness - 60) << 8) | 0xFF;
        
        s->star_id = idx;
        s->seed = get_seed_from_id(s->star_id);
    }
    
    // ===== HALO (Inner vs Outer) =====
    // Remaining stars go here
    for (; idx < star_count; idx++) {
        GalaxyStar *s = &galaxy->stars[idx];
        
        // Split Inner/Outer probability
        bool is_inner = (rand() % 100 < 70); 
        
        float r, z;
        if (is_inner) {
             // Inner Halo: 5 - 40 kLY, somewhat flattened
             r = rand_float(5.0, 40.0);
             float z_scale = rand_float(5.0, 15.0);
             z = rand_float(-1.0, 1.0) * z_scale;
        } else {
             // Outer Halo: Very distant, spherical
             r = rand_float(20.0, 80.0); // Up to 80kLY
             float theta_sph = rand_float(0, 6.28);
             float phi_sph = acosf(rand_float(-1, 1));
             z = r * cosf(phi_sph); // Convert to Z
             r = r * sinf(phi_sph); // Convert to projected R
        }

        float theta = rand_float(0, 6.28318530718);
        
        s->orbit_radius = r;
        s->orbit_angle = theta;
        s->z_offset = z;
        
        s->orbit_speed = V_FLAT / (MAX(r, 0.1) * 3.086e16) * 3.154e7 * rand_float(0.5, 1.2);
        
        // Very Dim
        int brightness = 50 + rand() % 50; 
        
        // Reddish
        s->color = (brightness << 24) | ((brightness - 10) << 16) | ((brightness - 40) << 8) | 0xFF;
        
        s->star_id = idx;
        s->seed = get_seed_from_id(s->star_id);
    }
}
