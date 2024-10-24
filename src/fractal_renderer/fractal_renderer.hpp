#ifndef FRACTAL_RENDERER_H
#define FRACTAL_RENDERER_H

#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <SDL2/SDL.h>

#include "../fractals/fractals.hpp"

const unsigned int INITIAL_ZOOM = 1;
const float INITIAL_OFFSET_X = 0.0;
const float INITIAL_OFFSET_Y = 0.0;

class FractalRenderer {
    public:
        FractalRenderer(unsigned int width, unsigned int height);
        ~FractalRenderer();

        void run();

    private:
        void handleEvents();
        void setWinSize(unsigned int width, unsigned int height);
        void updateFractalSize();
        void resetFractal();
        void setOffset(long double real, long double imag);
        void setZoom(long double zoomPower);
        void startAsyncRendering();
        void renderFrame();
        void renderFractalInfo();
        void renderFractalControls();
        void renderProgressBar();

        unsigned int winWidth;
        unsigned int winHeight;
        float halfWinWidth;
        float halfWinHeight;
        double fractalWidth;
        double fractalHeight;
        double fractalWidthRatio;
        double fractalHeightRatio;

        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* fractalTexture = nullptr;
        SDL_Texture* fractalTextureBuffer = nullptr;

        double zoom = INITIAL_ZOOM;
        long double numZooms = 0.0;
        long double offsetX = INITIAL_OFFSET_X;
        long double offsetY = INITIAL_OFFSET_Y;
        unsigned int currentMaxIterations;

        bool running = true;

        std::atomic<bool> recalculating = false;
        std::atomic<bool> cancelRender = false;
        std::future<void> renderingTask;
        std::mutex renderMutex;
        std::vector<unsigned int> pixelDataBuffer;
        std::atomic<unsigned int> renderProgress;
        unsigned int renderMaxProgress;

        std::function<colour(complex, unsigned int)> fractalFunc;
        std::unordered_map<SDL_Keycode, std::function<colour(complex, unsigned int) >> fractalMap;
        SDL_Keycode curFractalFuncKey;
};

#endif