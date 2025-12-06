#include <SDL2/SDL.h>
#include "galaxy.h"
#include "font.h"
#include <math.h>
#include <stdio.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius) {
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w; // horizontal offset
            int dy = radius - h; // vertical offset
            if ((dx*dx + dy*dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("C-Galaxy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    GameState state;
    state.mode = VIEW_GALAXY;
    state.camera_pos.x = 0;
    state.camera_pos.y = 0;
    state.zoom = 20.0; // Grid units per screen
    state.running = true;
    state.selected_planet_idx = -1;
    state.selected_star = false;
    state.paused = false;
    state.time_speed = 1.0f;
    
    // Initialize Galaxy State Cache
    state.galaxy_camera_pos.x = 0;
    state.galaxy_camera_pos.y = 0;
    state.galaxy_zoom = 20.0;

    SDL_Event e;
    Uint32 last_time = SDL_GetTicks();

    while (state.running) {
        Uint32 current_time = SDL_GetTicks();
        float dt = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        // Events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) state.running = false;
            
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx, my;
                SDL_GetMouseState(&mx, &my);

                if (e.button.button == SDL_BUTTON_LEFT) {
                    if (state.mode == VIEW_GALAXY) {
                        // Screen to World
                        float wx = state.camera_pos.x + (mx - SCREEN_WIDTH/2) / state.zoom;
                        float wy = state.camera_pos.y + (my - SCREEN_HEIGHT/2) / state.zoom;
                        
                        int ix = round(wx);
                        int iy = round(wy);
                        
                        if (star_exists(ix, iy)) {
                            printf("Entering System %d, %d\n", ix, iy);
                            
                            // SAVE GALAXY STATE
                            state.galaxy_camera_pos = state.camera_pos;
                            state.galaxy_zoom = state.zoom;
                            
                            state.current_system = generate_system(ix, iy);
                            state.mode = VIEW_SYSTEM;
                            
                            // RESET FOR SYSTEM VIEW
                            state.camera_pos.x = 0;
                            state.camera_pos.y = 0;
                            state.zoom = 100.0; 
                            state.selected_planet_idx = -1;
                            state.selected_star = true;
                        }
                    } else {
                        // System View Click
                        int cx = SCREEN_WIDTH/2;
                        int cy = SCREEN_HEIGHT/2;
                        state.selected_planet_idx = -1;
                        state.selected_star = false;
                        
                        // Check Star Click
                        float d_star = sqrt(pow(mx - cx, 2) + pow(my - cy, 2));
                        if (d_star < 30) {
                            state.selected_star = true;
                        } else {
                            // Check Planets
                            for (int i=0; i<state.current_system.planet_count; i++) {
                                Planet p = state.current_system.planets[i];
                                int px = cx + cos(p.orbit_angle) * p.orbit_dist * state.zoom;
                                int py = cy + sin(p.orbit_angle) * p.orbit_dist * state.zoom;
                                
                                float dist = sqrt(pow(mx - px, 2) + pow(my - py, 2));
                                if (dist < 20) { // Click radius
                                    state.selected_planet_idx = i;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (e.button.button == SDL_BUTTON_RIGHT) {
                    if (state.mode == VIEW_SYSTEM) {
                        state.mode = VIEW_GALAXY;
                        // RESTORE GALAXY STATE
                        state.camera_pos = state.galaxy_camera_pos;
                        state.zoom = state.galaxy_zoom;
                    }
                }
            }
            
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_LEFT) state.camera_pos.x -= 10.0 / (state.zoom/20.0);
                if (e.key.keysym.sym == SDLK_RIGHT) state.camera_pos.x += 10.0 / (state.zoom/20.0);
                
                if (state.mode == VIEW_GALAXY) {
                    if (e.key.keysym.sym == SDLK_UP) state.camera_pos.y -= 10.0 / (state.zoom/20.0);
                    if (e.key.keysym.sym == SDLK_DOWN) state.camera_pos.y += 10.0 / (state.zoom/20.0);
                } else {
                    // System Controls
                    if (e.key.keysym.sym == SDLK_SPACE) state.paused = !state.paused;
                    if (e.key.keysym.sym == SDLK_UP) state.time_speed *= 1.5;
                    if (e.key.keysym.sym == SDLK_DOWN) state.time_speed /= 1.5;
                }
                
                // Zoom
                if (e.key.keysym.sym == SDLK_EQUALS || e.key.keysym.sym == SDLK_PLUS) state.zoom *= 1.1;
                if (e.key.keysym.sym == SDLK_MINUS) state.zoom /= 1.1;
            }
            
            if (e.type == SDL_MOUSEWHEEL) {
                if (e.wheel.y > 0) state.zoom *= 1.1;
                if (e.wheel.y < 0) state.zoom /= 1.1;
            }
        }

        // Update Physics (System View)
        if (state.mode == VIEW_SYSTEM && !state.paused) {
            for (int i=0; i<state.current_system.planet_count; i++) {
                state.current_system.planets[i].orbit_angle += state.current_system.planets[i].orbit_speed * dt * 0.5 * state.time_speed; 
                // Moons
                for (int m=0; m<state.current_system.planets[i].moon_count; m++) {
                     state.current_system.planets[i].moons[m].orbit_angle += state.current_system.planets[i].moons[m].orbit_speed * dt * state.time_speed;
                }
            }
        }

        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (state.mode == VIEW_GALAXY) {
            // Draw Stars
            int view_w = SCREEN_WIDTH / state.zoom;
            int view_h = SCREEN_HEIGHT / state.zoom;
            
            int start_x = (int)state.camera_pos.x - view_w/2 - 1;
            int end_x = (int)state.camera_pos.x + view_w/2 + 1;
            int start_y = (int)state.camera_pos.y - view_h/2 - 1;
            int end_y = (int)state.camera_pos.y + view_h/2 + 1;

            for (int x = start_x; x <= end_x; x++) {
                for (int y = start_y; y <= end_y; y++) {
                    if (star_exists(x, y)) {
                        // Draw Star
                        int sx = (x - state.camera_pos.x) * state.zoom + SCREEN_WIDTH/2;
                        int sy = (y - state.camera_pos.y) * state.zoom + SCREEN_HEIGHT/2;
                        
                        // Deterministic Color
                        unsigned int s = get_seed(x,y);
                        srand(s);
                        float mass = rand_float(0.1, 5.0);
                        float temp = 5778 * powf(mass, 0.5);
                        
                        int r=255, g=255, b=255;
                        if (temp > 10000) { r=136; g=136; b=255; } // Blue
                        else if (temp > 6000) { r=255; g=255; b=255; } // White
                        else if (temp > 4000) { r=255; g=255; b=0; } // Yellow
                        else { r=255; g=68; b=0; } // Red
                        
                        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                        draw_circle(renderer, sx, sy, MAX(2, state.zoom/5));
                    }
                }
            }
            
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            draw_text(renderer, 10, 10, "GALAXY VIEW - Click star to enter | Arrows: Pan | +/-: Zoom", 2);
            
        } else {
            // Draw System
            int cx = SCREEN_WIDTH/2;
            int cy = SCREEN_HEIGHT/2;
            
            // Star
            Uint32 c = state.current_system.star.color;
            SDL_SetRenderDrawColor(renderer, (c>>24)&0xFF, (c>>16)&0xFF, (c>>8)&0xFF, 255);
            draw_circle(renderer, cx, cy, 20); 

            // Planets
            for (int i=0; i<state.current_system.planet_count; i++) {
                Planet p = state.current_system.planets[i];
                
                int px = cx + cos(p.orbit_angle) * p.orbit_dist * state.zoom;
                int py = cy + sin(p.orbit_angle) * p.orbit_dist * state.zoom;
                
                // Orbit Path
                SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
                // Draw dot
                
                // Body
                c = p.color;
                SDL_SetRenderDrawColor(renderer, (c>>24)&0xFF, (c>>16)&0xFF, (c>>8)&0xFF, 255);
                int radius = (p.type == TYPE_GAS_GIANT || p.type == TYPE_ICE_GIANT) ? 6 : 3;
                draw_circle(renderer, px, py, radius);
                
                // Moons
                for (int m=0; m<p.moon_count; m++) {
                    Moon mn = p.moons[m];
                    int mx = px + cos(mn.orbit_angle) * mn.orbit_dist * (state.zoom/50.0 + 5.0); 
                    int my = py + sin(mn.orbit_angle) * mn.orbit_dist * (state.zoom/50.0 + 5.0);
                    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
                    draw_circle(renderer, mx, my, 1);
                }
                
                // Label
                SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
                draw_text(renderer, px, py + 10, p.name, 1);
            }
            
            // UI Overlay
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            char title[256];
            char speed_str[32];
            if (state.paused) sprintf(speed_str, "PAUSED");
            else sprintf(speed_str, "Speed: %.1fx", state.time_speed);
            
            sprintf(title, "SYSTEM: %s | %s | Space: Pause | Up/Down: Speed | +/-: Zoom", state.current_system.star.name, speed_str);
            draw_text(renderer, 10, 10, title, 2);
            
            int y = 60;
            if (state.selected_star) {
                Star s = state.current_system.star;
                char buf[128];
                draw_text(renderer, 10, y, "SELECTED STAR:", 1); y+=15;
                sprintf(buf, "Name: %s", s.name); draw_text(renderer, 10, y, buf, 1); y+=15;
                sprintf(buf, "Mass: %.2f Sol", s.mass); draw_text(renderer, 10, y, buf, 1); y+=15;
                sprintf(buf, "Temp: %.0f K", s.temp); draw_text(renderer, 10, y, buf, 1); y+=15;
            }
            else if (state.selected_planet_idx != -1) {
                Planet p = state.current_system.planets[state.selected_planet_idx];
                char buf[128];
                
                draw_text(renderer, 10, y, "SELECTED PLANET:", 1); y+=15;
                
                sprintf(buf, "Name: %s", p.name);
                draw_text(renderer, 10, y, buf, 1); y+=15;
                
                const char* types[] = {"Rocky", "Terrestrial", "Gas Giant", "Ice Giant", "Molten", "Icy World"};
                sprintf(buf, "Type: %s", types[p.type]);
                draw_text(renderer, 10, y, buf, 1); y+=15;
                
                sprintf(buf, "Mass: %.2f Earths", p.mass);
                draw_text(renderer, 10, y, buf, 1); y+=15;
                
                sprintf(buf, "Radius: %.2f Earths", p.radius);
                draw_text(renderer, 10, y, buf, 1); y+=15;
                
                sprintf(buf, "Orbit: %.2f AU", p.orbit_dist);
                draw_text(renderer, 10, y, buf, 1); y+=15;
                
                sprintf(buf, "Temp: %.0f K", p.surface_temp);
                draw_text(renderer, 10, y, buf, 1); y+=15;
                
                sprintf(buf, "Atmosphere: %s", p.atmosphere);
                draw_text(renderer, 10, y, buf, 1); y+=15;
                
                sprintf(buf, "Gravity: %.2f g", p.gravity);
                draw_text(renderer, 10, y, buf, 1); y+=15;
                
                sprintf(buf, "Locked: %s", p.is_tidally_locked ? "YES" : "NO");
                draw_text(renderer, 10, y, buf, 1); y+=15;
                
                sprintf(buf, "Day: %.2f Days", p.rotation_period);
                draw_text(renderer, 10, y, buf, 1); y+=15;
                
                sprintf(buf, "Moons: %d", p.moon_count);
                draw_text(renderer, 10, y, buf, 1); y+=15;
            }
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}