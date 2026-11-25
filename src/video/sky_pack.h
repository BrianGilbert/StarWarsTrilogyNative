// Procedural skies for the 2D layers below the 3D image.
//
// Each of those layers is a 512x512 picture that wraps around as the game
// scrolls it. It is identified by a hash of that picture (without the
// brightness offsets of fades), so the same sky is recognised in every stage.
// With the procedural sky on, the stages' skies (any backdrop below the 3D
// image that wraps around as a panorama) are drawn instead with soft drifting
// clouds, at any resolution (compose.frag sky_procedural), in the colours of
// the original; the trees on the horizon still come from the original picture.
// A 512x512 version of it stands in for the layer everywhere else (the
// distance haze, the horizon colour).
#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "image_io.h"

class SkyPack {
public:
    // Slots of the backdrop pictures (compose.frag overlays[]): procedural
    // skies from the last one down, holding the original for the trees on the horizon
    static constexpr uint32_t FIRST_SLOT = 56, LAST_SLOT = 63;
    static constexpr int SIZE = 512;
    // Colours of a procedural sky, from its original: per band of 16 rows the
    // clear sky and the lit clouds (BGRA)
    static constexpr int BANDS = 32;

    bool active() const { return !disabled_ && procedural_on_; }
    // For good: the GPU cannot bind the pictures (R3DRenderer::overlay_slots)
    void disable() { disabled_ = true; }
    // Procedural sky instead of the stages' own
    void set_procedural(bool on) { procedural_on_ = on; }

    struct Picture {
        uint32_t slot;
        std::vector<uint32_t> low;   // SIZE x SIZE BGRA (alpha 0 or 255), for the layer's own pixels
        bool upper = false;          // the sky above the one with the horizon
        uint32_t bands[BANDS * 2] = {};   // sky and cloud colour per band
    };
    // Procedural sky for a layer below the 3D image (TileGen::compose), nullptr = none
    const Picture* find(const uint8_t* vram, int layer, bool four_bpp);

    // Pictures loaded since the last call, to be uploaded by the renderer
    std::vector<RgbaImage> take_new_images();

private:
    const Picture* procedural(uint64_t hash, bool upper);

    bool procedural_on_ = false, disabled_ = false;
    std::unordered_map<uint64_t, Picture> procedural_;   // sky hash -> procedural picture
    std::unordered_map<uint64_t, bool> panorama_;        // picture hash -> wraps around as a sky
    std::unordered_map<uint64_t, uint64_t> hashes_;      // layer contents key -> picture hash
    std::vector<RgbaImage> pending_;
    std::vector<uint32_t> plane_;
    const Picture* last_lower_ = nullptr;   // procedural sky with the horizon seen last
};
