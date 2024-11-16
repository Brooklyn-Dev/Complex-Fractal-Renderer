#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <regex>
#include <string>
#include <thread>

#include <SDL2/SDL_image.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "fractal_renderer.hpp"
#include "../utils/io/image.hpp"

const unsigned int MIN_WIN_WIDTH = 600;
const unsigned int MIN_WIN_HEIGHT = 450;

const float MIN_REAL = -2.5;
const float MAX_REAL = 2.5;
const float MIN_IMAG = -2.5;
const float MAX_IMAG = 2.5;

const double MIN_ZOOM = 1;
const unsigned int ZOOM_SF = 2;

const unsigned int INITIAL_ITERATIONS = 96;     
const unsigned int ITERATION_INCREMENT = 40;
const unsigned int MAX_ITERATIONS_LIMIT = 10000;

const std::string IMAGE_PATH = "./saved_images";

const ImGuiWindowFlags BASE_WINDOW_FLAGS =
ImGuiWindowFlags_AlwaysAutoResize |
ImGuiWindowFlags_NoSavedSettings |
ImGuiWindowFlags_NoFocusOnAppearing |
ImGuiWindowFlags_NoNav;

FractalRenderer::FractalRenderer(unsigned int width, unsigned int height)
    : winWidth(width), winHeight(height),
    halfWinWidth(width / 2.0), halfWinHeight(height / 2.0),
    isRecalculatingFractal(false),
    cancelRender(false)
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

    refreshFractalSize();

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

    curFractalIdx = 0;
    fractalOptions = {
        { "Mandelbrot Set", SDLK_1, processMandelbrot, calcTrajectoryMandelbrot },
        { "Tricorn", SDLK_2, processTricorn, calcTrajectoryTricorn },
        { "Burning Ship", SDLK_3, processBurningShip, calcTrajectoryBurningShip },
        { "Newton Fractal", SDLK_4, processNewtonFractal, calcTrajectoryNewtonFractal }
    };

    curResolutionIdx = 0;
    resolutionOptions = {
        { "100%", 1.0f },
        { "50%", std::sqrt(0.5f) },
        { "25%", 0.5f },
        { "12.5%", std::sqrt(0.125f) },
        { "6.25%", 0.25f },
    };
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

                    Complex c = screenToFractal(mx, my, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);
                    setFractalOffset((long double)c.real(), (long double)c.imag());

                    beginAsyncRendering();
                }
                else if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (isRecalculatingFractal)
                        break;

                    // Render the trajectory of the point clicked
                    SDL_GetMouseState(&mx, &my);

                    Complex c = screenToFractal(mx, my, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);

                    auto trajectoryFuncCopy = fractalOptions[curFractalIdx].trajectoryFunc;
                    std::vector<Complex> trajectoryPoints = trajectoryFuncCopy(c, curMaxIterations);
                    drawTrajectory(trajectoryPoints);
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

                    setZoomLevel(zoomPower);
                    beginAsyncRendering();
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
                    else if (eventKey == SDLK_f) {
                        // Perform full render
                        beginAsyncRendering(true);
                    }
                    else if (eventKey == SDLK_r) {
                        // Reset zoom and offset
                        resetToInitialFractal();
                    }
                    else if (eventKey == SDLK_s) {
                        if (curResolutionIdx != 0) // Doesn't work at lower resolutions
                            break;
                            
                        // Save snapshot of the window
                        std::string filename = generatePNGFilename();
                        saveTextureAsPNG(renderer, fractalTexture, filename);
                    }
                    else {
                        for (int i = 0; i < fractalOptions.size(); i++)
                        {
                            if (eventKey == fractalOptions[i].key) {
                                // Change selected fractal
                                selectFractal(i);
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
                    setWindowSize(newWidth, newHeight);
                }
                break;

            default:
                break;
        }
    }
}

void FractalRenderer::setWindowSize(unsigned int width, unsigned int height) {
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

    destroyTrajectory = true;

    refreshFractalSize();
    beginAsyncRendering();
}

void FractalRenderer::refreshFractalSize() {
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

void FractalRenderer::resetToInitialFractal() {
    if (zoom == INITIAL_ZOOM && offsetX == INITIAL_OFFSET_X && offsetY == INITIAL_OFFSET_Y)
        return;

    zoom = INITIAL_ZOOM;
    numZooms = 0;
    offsetX = INITIAL_OFFSET_X;
    offsetY = INITIAL_OFFSET_Y;

    destroyTrajectory = true;

    refreshFractalSize();
    beginAsyncRendering();
}

void FractalRenderer::setFractalOffset(long double real, long double imag) {
    offsetX = fmin(fmax(real, MIN_REAL), MAX_REAL);
    offsetY = fmin(fmax(imag, MIN_IMAG), MAX_IMAG);

    destroyTrajectory = true;
}

void FractalRenderer::setZoomLevel(long double zoomPower) {
    if (zoomPower < 0)
        return;

    // 2^numZooms == 10^zoomPower
    zoom = pow(10.0, zoomPower);
    numZooms = zoomPower / std::log10(2.0);

    destroyTrajectory = true;

    refreshFractalSize();
}

void FractalRenderer::selectResolution(unsigned int resolutionIndex) {
    if (resolutionIndex == curResolutionIdx)
        return;

    curResolutionIdx = resolutionIndex;
    beginAsyncRendering();
}

void FractalRenderer::selectFractal(unsigned int fractalIndex) {
    if (fractalIndex == curFractalIdx)
        return;

    curFractalIdx = fractalIndex;
    destroyTrajectory = true;

    beginAsyncRendering();
}

void FractalRenderer::beginAsyncRendering(bool fullRender) {
    if (isRecalculatingFractal) {
        cancelRender = true;
        if (renderingTask.valid())
            renderingTask.wait();

        cancelRender = false;
    }

    isRecalculatingFractal = true;

    // Calculate render size based off resolution
    float lengthScaleFactor = resolutionOptions[curResolutionIdx].lengthScaleFactor;
    unsigned int renderWidth = (int)(winWidth * lengthScaleFactor);
    unsigned int renderHeight = (int)(winHeight * lengthScaleFactor);

    pixelDataBuffer.resize((unsigned long long int)renderWidth * renderHeight, 0);

    renderProgress = 0;
    renderMaxProgress = renderWidth;

    // Set up fractal texture buffer
    if (fractalTextureBuffer)
        SDL_DestroyTexture(fractalTextureBuffer);

    fractalTextureBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, renderWidth, renderHeight);
    if (fractalTextureBuffer == nullptr) {
        SDL_Log("Failed to create texture: %s", SDL_GetError());
        return;
    }

    auto fractalFuncCopy = fractalOptions[curFractalIdx].func;

    renderingTask = std::async(std::launch::async, [this, fullRender, fractalFuncCopy, lengthScaleFactor, renderWidth, renderHeight]() {
        int numThreads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;
        int sectionWidth = renderWidth / numThreads;

        // Set max iterations based on render mode
        curMaxIterations = fullRender ? maxIterations : calculateIterations(numZooms, INITIAL_ITERATIONS, ITERATION_INCREMENT, maxIterations);

        SDL_PixelFormat* pixelFormat = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
        if (pixelFormat == nullptr) {
            SDL_Log("Failed to allocate pixel format: %s", SDL_GetError());
            return;
        }

        // Create a thread for each vertical slice
        for (int i = 0; i < numThreads; ++i) {
            int startX = i * sectionWidth;
            int endX = (i == numThreads - 1) ? renderWidth : (i + 1) * sectionWidth;

            threads.emplace_back(std::thread([this, startX, endX, fractalFuncCopy, pixelFormat, lengthScaleFactor, renderWidth, renderHeight]() {
                for (int x = startX; x < endX; x++) {
                    for (int y = 0; y < renderHeight; y++) {
                        if (cancelRender)
                            return;

                        Complex c = screenToFractal(x / lengthScaleFactor, y / lengthScaleFactor, halfWinWidth, halfWinHeight, fractalWidthRatio, fractalHeightRatio, offsetX, offsetY);
                        colour col = fractalFuncCopy(c, curMaxIterations);

                        std::lock_guard<std::mutex> lock(renderMutex);
                        if (y < renderHeight && x < renderWidth)
                            pixelDataBuffer[(unsigned long long int)y * renderWidth + x] = SDL_MapRGB(pixelFormat, col.r, col.g, col.b);
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

            if (SDL_LockTexture(fractalTextureBuffer, nullptr, &pixels, &pitch) < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Couldn't lock fractal texture buffer: %s", SDL_GetError());
                return;
            }

            memcpy(pixels, pixelDataBuffer.data(), pixelDataBuffer.size() * sizeof(unsigned int));

            SDL_UnlockTexture(fractalTextureBuffer);

            std::swap(fractalTexture, fractalTextureBuffer);
        }

        isRecalculatingFractal = false;
    });
}

void FractalRenderer::drawTrajectory(const std::vector<Complex>& trajectoryPoints) {
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
    SDL_Rect startRect = { static_cast<int>(startX - halfPointSize), static_cast<int>(startY - halfPointSize), pointSize, pointSize };
    SDL_RenderFillRect(renderer, &startRect);

    SDL_SetRenderTarget(renderer, nullptr);
    isRecalculatingTrajectory = false;
}

void FractalRenderer::drawFractalInfo() {
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);

    ImGui::Begin("Fractal Info", nullptr, windowFlags);
    ImGui::Text("Zoom: 10^%.5Lf", std::log10(zoom));
    ImGui::Text("Real: %.10Lf", offsetX);
    ImGui::Text("Imag: %.10Lf", offsetY);
    ImGui::Text("Iterations: %i", curMaxIterations);
    ImGui::Text("Max Iterations: %i", maxIterations);
    ImGui::End();
}

void FractalRenderer::drawFractalControls() {
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(ImVec2(180, 10), ImGuiCond_Once);

    ImGui::Begin("Fractal Controls", nullptr, windowFlags);

    static double inputZoom = std::log10(zoom);
    static double inputReal = offsetX;
    static double inputImag = offsetY;

    ImGui::Text("Zoom");
    ImGui::SameLine();
    ImGui::PushItemWidth(128.0f);
    ImGui::InputDouble("##Zoom", &inputZoom, 0.0f, 0.0f, "%.15f");

    ImGui::Text("Real");
    ImGui::SameLine();
    ImGui::InputDouble("##Real", &inputReal, 0.0f, 0.0f, "%.15f");

    ImGui::Text("Imag");
    ImGui::SameLine();
    ImGui::InputDouble("##Imag", &inputImag, 0.0f, 0.0f, "%.15f");

    if (ImGui::Button("Go")) {
        if (inputZoom == INITIAL_ZOOM && inputReal == INITIAL_OFFSET_X && inputImag == INITIAL_OFFSET_Y)
            resetToInitialFractal();
        else {
            setFractalOffset(inputReal, inputImag);
            setZoomLevel(inputZoom);

            beginAsyncRendering();
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset"))
        resetToInitialFractal();

    ImGui::Separator();

    static int inputMaxIterations = maxIterations;

    ImGui::Text("Max Iterations");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(56.0f);
    ImGui::InputInt("##Max Iterations", &inputMaxIterations, 0, 0);

    if (ImGui::Button("Set") && inputMaxIterations > 0 && inputMaxIterations) {
        maxIterations = std::fmin(inputMaxIterations, MAX_ITERATIONS_LIMIT);
        beginAsyncRendering();
    }

    ImGui::SameLine();
    if (ImGui::Button("Full Render"))
        beginAsyncRendering(true);

    ImGui::End();
}

void FractalRenderer::drawFractalSelector() {
    ImGui::SetNextWindowPos(ImVec2(366, 10), ImGuiCond_Once);

    ImGui::Begin("Fractal Selector", nullptr, BASE_WINDOW_FLAGS);

    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo("##Select Fractal", fractalOptions[curFractalIdx].name.c_str())) {
        for (int i = 0; i < fractalOptions.size(); i++) {
            bool isSelected = curFractalIdx == i;
            if (ImGui::Selectable(fractalOptions[i].name.c_str(), isSelected))
                selectFractal(i);    

            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::End();
}

void FractalRenderer::drawRenderingSettings() {
    ImGui::SetNextWindowPos(ImVec2(548, 10), ImGuiCond_Once);

    ImGui::Begin("Rendering Settings", nullptr, BASE_WINDOW_FLAGS);

    ImGui::Text("Resolution");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(64);
    if (ImGui::BeginCombo("##Resolution", resolutionOptions[curResolutionIdx].name.c_str())) {
        for (int i = 0; i < resolutionOptions.size(); i++) {
            bool isSelected = curResolutionIdx == i;
            if (ImGui::Selectable(resolutionOptions[i].name.c_str(), isSelected))
                selectResolution(i);

            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::End();
}

void  FractalRenderer::drawProgressBar() {
    ImGui::SetNextWindowPos(ImVec2(712, 10), ImGuiCond_Once);

    ImGui::Begin("Render Progress", nullptr, BASE_WINDOW_FLAGS);
    float progress = static_cast<float>(renderProgress) / renderMaxProgress;
    ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f), progress == 1.0f ? "Finished" : "Rendering...");
    ImGui::End();
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

        drawFractalInfo();
        drawFractalControls();
        drawFractalSelector();
        drawRenderingSettings();
        drawProgressBar();

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
    }

    SDL_RenderPresent(renderer);
}

void FractalRenderer::run() {
    beginAsyncRendering();

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

std::string FractalRenderer::generatePNGFilename() {
    if (!std::filesystem::exists(IMAGE_PATH))
        std::filesystem::create_directories(IMAGE_PATH);
    
    int highestNum = 0;
    std::regex pattern(fractalOptions[curFractalIdx].name + "-(\\d+)\\.png");  // Match format "fractalName-<number>.png"

    // Loop through files in the saved images directory
    for (const auto& entry : std::filesystem::directory_iterator(IMAGE_PATH)) {
        std::string filename = entry.path().filename().string();
        std::smatch match;

        // Check if filename matches the pattern
        if (std::regex_match(filename, match, pattern) && match.size() > 1) {
            int fileNumber = std::stoi(match[1].str());
            highestNum = std::max(highestNum, fileNumber);
        }
    }

    // Increment the highest number to get the next available filename
    int nextNumber = highestNum + 1;
    std::string newFilename = fractalOptions[curFractalIdx].name + "-" + std::to_string(nextNumber) + ".png";
    return (std::filesystem::path(IMAGE_PATH) / newFilename).string();
}