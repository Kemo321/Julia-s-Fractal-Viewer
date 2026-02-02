#include "Renderer.hpp"
#include <cmath>
#include <iostream>

namespace xs = xsimd;
using floatv = xs::simd_type<float>;

// Constructor initializes using Parameters.hpp
Renderer::Renderer(SDL_Window* window)
    : window(window), renderer(nullptr), texture(nullptr),
      scale_x(2.0f * 4 / (Parameters::WIDTH * Parameters::ZOOM)),
      scale_y(2.0f * 4 / (Parameters::HEIGHT * Parameters::ZOOM)),
      stop(false), tasks_available(false), tasks_completed(0) {
    std::cout << "Renderer constructor started\n";

    if (!window) {
        std::cerr << "Error: Null window passed to Renderer\n";
        throw std::runtime_error("Null window");
    }

    pixel_buffer.resize(Parameters::WIDTH * Parameters::HEIGHT, 0);
    std::cout << "Pixel buffer allocated\n";

    init();
    std::cout << "Renderer initialized\n";

    const int num_threads = std::thread::hardware_concurrency();
    std::cout << "Starting " << num_threads << " worker threads\n";
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back(&Renderer::worker_thread, this);
    }
    std::cout << "Renderer constructor completed\n";
}

Renderer::~Renderer() {
    std::cout << "Renderer destructor started\n";

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();

    for (auto& worker : workers) {
        if (worker.joinable()) worker.join();
    }

    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);

    std::cout << "Renderer destructor completed\n";
}

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

    generate_gradient();
    std::cout << "Renderer initialization completed\n";
}

void Renderer::draw_fractal() {
    const int num_threads = workers.size();
    const int chunk_size = Parameters::HEIGHT / num_threads;
    tasks_completed = 0;

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!tasks.empty()) tasks.pop();
        for (int i = 0; i < num_threads; ++i) {
            int start_y = i * chunk_size;
            int end_y = (i == num_threads - 1) ? Parameters::HEIGHT : start_y + chunk_size;
            tasks.push({start_y, end_y});
        }
        tasks_available = true;
    }
    condition.notify_all();

    std::unique_lock<std::mutex> lock(queue_mutex);
    condition.wait(lock, [this, num_threads] { return tasks_completed >= num_threads; });

    SDL_UpdateTexture(texture, nullptr, pixel_buffer.data(), Parameters::WIDTH * sizeof(Uint32));
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

void Renderer::worker_thread() {
    while (!stop) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this] { return stop || (!tasks.empty() && tasks_available); });
            if (stop && tasks.empty()) return;

            if (!tasks.empty()) {
                task = tasks.front();
                tasks.pop();
                if (tasks.empty()) tasks_available = false;
                lock.unlock();

                compute_fractal_chunk(task.start_y, task.end_y);
                tasks_completed++;
                condition.notify_all();
            }
        }
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
    }
}

void Renderer::setup_constants(float& cy, int y, floatv& c_re, floatv& c_im,
                               floatv& four, floatv& two, floatv& max_iter) const {
    cy = (static_cast<float>(y) - Parameters::CENTER_Y) * scale_y;
    c_re = xs::broadcast<float>(Parameters::C_REAL);
    c_im = xs::broadcast<float>(Parameters::C_IMAG);
    four = xs::broadcast<float>(Parameters::ESCAPE_RADIUS_SQ);
    two = xs::broadcast<float>(Parameters::TWO_MULTIPLIER);
    max_iter = xs::broadcast<float>(static_cast<float>(Parameters::MAX_ITERATIONS));
}

void Renderer::compute_simd_chunk(int x, int y, const floatv& c_re, const floatv& c_im,
                                  const floatv& four, const floatv& two, const floatv& max_iter,
                                  float cy) {
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
    std::array<float, vec_size> zx_array;
    std::array<float, vec_size> zy_array;
    iterations.store_unaligned(iter_array.data());
    zx.store_unaligned(zx_array.data());
    zy.store_unaligned(zy_array.data());

    for (int i = 0; i < vec_size; ++i) {
        int idx = y * Parameters::WIDTH + x + i;
        pixel_buffer[idx] = colors[std::min(static_cast<int>(iter_array[i]), static_cast<int>(colors.size()) - 1)];
    }
}

xsimd::batch<float, xsimd::default_arch> Renderer::compute_simd_iterations(
    xsimd::batch<float, xsimd::default_arch>& zx,
    xsimd::batch<float, xsimd::default_arch>& zy,
    const xsimd::batch<float, xsimd::default_arch>& c_re,
    const xsimd::batch<float, xsimd::default_arch>& c_im,
    const xsimd::batch<float, xsimd::default_arch>& four,
    const xsimd::batch<float, xsimd::default_arch>& max_iter,
    const xsimd::batch<float, xsimd::default_arch>& two) const {
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

        if (zx2 + zy2 >= Parameters::ESCAPE_RADIUS_SQ) break;
    }

    int idx = y * Parameters::WIDTH + x;
    pixel_buffer[idx] = colors[std::min(iter, static_cast<int>(colors.size()) - 1)];
}

void Renderer::generate_gradient() {
    const int gradient_steps = 256;
    std::vector<Uint32> random_colors(10);

    for (int i = 0; i < 10; ++i) {
        int r = rand() % 256;
        int g = rand() % 256;
        int b = rand() % 256;
        random_colors[i] = (r << 16) | (g << 8) | b;
    }

    colors.resize(gradient_steps);
    int segment_length = gradient_steps / (random_colors.size() - 1);

    for (size_t i = 0; i < random_colors.size() - 1; ++i) {
        Uint32 start_color = random_colors[i];
        Uint32 end_color = random_colors[i + 1];

        int start_r = (start_color >> 16) & 0xFF;
        int start_g = (start_color >> 8) & 0xFF;
        int start_b = start_color & 0xFF;

        int end_r = (end_color >> 16) & 0xFF;
        int end_g = (end_color >> 8) & 0xFF;
        int end_b = end_color & 0xFF;

        for (int j = 0; j < segment_length; ++j) {
            float t = static_cast<float>(j) / segment_length;
            int r = static_cast<int>(start_r + t * (end_r - start_r));
            int g = static_cast<int>(start_g + t * (end_g - start_g));
            int b = static_cast<int>(start_b + t * (end_b - start_b));
            colors[i * segment_length + j] = (r << 16) | (g << 8) | b;
        }
    }

    colors[gradient_steps - 1] = random_colors.back();
}

void Renderer::save_screenshot() {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(pixel_buffer.data(), Parameters::WIDTH, Parameters::HEIGHT,
    32, Parameters::WIDTH * sizeof(Uint32), SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        std::cerr << "SDL_CreateRGBSurface Error: " << SDL_GetError() << std::endl;
        return;
    }
    
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char filename[64];
    strftime(filename, sizeof(filename), "screenshot_%Y%m%d_%H%M%S.bmp", timeinfo);
    std::cout << "Screenshot filename: " << filename << std::endl;

    SDL_SaveBMP(surface, filename);
    SDL_FreeSurface(surface);
}
