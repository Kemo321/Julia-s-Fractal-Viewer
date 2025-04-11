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
    int startY;
    int endY;
};

class Renderer {
    SDL_Renderer* renderer;
    SDL_Window* window;
    SDL_Texture* texture;
    std::vector<int> colors;
    std::vector<Uint32> pixelBuffer;
    float scaleX;
    float scaleY;

    // Thread pool members
    std::vector<std::thread> workers;
    std::queue<Task> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<bool> tasksAvailable;
    std::atomic<int> tasksCompleted;

public:
    Renderer(SDL_Window* window);
    ~Renderer();
    void drawFractal();
    void init();
    void generateGradient();
    void saveScreenshot();
    void setZoom(float zoom) { 
        scaleX = 2.0f * 4 / (Parameters::WIDTH * zoom); 
        scaleY = 2.0f * 4 / (Parameters::HEIGHT * zoom); 
    }

private:
    void workerThread();
    void computeFractalChunk(int startY, int endY);
    void setupConstants(float& cy, int y, xsimd::simd_type<float>& cRe, xsimd::simd_type<float>& cIm,
                        xsimd::simd_type<float>& four, xsimd::simd_type<float>& two,
                        xsimd::simd_type<float>& maxIter) const;
    void computeSIMDChunk(int x, int y, const xsimd::simd_type<float>& cRe,
                          const xsimd::simd_type<float>& cIm, const xsimd::simd_type<float>& four,
                          const xsimd::simd_type<float>& two, const xsimd::simd_type<float>& maxIter,
                          float cy);
    void computeScalarPixel(int x, int y, float cy);
    xsimd::simd_type<float> computeSIMDIterations(xsimd::simd_type<float>& zx,
                                                  xsimd::simd_type<float>& zy,
                                                  const xsimd::simd_type<float>& cRe,
                                                  const xsimd::simd_type<float>& cIm,
                                                  const xsimd::simd_type<float>& four,
                                                  const xsimd::simd_type<float>& maxIter,
                                                  const xsimd::simd_type<float>& two
                                                ) const;
};