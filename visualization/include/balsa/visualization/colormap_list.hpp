#if !defined(BALSA_VISUALIZATION_COLORMAP_LIST_HPP)
#define BALSA_VISUALIZATION_COLORMAP_LIST_HPP

#include <cstddef>
#include <string>

namespace balsa::visualization {

// ── Colormap names ──────────────────────────────────────────────────
//
// Canonical list of available colormaps from the colormap_shaders
// subproject.  Shared by both ImGui and Qt UI panels so the list
// stays in sync.  Each name corresponds to a Qt resource alias
// at ":/colormap-shaders/<name>.frag".

inline constexpr const char *k_colormap_names[] = {
    // MATLAB family
    "MATLAB_jet",
    "MATLAB_parula",
    "MATLAB_hot",
    "MATLAB_cool",
    "MATLAB_spring",
    "MATLAB_summer",
    "MATLAB_autumn",
    "MATLAB_winter",
    "MATLAB_bone",
    "MATLAB_copper",
    "MATLAB_pink",
    "MATLAB_hsv",
    // transform family
    "transform_rainbow",
    "transform_seismic",
    "transform_hot_metal",
    "transform_grayscale_banded",
    "transform_space",
    "transform_supernova",
    "transform_ether",
    "transform_malachite",
    "transform_morning_glory",
    "transform_peanut_butter_and_jerry",
    "transform_purple_haze",
    "transform_rose",
    "transform_saturn",
    "transform_lava_waves",
    "transform_apricot",
    "transform_carnation",
    // IDL ColorBrewer (sequential / diverging)
    "IDL_CB-Blues",
    "IDL_CB-Greens",
    "IDL_CB-Greys",
    "IDL_CB-Oranges",
    "IDL_CB-Purples",
    "IDL_CB-Reds",
    "IDL_CB-BuGn",
    "IDL_CB-BuPu",
    "IDL_CB-GnBu",
    "IDL_CB-OrRd",
    "IDL_CB-PuBu",
    "IDL_CB-PuBuGn",
    "IDL_CB-PuOr",
    "IDL_CB-PuRd",
    "IDL_CB-RdBu",
    "IDL_CB-RdGy",
    "IDL_CB-RdPu",
    "IDL_CB-RdYiBu",
    "IDL_CB-RdYiGn",
    "IDL_CB-BrBG",
    "IDL_CB-PiYG",
    "IDL_CB-PRGn",
    "IDL_CB-Spectral",
    "IDL_CB-YIGn",
    "IDL_CB-YIGnBu",
    "IDL_CB-YIOrBr",
    // IDL ColorBrewer (qualitative)
    "IDL_CB-Accent",
    "IDL_CB-Dark2",
    "IDL_CB-Paired",
    "IDL_CB-Pastel1",
    "IDL_CB-Pastel2",
    "IDL_CB-Set1",
    "IDL_CB-Set2",
    "IDL_CB-Set3",
    // IDL classic
    "IDL_Rainbow",
    "IDL_Rainbow_2",
    "IDL_Rainbow_18",
    "IDL_Rainbow+Black",
    "IDL_Rainbow+White",
    "IDL_Blue-Red",
    "IDL_Blue-Red_2",
    "IDL_Blue-White_Linear",
    "IDL_Blue-Green-Red-Yellow",
    "IDL_Blue-Pastel-Red",
    "IDL_Blue_Waves",
    "IDL_Green-Pink",
    "IDL_Green-Red-Blue-White",
    "IDL_Green-White_Exponential",
    "IDL_Green-White_Linear",
    "IDL_Red-Purple",
    "IDL_Red_Temperature",
    "IDL_Black-White_Linear",
    "IDL_16_Level",
    "IDL_Beach",
    "IDL_Eos_A",
    "IDL_Eos_B",
    "IDL_Hardcandy",
    "IDL_Haze",
    "IDL_Hue_Sat_Lightness_1",
    "IDL_Hue_Sat_Lightness_2",
    "IDL_Hue_Sat_Value_1",
    "IDL_Hue_Sat_Value_2",
    "IDL_Mac_Style",
    "IDL_Nature",
    "IDL_Ocean",
    "IDL_Pastels",
    "IDL_Peppermint",
    "IDL_Plasma",
    "IDL_Prism",
    "IDL_Purple-Red+Stripes",
    "IDL_Standard_Gamma-II",
    "IDL_Steps",
    "IDL_Stern_Special",
    "IDL_Volcano",
    "IDL_Waves",
    // Other
    "gnuplot",
    "kbinani_altitude",
};

inline constexpr int k_colormap_count = static_cast<int>(
  sizeof(k_colormap_names) / sizeof(k_colormap_names[0]));

// Find the index of a colormap name, or -1 if not found.
inline int find_colormap_index(const std::string &name) {
    for (int i = 0; i < k_colormap_count; ++i) {
        if (name == k_colormap_names[i]) return i;
    }
    return -1;
}

}// namespace balsa::visualization

#endif
