#include <SDL2/SDL.h>
#include "galaxy.h"
#include "font.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define G 39.478 // Gravity Constant

// Cache Helpers
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

#include <string.h>

// ...

int main(int argc, char* argv[]) {
    // Parse Arguments
    for (int i=1; i<argc; i++) {
        if (strncmp(argv[i], "--stars=", 8) == 0) {
            int count = atoi(argv[i] + 8);
            if (count >= 1 && count <= 3) {
                set_forced_star_count(count);
                printf("Forcing Star Count: %d\n", count);
            }
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    SDL_Window* window = SDL_CreateWindow("C-Galaxy N-Body", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    GameState state;
    state.mode = VIEW_GALAXY;
    state.camera_pos.x = 0; state.camera_pos.y = 0;
    state.zoom = 20.0;
    state.running = true;
    state.selected_planet_idx = -1;
    state.selected_star = false;
    state.centered_planet_idx = -1;
    state.system_center_offset.x = 0; state.system_center_offset.y = 0;
    state.paused = false;
    state.time_speed = 1.0f;
    state.galaxy_camera_pos.x = 0; state.galaxy_camera_pos.y = 0; state.galaxy_zoom = 20.0;
    
    init_cache(&state);
    SDL_Event e;
    Uint32 last_time = SDL_GetTicks();

    while (state.running) {
        Uint32 current_time = SDL_GetTicks();
        float dt = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) state.running = false;
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx, my; SDL_GetMouseState(&mx, &my);
                if (e.button.button == SDL_BUTTON_LEFT) {
                    if (state.mode == VIEW_GALAXY) {
                        float wx = state.camera_pos.x + (mx - SCREEN_WIDTH/2) / state.zoom;
                        float wy = state.camera_pos.y + (my - SCREEN_HEIGHT/2) / state.zoom;
                        int ix = round(wx); int iy = round(wy);
                        if (star_exists(ix, iy)) {
                            state.galaxy_camera_pos = state.camera_pos; state.galaxy_zoom = state.zoom;
                            int idx = find_cached_system(&state, ix, iy);
                            if (idx != -1) state.current_system = state.visited_systems[idx];
                            else state.current_system = generate_system(ix, iy);
                            state.mode = VIEW_SYSTEM; state.zoom = 100.0;
                            state.selected_planet_idx = -1; state.selected_star = true;
                            state.centered_planet_idx = -1; state.system_center_offset.x = 0; state.system_center_offset.y = 0;
                        }
                    } else {
                        // System Click
                        int cx = SCREEN_WIDTH/2 - state.system_center_offset.x * state.zoom;
                        int cy = SCREEN_HEIGHT/2 - state.system_center_offset.y * state.zoom;
                        state.selected_planet_idx = -1; state.selected_star = false;
                        
                        // Click Star?
                        for (int s=0; s<state.current_system.star_count; s++) {
                            Star *star = &state.current_system.stars[s];
                            int sx = cx + star->x * state.zoom;
                            int sy = cy + star->y * state.zoom;
                            if (sqrt(pow(mx-sx,2)+pow(my-sy,2)) < 30) {
                                state.selected_star = true;
                                if (e.button.clicks == 2) { state.centered_planet_idx = -1; state.system_center_offset.x = star->x; state.system_center_offset.y = star->y; }
                                goto clicked;
                            }
                        }
                        
                        // Click Planet?
                        for (int i=0; i<state.current_system.planet_count; i++) {
                            Planet *p = &state.current_system.planets[i];
                            int px = cx + p->x * state.zoom;
                            int py = cy + p->y * state.zoom;
                            if (sqrt(pow(mx-px,2)+pow(my-py,2)) < 20) {
                                state.selected_planet_idx = i;
                                if (e.button.clicks == 2) state.centered_planet_idx = i;
                                goto clicked;
                            }
                        }
                        clicked:;
                    }
                }
                if (e.button.button == SDL_BUTTON_RIGHT && state.mode == VIEW_SYSTEM) {
                    cache_current_system(&state);
                    state.mode = VIEW_GALAXY;
                    state.camera_pos = state.galaxy_camera_pos; state.zoom = state.galaxy_zoom;
                }
            }
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_LEFT) state.camera_pos.x -= 10.0/state.zoom*20;
                if (e.key.keysym.sym == SDLK_RIGHT) state.camera_pos.x += 10.0/state.zoom*20;
                if (e.key.keysym.sym == SDLK_UP) {
                    if (state.mode == VIEW_GALAXY) state.camera_pos.y -= 10.0/state.zoom*20;
                    else state.time_speed *= 1.5;
                }
                if (e.key.keysym.sym == SDLK_DOWN) {
                    if (state.mode == VIEW_GALAXY) state.camera_pos.y += 10.0/state.zoom*20;
                    else state.time_speed /= 1.5;
                }
                if (e.key.keysym.sym == SDLK_SPACE) state.paused = !state.paused;
                if (e.key.keysym.sym == SDLK_EQUALS) state.zoom *= 1.1;
                if (e.key.keysym.sym == SDLK_MINUS) state.zoom /= 1.1;
            }
            if (e.type == SDL_MOUSEWHEEL) state.zoom *= (e.wheel.y > 0 ? 1.1 : 0.9);
        }

        // --- N-BODY PHYSICS ---
        if (state.mode == VIEW_SYSTEM && !state.paused) {
            float sim_dt = dt * state.time_speed * 0.5;
            
            // Update Stars
            if (state.current_system.star_count > 1) {
                for (int i=0; i<state.current_system.star_count; i++) {
                    Star *s1 = &state.current_system.stars[i];
                    float ax = 0, ay = 0;
                    for (int j=0; j<state.current_system.star_count; j++) {
                        if (i==j) continue;
                        Star *s2 = &state.current_system.stars[j];
                        float dx = s2->x - s1->x;
                        float dy = s2->y - s1->y;
                        float r2 = dx*dx + dy*dy;
                        float r = sqrt(r2);
                        float f = G * s2->mass / r2;
                        ax += f * dx / r;
                        ay += f * dy / r;
                    }
                    s1->vx += ax * sim_dt; s1->vy += ay * sim_dt;
                }
                for (int i=0; i<state.current_system.star_count; i++) {
                    state.current_system.stars[i].x += state.current_system.stars[i].vx * sim_dt;
                    state.current_system.stars[i].y += state.current_system.stars[i].vy * sim_dt;
                }
            }

            // Update Planets
            for (int i=0; i<state.current_system.planet_count; i++) {
                Planet *p = &state.current_system.planets[i];
                float ax = 0, ay = 0;
                for (int s=0; s<state.current_system.star_count; s++) {
                    Star *star = &state.current_system.stars[s];
                    float dx = star->x - p->x;
                    float dy = star->y - p->y;
                    float r2 = dx*dx + dy*dy;
                    float r = sqrt(r2);
                    float f = G * star->mass / r2;
                    ax += f * dx / r;
                    ay += f * dy / r;
                }
                p->vx += ax * sim_dt; p->vy += ay * sim_dt;
                p->x += p->vx * sim_dt; p->y += p->vy * sim_dt;
                
                // Update Rotation
                float rot_speed = 6.28 / (p->rotation_period / 365.25);
                p->rotation_angle += rot_speed * sim_dt;
                
                for (int m=0; m<p->moon_count; m++) {
                    p->moons[m].x += p->moons[m].vx * sim_dt * 5.0; 
                }
            }
            
            if (state.centered_planet_idx != -1) {
                Planet *p = &state.current_system.planets[state.centered_planet_idx];
                state.system_center_offset.x = p->x;
                state.system_center_offset.y = p->y;
            }
        }

        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (state.mode == VIEW_GALAXY) {
            int view_w = SCREEN_WIDTH / state.zoom;
            int view_h = SCREEN_HEIGHT / state.zoom;
            for (int x = (int)state.camera_pos.x - view_w/2; x <= (int)state.camera_pos.x + view_w/2; x++) {
                for (int y = (int)state.camera_pos.y - view_h/2; y <= (int)state.camera_pos.y + view_h/2; y++) {
                    if (star_exists(x, y)) {
                        int sx = (x - state.camera_pos.x) * state.zoom + SCREEN_WIDTH/2;
                        int sy = (y - state.camera_pos.y) * state.zoom + SCREEN_HEIGHT/2;
                        unsigned int s = get_seed(x,y); srand(s);
                        float temp = 5778 * powf(rand_float(0.1, 5.0), 0.5);
                        int r=255,g=255,b=255;
                        if (temp > 10000) {r=136;g=136;} else if (temp<4000) {g=68;b=0;} else if (temp<6000) {b=0;}
                        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                        draw_circle(renderer, sx, sy, MAX(2, state.zoom/5));
                    }
                }
            }
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            draw_text(renderer, 10, 10, "GALAXY N-BODY - Click star", 2);
        } else {
            // Draw System
            int cx = SCREEN_WIDTH/2 - state.system_center_offset.x * state.zoom;
            int cy = SCREEN_HEIGHT/2 - state.system_center_offset.y * state.zoom;
            
            // Stars
            for (int i=0; i<state.current_system.star_count; i++) {
                Star *s = &state.current_system.stars[i];
                int sx = cx + s->x * state.zoom;
                int sy = cy + s->y * state.zoom;
                Uint32 c = s->color;
                SDL_SetRenderDrawColor(renderer, (c>>24)&0xFF, (c>>16)&0xFF, (c>>8)&0xFF, 255);
                draw_circle(renderer, sx, sy, MAX(10, s->radius * 5 * (state.zoom/100.0)));
            }

            // Planets
            for (int i=0; i<state.current_system.planet_count; i++) {
                Planet *p = &state.current_system.planets[i];
                int px = cx + p->x * state.zoom;
                int py = cy + p->y * state.zoom;
                
                // Draw Body
                Uint32 c = p->color;
                SDL_SetRenderDrawColor(renderer, (c>>24)&0xFF, (c>>16)&0xFF, (c>>8)&0xFF, 255);
                int radius = (p->type==TYPE_GAS_GIANT)?6:3;
                draw_circle(renderer, px, py, radius);
                
                // Draw Rotation Line
                int rx = px + cos(p->rotation_angle) * radius;
                int ry = py + sin(p->rotation_angle) * radius;
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderDrawLine(renderer, px, py, rx, ry);
                
                // Moons
                for (int m=0; m<p->moon_count; m++) {
                    Moon *mn = &p->moons[m];
                    int mx = px + cos(mn->x) * mn->orbit_dist * (state.zoom/50.0 + 2.0);
                    int my = py + sin(mn->x) * mn->orbit_dist * (state.zoom/50.0 + 2.0);
                    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
                    draw_circle(renderer, mx, my, 1);
                }
                SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
                draw_text(renderer, px, py+10, p->name, 1);
            }
            
            // UI
            char buf[256];
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            sprintf(buf, "SYSTEM: %s (%d Stars) | Speed: %.1f", state.current_system.stars[0].name, state.current_system.star_count, state.time_speed);
            draw_text(renderer, 10, 10, buf, 2);
            
            int y=50;
            if (state.selected_star) {
                Star *s = &state.current_system.stars[0];
                draw_text(renderer, 10, y+=15, "SELECTED STAR:", 1);
                sprintf(buf, "Name: %s", s->name); draw_text(renderer, 10, y+=15, buf, 1);
                sprintf(buf, "Mass: %.2f Sol", s->mass); draw_text(renderer, 10, y+=15, buf, 1);
                sprintf(buf, "Lum: %.2f Lsol", s->luminosity); draw_text(renderer, 10, y+=15, buf, 1);
                sprintf(buf, "Temp: %.0f K", s->temp); draw_text(renderer, 10, y+=15, buf, 1);
            } else if (state.selected_planet_idx != -1) {
                Planet *p = &state.current_system.planets[state.selected_planet_idx];
                draw_text(renderer, 10, y+=15, "SELECTED PLANET:", 1);
                sprintf(buf, "Name: %s", p->name); draw_text(renderer, 10, y+=15, buf, 1);
                sprintf(buf, "Mass: %.2f", p->mass); draw_text(renderer, 10, y+=15, buf, 1);
                sprintf(buf, "Temp: %.0f K", p->surface_temp); draw_text(renderer, 10, y+=15, buf, 1);
                sprintf(buf, "Atm: %s", p->atmosphere); draw_text(renderer, 10, y+=15, buf, 1);
                sprintf(buf, "Locked: %s", p->is_tidally_locked?"YES":"NO"); draw_text(renderer, 10, y+=15, buf, 1);
                sprintf(buf, "Day: %.2f Days", p->rotation_period); draw_text(renderer, 10, y+=15, buf, 1);
                
                sprintf(buf, "Moons: %d", p->moon_count); draw_text(renderer, 10, y+=15, buf, 1);
                
                draw_text(renderer, 10, y+=15, "RESOURCES:", 1);
                // Find Top 3
                for (int k=0; k<3; k++) {
                    int max_idx = -1;
                    float max_val = -1.0;
                    for (int r=0; r<8; r++) {
                        if (p->resources[r] > max_val) {
                            bool used = false;
                            // Check if already displayed (hacky sort)
                            // To do this properly without modifying struct, we need a temp array or just print > threshold
                            // Let's just print any > 10% for now or fix sort
                        }
                    }
                }
                // Simpler approach: Iterate and print all > 15%
                const char* res_names[] = {"Iron", "Silicon", "Nickel", "Water", "Hydrogen", "Helium", "Gold", "RareEarths"};
                int printed = 0;
                for (int r=0; r<8; r++) {
                    if (p->resources[r] > 15.0) {
                        sprintf(buf, " %s: %.1f%%", res_names[r], p->resources[r]);
                        draw_text(renderer, 10, y+=15, buf, 1);
                        printed++;
                    }
                }
                if (printed == 0) draw_text(renderer, 10, y+=15, " Trace Elements", 1);
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