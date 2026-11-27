#pragma once
#include <SDL2/SDL.h>
#include <proto-include.h>
#include <string>

class ArabicText {
public:
    bool initialize(const char* fontPath, int pixelSize, const char* fallbackPath = nullptr);
    SDL_Texture* texture(SDL_Renderer* renderer, const std::string& utf8, SDL_Color color);
    void shutdown();
private:
    FT_Library library_ = nullptr;
    FT_Face face_ = nullptr;
    FT_Face fallbackFace_ = nullptr;
};
