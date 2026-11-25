// The command-line table (options.cpp): every kind of entry, the fallback for
// options of other modules and the test-run detection that leaves the menu out
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "options.h"
#include "settings_bridge.h"
#include "frontend_settings.h"

static int g_failed = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAILED line %d: %s\n", __LINE__, #cond);            \
            g_failed++;                                                 \
        }                                                               \
    } while (0)

static bool close_to(float a, float b) { return std::fabs(a - b) < 1e-4f; }

// Parses a list of arguments; returns whether the game would run
static bool parse(Options& o, std::vector<const char*> args, int* code = nullptr) {
    args.insert(args.begin(), "StarWarsTrilogy.exe");
    int exit_code = -1;
    bool run = parse_options(o, int(args.size()), const_cast<char**>(args.data()), exit_code);
    if (code) *code = exit_code;
    return run;
}

int main() {
    {   // defaults
        Options o;
        CHECK(parse(o, {}));
        CHECK(o.msaa == 4 && !o.dlaa && o.aniso == 8 && close_to(o.bloom, 1) && close_to(o.ao, 1) && close_to(o.haze, 1));
        CHECK(close_to(o.saturation, 0.8f) && o.frame_limit == INT32_MAX && !o.headless);
        CHECK(o.hdr && o.sun && o.shadows && o.menu && o.data_dir == "data" && o.rom_zip.empty());
    }
    {   // the capture used to verify the renderer: values, optional strengths and plain flags
        Options o;
        CHECK(parse(o, {"--shot", "2600", "--bloom", "1.6", "--haze", "1", "--ao", "1"}));
        CHECK(o.shot_frame == 2600 && close_to(o.bloom, 1.6f) && close_to(o.haze, 1) && close_to(o.ao, 1));
        Options p;
        CHECK(parse(p, {"--no-ao", "--no-bloom", "--no-haze", "--ao", "--no-sun", "--letterbox", "--pixel2d"}));
        CHECK(close_to(p.ao, 1) && close_to(p.bloom, 0) && close_to(p.haze, 0) && !p.sun && p.letterbox && p.pixel_2d);
        Options q;   // a strength followed by another option keeps the default 1
        CHECK(parse(q, {"--no-haze", "--haze", "--no-hdr", "--bloom", "0.5"}));
        CHECK(close_to(q.haze, 1) && !q.hdr && close_to(q.bloom, 0.5f));
    }
    {   // custom values
        Options o;
        CHECK(parse(o, {"--aa", "dlaa", "--aspect", "21:9", "--aim",
                        "100,200", "--msaa", "8", "--scale", "1.5", "--volume", "3", "--wav", "out/a.wav", "--data",
                        "roms"}));
        CHECK(o.dlaa && o.msaa == 8 && close_to(o.max_aspect, 21.0f / 9.0f));
        CHECK(o.fixed_aim && o.aim_x == 100 && o.aim_y == 200 && close_to(o.supersample, 1.5f) && close_to(o.volume, 3));
        CHECK(o.wav_path == "out/a.wav" && o.data_dir == "roms");
        Options r;
        CHECK(parse(r, {"--rom", "D:\\roms\\swtrilgy.zip"}) && r.rom_zip == "D:\\roms\\swtrilgy.zip");
        Options sp;
        CHECK(sp.speakers == 0 && sp.cubic_samples && sp.rear_boost == 6);
        CHECK(parse(sp, {"--speakers", "headphones", "--linear-samples"}));
        CHECK(sp.speakers == 2 && !sp.cubic_samples);
        CHECK(parse(sp, {"--speakers", "surround"}) && sp.speakers == 1);
        CHECK(parse(sp, {"--rear-boost", "0"}) && sp.rear_boost == 0);
        Options ex;   // the new explosions unless asked
        CHECK(ex.explosions && parse(ex, {"--explosions", "arcade"}) && !ex.explosions);
        CHECK(parse(ex, {"--explosions", "enhanced"}) && ex.explosions);
        Options p;
        CHECK(parse(p, {"--aa", "none", "--aspect", "1.5"}));
        CHECK(!p.dlaa && p.msaa == 1 && close_to(p.max_aspect, 1.5f));
        Options q;
        CHECK(parse(q, {"--aa", "msaa", "--auto", "coin@10", "--auto", "start@20", "--trace",
                        "10F650,10D564", "--diff", "10F650@300", "--replay", "out/snap/snap_1.bin"}));
        CHECK(!q.dlaa && q.msaa == 4 && q.auto_scripts.size() == 2);
        Options d;   // the DLSS modes and the model
        CHECK(parse(d, {"--aa", "dlss"}) && d.dlaa && d.dlss == 1 && d.dlss_model == 0);
        CHECK(parse(d, {"--aa", "dlss_balanced", "--dlss-model", "m"}) && d.dlaa && d.dlss == 2 && d.dlss_model == 2);
        CHECK(parse(d, {"--aa", "dlss_performance", "--dlss-model", "l"}) && d.dlss == 3 && d.dlss_model == 3);
        CHECK(parse(d, {"--aa", "dlss_ultra", "--dlss-model", "auto"}) && d.dlss == 4 && d.dlss_model == 0);
        CHECK(parse(d, {"--aa", "dlaa", "--dlss-model", "k"}) && d.dlaa && d.dlss == 0 && d.dlss_model == 1);
        CHECK(!d.fxaa);
        Options f;   // FXAA: no MSAA, no DLAA
        CHECK(parse(f, {"--aa", "fxaa"}) && f.fxaa && !f.dlaa && f.msaa == 1);
        CHECK(q.auto_scripts[1] == "start@20");
        CHECK(q.traces.size() == 1 && q.diff == "10F650@300" && q.replay == "out/snap/snap_1.bin");
    }
    {   // saved widescreen levels reach the runtime; command-line options still win
        const float aspects[] = {4.0f / 3.0f, 16.0f / 9.0f, 2.4f, 3.6f, 6.0f};
        for (int level = 0; level < 5; level++) {
            FrontendSettings settings;
            settings.aspect = level;
            Options o;
            options_from_settings(o, settings);
            CHECK(close_to(o.max_aspect, aspects[level]));
            FrontendSettings shown;
            settings_from_options(shown, o);
            CHECK(shown.aspect == level);
        }
        FrontendSettings settings;
        settings.aspect = 0;
        Options o;
        options_from_settings(o, settings);
        CHECK(parse(o, {"--aspect", "2.6"}) && close_to(o.max_aspect, 2.6f));
        settings_from_options(settings, o);
        options_from_settings(o, settings);
        CHECK(close_to(o.max_aspect, 2.6f));   // the menu keeps a custom ratio within a named level
        FrontendSettings auto_view;   // AUTO: 48:9's level, and still AUTO in the menu after
        auto_view.aspect = 5;
        Options wide;
        options_from_settings(wide, auto_view);
        CHECK(close_to(wide.max_aspect, 6.0f));
        settings_from_options(auto_view, wide);
        CHECK(auto_view.aspect == 5);
        CHECK(parse(wide, {"--aspect", "auto"}) && close_to(wide.max_aspect, 6.0f));
    }
    {   // a change in the menu while the game runs (live_options_from_settings)
        Options o;
        CHECK(parse(o, {"--haze", "0.6", "--aa", "dlaa"}));
        FrontendSettings s;
        s.haze = true;
        s.bloom = 3;
        s.aa = 2;   // a restart's option: not applied at once
        live_options_from_settings(o, s);
        CHECK(close_to(o.haze, 0.6f) && close_to(o.bloom, BLOOM_STRENGTH[3]) && o.dlaa);   // the haze keeps its strength
        s.haze = false;
        live_options_from_settings(o, s);
        CHECK(close_to(o.haze, 0.0f));
        s.haze = true;
        live_options_from_settings(o, s);
        CHECK(close_to(o.haze, 1.0f));   // on again: the menu's strength
    }
    {   // run modes
        Options o;
        CHECK(parse(o, {"--headless"}));
        CHECK(o.headless && o.frame_limit == INT32_MAX);   // 600 is applied by main()
        Options p;
        CHECK(parse(p, {"--headless", "--frames", "50", "--no-menu", "--boot"}));
        CHECK(p.headless && p.frame_limit == 50 && !p.menu && p.show_boot);
    }
    {   // errors and help
        Options o;
        int code = -1;
        CHECK(!parse(o, {"--frames"}, &code) && code == 1);   // value missing
        CHECK(!parse(o, {"--bogus"}, &code) && code == 1);
        CHECK(!parse(o, {"--help"}, &code) && code == 0);
        CHECK(!parse(o, {"--shot", "1", "-h"}, &code) && code == 0);
    }
    {   // test runs leave the menu out
        const char* a1[] = {"x", "--bloom", "1.6", "--shot", "1400"};
        const char* a2[] = {"x", "--bloom", "1.6", "--no-hdr"};
        CHECK(options_disable_menu(5, const_cast<char**>(a1)));
        CHECK(!options_disable_menu(4, const_cast<char**>(a2)));
    }
    if (g_failed) {
        printf("options: %d checks failed\n", g_failed);
        return 1;
    }
    printf("options: all checks passed\n");
    return 0;
}
