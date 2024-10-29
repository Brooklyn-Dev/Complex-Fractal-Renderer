#ifndef IMAGE_H
#define IMAGE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <string>

void saveTextureAsPNG(SDL_Renderer* renderer, SDL_Texture* texture, const std::string& filename);

#endif