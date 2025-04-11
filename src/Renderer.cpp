#include "Renderer.hpp"
#include <cmath>
#include <iostream>

namespace xs = xsimd;
using floatv = xs::simd_type<float>;

// Constructor initializes using Parameters.hpp
Renderer::Renderer(SDL_Window* window)
    : window(window), renderer(nullptr), texture(nullptr),
      scaleX(2.0f * 4 / (Parameters::WIDTH * Parameters::ZOOM)),
      scaleY(2.0f * 4 / (Parameters::HEIGHT * Parameters::ZOOM)),
      stop(false), tasksAvailable(false), tasksCompleted(0) {
    std::cout << "Renderer constructor started\n";

    // Check if the window is valid
    if (!window) {
        std::cerr << "Error: Null window passed to Renderer\n";
        throw std::runtime_error("Null window");
    }

    // Allocate pixel buffer
    pixelBuffer.resize(Parameters::WIDTH * Parameters::HEIGHT, 0);
    std::cout << "Pixel buffer allocated\n";

    // Initialize SDL renderer and texture
    init();
    std::cout << "Renderer initialized\n";

    // Start worker threads
    const int numThreads = std::thread::hardware_concurrency();
    std::cout << "Starting " << numThreads << " worker threads\n";
    for (int i = 0; i < numThreads; ++i) {
        workers.emplace_back(&Renderer::workerThread, this);
    }
    std::cout << "Renderer constructor completed\n";
}

Renderer::~Renderer() {
    std::cout << "Renderer destructor started\n";

    // Signal worker threads to stop
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();

    // Join all worker threads
    for (auto& worker : workers) {
        if (worker.joinable()) worker.join();
    }

    // Destroy SDL resources
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);

    std::cout << "Renderer destructor completed\n";
}

// Initialize SDL renderer and texture
void Renderer::init() {
    std::cout << "Initializing renderer\n";

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        throw std::runtime_error("Failed to create SDL renderer");
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                Parameters::WIDTH, Parameters::HEIGHT);
    if (!texture) {
        std::cerr << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_Quit();
        throw std::runtime_error("Failed to create SDL texture");
    }

    // Generate color gradient
    generateGradient();
    std::cout << "Renderer initialization completed\n";
}

// Draw the fractal
void Renderer::drawFractal() {
    const int numThreads = workers.size();
    const int chunkSize = Parameters::HEIGHT / numThreads;
    tasksCompleted = 0;

    // Prepare tasks for worker threads
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!tasks.empty()) tasks.pop();
        for (int i = 0; i < numThreads; ++i) {
            int startY = i * chunkSize;
            int endY = (i == numThreads - 1) ? Parameters::HEIGHT : startY + chunkSize;
            tasks.push({startY, endY});
        }
        tasksAvailable = true;
    }
    condition.notify_all();

    // Wait for all tasks to complete
    std::unique_lock<std::mutex> lock(queueMutex);
    condition.wait(lock, [this, numThreads] { return tasksCompleted >= numThreads; });

    // Update texture and render
    SDL_UpdateTexture(texture, nullptr, pixelBuffer.data(), Parameters::WIDTH * sizeof(Uint32));
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

// Worker thread function
void Renderer::workerThread() {
    while (!stop) {
        Task task;

        // Wait for tasks or stop signal
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return stop || (!tasks.empty() && tasksAvailable); });
            if (stop && tasks.empty()) return;

            if (!tasks.empty()) {
                task = tasks.front();
                tasks.pop();
                if (tasks.empty()) tasksAvailable = false;
                lock.unlock();

                // Compute the fractal chunk
                computeFractalChunk(task.startY, task.endY);
                tasksCompleted++;
                condition.notify_all();
            }
        }
    }
}

// Compute a chunk of the fractal
void Renderer::computeFractalChunk(int startY, int endY) {
    using floatv = xs::simd_type<float>;
    const int vecSize = floatv::size;

    floatv cRe, cIm, four, two, maxIter;
    float cy;

    for (int y = startY; y < endY; ++y) {
        setupConstants(cy, y, cRe, cIm, four, two, maxIter);

        int x = 0;
        for (; x <= Parameters::WIDTH - vecSize; x += vecSize) {
            computeSIMDChunk(x, y, cRe, cIm, four, two, maxIter, cy);
        }
    }
}

// Setup constants for SIMD computation
void Renderer::setupConstants(float& cy, int y, floatv& cRe, floatv& cIm,
                              floatv& four, floatv& two, floatv& maxIter) const {
    cy = (static_cast<float>(y) - Parameters::CENTER_Y) * scaleY;
    cRe = xs::broadcast<float>(Parameters::C_REAL);
    cIm = xs::broadcast<float>(Parameters::C_IMAG);
    four = xs::broadcast<float>(Parameters::ESCAPE_RADIUS_SQ);
    two = xs::broadcast<float>(Parameters::TWO_MULTIPLIER);
    maxIter = xs::broadcast<float>(static_cast<float>(Parameters::MAX_ITERATIONS));
}

// Compute a chunk of the fractal using SIMD
void Renderer::computeSIMDChunk(int x, int y, const floatv& cRe, const floatv& cIm,
                                const floatv& four, const floatv& two, const floatv& maxIter,
                                float cy) {
    const int vecSize = floatv::size;

    std::array<float, vecSize> xCoords;
    for (int i = 0; i < vecSize; ++i) {
        xCoords[i] = static_cast<float>(x + i);
    }

    floatv xVec = xs::load_aligned(xCoords.data());
    floatv zx = (xVec - floatv(Parameters::CENTER_X)) * floatv(scaleX);
    floatv zy = xs::broadcast<float>(cy);

    floatv iterations = computeSIMDIterations(zx, zy, cRe, cIm, four, maxIter, two);

    std::array<float, vecSize> iterArray;
    std::array<float, vecSize> zxArray;
    std::array<float, vecSize> zyArray;
    iterations.store_unaligned(iterArray.data());
    zx.store_unaligned(zxArray.data());
    zy.store_unaligned(zyArray.data());

    for (int i = 0; i < vecSize; ++i) {
        int idx = y * Parameters::WIDTH + x + i;
        pixelBuffer[idx] = colors[std::min(static_cast<int>(iterArray[i]), static_cast<int>(colors.size()) - 1)];
    }
}

// Compute iterations for SIMD vectors
xsimd::batch<float, xsimd::default_arch> Renderer::computeSIMDIterations(
    xsimd::batch<float, xsimd::default_arch>& zx,
    xsimd::batch<float, xsimd::default_arch>& zy,
    const xsimd::batch<float, xsimd::default_arch>& cRe,
    const xsimd::batch<float, xsimd::default_arch>& cIm,
    const xsimd::batch<float, xsimd::default_arch>& four,
    const xsimd::batch<float, xsimd::default_arch>& maxIter,
    const xsimd::batch<float, xsimd::default_arch>& two) const {
    auto iterations = xsimd::batch<float, xsimd::default_arch>(0.0f);

    for (int iter = 0; iter < Parameters::MAX_ITERATIONS; ++iter) {
        auto zx2 = zx * zx;
        auto zy2 = zy * zy;
        auto mag2 = zx2 + zy2;
        auto mask = (mag2 < four) & (iterations < maxIter);

        if (xsimd::all(!(mask))) {
            break;
        }

        auto temp = zx2 - zy2 + cRe;
        zy = two * zx * zy + cIm;
        zx = temp;
        iterations = iterations + xsimd::select(mask, xsimd::batch<float, xsimd::default_arch>(1.0f),
                                                     xsimd::batch<float, xsimd::default_arch>(0.0f));
    }
    return iterations;
}

// Compute a single pixel using scalar computation
void Renderer::computeScalarPixel(int x, int y, float cy) {
    float zx = (static_cast<float>(x) - Parameters::CENTER_X) * scaleX;
    float zy = cy;
    const float two = Parameters::TWO_MULTIPLIER;
    int iter = 0;

    for (; iter < Parameters::MAX_ITERATIONS; ++iter) {
        float zx2 = zx * zx;
        float zy2 = zy * zy;
        if (zx2 + zy2 >= Parameters::ESCAPE_RADIUS_SQ) break;
        float temp = zx2 - zy2 + Parameters::C_REAL;
        zy = two * zx * zy + Parameters::C_IMAG;
        zx = temp;

        if (zx2 + zy2 >= Parameters::ESCAPE_RADIUS_SQ) break;

    }

    int idx = y * Parameters::WIDTH + x;
    pixelBuffer[idx] = colors[std::min(iter, static_cast<int>(colors.size()) - 1)];
}

// Generate a color gradient
void Renderer::generateGradient() {
    const int gradientSteps = 256;
    std::vector<Uint32> randomColors(10);

    // Generate 10 random colors
    for (int i = 0; i < 10; ++i) {
        int r = rand() % 256;
        int g = rand() % 256;
        int b = rand() % 256;
        randomColors[i] = (r << 16) | (g << 8) | b;
    }

    // Create a gradient by interpolating between the random colors
    colors.resize(gradientSteps);
    int segmentLength = gradientSteps / (randomColors.size() - 1);

    for (size_t i = 0; i < randomColors.size() - 1; ++i) {
        Uint32 startColor = randomColors[i];
        Uint32 endColor = randomColors[i + 1];

        int startR = (startColor >> 16) & 0xFF;
        int startG = (startColor >> 8) & 0xFF;
        int startB = startColor & 0xFF;

        int endR = (endColor >> 16) & 0xFF;
        int endG = (endColor >> 8) & 0xFF;
        int endB = endColor & 0xFF;

        for (int j = 0; j < segmentLength; ++j) {
            float t = static_cast<float>(j) / segmentLength;
            int r = static_cast<int>(startR + t * (endR - startR));
            int g = static_cast<int>(startG + t * (endG - startG));
            int b = static_cast<int>(startB + t * (endB - startB));
            colors[i * segmentLength + j] = (r << 16) | (g << 8) | b;
        }
    }

    // Handle the last color to ensure the gradient fills completely
    colors[gradientSteps - 1] = randomColors.back();
}

// Save a screenshot to a file
void Renderer::saveScreenshot() {
    
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(pixelBuffer.data(), Parameters::WIDTH, Parameters::HEIGHT,
    32, Parameters::WIDTH * sizeof(Uint32), SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        std::cerr << "SDL_CreateRGBSurface Error: " << SDL_GetError() << std::endl;
        return;
    }
    
    // Generate a filename based on the current time
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char filename[64];
    strftime(filename, sizeof(filename), "screenshot_%Y%m%d_%H%M%S.bmp", timeinfo);
    std::cout << "Screenshot filename: " << filename << std::endl;

    SDL_SaveBMP(surface, filename);
    SDL_FreeSurface(surface);
}