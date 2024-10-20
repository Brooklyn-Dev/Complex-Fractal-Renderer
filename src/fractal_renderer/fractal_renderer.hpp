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

const float INITIAL_ZOOM = 1.0;
const float INITIAL_OFFSET_X = 0.0;
const float INITIAL_OFFSET_Y = 0.0;

class FractalRenderer {
    public:
        FractalRenderer(int width, int height);
        ~FractalRenderer();

        void run();

    private:
        void handleEvents();
        void startAsyncRendering();
        void renderFrame();

        int winWidth;
        int winHeight;

        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* cachedFrame = nullptr;

        double zoom = INITIAL_ZOOM;
        double offsetX = INITIAL_OFFSET_X;
        double offsetY = INITIAL_OFFSET_Y;

        bool running = true;
        bool recalculating = false;
        bool updateCachedFrame = false;

        std::future<void> renderingTask;
        std::atomic<int> completedThreads;
        std::vector<std::vector<colour>> pixelBuffer;
        std::mutex bufferMutex;
        std::condition_variable bufferCond;

        std::function<colour(complex, int)> fractalFunc;
        std::unordered_map<SDL_Keycode, std::function<colour(complex, int) >> fractalMap;
        SDL_Keycode curFractalFuncKey;
};

#endif