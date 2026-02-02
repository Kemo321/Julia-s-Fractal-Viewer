#pragma once
#include <SDL2/SDL.h>
#include "Renderer.hpp"
#include "Parameters.hpp"

class Window {
    SDL_Window* window;
    Renderer* renderer;
    SDL_Event event;
    bool is_running;

public:
    Window(const char* title, int x, int y, int width, int height, Uint32 flags);
    ~Window();
    void start();
    void draw_fractal();

private:
    void main_loop();
};