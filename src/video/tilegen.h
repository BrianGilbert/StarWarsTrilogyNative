// Model 3 2D tile generator: 4 layers (A, A', B, B') of 64x64 tiles of 8x8
// pixels, drawn below or above the 3D image. See docs/real3d_spec.md 7.6:
//   0x000000  patterns (4 bpp: 32 bytes per tile, 8 bpp: 64 bytes)
//   0x0F6000  line scroll tables, 0x0F7000 A/A' and B/B' selection mask
//   0x0F8000  name tables, 0x2000 bytes per layer; each 32-bit
//             (little-endian) word holds two 16-bit entries, left in the high half
//   0x100000  palette: 32768 colours, R in the low bits, bit 15 = transparent
//   0x180000  registers
#pragma once
#include <cstdint>
#include <vector>

class SkyPack;

constexpr int SCREEN_W = 496, SCREEN_H = 384;
// The layers below the 3D image (the sky) are also drawn beyond both sides of
// the screen for widescreen: the tilemaps wrap every 512 pixels, so the extra
// columns continue the background as the game scrolls it.
constexpr int BACK_MARGIN = 200, BACK_W = SCREEN_W + 2 * BACK_MARGIN;
constexpr uint32_t TILEGEN_BASE = 0xF1000000, TILEGEN_SIZE = 0x190000;

class TileGen {
public:
    // Images compose() fills from the frame's layers (BGRA pixels, 0xAARRGGBB,
    // alpha 0 = empty). back and front are required; the rest are optional
    // (nullptr = not wanted) and cost time only when asked for.
    struct Output {
        uint32_t* back = nullptr;    // BACK_W x SCREEN_H: the layers below the 3D image
        uint32_t* front = nullptr;   // SCREEN_W x SCREEN_H: the layers above it
        // The foreground without the brightness offsets the game uses for
        // fades (boot_logo_up in runtime.cpp)
        uint32_t* front_raw = nullptr;
        // With sky: which procedural sky and point each back pixel shows
        // (SKY_COORDS_W x SKY_COORDS_H; layout below)
        uint32_t* sky_coords = nullptr;
        // SCREEN_H x 3: on each row, the layer that shows most of the back
        // image on screen (-1: none) and where its picture is at screen
        // column 0 (x, y in its 512x512 picture), to follow its scroll
        int32_t* back_rows = nullptr;
    };
    // vram: copy of 0xF1000000..0xF118FFFF as written by the game.
    // sky (optional): procedural skies for the layers below the 3D image; their
    // pixels in `back` come from the sky (needs out.sky_coords).
    // Reentrant: the only state kept between calls is a per-thread palette cache.
    static void compose(const uint8_t* vram, const Output& out, SkyPack* sky = nullptr);
    // Sky coordinates: per back pixel B = upper sky << 6 | layer pair << 5 |
    // picture slot << 2 | y bit 8 << 1 | x bit 8,
    // R = x, G = y (low bits, in the 512x512 layer), A = 255 where a procedural
    // sky shows; the first extra row starts with the fade offset of each layer pair
    // (R, G, B = offset / 2 + 128), and from texel 8 on holds the colours of
    // each procedural sky (SkyPack::Picture::bands, 64 texels per slot). The
    // second one is not written here: the window thread puts there how far
    // the back layers scrolled on each row since the previous game frame
    // (texel y: R, G = x, y pixels + 128, B = 255 where known), for the interpolation
    static constexpr int SKY_COORDS_W = BACK_W, SKY_COORDS_H = SCREEN_H + 2;
    // The whole 512x512 picture of a layer, without scroll or fade offsets
    static void layer_plane(const uint8_t* vram, int layer, bool four_bpp, std::vector<uint32_t>& out);
    // Hash of the data a layer is drawn from (names, patterns, palettes)
    static uint64_t layer_key(const uint8_t* vram, int layer, bool four_bpp);
};
