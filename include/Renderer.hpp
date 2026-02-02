#pragma once
#include <SDL2/SDL.h>
#include <xsimd/xsimd.hpp>
#include <array>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "Parameters.hpp"

struct Task {
    int start_y;
    int end_y;
};

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
    std::queue<Task> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<bool> tasks_available;
    std::atomic<int> tasks_completed;

public:
    Renderer(SDL_Window* window);
    ~Renderer();
    void draw_fractal();
    void init();
    void generate_gradient();
    void save_screenshot();
    void set_zoom(float zoom) { 
        scale_x = 2.0f * 4 / (Parameters::WIDTH * zoom); 
        scale_y = 2.0f * 4 / (Parameters::HEIGHT * zoom); 
    }

private:
    void worker_thread();
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