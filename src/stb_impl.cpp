// تنفيذ stb_image (PNG فقط) — SDL2_image في OpenOrbis لا يدعم PNG، لذلك نفكّ الترميز بأنفسنا.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#include "stb_image.h"
