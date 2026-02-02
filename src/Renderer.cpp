#include "Renderer.hpp"
#include <cmath>
#include <iostream>
#include <ctime>

namespace xs = xsimd;

Renderer::Renderer(SDL_Window* window)
    : window(window), 
      renderer(nullptr), 
      texture(nullptr),
      scale_x(2.0f * 4 / (Parameters::WIDTH * Parameters::ZOOM)),
      scale_y(2.0f * 4 / (Parameters::HEIGHT * Parameters::ZOOM)),
      stop(false),
      sync_barrier(std::thread::hardware_concurrency() + 1) 
{
    std::cout << "Renderer constructor started\n";

    if (!window) {
        throw std::runtime_error("Null window");
    }

    pixel_buffer.resize(Parameters::WIDTH * Parameters::HEIGHT, 0);
    init();

    const int num_threads = std::thread::hardware_concurrency();
    std::cout << "Starting " << num_threads << " worker threads\n";

    for (int i = 0; i < num_threads; ++i) {
        int start_y = i * (Parameters::HEIGHT / num_threads);
        int end_y = (i == num_threads - 1) ? Parameters::HEIGHT : start_y + (Parameters::HEIGHT / num_threads);
        workers.emplace_back(&Renderer::worker_thread, this, start_y, end_y);
    }
}

Renderer::~Renderer() {
    std::cout << "Renderer destructor started\n";
    stop_loop();
    sync_barrier.arrive_and_drop();

    for (auto& worker : workers) {
        if (worker.joinable()) worker.join();
    }

    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
}

void Renderer::stop_loop() {
    stop = true;
}

void Renderer::init() {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                Parameters::WIDTH, Parameters::HEIGHT);
    generate_gradient();
}

void Renderer::render() {
    sync_barrier.arrive_and_wait();
    sync_barrier.arrive_and_wait();

    SDL_UpdateTexture(texture, nullptr, pixel_buffer.data(), Parameters::WIDTH * sizeof(Uint32));
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

void Renderer::worker_thread(int start_y, int end_y) {
    while (true) {
        sync_barrier.arrive_and_wait();
        
        if (stop) {
            sync_barrier.arrive_and_drop(); 
            break;
        }

        compute_fractal_chunk(start_y, end_y);
        sync_barrier.arrive_and_wait();
    }
}

void Renderer::compute_fractal_chunk(int start_y, int end_y) {
    using floatv = xs::simd_type<float>;
    const int vec_size = floatv::size;
    floatv c_re, c_im, four, two, max_iter;
    float cy;

    for (int y = start_y; y < end_y; ++y) {
        setup_constants(cy, y, c_re, c_im, four, two, max_iter);
        int x = 0;
        for (; x <= Parameters::WIDTH - vec_size; x += vec_size) {
            compute_simd_chunk(x, y, c_re, c_im, four, two, max_iter, cy);
        }
        for (; x < Parameters::WIDTH; ++x) {
            compute_scalar_pixel(x, y, cy);
        }
    }
}

void Renderer::setup_constants(float& cy, int y, xsimd::simd_type<float>& c_re, xsimd::simd_type<float>& c_im,
                               xsimd::simd_type<float>& four, xsimd::simd_type<float>& two, xsimd::simd_type<float>& max_iter) const {
    cy = (static_cast<float>(y) - Parameters::CENTER_Y) * scale_y;
    c_re = xs::broadcast<float>(Parameters::C_REAL);
    c_im = xs::broadcast<float>(Parameters::C_IMAG);
    four = xs::broadcast<float>(Parameters::ESCAPE_RADIUS_SQ);
    two = xs::broadcast<float>(Parameters::TWO_MULTIPLIER);
    max_iter = xs::broadcast<float>(static_cast<float>(Parameters::MAX_ITERATIONS));
}

void Renderer::compute_simd_chunk(int x, int y, const xsimd::simd_type<float>& c_re, const xsimd::simd_type<float>& c_im,
                                  const xsimd::simd_type<float>& four, const xsimd::simd_type<float>& two, const xsimd::simd_type<float>& max_iter,
                                  float cy) {
    using floatv = xs::simd_type<float>;
    const int vec_size = floatv::size;

    std::array<float, vec_size> x_coords;
    for (int i = 0; i < vec_size; ++i) {
        x_coords[i] = static_cast<float>(x + i);
    }

    floatv x_vec = xs::load_aligned(x_coords.data());
    floatv zx = (x_vec - floatv(Parameters::CENTER_X)) * floatv(scale_x);
    floatv zy = xs::broadcast<float>(cy);

    floatv iterations = compute_simd_iterations(zx, zy, c_re, c_im, four, max_iter, two);

    std::array<float, vec_size> iter_array;
    iterations.store_unaligned(iter_array.data());

    for (int i = 0; i < vec_size; ++i) {
        int idx = y * Parameters::WIDTH + x + i;
        pixel_buffer[idx] = colors[std::min(static_cast<int>(iter_array[i]), static_cast<int>(colors.size()) - 1)];
    }
}

xsimd::simd_type<float> Renderer::compute_simd_iterations(
    xsimd::simd_type<float>& zx, xsimd::simd_type<float>& zy,
    const xsimd::simd_type<float>& c_re, const xsimd::simd_type<float>& c_im,
    const xsimd::simd_type<float>& four, const xsimd::simd_type<float>& max_iter,
    const xsimd::simd_type<float>& two) const {
    
    auto iterations = xsimd::batch<float, xsimd::default_arch>(0.0f);

    for (int iter = 0; iter < Parameters::MAX_ITERATIONS; ++iter) {
        auto zx2 = zx * zx;
        auto zy2 = zy * zy;
        auto mag2 = zx2 + zy2;
        auto mask = (mag2 < four) & (iterations < max_iter);

        if (xsimd::all(!(mask))) {
            break;
        }

        auto temp = zx2 - zy2 + c_re;
        zy = two * zx * zy + c_im;
        zx = temp;
        iterations = iterations + xsimd::select(mask, xsimd::batch<float, xsimd::default_arch>(1.0f),
                                                     xsimd::batch<float, xsimd::default_arch>(0.0f));
    }
    return iterations;
}

void Renderer::compute_scalar_pixel(int x, int y, float cy) {
    float zx = (static_cast<float>(x) - Parameters::CENTER_X) * scale_x;
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
    }

    int idx = y * Parameters::WIDTH + x;
    pixel_buffer[idx] = colors[std::min(iter, static_cast<int>(colors.size()) - 1)];
}

void Renderer::generate_gradient() {
    const int gradient_steps = 256;
    std::vector<Uint32> random_colors(10);
    for (int i = 0; i < 10; ++i) {
        random_colors[i] = (rand() % 256 << 16) | (rand() % 256 << 8) | rand() % 256;
    }
    colors.resize(gradient_steps);
    int segment_length = gradient_steps / (random_colors.size() - 1);

    for (size_t i = 0; i < random_colors.size() - 1; ++i) {
        Uint32 start = random_colors[i];
        Uint32 end = random_colors[i + 1];
        for (int j = 0; j < segment_length; ++j) {
            float t = (float)j / segment_length;
            int r = ((start >> 16) & 0xFF) * (1-t) + ((end >> 16) & 0xFF) * t;
            int g = ((start >> 8) & 0xFF) * (1-t) + ((end >> 8) & 0xFF) * t;
            int b = (start & 0xFF) * (1-t) + (end & 0xFF) * t;
            colors[i * segment_length + j] = (r << 16) | (g << 8) | b;
        }
    }
    for(size_t i=0; i<colors.size(); ++i) if(colors[i]==0) colors[i] = random_colors.back();
}

void Renderer::save_screenshot() {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(pixel_buffer.data(), Parameters::WIDTH, Parameters::HEIGHT,
    32, Parameters::WIDTH * sizeof(Uint32), SDL_PIXELFORMAT_ARGB8888);
    if (surface) {
        char filename[64];
        time_t now = time(nullptr);
        strftime(filename, sizeof(filename), "screenshot_%Y%m%d_%H%M%S.bmp", localtime(&now));
        SDL_SaveBMP(surface, filename);
        SDL_FreeSurface(surface);
        std::cout << "Saved " << filename << "\n";
    }
}