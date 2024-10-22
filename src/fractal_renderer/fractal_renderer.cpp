#include <functional>
#include <thread>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "fractal_renderer.hpp"
#include "../utils/math.hpp"

const float MIN_REAL = -2.5;
const float MAX_REAL = 2.5;
const float MIN_IMAG = -2.5;
const float MAX_IMAG = 2.5;

const double MIN_ZOOM = 1;
const unsigned int ZOOM_SF = 4;

const unsigned int BASE_ITERATIONS = 64;

FractalRenderer::FractalRenderer(unsigned int width, unsigned int height)
    : winWidth(width), winHeight(height), halfWinWidth(width / 2.0), halfWinHeight(height / 2.0)
{
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialise SDL: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    Uint32 windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
    window = SDL_CreateWindow("Complex Fractal Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winWidth, winHeight, windowFlags);
    if (window == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    updateFractalSize();

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Bind number keys to fractals
    fractalMap[SDLK_1] = processMandelbrot;
    fractalMap[SDLK_2] = processTricorn;
    fractalMap[SDLK_3] = processBurningShip;
    fractalMap[SDLK_4] = processNewtonFractal;

    curFractalFuncKey = SDLK_1;
    fractalFunc = fractalMap[curFractalFuncKey];
}

FractalRenderer::~FractalRenderer() {
    if (renderingTask.valid())
        renderingTask.wait();

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (cachedFrame) {
        SDL_DestroyTexture(cachedFrame);
        cachedFrame = nullptr;
    }

    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    pixelBuffer.clear();

    SDL_Quit();
}

void FractalRenderer::handleEvents() {
    SDL_Event event;
    SDL_Keycode eventKey;
    int mx, my;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            // Quit
            running = false;
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON(SDL_BUTTON_LEFT)) {
                // Set centre of screen at the point clicked
                SDL_GetMouseState(&mx, &my);

                complex c = screenToFractal(mx, my, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);

                offsetX = fmin(fmax(c.re, MIN_REAL), MAX_REAL);
                offsetY = fmin(fmax(c.im, MIN_IMAG), MAX_IMAG);

                startAsyncRendering();
            }
            break;

        case SDL_MOUSEWHEEL:
            // Update zoom level based on direction of scroll
            if (event.wheel.y > 0) {
                zoom *= ZOOM_SF;
                numZooms += 1;
            }
            else if (zoom > MIN_ZOOM) { 
                zoom /= ZOOM_SF;
                numZooms -= 1;
            }

            updateFractalSize();
            startAsyncRendering();
            break;

        case SDL_KEYDOWN:
            eventKey = event.key.keysym.sym;

            if (eventKey == SDLK_ESCAPE) {
                // Quit
                running = false;
                break;
            }
            else if (fractalMap.find(eventKey) != fractalMap.end()) {
                if (eventKey == curFractalFuncKey)
                    break;

                // Set the fractal function
                fractalFunc = fractalMap[eventKey];
                curFractalFuncKey = eventKey;
                startAsyncRendering();
            }
            else if (eventKey == SDLK_r) {
                if (zoom == INITIAL_ZOOM && offsetX == INITIAL_OFFSET_X && offsetY == INITIAL_OFFSET_Y)
                    break;

                // Reset zoom and offset
                zoom = INITIAL_ZOOM;
                numZooms = 0;
                offsetX = INITIAL_OFFSET_X;
                offsetY = INITIAL_OFFSET_Y;

                updateFractalSize();
                startAsyncRendering();
            }
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                setWinSize(event.window.data1, event.window.data2);
            }

            break;

        default:
            break;
        }
    }
}

void FractalRenderer::startAsyncRendering() {
    if (recalculating) return;
    recalculating = true;

    int numThreads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    int sectionWidth = winWidth / numThreads;
    int sectionHeight = winHeight / 2;

    // Dynamically calculate the number of iterations based on the zoom level
    int maxIterations = calculateIterations(numZooms, BASE_ITERATIONS);

    for (int i = 0; i < numThreads; ++i) {
        int startX = i * sectionWidth;
        int endX = (i + 1) * sectionWidth;

        // Each thread calculates and renders two vertical half-height tiles
        threads.push_back(std::thread([this, startX, endX, sectionHeight, maxIterations]() {
            // Render top tile (first half of height)
            for (int x = startX; x < endX; x++) {
                for (int y = 0; y < sectionHeight; y++) {
                    complex c = screenToFractal(x, y, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);
                    colour col = fractalFunc(c, maxIterations);
                    pixelBuffer[x][y] = col;
                }
            }

            // Lock and update the rendering buffer for the top tile
            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                for (int x = startX; x < endX; ++x) {
                    for (int y = 0; y < sectionHeight; ++y) {
                        const colour& col = pixelBuffer[x][y];
                        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
                        SDL_RenderDrawPoint(renderer, x, y);
                    }
                }
                SDL_RenderPresent(renderer);  // Present the top tile after it is done
            }

            // Render bottom tile (second half of height)
            for (int x = startX; x < endX; x++) {
                for (int y = sectionHeight; y < winHeight; y++) {
                    complex c = screenToFractal(x, y, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);
                    colour col = fractalFunc(c, maxIterations);
                    pixelBuffer[x][y] = col;
                }
            }

            // Lock and update the rendering buffer for the bottom tile
            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                for (int x = startX; x < endX; ++x) {
                    for (int y = sectionHeight; y < winHeight; ++y) {
                        const colour& col = pixelBuffer[x][y];
                        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
                        SDL_RenderDrawPoint(renderer, x, y);
                    }
                }
                SDL_RenderPresent(renderer);  // Present the bottom tile after it is done
            }
        }));
    }

    // Wait for all threads to finish
    for (auto& t : threads)
        t.join();

    recalculating = false;
    updateCachedFrame = true;
}

void FractalRenderer::setWinSize(unsigned int width, unsigned int height) {
    if (width == winWidth && height == winHeight)
        return;

    winWidth = width;
    winHeight = height;
    halfWinWidth = winWidth / 2.0;
    halfWinHeight = winHeight / 2.0;

    SDL_SetWindowSize(window, winWidth, winHeight);

    SDL_RenderSetLogicalSize(renderer, winWidth, winHeight);

    pixelBuffer.resize(winWidth, std::vector<colour>(winHeight));

    updateFractalSize();
    startAsyncRendering();
}

void FractalRenderer::updateFractalSize() {
    double aspectRatio = winWidth / (double)winHeight;

    if (winWidth < winHeight) {
        fractalWidth = 4.0 / zoom;   // -2 to 2 for the real axis
        fractalHeight = fractalWidth / aspectRatio;  // Scale imaginary axis proportionally
    }
    else {
        fractalHeight = 4.0 / zoom;  // -2i to 2i for the imaginary axis
        fractalWidth = fractalHeight * aspectRatio;  // Scale real axis proportionally
    }

    fractalWidthRatio = fractalWidth / winWidth;
    fractalHeightRatio = fractalHeight / winHeight;
}

void FractalRenderer::renderFrame() {
    if (!updateCachedFrame)
        return;
        
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    renderImGuiOverlay();

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

    SDL_RenderPresent(renderer);
    updateCachedFrame = false;
}

void  FractalRenderer::renderImGuiOverlay()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                    ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoFocusOnAppearing |
                                    ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);

    ImGui::Begin("ImGuiOverlay", nullptr, window_flags);

    ImGui::SetWindowFontScale(1.25f);

    ImGui::Text("Zoom: 10^%.5Lf", std::log10(zoom));
    ImGui::Text("Centre: %.10Lf %s %.10Lf i\n", offsetX, -offsetY >= 0 ? "+" : "-" , fabs(offsetY));

    ImGui::End();
}

void FractalRenderer::run() {
    pixelBuffer.resize(winWidth, std::vector<colour>(winHeight));

    startAsyncRendering();

    renderFrame();

    while (running) {
        handleEvents();

        // Wait for the rendering thread to notify that recalculation is done
        {
            std::unique_lock<std::mutex> lock(bufferMutex);
            bufferCond.wait(lock, [this]() { return !recalculating; });
        }

        renderFrame();
    }
}