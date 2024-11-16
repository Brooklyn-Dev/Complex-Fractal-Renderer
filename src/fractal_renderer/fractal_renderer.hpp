#ifndef FRACTAL_RENDERER_H
#define FRACTAL_RENDERER_H

#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <SDL2/SDL.h>

#include "../complex/complex.hpp"
#include "../fractals/fractals.hpp"
#include "../options/fractal_option.hpp"
#include "../options/resolution_option.hpp"

const unsigned int INITIAL_ZOOM = 1;
const float INITIAL_OFFSET_X = 0.0;
const float INITIAL_OFFSET_Y = 0.0;
const unsigned int INITIAL_MAX_ITERATIONS = 5000;

class FractalRenderer {
    public:
        FractalRenderer(unsigned int width, unsigned int height);
        ~FractalRenderer();

        void run();

    private:
        void handleEvents();

        void setWindowSize(unsigned int width, unsigned int height);
        void refreshFractalSize();
        void resetToInitialFractal();
        void setFractalOffset(long double real, long double imag);
        void setZoomLevel(long double zoomPower);
        void selectResolution(unsigned int resolutionIndex);
        void selectFractal(unsigned int fractalIndex);

        void beginAsyncRendering(bool fullRender = false);
        void drawTrajectory(const std::vector<Complex>& trajectoryPoints);

        void drawFractalInfo();
        void drawFractalControls();
        void drawFractalSelector();
        void drawRenderingSettings();
        void drawProgressBar();

        void renderFrame();

        std::string generatePNGFilename();

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

        long double zoom = INITIAL_ZOOM;
        long double numZooms = 0.0;
        long double offsetX = INITIAL_OFFSET_X;
        long double offsetY = INITIAL_OFFSET_Y;
        unsigned int maxIterations = INITIAL_MAX_ITERATIONS;
        unsigned int curMaxIterations;

        SDL_Texture* trajectoryTexture = nullptr;
        bool recalculateTrajectory = false;
        bool isRecalculatingTrajectory = false;
        bool destroyTrajectory = false;

        bool running = true;
        bool uiVisible = true;

        std::atomic<bool> isRecalculatingFractal;
        std::atomic<bool> cancelRender;
        std::future<void> renderingTask;
        std::mutex renderMutex;
        std::vector<unsigned int> pixelDataBuffer;
        std::atomic<unsigned int> renderProgress;
        unsigned int renderMaxProgress;

        std::vector<resolutionOption> resolutionOptions;
        unsigned int curResolutionIdx;

        std::vector<fractalOption> fractalOptions;
        unsigned int curFractalIdx;
};

#endif