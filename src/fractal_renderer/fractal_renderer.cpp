#include <functional>
#include <thread>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "fractal_renderer.hpp"
#include "../utils/math.hpp"

const unsigned int MIN_WIN_WIDTH = 600;
const unsigned int MIN_WIN_HEIGHT = 450;

const float MIN_REAL = -2.5;
const float MAX_REAL = 2.5;
const float MIN_IMAG = -2.5;
const float MAX_IMAG = 2.5;

const double MIN_ZOOM = 1;
const unsigned int ZOOM_SF = 2;

const unsigned int INITIAL_ITERATIONS = 96;
const unsigned int ITERATION_INCREMENT = 32;

FractalRenderer::FractalRenderer(unsigned int width, unsigned int height)
    : winWidth(width), winHeight(height), halfWinWidth(width / 2.0), halfWinHeight(height / 2.0)
{
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialise SDL: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    unsigned int windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
    window = SDL_CreateWindow("Complex Fractal Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winWidth, winHeight, windowFlags);
    if (window == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_SetWindowMinimumSize(window, MIN_WIN_WIDTH, MIN_WIN_HEIGHT);

    updateFractalSize();

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

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

    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    
    pixelDataBuffer.clear();

    SDL_Quit();
}

void FractalRenderer::handleEvents() {
    SDL_Event event;
    SDL_Keycode eventKey;
    int mx, my;

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        ImGuiIO& io = ImGui::GetIO();
        bool mouseInImGui = io.WantCaptureMouse;
        bool keyboardInImGui = io.WantCaptureKeyboard;

        switch (event.type) {
            case SDL_QUIT:
                // Quit
                running = false;
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (!mouseInImGui && event.button.button == SDL_BUTTON(SDL_BUTTON_LEFT)) {
                    // Set centre of screen at the point clicked
                    SDL_GetMouseState(&mx, &my);

                    complex c = screenToFractal(mx, my, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);
                    setOffset(c.re, c.im);

                    startAsyncRendering();
                }
                break;

            case SDL_MOUSEWHEEL:
                if (!mouseInImGui) {
                    // Update zoom level based on direction of scroll
                    long double zoomPower;
                    if (event.wheel.y > 0)
                        zoomPower = std::log10(zoom * ZOOM_SF);
                    else
                        zoomPower = std::log10(zoom / ZOOM_SF);

                    if (zoomPower < 0)
                        break;

                    setZoom(zoomPower);
                    startAsyncRendering();
                }
                break;

            case SDL_KEYDOWN:
                if (!keyboardInImGui) {
                    eventKey = event.key.keysym.sym;

                    if (eventKey == SDLK_ESCAPE) {
                        // Quit
                        running = false;
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
                        // Reset zoom and offset
                        resetFractal();
                    }
                }
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    int newWidth, newHeight;
                    SDL_GetWindowSize(window, &newWidth, &newHeight);
                    setWinSize(newWidth, newHeight);
                }
                break;

            default:
                break;
        }
    }
}

void FractalRenderer::startAsyncRendering() {
    if (recalculating) {
        cancelRender = true;
        if (renderingTask.valid())
            renderingTask.wait();
        cancelRender = false;
    }
  
    recalculating = true;
    renderProgress = 0;
    renderMaxProgress = winWidth;

    if (fractalTextureBuffer != nullptr)
        SDL_DestroyTexture(fractalTextureBuffer);

    fractalTextureBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, winWidth, winHeight);

    auto fractalFuncCopy = fractalFunc;

    renderingTask = std::async(std::launch::async, [this, fractalFuncCopy]() {
        int numThreads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;
        int sectionWidth = winWidth / numThreads;

        currentMaxIterations = calculateIterations(numZooms, INITIAL_ITERATIONS, ITERATION_INCREMENT);

        // Create a thread for each vertical slice
        for (int i = 0; i < numThreads; ++i) {
            int startX = i * sectionWidth;
            int endX = (i == numThreads - 1) ? winWidth : (i + 1) * sectionWidth;

            threads.emplace_back(std::thread([this, startX, endX, fractalFuncCopy]() {
                for (int x = startX; x < endX; x++) {
                    for (int y = 0; y < winHeight; y++) {
                        if (cancelRender)
                            return;

                        complex c = screenToFractal(x, y, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);
                        colour col = fractalFuncCopy(c, currentMaxIterations);

                        std::lock_guard<std::mutex> lock(renderMutex);
                        pixelDataBuffer[y * winWidth + x] = SDL_MapRGBA(SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888), col.r, col.g, col.b, 255);
                    }

                    renderProgress++;
                }
            }));
        }

        // Wait for all threads to finish
        for (auto& t : threads)
            if (t.joinable())
                t.join();

        if (cancelRender)
            return;

        // Update fractal texture
        {
            std::lock_guard<std::mutex> lock(renderMutex);
            void* pixels;
            int pitch;
            SDL_LockTexture(fractalTextureBuffer, nullptr, &pixels, &pitch);
            memcpy(pixels, pixelDataBuffer.data(), pixelDataBuffer.size() * sizeof(unsigned int));
            SDL_UnlockTexture(fractalTextureBuffer);
        }

        {
            std::lock_guard<std::mutex> lock(renderMutex);
            std::swap(fractalTexture, fractalTextureBuffer);
        }

        recalculating = false;
    });
}

void FractalRenderer::setWinSize(unsigned int width, unsigned int height) {
    if (width == winWidth && height == winHeight)
        return;

    winWidth = width;
    winHeight = height;
    halfWinWidth = winWidth / 2.0;
    halfWinHeight = winHeight / 2.0;

    SDL_RenderSetLogicalSize(renderer, winWidth, winHeight);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    pixelDataBuffer.resize(winWidth * winHeight, 0);

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

void FractalRenderer::resetFractal() {
    if (zoom == INITIAL_ZOOM && offsetX == INITIAL_OFFSET_X && offsetY == INITIAL_OFFSET_Y)
        return;

    zoom = INITIAL_ZOOM;
    numZooms = 0;
    offsetX = INITIAL_OFFSET_X;
    offsetY = INITIAL_OFFSET_Y;

    updateFractalSize();
    startAsyncRendering();
}

void FractalRenderer::setOffset(long double real, long double imag) {
    offsetX = fmin(fmax(real, MIN_REAL), MAX_REAL);
    offsetY = fmin(fmax(imag, MIN_IMAG), MAX_IMAG);
}

void FractalRenderer::setZoom(long double zoomPower) {
    if (zoomPower < 0)
        return;

    // 2^numZooms == 10^zoomPower
    zoom = pow(10.0, zoomPower);
    numZooms = zoomPower / std::log10(2.0);

    updateFractalSize();
}

void FractalRenderer::renderFrame() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (fractalTexture != nullptr)
        SDL_RenderCopy(renderer, fractalTexture, nullptr, nullptr);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    renderFractalInfo();
    renderFractalControls();
    renderProgressBar();

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

    SDL_RenderPresent(renderer);
}

void  FractalRenderer::renderFractalInfo() {
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);

    ImGui::Begin("Fractal Info", nullptr, windowFlags);
    ImGui::Text("Zoom: 10^%.5Lf", std::log10(zoom));
    ImGui::Text("Real: %.10Lf", offsetX);
    ImGui::Text("Imag: %.10Lf", -offsetY);
    ImGui::Text("Iterations: %i", currentMaxIterations);
    ImGui::End();
}

void  FractalRenderer::renderFractalControls() {
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(ImVec2(172, 10), ImGuiCond_Once);

    ImGui::Begin("Fractal Controls", nullptr, windowFlags);

    static double inputReal = offsetX;
    static double inputImag = offsetY;
    static double inputZoom = std::log10(zoom);
    ImGui::Text("Zoom");
    ImGui::SameLine();
    ImGui::PushItemWidth(128);
    ImGui::InputDouble("##Zoom", &inputZoom, 0.0f, 0.0f, "%.15f");
    ImGui::Text("Real");
    ImGui::SameLine();
    ImGui::InputDouble("##Real", &inputReal, 0.0f, 0.0f, "%.15f");
    ImGui::Text("Imag");
    ImGui::SameLine();
    ImGui::InputDouble("##Imag", &inputImag, 0.0f, 0.0f, "%.15f");

    ImGui::Separator();

    if (ImGui::Button("Go")) {
        if (inputZoom == INITIAL_ZOOM && inputReal == INITIAL_OFFSET_X && inputImag == INITIAL_OFFSET_Y)
            resetFractal();
        else {
            setOffset(inputReal, inputImag);
            setZoom(inputZoom);

            startAsyncRendering();
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset"))
        resetFractal();

    ImGui::End();
}

void  FractalRenderer::renderProgressBar() {
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(ImVec2(366, 10), ImGuiCond_Once);

    ImGui::Begin("Render Progress", nullptr, windowFlags);
    float progress = static_cast<float>(renderProgress) / renderMaxProgress;
    ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f), progress == 1.0f ? "Finished" : "Rendering...");
    ImGui::End();
}

void FractalRenderer::run() {
    pixelDataBuffer.resize(winWidth * winHeight, 0);

    startAsyncRendering();

    while (running) {
        handleEvents();

        renderFrame();
    }
}