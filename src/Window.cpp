#include "Window.hpp"
#include <iostream>

Window::Window(const char* title, int x, int y, int width, int height, Uint32 flags)
    : window(nullptr), renderer(nullptr), isRunning(false) {
    std::cout << "Initializing SDL\n";
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        throw std::runtime_error("SDL initialization failed");
    }

    std::cout << "Creating window\n";
    // Update Parameters with constructor values
    Parameters::WIDTH = width;
    Parameters::HEIGHT = height;
    Parameters::CENTER_X = width / 2.0f;
    Parameters::CENTER_Y = height / 2.0f;

    window = SDL_CreateWindow(title, x, y, width, height, flags);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        throw std::runtime_error("Failed to create window");
    }

    std::cout << "Creating renderer\n";
    renderer = new Renderer(window);

    std::cout << "Window initialized\n";
    isRunning = true; // Set to true after successful initialization
}

Window::~Window() {
    std::cout << "Destroying window\n";
    delete renderer;
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    std::cout << "Window destroyed\n";
}

void Window::start() {
    std::cout << "Starting window\n";
    drawFractal(); // Initial draw
    mainLoop();    // Enter main event loop
}

void Window::mainLoop() {
    std::cout << "Starting event loop\n";
    while (isRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                isRunning = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP:
                        Parameters::CENTER_Y += 50.0;
                        break; // Pan up
                    case SDLK_DOWN:
                        Parameters::CENTER_Y -= 50.0;
                        break; // Pan down
                    case SDLK_LEFT:
                        Parameters::CENTER_X += 50.0 ;
                        break; // Pan left
                    case SDLK_RIGHT:
                        Parameters::CENTER_X -= 50.0;
                        break; // Pan right
                    case SDLK_MINUS:
                        Parameters::ZOOM *= 0.8f;
                        break; // Zoom out
                    case SDLK_EQUALS:
                        Parameters::ZOOM *= 1.25f;
                        break; // Zoom in
                    case SDLK_w:
                        Parameters::C_IMAG += 0.01f;
                        break; // Adjust cImag up
                    case SDLK_s:
                        Parameters::C_IMAG -= 0.01f;
                        break; // Adjust cImag down
                    case SDLK_a:
                        Parameters::C_REAL -= 0.01f;
                        break; // Adjust cReal left
                    case SDLK_d:
                        Parameters::C_REAL += 0.01f;
                        break; // Adjust cReal right
                    case SDLK_r:
                        renderer->generateGradient();
                        break; // Regenerate gradient
                    case SDLK_f:
                        renderer->saveScreenshot();
                        break; // Save screenshot
                    case SDLK_ESCAPE:
                        isRunning = false; // Exit on ESC
                        break;
                    
                }
                // Update Renderer scale based on new ZOOM value
                renderer->setZoom(Parameters::ZOOM);
            }
        }
        drawFractal(); // Redraw after each event to reflect changes
    }
    std::cout << "Event loop exited\n";
}

void Window::drawFractal() {
    renderer->drawFractal();
}