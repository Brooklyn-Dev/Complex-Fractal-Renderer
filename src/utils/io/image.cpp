#include "./image.hpp"

void saveTextureAsPNG(SDL_Renderer* renderer, SDL_Texture* texture, const std::string& filename) {
    int width, height;
    SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);

    SDL_SetRenderTarget(renderer, texture);
    SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, surface->pixels, surface->pitch);
    SDL_SetRenderTarget(renderer, nullptr);

    IMG_SavePNG(surface, filename.c_str());
    SDL_FreeSurface(surface);
}