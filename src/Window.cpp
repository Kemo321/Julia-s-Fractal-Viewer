#include "Window.hpp"
#include <iostream>

Window::Window(const char* title, int x, int y, int width, int height, Uint32 flags)
    : window(nullptr), renderer(nullptr), is_running(false) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) throw std::runtime_error("SDL Init failed");

    Parameters::WIDTH = width;
    Parameters::HEIGHT = height;
    Parameters::CENTER_X = width / 2.0f;
    Parameters::CENTER_Y = height / 2.0f;

    window = SDL_CreateWindow(title, x, y, width, height, flags);
    if (!window) throw std::runtime_error("Window creation failed");

    renderer = new Renderer(window);
    is_running = true;
}

Window::~Window() {
    delete renderer;
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void Window::start() {
    main_loop();
}

void Window::main_loop() {
    bool needs_redraw = true;

    while (is_running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                is_running = false;
            } else if (event.type == SDL_KEYDOWN) {
                bool params_changed = false;

                switch (event.key.keysym.sym) {
                    case SDLK_r:
                        Parameters::CENTER_X = 400.0f;
                        Parameters::CENTER_Y = 300.0f;
                        Parameters::ZOOM = 1.0f;
                        Parameters::C_REAL = -0.8f;
                        Parameters::C_IMAG = 0.156f;
                        params_changed = true;
                        break;
                    case SDLK_UP: Parameters::CENTER_Y += 50.0; params_changed = true; break;
                    case SDLK_DOWN: Parameters::CENTER_Y -= 50.0; params_changed = true; break;
                    case SDLK_LEFT: Parameters::CENTER_X += 50.0; params_changed = true; break;
                    case SDLK_RIGHT: Parameters::CENTER_X -= 50.0; params_changed = true; break;
                    case SDLK_MINUS: Parameters::ZOOM *= 0.8f; params_changed = true; break;
                    case SDLK_EQUALS: Parameters::ZOOM *= 1.25f; params_changed = true; break;
                    case SDLK_w: Parameters::C_IMAG += 0.001f; params_changed = true; break;
                    case SDLK_s: Parameters::C_IMAG -= 0.001f; params_changed = true; break;
                    case SDLK_a: Parameters::C_REAL -= 0.001f; params_changed = true; break;
                    case SDLK_d: Parameters::C_REAL += 0.001f; params_changed = true; break;
                    case SDLK_g: 
                        renderer->generate_gradient(); 
                        params_changed = true; 
                        break;
                    case SDLK_f: 
                        renderer->save_screenshot(); 
                        break;
                    case SDLK_ESCAPE:
                        is_running = false;
                        renderer->stop_loop();
                        break;
                }
                
                if (params_changed) {
                    renderer->set_zoom(Parameters::ZOOM);
                    needs_redraw = true;
                }
            }
        }
        
        if (is_running) {
            if (needs_redraw) {
                renderer->render();
                needs_redraw = false;
            } else {
                SDL_Delay(16);
            }
        }
    }
}