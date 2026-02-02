#pragma once

namespace Parameters {
    inline int WIDTH = 807;           // Window width
    inline int HEIGHT = 600;          // Window height
    inline float CENTER_X = 400.0f;   // Center x-coordinate (width / 2)
    inline float CENTER_Y = 300.0f;   // Center y-coordinate (height / 2)
    inline float ZOOM = 1.0f;         // Zoom level
    inline int MAX_ITERATIONS = 100;  // Max iterations for fractal computation
    inline float C_REAL = -0.8f;      // Real part of Julia set constant c
    inline float C_IMAG = 0.156f;     // Imaginary part of Julia set constant c
    inline float ESCAPE_RADIUS_SQ = 4.0f; // Escape radius squared
    inline float TWO_MULTIPLIER = 2.0f;   // Multiplier for imaginary part
}