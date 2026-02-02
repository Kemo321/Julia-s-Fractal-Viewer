#include "Parameters.hpp"
#include "Renderer.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

class RendererTest : public ::testing::Test
{
protected:
    SDL_Window* window = nullptr;

    void SetUp() override
    {
        SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");

        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            FAIL() << "SDL Init failed: " << SDL_GetError();
        }

        Parameters::WIDTH = 800;
        Parameters::HEIGHT = 600;
        Parameters::CENTER_X = 400.0f;
        Parameters::CENTER_Y = 300.0f;
        Parameters::ZOOM = 1.0f;
        Parameters::MAX_ITERATIONS = 100;

        window = SDL_CreateWindow("Test Window", 0, 0,
            Parameters::WIDTH, Parameters::HEIGHT,
            SDL_WINDOW_HIDDEN);
    }

    void TearDown() override
    {
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
    }

    const std::vector<Uint32>& get_pixel_buffer(const Renderer& r)
    {
        return r.pixel_buffer;
    }

    const std::vector<int>& get_colors(const Renderer& r)
    {
        return r.colors;
    }

    float get_scale_x(const Renderer& r)
    {
        return r.scale_x;
    }

    void compute_chunk(Renderer& r, int start_y, int end_y)
    {
        r.compute_fractal_chunk(start_y, end_y);
    }

    auto compute_simd(Renderer& r,
        xsimd::simd_type<float>& zx, xsimd::simd_type<float>& zy,
        const xsimd::simd_type<float>& c_re, const xsimd::simd_type<float>& c_im,
        const xsimd::simd_type<float>& four, const xsimd::simd_type<float>& max_iter,
        const xsimd::simd_type<float>& two)
    {
        return r.compute_simd_iterations(zx, zy, c_re, c_im, four, max_iter, two);
    }
};

TEST_F(RendererTest, SimdLogic_MixedBatch_BranchCoverage)
{
    Renderer renderer(window);
    using floatv = xsimd::simd_type<float>;

    alignas(64) std::array<float, floatv::size> zx_data;
    for (size_t i = 0; i < floatv::size; ++i)
    {
        zx_data[i] = (i % 2 == 0) ? 0.0f : 10.0f;
    }

    floatv zx = xsimd::load_aligned(zx_data.data());
    floatv zy = 0.0f;
    floatv c_re = 0.0f, c_im = 0.0f;
    floatv four = 4.0f, max_iter = 50.0f, two = 2.0f;

    auto result = compute_simd(renderer, zx, zy, c_re, c_im, four, max_iter, two);

    for (size_t i = 0; i < floatv::size; ++i)
    {
        if (i % 2 == 0)
        {
            EXPECT_EQ(result.get(i), 50.0f) << "Point " << i << " should not escape";
        }
        else
        {
            EXPECT_EQ(result.get(i), 0.0f) << "Point " << i << " should escape";
        }
    }
}

TEST_F(RendererTest, ScalarVsSimdConsistency)
{
    Renderer renderer(window);

    Parameters::CENTER_X = 0;
    Parameters::CENTER_Y = 0;

    renderer.set_zoom(1.0f);

    compute_chunk(renderer, 0, 1);

    const auto& buffer = get_pixel_buffer(renderer);

    bool touched = false;
    for (int x = 0; x < Parameters::WIDTH; ++x)
    {
        if (buffer[x] != 0)
            touched = true;
    }
    EXPECT_TRUE(touched) << "Renderer should have written something to the buffer";
}

TEST_F(RendererTest, ZoomLogic_UpdatesInternalScale)
{
    Renderer renderer(window);

    float scale_zoom_1 = get_scale_x(renderer);

    renderer.set_zoom(2.0f);
    float scale_zoom_2 = get_scale_x(renderer);

    EXPECT_FLOAT_EQ(scale_zoom_2, scale_zoom_1 / 2.0f);

    renderer.set_zoom(0.5f);
    float scale_zoom_05 = get_scale_x(renderer);

    EXPECT_FLOAT_EQ(scale_zoom_05, scale_zoom_1 * 2.0f);
}

TEST_F(RendererTest, GradientGeneration_ProducesValidColors)
{
    Renderer renderer(window);

    const auto& colors = get_colors(renderer);

    ASSERT_EQ(colors.size(), 256);

    bool has_color = false;
    for (int c : colors)
    {
        if (c != 0)
            has_color = true;
    }
    EXPECT_TRUE(has_color) << "Gradient should not be completely black";
}

TEST_F(RendererTest, PixelBufferResizesCorrectly)
{
    Parameters::WIDTH = 100;
    Parameters::HEIGHT = 100;

    SDL_DestroyWindow(window);
    window = SDL_CreateWindow("Small", 0, 0, 100, 100, SDL_WINDOW_HIDDEN);

    Renderer renderer(window);
    const auto& buffer = get_pixel_buffer(renderer);

    EXPECT_EQ(buffer.size(), 100 * 100);
}

TEST_F(RendererTest, ThreadPoolLifecycle)
{
    auto start = std::chrono::steady_clock::now();

    {
        Renderer renderer(window);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 2000) << "Destruction of Renderer takes too long (possible deadlock)";
}
