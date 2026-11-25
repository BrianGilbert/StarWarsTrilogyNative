#include "sky_pack.h"

#include "../common/fnv1a.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "tilegen.h"

const SkyPack::Picture* SkyPack::find(const uint8_t* vram, int layer, bool four_bpp) {
    // The picture is only rebuilt and hashed when the layer's data changes
    uint64_t key = TileGen::layer_key(vram, layer, four_bpp);
    auto it = hashes_.find(key);
    bool rendered = false, seen = it != hashes_.end();
    uint64_t hash;
    if (seen) {
        hash = it->second;
    } else {
        if (hashes_.size() > 4096) hashes_.clear();   // animated title screens
        TileGen::layer_plane(vram, layer, four_bpp, plane_);
        rendered = true;
        hash = fnv1a_words(plane_.data(), plane_.size());
        hashes_[key] = hash;
    }
    // Procedural: skies are 8 bpp pictures in the back pair (layer 3, the
    // alternate, is the one that continues above) that wrap around as a
    // panorama, unlike framed pictures such as the title screen
    if (procedural_on_ && layer >= 2 && !four_bpp) {
        auto p = panorama_.find(hash);
        if (p == panorama_.end()) {
            if (!rendered) TileGen::layer_plane(vram, layer, four_bpp, plane_);
            rendered = true;
            int opaque = 0, lit = 0, edge = 0;
            for (int y = 0; y < SIZE; y++)
                for (int x = 0; x < SIZE; x++) {
                    uint32_t c = plane_[y * SIZE + x];
                    opaque += (c >> 24) != 0;
                    if (x >= 496) {   // the columns the screen does not show
                        edge++;
                        lit += ((c >> 16) & 255) >= 24 || ((c >> 8) & 255) >= 24 || (c & 255) >= 24;
                    }
                }
            p = panorama_.emplace(hash, opaque > SIZE * SIZE * 95 / 100 && lit * 10 > edge * 9).first;
        }
        if (p->second) {
            if (!procedural_.count(hash) && !rendered) TileGen::layer_plane(vram, layer, four_bpp, plane_);
            return procedural(hash, layer == 3);
        }
    }
    return nullptr;
}

// ---- Procedural sky ----
// The same sky as compose.frag sky_procedural (keep both in step), without the
// clouds' drift: it stands in for the layer's own pixels (haze, bloom, horizon).

namespace {
float sky_hash(int x, int y) {
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return float((h ^ (h >> 16)) & 0xFFFFFFu) / 16777216.0f;
}

// Value noise that repeats every `period` cells along x (the layer wraps around)
float sky_noise(float x, float y, int period) {
    float fx = std::floor(x), fy = std::floor(y);
    int ix = int(fx), iy = int(fy);
    int x0 = ((ix % period) + period) % period, x1 = (x0 + 1) % period;
    float u = x - fx, v = y - fy;
    u = u * u * (3 - 2 * u);
    v = v * v * (3 - 2 * v);
    float a = sky_hash(x0, iy) + (sky_hash(x1, iy) - sky_hash(x0, iy)) * u;
    float b = sky_hash(x0, iy + 1) + (sky_hash(x1, iy + 1) - sky_hash(x0, iy + 1)) * u;
    return a + (b - a) * v;
}

float sky_fbm(float x, float y, int period, int octaves) {
    float s = 0, a = 0.5f;
    for (int i = 0; i < octaves; i++) {
        s += a * sky_noise(x, y, period);
        x = x * 2 + 5.3f;
        y = y * 2 + 9.7f;
        period *= 2;
        a *= 0.5f;
    }
    return s;
}

float sky_density(float u, float v) { return (std::max)(sky_fbm(u, v, 8, 6) - 0.45f, 0.0f) * 10.0f; }

float smooth(float e0, float e1, float x) {
    float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

// Colour of a band of a procedural sky (fy: row / 16 - 0.5, interpolated)
void band_colour(const uint32_t* bands, float fy, int which, float out[3]) {
    fy = std::clamp(fy, 0.0f, float(SkyPack::BANDS - 1));
    int i0 = int(fy), i1 = (std::min)(i0 + 1, SkyPack::BANDS - 1);
    float f = fy - float(i0);
    uint32_t a = bands[i0 * 2 + which], b = bands[i1 * 2 + which];
    for (int c = 0; c < 3; c++) {
        int shift = 16 - 8 * c;
        out[c] = (float((a >> shift) & 255) * (1 - f) + float((b >> shift) & 255) * f) / 255;
    }
}

// Sky colour at a point of a sky layer: x, y in its 512x512 picture; the
// upper layer continues above the lower one
void sky_colour(float x, float y, bool upper, const uint32_t* bands, float drift, float out[3]) {
    float sky[3], lit[3];
    band_colour(bands, y / 16 - 0.5f, 0, sky);
    band_colour(bands, y / 16 - 0.5f, 1, lit);
    if (upper) y -= SkyPack::SIZE;
    const float HORIZON = 500.0f, HEIGHT = 1012.0f;
    float t = std::clamp((HORIZON - y) / HEIGHT, 0.0f, 1.0f);   // 0 at the horizon, 1 at the top
    // More depth than the original: richer colour, darker towards the top,
    // a pale glow at the horizon, and whiter cloud tops
    float l = 0.2126f * sky[0] + 0.7152f * sky[1] + 0.0722f * sky[2];
    for (int c = 0; c < 3; c++) {
        sky[c] = (std::max)(l + (sky[c] - l) * 1.3f, 0.0f) * (1 - 0.3f * smooth(0.1f, 0.8f, t)) +
                 0.06f * (1 - smooth(0.0f, 0.15f, t));
        lit[c] += (1 - lit[c]) * 0.3f;
    }
    // The layer is a cylinder around the camera: u around it (8 cells), v up
    // it, squeezed towards the horizon by perspective
    float el = 0.03f + 1.5f * t;   // elevation (radians)
    float u = x * (8.0f / 512.0f) + drift;
    float v = 6.0f * std::tanh(2.5f / std::tan(el) / 6.0f) - 6.0f * t;
    float wu = sky_fbm(u * 0.5f + 3.1f, v * 0.5f, 4, 5), wv = sky_fbm(u * 0.5f, v * 0.5f + 7.7f, 4, 5);
    u += 0.5f * wu;
    v += 0.5f * wv;
    // Cloud density, then the light that reaches each point from above
    // through the cloud: lit tops, shaded bases, bright thin edges
    float density = sky_density(u, v);
    float above = 0;
    for (int i = 1; i <= 4; i++) above += sky_density(u, v - 0.06f * i);
    float light = std::exp(-above * 0.9f);
    float cover = (1 - std::exp(-density * 3.0f)) * smooth(0.0f, 0.12f, t);
    float billows = 0.8f + 0.4f * sky_fbm(u * 4 + 1.7f, v * 4 + 2.9f, 32, 3);
    float distant = smooth(0.17f, 0.0f, t) * 0.6f;
    for (int c = 0; c < 3; c++) {
        float shadow = (sky[c] * 0.6f + lit[c] * 0.3f) * 0.75f;
        float cloud = (shadow + ((std::min)(lit[c] * 1.05f, 1.0f) - shadow) * light + (1 - cover) * 0.15f) * billows;
        cloud += (sky[c] - cloud) * distant;
        out[c] = sky[c] + (cloud - sky[c]) * cover;
    }
}
}  // namespace

// plane_ holds the original picture
const SkyPack::Picture* SkyPack::procedural(uint64_t hash, bool upper) {
    auto it = procedural_.find(hash);
    if (it != procedural_.end()) {
        if (!upper) last_lower_ = &it->second;
        return &it->second;
    }
    uint32_t slot = LAST_SLOT - uint32_t(procedural_.size());
    if (slot < FIRST_SLOT) return nullptr;   // no slot left: the original stays
    Picture& p = procedural_[hash];
    p.slot = slot;
    p.upper = upper;
    p.low.resize(SIZE * SIZE);
    // Colours per band: the clear sky (a quarter of the way up from the
    // darkest) and the lit clouds (nine tenths), leaving out the trees
    auto lum = [](uint32_t c) { return 0.2126f * ((c >> 16) & 255) + 0.7152f * ((c >> 8) & 255) + 0.0722f * (c & 255); };
    for (int band = 0; band < BANDS; band++) {
        std::vector<uint32_t> px;
        for (int y = band * 16; y < band * 16 + 16; y++)
            for (int x = 0; x < SIZE; x += 2) {
                uint32_t c = plane_[y * SIZE + x];
                if (!upper && y > 455 && lum(c) < 0.2f * 255) continue;
                px.push_back(c);
            }
        if (px.empty()) {
            p.bands[band * 2] = band ? p.bands[band * 2 - 2] : 0xFF203860u;
            p.bands[band * 2 + 1] = band ? p.bands[band * 2 - 1] : 0xFFE0E0E8u;
            continue;
        }
        std::sort(px.begin(), px.end(), [&](uint32_t a, uint32_t b) { return lum(a) < lum(b); });
        p.bands[band * 2] = px[px.size() / 4] | 0xFF000000u;
        p.bands[band * 2 + 1] = px[px.size() * 9 / 10] | 0xFF000000u;
    }
    // Smooth from band to band, and clouds always lighter than the sky
    for (int pass = 0; pass < 2; pass++) {
        uint32_t old[BANDS * 2];
        memcpy(old, p.bands, sizeof(old));
        for (int band = 0; band < BANDS; band++)
            for (int which = 0; which < 2; which++) {
                int a = (std::max)(band - 1, 0), b = (std::min)(band + 1, BANDS - 1);
                uint32_t out = 0xFF000000u;
                for (int shift = 0; shift <= 16; shift += 8) {
                    uint32_t v = ((old[a * 2 + which] >> shift & 255) + 2 * (old[band * 2 + which] >> shift & 255) +
                                  (old[b * 2 + which] >> shift & 255) + 2) / 4;
                    out |= v << shift;
                }
                p.bands[band * 2 + which] = out;
            }
    }
    // The upper sky's bottom rows meet the top of the lower one: blend into it
    if (upper && last_lower_)
        for (int band = BANDS - 8; band < BANDS; band++) {
            float w = float(band - (BANDS - 9)) / 9;
            for (int which = 0; which < 2; which++) {
                uint32_t a = p.bands[band * 2 + which], b = last_lower_->bands[which], out = 0xFF000000u;
                for (int shift = 0; shift <= 16; shift += 8)
                    out |= uint32_t(float(a >> shift & 255) * (1 - w) + float(b >> shift & 255) * w + 0.5f) << shift;
                p.bands[band * 2 + which] = out;
            }
        }
    for (int band = 0; band < BANDS; band++) {
        uint32_t sky = p.bands[band * 2], lit = p.bands[band * 2 + 1], out = 0xFF000000u;
        for (int shift = 0; shift <= 16; shift += 8) {
            uint32_t least = (std::min)(uint32_t((sky >> shift & 255) * 1.3f + 20), 255u);
            out |= (std::max)(lit >> shift & 255, least) << shift;
        }
        p.bands[band * 2 + 1] = out;
    }
    RgbaImage img{slot, SIZE, SIZE, std::vector<uint8_t>(SIZE * SIZE * 4)};
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            uint32_t o = plane_[y * SIZE + x];
            float orig[3] = {float((o >> 16) & 255) / 255, float((o >> 8) & 255) / 255, float(o & 255) / 255};
            uint8_t* d = &img.rgba[(y * SIZE + x) * 4];
            d[0] = uint8_t(o >> 16);
            d[1] = uint8_t(o >> 8);
            d[2] = uint8_t(o);
            d[3] = 255;
            float c[3];
            sky_colour(x + 0.5f, y + 0.5f, upper, p.bands, 0, c);
            // Trees on the horizon: the dark parts of the original's bottom rows
            float tree = !upper && y > 455 ? 1 - smooth(0.10f, 0.20f, lum(o) / 255) : 0;
            uint32_t rgb = 0;
            for (int k = 0; k < 3; k++) {
                float v = c[k] + (orig[k] * 0.9f - c[k]) * tree;
                rgb = rgb << 8 | uint32_t(std::clamp(v, 0.0f, 1.0f) * 255 + 0.5f);
            }
            p.low[y * SIZE + x] = 0xFF000000u | rgb;
        }
    if (!upper) {
        last_lower_ = &p;
        pending_.push_back(std::move(img));
    }   // the upper one has no trees to show
    printf("[sky] procedural sky for %016llx%s\n", (unsigned long long)hash, upper ? " (upper)" : "");
    return &p;
}

std::vector<RgbaImage> SkyPack::take_new_images() {
    std::vector<RgbaImage> out;
    out.swap(pending_);
    return out;
}
