
#include <SDL2/SDL.h>
#include "galaxy.h"
#include "font.h"
#include "math3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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
int find_cached_system(GameState *state, int x, int y) {
    for (int i = 0; i < state->visited_count; i++) {
        if (state->visited_systems[i].x == x && state->visited_systems[i].y == y) return i;
    }
    return -1;
}
void cache_current_system(GameState *state) {
    int idx = find_cached_system(state, state->current_system.x, state->current_system.y);
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
        SDL_SetRenderDrawColor(renderer, (c>>24)&0xFF, (c>>16)&0xFF, (c>>8)&0xFF, 255);
        draw_circle(renderer, it->x, it->y, it->r);
        
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
    for (int i=1; i<argc; i++) {
        if (strncmp(argv[i], "--stars=", 8) == 0) set_forced_star_count(atoi(argv[i] + 8));
    }

    SDL_Event e;
    Uint32 last_time = SDL_GetTicks();
    
    // Input State
    bool k_up=0, k_down=0, k_left=0, k_right=0, k_q=0, k_e=0, k_w=0, k_s=0, k_a=0, k_d=0, k_r=0, k_f=0;

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
                        float wx = state.map_cam_pos.x + (mx - SCREEN_WIDTH/2) / state.map_zoom;
                        float wy = state.map_cam_pos.y + (my - SCREEN_HEIGHT/2) / state.map_zoom;
                        int ix = round(wx); int iy = round(wy);
                        if (star_exists(ix, iy)) {
                            int idx = find_cached_system(&state, ix, iy);
                            if (idx != -1) state.current_system = state.visited_systems[idx];
                            else state.current_system = generate_system(ix, iy);
                            state.mode = VIEW_TACTICAL; 
                            
                            // Reset Player to outer rim of system
                            state.player.pos = (Vec3){state.current_system.planet_count * 2.0f, 0, 2.0f};
                            state.player.vel = (Vec3){0,0,0};
                            // Look at star (0,0,0)
                            Vec3 dir = vec3_norm(vec3_sub((Vec3){0,0,0}, state.player.pos));
                            // Simple look-at setup (approx)
                            state.player.rot = (Mat3){{{0,1,0},{0,0,1},{dir.x, dir.y, dir.z}}}; // Hacky init
                            // Actually better to just keep identity or look inward
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

            // N-Body Sim (Stars/Planets)
            if (!state.paused) {
                float sim_dt = dt * state.time_speed * 0.5;
                // (Star/Planet Gravity Logic - Simplified from previous for brevity, but keeping N-body)
                // ... [Insert N-Body Logic Here - kept minimal for 3D test focus] ...
                // Re-using previous logic:
                for (int i=0; i<state.current_system.planet_count; i++) {
                    Planet *p = &state.current_system.planets[i];
                    Vec3 acc = {0,0,0};
                    for (int s=0; s<state.current_system.star_count; s++) {
                        Star *star = &state.current_system.stars[s];
                        Vec3 d = vec3_sub(star->pos, p->pos);
                        float r = vec3_len(d);
                        float f = G * star->mass / (r*r);
                        acc = vec3_add(acc, vec3_mul(d, f/r));
                    }
                    p->vel = vec3_add(p->vel, vec3_mul(acc, sim_dt));
                    p->pos = vec3_add(p->pos, vec3_mul(p->vel, sim_dt));
                    
                    // Moons
                    for (int m=0; m<p->moon_count; m++) {
                        p->moons[m].pos.x += p->moons[m].vel.x * sim_dt * 5.0;
                    }
                }
            }
        }

        // --- RENDER ---
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (state.mode == VIEW_GALAXY) {
            // Galaxy Map Render (Same as before)
            int view_w = SCREEN_WIDTH / state.map_zoom;
            int view_h = SCREEN_HEIGHT / state.map_zoom;
            for (int x = (int)state.map_cam_pos.x - view_w/2; x <= (int)state.map_cam_pos.x + view_w/2; x++) {
                for (int y = (int)state.map_cam_pos.y - view_h/2; y <= (int)state.map_cam_pos.y + view_h/2; y++) {
                    if (star_exists(x, y)) {
                        int sx = (x - state.map_cam_pos.x) * state.map_zoom + SCREEN_WIDTH/2;
                        int sy = (y - state.map_cam_pos.y) * state.map_zoom + SCREEN_HEIGHT/2;
                        unsigned int s = get_seed(x,y); srand(s);
                        int r=255,g=255,b=255; 
                        if (rand()%2==0) {r=200; g=200;}
                        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                        draw_circle(renderer, sx, sy, MAX(2, state.map_zoom/5));
                    }
                }
            }
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            draw_text(renderer, 10, 10, "GALAXY MAP - Click Star | TAB: Return to Cockpit", 2);
            
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
                 draw_text(renderer, 10, 10, "TACTICAL VIEW | T: Cockpit", 2);
            }
        }
        
        SDL_RenderPresent(renderer);
    }

    free(state.visited_systems);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
