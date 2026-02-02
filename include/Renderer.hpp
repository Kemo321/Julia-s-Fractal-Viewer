#pragma once
#include <SDL2/SDL.h>
#include <xsimd/xsimd.hpp>
#include <vector>
#include <thread>
#include <atomic>
#include <barrier> 
#include "Parameters.hpp"

class Renderer {
    SDL_Renderer* renderer;
    SDL_Window* window;
    SDL_Texture* texture;
    std::vector<int> colors;
    std::vector<Uint32> pixel_buffer;
    float scale_x;
    float scale_y;

    // Thread pool members
    std::vector<std::thread> workers;
    std::atomic<bool> stop;
    std::barrier<> sync_barrier; 

public:
    Renderer(SDL_Window* window);
    ~Renderer();
    void init();
    void generate_gradient();
    void save_screenshot();
    void stop_loop();
    void set_zoom(float zoom) { 
        scale_x = 2.0f * 4 / (Parameters::WIDTH * zoom); 
        scale_y = 2.0f * 4 / (Parameters::HEIGHT * zoom); 
    }
    
    void render();

private:
    void worker_thread(int start_y, int end_y);
    void compute_fractal_chunk(int start_y, int end_y);
    void setup_constants(float& cy, int y, xsimd::simd_type<float>& c_re, xsimd::simd_type<float>& c_im,
                        xsimd::simd_type<float>& four, xsimd::simd_type<float>& two,
                        xsimd::simd_type<float>& max_iter) const;
    void compute_simd_chunk(int x, int y, const xsimd::simd_type<float>& c_re,
                          const xsimd::simd_type<float>& c_im, const xsimd::simd_type<float>& four,
                          const xsimd::simd_type<float>& two, const xsimd::simd_type<float>& max_iter,
                          float cy);
    void compute_scalar_pixel(int x, int y, float cy);
    xsimd::simd_type<float> compute_simd_iterations(xsimd::simd_type<float>& zx,
                                                  xsimd::simd_type<float>& zy,
                                                  const xsimd::simd_type<float>& c_re,
                                                  const xsimd::simd_type<float>& c_im,
                                                  const xsimd::simd_type<float>& four,
                                                  const xsimd::simd_type<float>& max_iter,
                                                  const xsimd::simd_type<float>& two
                                                ) const;
};