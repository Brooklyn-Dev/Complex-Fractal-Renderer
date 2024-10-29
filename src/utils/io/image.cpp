#include "./image.hpp"

void saveTextureAsPNG(SDL_Renderer* renderer, SDL_Texture* texture, const std::string& filename) {
    int width, height;
    SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create surface: %s", SDL_GetError());
        return;
    }

    SDL_SetRenderTarget(renderer, texture);

    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, surface->pixels, surface->pitch) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to read pixels: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    SDL_SetRenderTarget(renderer, nullptr);

    if (IMG_SavePNG(surface, filename.c_str()) < 0) 
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to save PNG: %s", SDL_GetError());

    SDL_FreeSurface(surface);
}