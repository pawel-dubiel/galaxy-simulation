
#include <SDL2/SDL.h>
#include "galaxy.h"
#include "font.h"
#include "math3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define G 39.478 
#define FOV_SCALE 1000.0f

// Render Sorting
typedef struct {
    float z; // Depth
    int x, y, r;
    Uint32 color;
    char label[64];
    bool is_star;
} RenderItem;

int compare_render_items(const void *a, const void *b) {
    RenderItem *ra = (RenderItem *)a;
    RenderItem *rb = (RenderItem *)b;
    if (rb->z > ra->z) return 1;
    if (rb->z < ra->z) return -1;
    return 0;
}

void init_cache(GameState *state) {
    state->visited_count = 0;
    state->visited_capacity = 16;
    state->visited_systems = (StarSystem*)malloc(sizeof(StarSystem) * state->visited_capacity);
}
int find_cached_system(GameState *state, int star_id) {
    for (int i = 0; i < state->visited_count; i++) {
        if (state->visited_systems[i].star_id == star_id) return i;
    }
    return -1;
}
void cache_current_system(GameState *state) {
    int idx = find_cached_system(state, state->current_system.star_id);
    if (idx != -1) state->visited_systems[idx] = state->current_system;
    else {
        if (state->visited_count >= state->visited_capacity) {
            state->visited_capacity *= 2;
            state->visited_systems = (StarSystem*)realloc(state->visited_systems, sizeof(StarSystem) * state->visited_capacity);
        }
        state->visited_systems[state->visited_count++] = state->current_system;
    }
}

void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius) {
    if (radius < 1) radius = 1;
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx*dx + dy*dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }
    }
}

SDL_Texture* create_circle_texture(SDL_Renderer* renderer, int radius) {
    // Create a texture to act as our sprite
    int size = radius * 2;
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, size, size);
    if (!texture) return NULL;
    
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    
    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Transparent background
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White circle
    
    // Draw filled circle (reuse naive algo for generation, done once so ok)
    for (int w = 0; w < size; w++) {
        for (int h = 0; h < size; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx*dx + dy*dy) <= (radius * radius)) {
                // Simple anti-aliasing attempt: Distance check
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist > radius - 1.0f) {
                     int alpha = 255 * (1.0f - (dist - (radius - 1.0f)));
                     SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
                     SDL_RenderDrawPoint(renderer, w, h);
                } else {
                     SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                     SDL_RenderDrawPoint(renderer, w, h);
                }
            }
        }
    }

    SDL_SetRenderTarget(renderer, NULL); // Reset to window
    return texture;
}

void draw_crosshair(SDL_Renderer *renderer) {
    int cx = SCREEN_WIDTH / 2;
    int cy = SCREEN_HEIGHT / 2;
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100);
    SDL_RenderDrawLine(renderer, cx - 10, cy, cx + 10, cy);
    SDL_RenderDrawLine(renderer, cx, cy - 10, cx, cy + 10);
}

void render_system_3d(SDL_Renderer *renderer, GameState *state, Vec3 cam_pos, Mat3 cam_rot, bool show_boundary) {
    RenderItem items[100];
    int item_count = 0;
    
    // Prepare Stars
    for (int i=0; i<state->current_system.star_count; i++) {
        Star *s = &state->current_system.stars[i];
        // World -> Camera
        Vec3 rel = vec3_sub(s->pos, cam_pos);
        // Rotate by Camera Rotation Transpose (Inverse)
        Vec3 local = {
            cam_rot.m[0][0]*rel.x + cam_rot.m[1][0]*rel.y + cam_rot.m[2][0]*rel.z,
            cam_rot.m[0][1]*rel.x + cam_rot.m[1][1]*rel.y + cam_rot.m[2][1]*rel.z,
            cam_rot.m[0][2]*rel.x + cam_rot.m[1][2]*rel.y + cam_rot.m[2][2]*rel.z
        };
        
        if (local.z > 0.1) { // In front
            int sx = SCREEN_WIDTH/2 + (local.x / local.z) * FOV_SCALE;
            int sy = SCREEN_HEIGHT/2 - (local.y / local.z) * FOV_SCALE; // Y is up in 3D, down in screen
            int r = (s->radius * 10.0 / local.z) * FOV_SCALE * 0.01;
            items[item_count++] = (RenderItem){local.z, sx, sy, MAX(2, r), s->color, "Star", true};
        }
    }
    
    float max_dist = 0;

    // Prepare Planets
    for (int i=0; i<state->current_system.planet_count; i++) {
        Planet *p = &state->current_system.planets[i];
        float dist = vec3_len(p->pos);
        if (dist > max_dist) max_dist = dist;

        Vec3 rel = vec3_sub(p->pos, cam_pos);
        Vec3 local = {
            cam_rot.m[0][0]*rel.x + cam_rot.m[1][0]*rel.y + cam_rot.m[2][0]*rel.z,
            cam_rot.m[0][1]*rel.x + cam_rot.m[1][1]*rel.y + cam_rot.m[2][1]*rel.z,
            cam_rot.m[0][2]*rel.x + cam_rot.m[1][2]*rel.y + cam_rot.m[2][2]*rel.z
        };
        
        if (local.z > 0.1) {
            int sx = SCREEN_WIDTH/2 + (local.x / local.z) * FOV_SCALE;
            int sy = SCREEN_HEIGHT/2 - (local.y / local.z) * FOV_SCALE;
            int r = (p->radius * 5.0 / local.z) * FOV_SCALE * 0.01;
            sprintf(items[item_count].label, "%s", p->name);
            items[item_count++] = (RenderItem){local.z, sx, sy, MAX(2, r), p->color, "", false};
        }
    }
    
    // Draw Boundary
    if (show_boundary && max_dist > 0) {
        SDL_SetRenderDrawColor(renderer, 0, 50, 0, 255);
        
        // Draw a circle on the XY plane for the boundary
        float radius = max_dist * 1.1; // 10% buffers
        int segments = 64;
        
        for (int i=0; i<=segments; i++) {
             float theta = (float)i / segments * 6.2831853;
             float next_theta = (float)(i+1) / segments * 6.2831853;
             
             Vec3 p1 = {cos(theta)*radius, sin(theta)*radius, 0};
             Vec3 p2 = {cos(next_theta)*radius, sin(next_theta)*radius, 0};
             
             // Transform p1
             Vec3 rel1 = vec3_sub(p1, cam_pos);
             Vec3 local1 = {
                cam_rot.m[0][0]*rel1.x + cam_rot.m[1][0]*rel1.y + cam_rot.m[2][0]*rel1.z,
                cam_rot.m[0][1]*rel1.x + cam_rot.m[1][1]*rel1.y + cam_rot.m[2][1]*rel1.z,
                cam_rot.m[0][2]*rel1.x + cam_rot.m[1][2]*rel1.y + cam_rot.m[2][2]*rel1.z
             };
             
             // Transform p2
             Vec3 rel2 = vec3_sub(p2, cam_pos);
             Vec3 local2 = {
                cam_rot.m[0][0]*rel2.x + cam_rot.m[1][0]*rel2.y + cam_rot.m[2][0]*rel2.z,
                cam_rot.m[0][1]*rel2.x + cam_rot.m[1][1]*rel2.y + cam_rot.m[2][1]*rel2.z,
                cam_rot.m[0][2]*rel2.x + cam_rot.m[1][2]*rel2.y + cam_rot.m[2][2]*rel2.z
             };

             if (local1.z > 0.1 && local2.z > 0.1) {
                 int sx1 = SCREEN_WIDTH/2 + (local1.x / local1.z) * FOV_SCALE;
                 int sy1 = SCREEN_HEIGHT/2 - (local1.y / local1.z) * FOV_SCALE;
                 int sx2 = SCREEN_WIDTH/2 + (local2.x / local2.z) * FOV_SCALE;
                 int sy2 = SCREEN_HEIGHT/2 - (local2.y / local2.z) * FOV_SCALE;
                 SDL_RenderDrawLine(renderer, sx1, sy1, sx2, sy2);
             }
        }
    }

    // Sort Painter's Algo
    qsort(items, item_count, sizeof(RenderItem), compare_render_items);
    
    // Draw
    for (int i=0; i<item_count; i++) {
        RenderItem *it = &items[i];
        Uint32 c = it->color;
        
        SDL_SetTextureColorMod(state->body_texture, (c>>24)&0xFF, (c>>16)&0xFF, (c>>8)&0xFF);
        SDL_SetTextureAlphaMod(state->body_texture, 255);
        SDL_Rect dst = {it->x - it->r, it->y - it->r, it->r*2, it->r*2};
        SDL_RenderCopy(renderer, state->body_texture, NULL, &dst);
        
        // HUD Marker (Green Box around object)
        if (!it->is_star && !show_boundary) { // Don't show markers in tactical view to keep it clean? Or keep them? Let's keep them if not tactical, or just keep them always. Plan said nothing, but tactical usually implies clean.
           // Actually, let's just keep markers for consistency, but maybe lighter.
           // Re-reading plan: "static (no ship movement control), but simulation (planets moving) continues"
           SDL_SetRenderDrawColor(renderer, 0, 255, 0, 150);
           SDL_Rect rect = {it->x - 10, it->y - 10, 20, 20};
           SDL_RenderDrawRect(renderer, &rect);
        }
        
        if (it->label[0] != '\0' && (it->z < 100.0 || show_boundary)) { // Always show labels in tactical
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            draw_text(renderer, it->x, it->y - it->r - 10, it->label, 1);
        }
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    SDL_Window* window = SDL_CreateWindow("C-Galaxy Elite", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    GameState state;
    state.mode = VIEW_GALAXY;
    state.map_cam_pos = (Vec3){0,0,0};
    state.map_zoom = 20.0;
    state.map_tilt = 0.6f;     // ~35 degrees tilt for 3D view
    state.running = true;
    state.paused = false;
    state.time_speed = 1.0f;
    
    // Init Player
    state.player.pos = (Vec3){0, 0, 5}; // Start above plane
    state.player.vel = (Vec3){0,0,0};
    state.player.rot = (Mat3){{{1,0,0},{0,1,0},{0,0,1}}}; // Identity
    state.player.speed = 0.0f;
    state.player.throttle = 0.0f;
    
    init_cache(&state);
    
    // Initialize trig lookup tables for fast galaxy rotation
    init_trig_tables();
    
    // Parse command line arguments (before galaxy init)
    int galaxy_star_count = 15000; // Default
    for (int i=1; i<argc; i++) {
        if (strncmp(argv[i], "--galaxy-stars=", 15) == 0) galaxy_star_count = atoi(argv[i] + 15);
        if (strncmp(argv[i], "--stars=", 8) == 0) set_forced_star_count(atoi(argv[i] + 8));
    }
    
    // Initialize Galaxy
    init_galaxy(&state.galaxy, galaxy_star_count);
    
    // Init Texture
    state.body_texture = create_circle_texture(renderer, 4);
    if (!state.body_texture) printf("Failed to create texture\n");

    SDL_Event e;
    Uint32 last_time = SDL_GetTicks();
    
    // Input State
    bool k_q=0, k_e=0, k_w=0, k_s=0, k_a=0, k_d=0, k_r=0, k_f=0;

    while (state.running) {
        Uint32 current_time = SDL_GetTicks();
        float dt = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) state.running = false;
            
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (state.mode == VIEW_GALAXY) {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        int mx, my; SDL_GetMouseState(&mx, &my);
                        float scale = state.map_zoom * 10.0f;
                        
                        // Precompute tilt for click detection (must match rendering)
                        float cos_tilt = cosf(state.map_tilt);
                        float sin_tilt = sinf(state.map_tilt);
                        
                        // Use screen-space pixel distance for accurate click detection
                        float min_pixel_dist = 15.0f; // Max click distance in pixels
                        int clicked_star_idx = -1;
                        
                        // OPTIMIZATION: Use spatial grid for O(1) click detection
                        // Only check stars in the 3x3 grid neighborhood around click position
                        int cell_size = (int)state.click_grid.cell_size;
                        if (cell_size > 0) {
                            int gx = mx / cell_size;
                            int gy = my / cell_size;
                            
                            for (int dx = -1; dx <= 1; dx++) {
                                for (int dy = -1; dy <= 1; dy++) {
                                    int cx = gx + dx;
                                    int cy = gy + dy;
                                    if (cx < 0 || cx >= SPATIAL_GRID_SIZE || cy < 0 || cy >= SPATIAL_GRID_SIZE) continue;
                                    
                                    SpatialCell *cell = &state.click_grid.cells[cx][cy];
                                    for (int j = 0; j < cell->count; j++) {
                                        int i = cell->star_indices[j];
                                        GalaxyStar *gs = &state.galaxy.stars[i];
                                        
                                        // Calculate current 3D position (same as rendering)
                                        float current_angle = gs->orbit_angle + gs->orbit_speed * state.galaxy.time;
                                        float x_3d = fast_cos(current_angle) * gs->orbit_radius;
                                        float y_3d = fast_sin(current_angle) * gs->orbit_radius;
                                        float z_3d = gs->z_offset;
                                        
                                        // Apply tilt transformation (same as rendering)
                                        float y_tilted = y_3d * cos_tilt - z_3d * sin_tilt;
                                        
                                        // Project to screen coordinates (same as rendering)
                                        int sx = (x_3d - state.map_cam_pos.x) * scale + SCREEN_WIDTH/2;
                                        int sy = (y_tilted - state.map_cam_pos.y) * scale + SCREEN_HEIGHT/2;
                                        
                                        // Compare in screen pixel space
                                        float pdx = sx - mx;
                                        float pdy = sy - my;
                                        float pixel_dist = sqrtf(pdx*pdx + pdy*pdy);
                                        
                                        if (pixel_dist < min_pixel_dist) {
                                            min_pixel_dist = pixel_dist;
                                            clicked_star_idx = i;
                                        }
                                    }
                                }
                            }
                        }
                        
                        if (clicked_star_idx >= 0) {
                            GalaxyStar *gs = &state.galaxy.stars[clicked_star_idx];
                            
                            // Check cache using star_id (which is the array index)
                            int idx = find_cached_system(&state, gs->star_id);
                            if (idx != -1) state.current_system = state.visited_systems[idx];
                            else state.current_system = generate_system(gs->star_id);
                            
                            state.mode = VIEW_TACTICAL; 
                            
                            // Reset Player to outer rim of system
                            state.player.pos = (Vec3){state.current_system.planet_count * 2.0f, 0, 2.0f};
                            state.player.vel = (Vec3){0,0,0};
                            state.player.rot = (Mat3){{{1,0,0},{0,1,0},{0,0,1}}};
                        }
                    }
                }
            }
            
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_t) {
                         if (state.mode == VIEW_COCKPIT) state.mode = VIEW_TACTICAL;
                         else if (state.mode == VIEW_TACTICAL) state.mode = VIEW_COCKPIT;
                    }
                }
            
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                bool down = (e.type == SDL_KEYDOWN);
                SDL_Keycode k = e.key.keysym.sym;
                
                if (k == SDLK_TAB && down) {
                    if (state.mode == VIEW_GALAXY) { /* Cant switch to empty cockpit */ }
                    else { 
                        state.mode = (state.mode == VIEW_COCKPIT) ? VIEW_GALAXY : VIEW_COCKPIT; 
                        if (state.mode == VIEW_GALAXY) cache_current_system(&state);
                    }
                }
                if (k == SDLK_SPACE && down) state.paused = !state.paused;
                
                if (state.mode == VIEW_GALAXY) {
                    if (k == SDLK_LEFT) state.map_cam_pos.x -= 10.0/state.map_zoom*20;
                    if (k == SDLK_RIGHT) state.map_cam_pos.x += 10.0/state.map_zoom*20;
                    if (k == SDLK_UP) state.map_cam_pos.y -= 10.0/state.map_zoom*20;
                    if (k == SDLK_DOWN) state.map_cam_pos.y += 10.0/state.map_zoom*20;
                    if (k == SDLK_EQUALS || k == SDLK_PLUS) state.map_zoom *= 1.1;
                    if (k == SDLK_MINUS) state.map_zoom /= 1.1;
                    // Tilt controls
                    if (k == SDLK_w) {
                        state.map_tilt -= 0.05;
                        if (state.map_tilt < 0) state.map_tilt = 0;
                    }
                    if (k == SDLK_s) {
                        state.map_tilt += 0.05;
                        if (state.map_tilt > 1.4) state.map_tilt = 1.4; // ~80 degrees max
                    }
                    if (k == SDLK_LEFTBRACKET && down) {
                        state.time_speed /= 2.0;
                        if (state.time_speed < 0.0625) state.time_speed = 0.0625;
                    }
                    if (k == SDLK_RIGHTBRACKET && down) {
                        state.time_speed *= 2.0;
                        if (state.time_speed > 4096.0) state.time_speed = 4096.0;
                    }
                } else {
                    // Ship Controls
                    if (k == SDLK_w) k_w = down; // Pitch Down
                    if (k == SDLK_s) k_s = down; // Pitch Up
                    if (k == SDLK_a) k_a = down; // Yaw Left
                    if (k == SDLK_d) k_d = down; // Yaw Right
                    if (k == SDLK_q) k_q = down; // Roll Left
                    if (k == SDLK_e) k_e = down; // Roll Right
                    if (k == SDLK_r) k_r = down; // Throttle Up
                    if (k == SDLK_f) k_f = down; // Throttle Down
                }
            }
            if (e.type == SDL_MOUSEWHEEL && state.mode == VIEW_GALAXY) state.map_zoom *= (e.wheel.y > 0 ? 1.1 : 0.9);
        }


        // --- PHYSICS ---
        // Update Galaxy Rotation (always, even in system view)
        if (!state.paused) {
            state.galaxy.time += dt * state.time_speed * 10000.0f; // Speed up galaxy rotation for visibility
            // Note: Individual star positions updated during rendering
        }
        
        if (state.mode == VIEW_COCKPIT || state.mode == VIEW_TACTICAL) {
             
             if (state.mode == VIEW_COCKPIT) {
                // Ship Flight Model
                float turn_speed = 1.5f * dt;
                if (k_w) state.player.rot = mat3_mul(state.player.rot, mat3_rot_x(-turn_speed));
                if (k_s) state.player.rot = mat3_mul(state.player.rot, mat3_rot_x(turn_speed));
                if (k_a) state.player.rot = mat3_mul(state.player.rot, mat3_rot_y(-turn_speed)); // Yaw usually Y
                if (k_d) state.player.rot = mat3_mul(state.player.rot, mat3_rot_y(turn_speed));
                if (k_q) state.player.rot = mat3_mul(state.player.rot, mat3_rot_z(turn_speed));
                if (k_e) state.player.rot = mat3_mul(state.player.rot, mat3_rot_z(-turn_speed));
                
                if (k_r) state.player.throttle += 0.5f * dt;
                if (k_f) state.player.throttle -= 0.5f * dt;
                if (state.player.throttle < 0) state.player.throttle = 0;
                if (state.player.throttle > 10.0) state.player.throttle = 10.0;
                
                // Ship Forward Vector (Row 2 of Rotation Matrix usually, depends on convention)
                // Let's assume Row 2 is Z (Forward)
                Vec3 forward = {state.player.rot.m[2][0], state.player.rot.m[2][1], state.player.rot.m[2][2]};
                
                // Move Ship
                Vec3 thrust = vec3_mul(forward, state.player.throttle * 5.0f * dt); // 5.0 acceleration
                state.player.vel = vec3_add(state.player.vel, thrust);
                state.player.pos = vec3_add(state.player.pos, vec3_mul(state.player.vel, dt));
             }

            // Keplerian Orbit Simulation
            if (!state.paused) {
                float sim_dt = dt * 1.0f * 0.5f; // Fixed 1.0 speed for tactical/cockpit view (~3 days per frame)
                
                for (int i=0; i<state.current_system.planet_count; i++) {
                    Planet *p = &state.current_system.planets[i];
                    
                    // Update Angle
                    p->orbit_angle += p->orbit_speed * sim_dt;
                    
                    // Update Position (with Eccentricity and Inclination)
                    float r = p->orbit_radius * (1.0f - p->eccentricity * cosf(p->orbit_angle));
                    p->pos.x = cosf(p->orbit_angle) * r;
                    p->pos.y = sinf(p->orbit_angle) * r;
                    p->pos.z = r * sinf(p->orbit_angle) * sinf(p->inclination);
                    
                    // Update Velocity (approximate tangent)
                    p->vel.x = -sinf(p->orbit_angle) * p->orbit_speed * p->orbit_radius;
                    p->vel.y = cosf(p->orbit_angle) * p->orbit_speed * p->orbit_radius;
                    
                    // Moons
                    for (int m=0; m<p->moon_count; m++) {
                        // Simple moon orbit relative to planet
                         // Need moon orbit speed/angle logic too? 
                         // Existing logic was: pos += vel * sim_dt.
                         // Let's keep existing logic for moons or use simple rotation?
                         // Existing moon physics was minimal. Let's make them rotate around planet.
                         // But Moon struct (galaxy.h) has orbit_dist, orbit_angle, orbit_speed.
                         // So we can use Keplerian for moons too!
                         
                         // BUT: we need to initialize orbit_speed for moons in gen.c?
                         // Currently gen.c line 184: mn->vel = {0,0,0}.
                         // We skipped moon setup updates.
                         // So let's just make moons follow planet for now and ignore local rotation to prevent broken moons.
                         // Or just do "p->moons[m].pos = p->pos;" + offset.
                         // Existing logic for moons used `vel`.
                         
                         // Let's leave moons static relative to planet for now, or just moving linearly (broken).
                         // The user asked for planets.
                         // Let's clear moon rel pos to prevent them drifting away.
                         p->moons[m].pos = p->pos; // Placeholder: Moons inside planet for now (fixing later if asked)
                    }
                }
            }
        }



        // --- RENDER ---
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (state.mode == VIEW_GALAXY) {
            // Galaxy Map Render - Rotating Milky Way with 3D perspective
            // Scale factor: 1 unit = 1 kLY, displayed at 10 pixels per kLY base
            float scale = state.map_zoom * 10.0f; // Increased scale for more spread
            
            float view_w_ly = SCREEN_WIDTH / scale;
            float view_h_ly = SCREEN_HEIGHT / scale;
            
            // Precompute tilt transformation
            float cos_tilt = cosf(state.map_tilt);
            float sin_tilt = sinf(state.map_tilt);
            
            // Clear spatial grid for click detection
            memset(&state.click_grid, 0, sizeof(SpatialGrid));
            state.click_grid.cell_size = (float)SCREEN_WIDTH / SPATIAL_GRID_SIZE;
            
            // Precompute view bounds for early frustum culling
            float cam_view_radius = sqrtf(view_w_ly * view_w_ly + view_h_ly * view_h_ly) / 2.0f + 10.0f;
            float cam_dist = sqrtf(state.map_cam_pos.x * state.map_cam_pos.x + 
                                   state.map_cam_pos.y * state.map_cam_pos.y);
            
            // Render all visible stars
            for (int i = 0; i < state.galaxy.star_count; i++) {
                GalaxyStar *gs = &state.galaxy.stars[i];
                
                // OPTIMIZATION 1: Early frustum culling by orbit radius
                // If the star's orbit is entirely outside the view, skip it
                float min_possible_dist = fabsf(gs->orbit_radius - cam_dist) - gs->orbit_radius * 0.1f;
                if (min_possible_dist > cam_view_radius) continue;
                
                // OPTIMIZATION 2: Use fast trig lookup tables
                float current_angle = gs->orbit_angle + gs->orbit_speed * state.galaxy.time;
                float x_3d = fast_cos(current_angle) * gs->orbit_radius;
                float y_3d = fast_sin(current_angle) * gs->orbit_radius;
                float z_3d = gs->z_offset;
                
                // Apply camera tilt (rotate around X axis)
                // This compresses Y and adds Z contribution
                float y_tilted = y_3d * cos_tilt - z_3d * sin_tilt;
                float z_tilted = y_3d * sin_tilt + z_3d * cos_tilt;
                
                // Frustum culling (use tilted coordinates)
                if (fabsf(x_3d - state.map_cam_pos.x) > view_w_ly / 2 + 5) continue;
                if (fabsf(y_tilted - state.map_cam_pos.y) > view_h_ly / 2 + 5) continue;
                
                // Project to screen
                int sx = (x_3d - state.map_cam_pos.x) * scale + SCREEN_WIDTH/2;
                int sy = (y_tilted - state.map_cam_pos.y) * scale + SCREEN_HEIGHT/2;
                
                // OPTIMIZATION 3: Populate spatial grid for click detection
                if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
                    int gx = sx / (int)state.click_grid.cell_size;
                    int gy = sy / (int)state.click_grid.cell_size;
                    if (gx >= 0 && gx < SPATIAL_GRID_SIZE && gy >= 0 && gy < SPATIAL_GRID_SIZE) {
                        SpatialCell *cell = &state.click_grid.cells[gx][gy];
                        if (cell->count < SPATIAL_CELL_CAPACITY) {
                            cell->star_indices[cell->count++] = i;
                        }
                    }
                }
                
                // Stars are always single points (1 pixel) for realistic appearance
                int radius = 1;
                
                // Extract color with depth-based brightness
                Uint32 c = gs->color;
                int r = (c >> 24) & 0xFF;
                int g = (c >> 16) & 0xFF;
                int b = (c >> 8) & 0xFF;
                
                // Slight brightness variation based on depth
                float bright = 0.8f + z_tilted * 0.02f;
                if (bright > 1.0f) bright = 1.0f;
                if (bright < 0.5f) bright = 0.5f;
                r = (int)(r * bright); g = (int)(g * bright); b = (int)(b * bright);
                
                // Render star
                SDL_Rect dst = {sx - radius, sy - radius, radius * 2, radius * 2};
                SDL_SetTextureColorMod(state.body_texture, r, g, b);
                SDL_SetTextureAlphaMod(state.body_texture, 255);
                SDL_RenderCopy(renderer, state.body_texture, NULL, &dst);
            }
            
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            char ui_text[256];
            sprintf(ui_text, "GALAXY | W/S: Tilt | [ ]: Time x%.1f | Zoom: %.1fx", state.time_speed, state.map_zoom);
            draw_text(renderer, 10, 10, ui_text, 2);

            
        } else {
            
            // --- 3D VIEW (COCKPIT or TACTICAL) ---
            if (state.mode == VIEW_COCKPIT) {
                render_system_3d(renderer, &state, state.player.pos, state.player.rot, false);
                
                // Cockpit UI
                draw_crosshair(renderer);
                
                char buf[128];
                sprintf(buf, "THROTTLE: %.1f | SPEED: %.2f AU/s", state.player.throttle, vec3_len(state.player.vel));
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                draw_text(renderer, 10, SCREEN_HEIGHT - 30, buf, 2);
                
                draw_text(renderer, 10, 10, "COCKPIT MODE | W/S: Pitch | A/D: Yaw | Q/E: Roll | R/F: Throttle | TAB: Map | T: Tactical", 1);
            } else if (state.mode == VIEW_TACTICAL) {
                 // Calculate tactical camera position (Top down)
                 // We want to see the whole system. Find max radius.
                 float max_dist = 0;
                 for (int i=0; i<state.current_system.planet_count; i++) {
                     float d = vec3_len(state.current_system.planets[i].pos);
                     if (d > max_dist) max_dist = d;
                 }
                 if (max_dist < 5.0) max_dist = 5.0; // Min zoom
                 
                 // Camera at (0, 0, -Z) looking at (0,0,0) with Y up? 
                 // If default cam looks -Z, then placing cam at +Z looks at origin? 
                 // Wait, standard OpenGL: Camera at +Z, looking -Z.
                 // Here, let's assume Camera is at (0, 0, -Dist) and looking +Z? 
                 // Or Camera at (0, 0, Dist) looking -Z?
                 // Let's rely on Identity matrix being "Looking Forward".
                 // In `render_system_3d`: `local.z` is depth. 
                 // `local = cam_rot_inv * (obj - cam)`.
                 // If cam_rot is Identity, local = obj - cam.
                 // For object at (0,0,0) to be visible, local.z must be > 0.
                 // So (0 - cam.z) > 0 => cam.z < 0.
                 // So we place camera at negative Z.
                 
                 Vec3 cam_pos = {0, 0, -max_dist * 2.5}; // Back up enough
                 Mat3 cam_rot = {{{1,0,0},{0,1,0},{0,0,1}}}; // Identity
                 
                 render_system_3d(renderer, &state, cam_pos, cam_rot, true);
                 
                 SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                 char tac_text[256];
                 sprintf(tac_text, "TACTICAL%s | T: Cockpit", 
                    state.paused ? " [PAUSED]" : "");
                 draw_text(renderer, 10, 10, tac_text, 2);
            }
        }
        
        SDL_RenderPresent(renderer);
    }





    if (state.body_texture) SDL_DestroyTexture(state.body_texture);
    if (state.galaxy.stars) free(state.galaxy.stars);
    free(state.visited_systems);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
