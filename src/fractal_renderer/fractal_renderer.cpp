#include <algorithm>
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

    fractalOptions = {
        { "Mandelbrot Set", SDLK_1, processMandelbrot, calcTrajectoryMandelbrot },
        { "Tricorn", SDLK_2, processTricorn, calcTrajectoryTricorn },
        { "Burning Ship", SDLK_3, processBurningShip, calcTrajectoryBurningShip },
        { "Newton Fractal", SDLK_4, processNewtonFractal, calcTrajectoryNewtonFractal }
    };

    curFractalIdx = 0;
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

    if (fractalTexture) {
        SDL_DestroyTexture(fractalTexture);
        fractalTexture = nullptr;
    }

    if (fractalTextureBuffer) {
        SDL_DestroyTexture(fractalTextureBuffer);
        fractalTextureBuffer = nullptr;
    }

    if (trajectoryTexture) {
        SDL_DestroyTexture(trajectoryTexture);
        trajectoryTexture = nullptr;
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
                if (mouseInImGui)
                    break;

                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Set centre of screen at the point clicked
                    SDL_GetMouseState(&mx, &my);

                    complex c = screenToFractal(mx, my, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);
                    setOffset(c.re, c.im);

                    startAsyncRendering();
                }
                else if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (isRecalculatingFractal)
                        break;

                    // Render the trajectory of the point clicked
                    SDL_GetMouseState(&mx, &my);

                    complex c = screenToFractal(mx, my, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);

                    auto trajectoryFuncCopy = fractalOptions[curFractalIdx].trajectoryFunc;
                    std::vector<complex> trajectoryPoints = trajectoryFuncCopy(c, curMaxIterations);
                    renderTrajectory(trajectoryPoints);
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
                    else if (eventKey == SDLK_TAB) {
                        // Toggle UI
                        uiVisible = !uiVisible;
                    }
                    else if (eventKey == SDLK_r) {
                        // Reset zoom and offset
                        resetFractal();
                    }
                    else {
                        for (int i = 0; i < fractalOptions.size(); i++)
                        {
                            if (eventKey == fractalOptions[i].key) {
                                // Change selected fractal
                                setFractal(i);
                                break;
                            }
                        }
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
    if (isRecalculatingFractal) {
        cancelRender = true;
        if (renderingTask.valid())
            renderingTask.wait();

        cancelRender = false;
    }
  
    isRecalculatingFractal = true;
    renderProgress = 0;
    renderMaxProgress = winWidth;

    if (fractalTextureBuffer != nullptr)
        SDL_DestroyTexture(fractalTextureBuffer);

    fractalTextureBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, winWidth, winHeight);

    auto fractalFuncCopy = fractalOptions[curFractalIdx].func;

    renderingTask = std::async(std::launch::async, [this, fractalFuncCopy]() {
        int numThreads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;
        int sectionWidth = winWidth / numThreads;

        curMaxIterations = calculateIterations(numZooms, INITIAL_ITERATIONS, ITERATION_INCREMENT);

        SDL_PixelFormat* pixelFormat = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);

        // Create a thread for each vertical slice
        for (int i = 0; i < numThreads; ++i) {
            int startX = i * sectionWidth;
            int endX = (i == numThreads - 1) ? winWidth : (i + 1) * sectionWidth;

            threads.emplace_back(std::thread([this, startX, endX, fractalFuncCopy, pixelFormat]() {
                for (int x = startX; x < endX; x++) {
                    for (int y = 0; y < winHeight; y++) {
                        if (cancelRender)
                            return;

                        complex c = screenToFractal(x, y, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);
                        colour col = fractalFuncCopy(c, curMaxIterations);

                        std::lock_guard<std::mutex> lock(renderMutex);
                        pixelDataBuffer[(long long int)y * winWidth + x] = SDL_MapRGB(pixelFormat, col.r, col.g, col.b);
                    }

                    renderProgress++;
                }
            }));
        }

        // Wait for all threads to finish
        for (auto& t : threads)
            if (t.joinable())
                t.join();

        SDL_FreeFormat(pixelFormat);

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

        isRecalculatingFractal = false;
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

    destroyTrajectory = true;

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

    destroyTrajectory = true;

    updateFractalSize();
    startAsyncRendering();
}

void FractalRenderer::setOffset(long double real, long double imag) {
    offsetX = fmin(fmax(real, MIN_REAL), MAX_REAL);
    offsetY = fmin(fmax(imag, MIN_IMAG), MAX_IMAG);

    destroyTrajectory = true;
}

void FractalRenderer::setZoom(long double zoomPower) {
    if (zoomPower < 0)
        return;

    // 2^numZooms == 10^zoomPower
    zoom = pow(10.0, zoomPower);
    numZooms = zoomPower / std::log10(2.0);

    destroyTrajectory = true;

    updateFractalSize();
}

void FractalRenderer::setFractal(unsigned int fractalIndex) {
    if (fractalIndex == curFractalIdx)
        return;

    curFractalIdx = fractalIndex;
    destroyTrajectory = true;

    startAsyncRendering();
}

void FractalRenderer::renderTrajectory(std::vector<complex> trajectoryPoints) {
    if (trajectoryPoints.empty())
        return;

    isRecalculatingTrajectory = true;

    // Trajectory texture setup
    if (trajectoryTexture)
        SDL_DestroyTexture(trajectoryTexture);

    trajectoryTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, winWidth, winHeight);
    SDL_SetTextureBlendMode(trajectoryTexture, SDL_BLENDMODE_BLEND);
    
    SDL_SetRenderTarget(renderer, trajectoryTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    unsigned char pointSize = 4;
    float halfPointSize = pointSize / 2.0f;

    // Get start point
    auto curPoint = fractalToScreen(trajectoryPoints[0], halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);
    double startX = curPoint.first;
    double startY = curPoint.second;

    std::vector<SDL_Rect> pointRects;
    std::vector<SDL_Point> linePoints = { SDL_Point{ static_cast<int>(startX), static_cast<int>(startY) } };

    for (int i = 1; i < trajectoryPoints.size(); i++) {
        // Calculate pixel coordinates
        int prevX = static_cast<int>(curPoint.first);
        int prevY = static_cast<int>(curPoint.second);

        curPoint = fractalToScreen(trajectoryPoints[i], halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);
        int curX = static_cast<int>(curPoint.first);
        int curY = static_cast<int>(curPoint.second);

        // Clamp current point
        int clampedX = std::clamp<int>(curX, 0, winWidth);
        int clampedY = std::clamp<int>(curY, 0, winHeight);
        bool clamped = (clampedX != curX) || (clampedY != curY);

        linePoints.push_back(SDL_Point{ clampedX, clampedY });

        // Calculate point rect
        if (!clamped) {
            SDL_Rect curRect = { static_cast<int>(curX - halfPointSize), static_cast<int>(curY - halfPointSize), pointSize, pointSize };
            pointRects.push_back(curRect);
        }
    }

    // Render lines
    if (linePoints.size() > 1) {
        SDL_Color lineColour = { 160, 0, 160, 255 };
        SDL_SetRenderDrawColor(renderer, lineColour.r, lineColour.g, lineColour.b, lineColour.a);
        SDL_RenderDrawLines(renderer, linePoints.data(), linePoints.size());
    }

    // Render points
    SDL_Color pointColour = { 255, 255, 255, 255 };
    SDL_SetRenderDrawColor(renderer, pointColour.r, pointColour.g, pointColour.b, pointColour.a);

    for (const SDL_Rect& rect : pointRects)
        SDL_RenderFillRect(renderer, &rect);

    // Render start point
    SDL_Rect startRect = { startX - halfPointSize, startY - halfPointSize, pointSize, pointSize };
    SDL_RenderFillRect(renderer, &startRect);

    SDL_SetRenderTarget(renderer, nullptr);
    isRecalculatingTrajectory = false;
}

void FractalRenderer::renderFrame() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (fractalTexture != nullptr)
        SDL_RenderCopy(renderer, fractalTexture, nullptr, nullptr);

    if (trajectoryTexture != nullptr)
        SDL_RenderCopy(renderer, trajectoryTexture, nullptr, nullptr);

    if (uiVisible) {
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        renderFractalInfo();
        renderFractalControls();
        renderFractalSelector();
        renderProgressBar();

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
    }

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
    ImGui::Text("Imag: %.10Lf", offsetY);
    ImGui::Text("Iterations: %i", curMaxIterations);
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

void  FractalRenderer::renderFractalSelector() {
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(ImVec2(366, 10), ImGuiCond_Once);

    ImGui::Begin("Fractal Selector", nullptr, windowFlags);

    if (ImGui::BeginCombo("##Select Fractal", fractalOptions[curFractalIdx].name.c_str())) {
        for (int i = 0; i < fractalOptions.size(); i++) {
            bool isSelected = curFractalIdx == i;
            if (ImGui::Selectable(fractalOptions[i].name.c_str(), isSelected))
                setFractal(i);    
        }
        ImGui::EndCombo();
    }

    ImGui::End();
}

void  FractalRenderer::renderProgressBar() {
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(ImVec2(604, 10), ImGuiCond_Once);

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

        if (destroyTrajectory && !isRecalculatingTrajectory) {
            if (trajectoryTexture) {
                SDL_DestroyTexture(trajectoryTexture);
                trajectoryTexture = nullptr;
            }

            destroyTrajectory = false;
        }

        renderFrame();
    }
}