#include "Parameters.hpp"
#include "Window.hpp"
#include <SDL2/SDL.h>
#include <iostream>

int main(int argc, char** argv)
{
    Window* window = new Window("Julia Set",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        Parameters::WIDTH, Parameters::HEIGHT,
        SDL_WINDOW_SHOWN);
    window->start();
    delete window;
    return 0;
}