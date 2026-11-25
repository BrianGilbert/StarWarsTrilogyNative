// RGBA8 pictures: the menus' pictures read from files (Windows Imaging
// Component; ImageIO on macOS, stb_image on Linux), and the procedural skies
// handed to the renderer. Rows top to bottom, 4 bytes per pixel.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// A picture to be uploaded by the renderer into its slot
struct RgbaImage {
    uint32_t slot = 0;
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
};

bool png_load(const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba);
