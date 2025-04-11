#include <iostream>
#include <SDL2/SDL.h>
#include "Window.hpp"
#include "Parameters.hpp"

int main(int argc, char** argv) {
    // Use parameters from Parameters.hpp for window creation
    Window* window = new Window("Julia Set", 
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                Parameters::WIDTH, Parameters::HEIGHT, 
                                SDL_WINDOW_SHOWN);
    window->start(); // Start the window's main loop
    delete window;   // Clean up
    return 0;
}