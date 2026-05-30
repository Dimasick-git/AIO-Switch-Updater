#include "ryazhenka_palette.hpp"

#include <array>
#include <cctype>
#include <string>

namespace ryazhenka::theme {

namespace {

// Index helper: two variants per palette, laid out [id*2 + variant].
constexpr int index(PaletteId id, Variant v) {
    return static_cast<int>(id) * 2 + static_cast<int>(v);
}

// clang-format off
const std::array<Palette, kPaletteCount * 2> kPalettes = {{
    // ---- Ryazhenka (cream + red-brown) — LIGHT ----
    {
        /*bg*/        {245, 230, 200}, /*surface*/   {234, 217, 181},
        /*text*/      {58, 36, 24},    /*textDim*/   {122, 90, 64},
        /*accent*/    {139, 58, 47},   /*accentText*/{245, 230, 200},
        /*highlight1*/{232, 74, 47},   /*highlight2*/{192, 74, 48},
        /*separator*/ {200, 181, 149},
        /*cardBg*/    {255, 255, 255, 160}, /*cardBorder*/{200, 165, 120, 200},
        /*glow*/      {232, 74, 47},   /*waveA*/     {212, 165, 116}, /*waveB*/{139, 58, 47},
    },
    // ---- Ryazhenka — DARK ----
    {
        /*bg*/        {26, 14, 8},     /*surface*/   {42, 24, 16},
        /*text*/      {245, 230, 200}, /*textDim*/   {184, 155, 122},
        /*accent*/    {212, 165, 116}, /*accentText*/{26, 14, 8},
        /*highlight1*/{232, 74, 47},   /*highlight2*/{245, 165, 130},
        /*separator*/ {74, 50, 37},
        /*cardBg*/    {42, 24, 16, 200},   /*cardBorder*/{90, 61, 42, 180},
        /*glow*/      {232, 74, 47},   /*waveA*/     {139, 58, 47},   /*waveB*/{212, 165, 116},
    },

    // ---- Cyberpunk (fuchsia + cyan) — LIGHT ----
    {
        /*bg*/        {236, 230, 250}, /*surface*/   {220, 210, 240},
        /*text*/      {26, 15, 46},    /*textDim*/   {106, 90, 144},
        /*accent*/    {214, 0, 107},   /*accentText*/{255, 255, 255},
        /*highlight1*/{255, 46, 136},  /*highlight2*/{0, 184, 200},
        /*separator*/ {192, 176, 224},
        /*cardBg*/    {255, 255, 255, 170}, /*cardBorder*/{184, 160, 224, 210},
        /*glow*/      {255, 46, 136},  /*waveA*/     {255, 46, 136},  /*waveB*/{0, 184, 200},
    },
    // ---- Cyberpunk — DARK ----
    {
        /*bg*/        {26, 15, 46},    /*surface*/   {42, 26, 69},
        /*text*/      {240, 233, 255}, /*textDim*/   {169, 155, 208},
        /*accent*/    {255, 46, 136},  /*accentText*/{20, 8, 31},
        /*highlight1*/{255, 46, 136},  /*highlight2*/{0, 240, 255},
        /*separator*/ {68, 48, 106},
        /*cardBg*/    {36, 22, 64, 200},   /*cardBorder*/{90, 61, 138, 190},
        /*glow*/      {0, 240, 255},   /*waveA*/     {255, 46, 136},  /*waveB*/{0, 240, 255},
    },

    // ---- Gold (black + amber) — LIGHT ----
    {
        /*bg*/        {251, 246, 232}, /*surface*/   {240, 232, 208},
        /*text*/      {42, 36, 16},    /*textDim*/   {128, 118, 80},
        /*accent*/    {176, 120, 0},   /*accentText*/{255, 255, 255},
        /*highlight1*/{255, 176, 0},   /*highlight2*/{208, 160, 48},
        /*separator*/ {216, 204, 160},
        /*cardBg*/    {255, 255, 255, 170}, /*cardBorder*/{216, 192, 120, 200},
        /*glow*/      {255, 176, 0},   /*waveA*/     {255, 176, 0},   /*waveB*/{208, 160, 48},
    },
    // ---- Gold — DARK ----
    {
        /*bg*/        {10, 10, 10},    /*surface*/   {26, 26, 20},
        /*text*/      {255, 246, 224}, /*textDim*/   {184, 173, 144},
        /*accent*/    {255, 176, 0},   /*accentText*/{26, 20, 0},
        /*highlight1*/{255, 176, 0},   /*highlight2*/{255, 238, 153},
        /*separator*/ {58, 53, 32},
        /*cardBg*/    {22, 22, 16, 205},   /*cardBorder*/{90, 80, 40, 180},
        /*glow*/      {255, 176, 0},   /*waveA*/     {255, 176, 0},   /*waveB*/{255, 238, 153},
    },

    // ---- Ocean (deep blue + teal) — LIGHT ----
    {
        /*bg*/        {232, 244, 250}, /*surface*/   {212, 232, 242},
        /*text*/      {13, 27, 42},    /*textDim*/   {90, 117, 136},
        /*accent*/    {0, 140, 158},   /*accentText*/{255, 255, 255},
        /*highlight1*/{0, 217, 197},   /*highlight2*/{46, 134, 197},
        /*separator*/ {176, 204, 221},
        /*cardBg*/    {255, 255, 255, 170}, /*cardBorder*/{160, 196, 216, 200},
        /*glow*/      {0, 197, 181},   /*waveA*/     {0, 217, 197},   /*waveB*/{46, 134, 197},
    },
    // ---- Ocean — DARK ----
    {
        /*bg*/        {13, 27, 42},    /*surface*/   {22, 41, 61},
        /*text*/      {224, 244, 255}, /*textDim*/   {143, 176, 197},
        /*accent*/    {0, 217, 197},   /*accentText*/{4, 32, 28},
        /*highlight1*/{0, 217, 197},   /*highlight2*/{127, 255, 230},
        /*separator*/ {42, 71, 99},
        /*cardBg*/    {18, 36, 54, 200},   /*cardBorder*/{46, 85, 119, 180},
        /*glow*/      {0, 217, 197},   /*waveA*/     {0, 217, 197},   /*waveB*/{46, 134, 197},
    },

    // ---- Standard (Horizon-style, white) — LIGHT ----
    {
        /*bg*/        {251, 251, 251}, /*surface*/   {245, 245, 245},
        /*text*/      {45, 45, 45},    /*textDim*/   {120, 120, 120},
        /*accent*/    {0, 199, 161},   /*accentText*/{52, 41, 55},
        /*highlight1*/{25, 138, 198},  /*highlight2*/{137, 241, 242},
        /*separator*/ {220, 220, 220},
        /*cardBg*/    {255, 255, 255, 200}, /*cardBorder*/{210, 210, 210, 220},
        /*glow*/      {25, 138, 198},  /*waveA*/     {200, 220, 235}, /*waveB*/{137, 241, 242},
    },
    // ---- Standard — DARK (mirrors HorizonDarkTheme) ----
    {
        /*bg*/        {45, 45, 45},    /*surface*/   {50, 50, 50},
        /*text*/      {255, 255, 255}, /*textDim*/   {163, 163, 163},
        /*accent*/    {0, 255, 204},   /*accentText*/{52, 41, 55},
        /*highlight1*/{25, 138, 198},  /*highlight2*/{137, 241, 242},
        /*separator*/ {78, 78, 78},
        /*cardBg*/    {57, 58, 60, 210}, /*cardBorder*/{81, 81, 81, 220},
        /*glow*/      {25, 138, 198},  /*waveA*/     {31, 34, 39},    /*waveB*/{88, 195, 169},
    },

    // ---- Slate (neutral grey) — LIGHT ----
    {
        /*bg*/        {241, 243, 246}, /*surface*/   {230, 233, 238},
        /*text*/      {32, 36, 42},    /*textDim*/   {110, 115, 122},
        /*accent*/    {72, 90, 110},   /*accentText*/{255, 255, 255},
        /*highlight1*/{96, 124, 150},  /*highlight2*/{160, 180, 200},
        /*separator*/ {210, 213, 218},
        /*cardBg*/    {255, 255, 255, 200}, /*cardBorder*/{200, 205, 212, 220},
        /*glow*/      {110, 130, 150}, /*waveA*/     {180, 195, 210}, /*waveB*/{220, 225, 232},
    },
    // ---- Slate — DARK ----
    {
        /*bg*/        {28, 32, 38},    /*surface*/   {38, 44, 52},
        /*text*/      {230, 234, 240}, /*textDim*/   {148, 156, 168},
        /*accent*/    {158, 178, 200}, /*accentText*/{24, 28, 33},
        /*highlight1*/{120, 140, 165}, /*highlight2*/{180, 200, 220},
        /*separator*/ {58, 66, 78},
        /*cardBg*/    {44, 50, 58, 210}, /*cardBorder*/{82, 92, 105, 220},
        /*glow*/      {130, 150, 175}, /*waveA*/     {60, 72, 88},    /*waveB*/{120, 140, 165},
    },
}};
// clang-format on

}  // namespace

const Palette& getPalette(PaletteId id, Variant variant) {
    return kPalettes[index(id, variant)];
}

PaletteId paletteFromString(std::string_view name) {
    std::string lower;
    lower.reserve(name.size());
    for (char c : name)
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    if (lower == "cyberpunk") return PaletteId::Cyberpunk;
    if (lower == "gold")      return PaletteId::Gold;
    if (lower == "ocean")     return PaletteId::Ocean;
    if (lower == "standard")  return PaletteId::Standard;
    if (lower == "slate")     return PaletteId::Slate;
    return PaletteId::Ryazhenka;
}

const char* paletteToString(PaletteId id) {
    switch (id) {
        case PaletteId::Cyberpunk: return "cyberpunk";
        case PaletteId::Gold:      return "gold";
        case PaletteId::Ocean:     return "ocean";
        case PaletteId::Standard:  return "standard";
        case PaletteId::Slate:     return "slate";
        case PaletteId::Ryazhenka:
        default:                   return "ryazhenka";
    }
}

const char* paletteI18nKey(PaletteId id) {
    switch (id) {
        case PaletteId::Cyberpunk: return "menus/ryazhenka/palette/cyberpunk";
        case PaletteId::Gold:      return "menus/ryazhenka/palette/gold";
        case PaletteId::Ocean:     return "menus/ryazhenka/palette/ocean";
        case PaletteId::Standard:  return "menus/ryazhenka/palette/standard";
        case PaletteId::Slate:     return "menus/ryazhenka/palette/slate";
        case PaletteId::Ryazhenka:
        default:                   return "menus/ryazhenka/palette/ryazhenka";
    }
}

}  // namespace ryazhenka::theme
