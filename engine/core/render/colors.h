#pragma once

#include "color.h"

namespace elysia::core
{
namespace colors
{
inline constexpr Color transparent{ 0, 0, 0, 0 };       // #00000000
inline constexpr Color white{ 255, 255, 255, 255 };     // #FFFFFF
inline constexpr Color black{ 0, 0, 0, 255 };           // #000000

// Common neutral tokens.
inline constexpr Color gray_100{ 245, 245, 245, 255 };  // #F5F5F5
inline constexpr Color gray_300{ 224, 224, 224, 255 };  // #E0E0E0
inline constexpr Color gray_500{ 158, 158, 158, 255 };  // #9E9E9E
inline constexpr Color gray_700{ 97, 97, 97, 255 };     // #616161
inline constexpr Color gray_900{ 33, 33, 33, 255 };     // #212121

// Common accent families. Simple names map to the 500 tone.
inline constexpr Color red_300{ 229, 115, 115, 255 };      // #E57373
inline constexpr Color red_500{ 244, 67, 54, 255 };        // #F44336
inline constexpr Color red_700{ 211, 47, 47, 255 };        // #D32F2F
inline constexpr Color red = red_500;                      // #F44336

inline constexpr Color orange_300{ 255, 183, 77, 255 };   // #FFB74D
inline constexpr Color orange_500{ 255, 152, 0, 255 };    // #FF9800
inline constexpr Color orange_700{ 245, 124, 0, 255 };    // #F57C00
inline constexpr Color orange = orange_500;               // #FF9800

inline constexpr Color yellow_300{ 255, 241, 118, 255 };  // #FFF176
inline constexpr Color yellow_500{ 255, 235, 59, 255 };   // #FFEB3B
inline constexpr Color yellow_700{ 251, 192, 45, 255 };   // #FBC02D
inline constexpr Color yellow = yellow_500;               // #FFEB3B

inline constexpr Color green_300{ 129, 199, 132, 255 };   // #81C784
inline constexpr Color green_500{ 76, 175, 80, 255 };     // #4CAF50
inline constexpr Color green_700{ 56, 142, 60, 255 };     // #388E3C
inline constexpr Color green = green_500;                 // #4CAF50

inline constexpr Color cyan_300{ 77, 208, 225, 255 };     // #4DD0E1
inline constexpr Color cyan_500{ 0, 188, 212, 255 };      // #00BCD4
inline constexpr Color cyan_700{ 0, 151, 167, 255 };      // #0097A7
inline constexpr Color cyan = cyan_500;                   // #00BCD4

inline constexpr Color blue_300{ 100, 181, 246, 255 };    // #64B5F6
inline constexpr Color blue_500{ 33, 150, 243, 255 };     // #2196F3
inline constexpr Color blue_700{ 25, 118, 210, 255 };     // #1976D2
inline constexpr Color blue = blue_500;                   // #2196F3

inline constexpr Color purple_300{ 186, 104, 200, 255 };  // #BA68C8
inline constexpr Color purple_500{ 156, 39, 176, 255 };   // #9C27B0
inline constexpr Color purple_700{ 123, 31, 162, 255 };   // #7B1FA2
inline constexpr Color purple = purple_500;               // #9C27B0

inline constexpr Color pink_300{ 240, 98, 146, 255 };     // #F06292
inline constexpr Color pink_500{ 233, 30, 99, 255 };      // #E91E63
inline constexpr Color pink_700{ 194, 24, 91, 255 };      // #C2185B
inline constexpr Color pink = pink_500;                   // #E91E63

inline constexpr Color brown_300{ 161, 136, 127, 255 };   // #A1887F
inline constexpr Color brown_500{ 121, 85, 72, 255 };     // #795548
inline constexpr Color brown_700{ 93, 64, 55, 255 };      // #5D4037
inline constexpr Color brown = brown_500;                 // #795548

// Reusable palette tokens. Semantic UI roles should compose from these.
inline constexpr Color frosted_white{ 248, 252, 255, 255 }; // #F8FCFF
inline constexpr Color alice_blue{ 240, 248, 255, 255 };    // #F0F8FF
inline constexpr Color glacial_white{ 245, 255, 255, 255 }; // #F5FFFF
inline constexpr Color powder_blue{ 172, 204, 232, 255 };   // #ACCCE8
inline constexpr Color steel_blue{ 140, 166, 194, 255 };    // #8CA6C2
inline constexpr Color sky_blue{ 118, 168, 214, 255 };      // #76A8D6
inline constexpr Color royal_blue{ 36, 76, 138, 255 };      // #244C8A
inline constexpr Color cobalt_blue{ 20, 48, 100, 255 };     // #143064
inline constexpr Color deep_cobalt_blue{ 0, 43, 100, 255 }; // #002B64
inline constexpr Color midnight_blue{ 12, 34, 78, 255 };    // #0C224E
inline constexpr Color slate_blue{ 18, 28, 42, 224 };       // #121C2AE0
inline constexpr Color abyss_blue{ 8, 16, 28, 255 };        // #08101C
inline constexpr Color blue_glass_button_idle{ 27, 59, 112, 255 }; // #1B3B70

// Elysia-inspired palette tokens. These stay semantic so built-in themes
// can mix soft bridal pinks with deep ocean blues without hardcoding RGBs.
inline constexpr Color elysia_blush_white{ 255, 249, 252, 255 }; // #FFF9FC
inline constexpr Color elysia_silk_white{ 255, 255, 255, 255 };  // #FFFFFF
inline constexpr Color elysia_pearl_white{ 253, 250, 252, 255 }; // #FDFAFC
inline constexpr Color elysia_chiffon_pink{ 247, 223, 246, 255 }; // #F7DFF6
inline constexpr Color elysia_opal_lilac{ 230, 220, 240, 255 };  // #E6DCF0
inline constexpr Color elysia_petal_pink{ 249, 223, 236, 255 };  // #F9DFEC
inline constexpr Color elysia_rose_pink{ 236, 176, 211, 255 };   // #ECB0D3
inline constexpr Color elysia_glow_pink{ 242, 170, 212, 255 };   // #F2AAD4
inline constexpr Color elysia_hair_rose{ 220, 135, 183, 255 };   // #DC87B7
inline constexpr Color elysia_crystal_lilac{ 206, 182, 228, 255 }; // #CEB6E4
inline constexpr Color elysia_iris_mauve{ 170, 153, 220, 255 };  // #AA99DC
inline constexpr Color elysia_lilac{ 204, 188, 243, 255 };       // #CCBCF3
inline constexpr Color elysia_plum{ 110, 88, 140, 255 };         // #6E588C
inline constexpr Color elysia_mist_blue{ 156, 175, 216, 255 };   // #9CAFD8
inline constexpr Color elysia_dusk_rose{ 184, 94, 145, 255 };    // #B85E91
inline constexpr Color elysia_velvet_rose{ 171, 84, 132, 255 };  // #AB5484
inline constexpr Color elysia_twilight_rose{ 100, 60, 115, 255 }; // #643C73
inline constexpr Color elysia_muted_mauve{ 105, 93, 116, 255 };  // #695D74
inline constexpr Color elysia_phantom_sea{ 26, 56, 104, 255 };   // #1A3868
inline constexpr Color elysia_mirage_blue{ 44, 84, 143, 255 };   // #2C548F
inline constexpr Color elysia_deep_sea{ 14, 30, 66, 255 };       // #0E1E42
inline constexpr Color elysia_starsea_navy{ 20, 29, 76, 255 };   // #141D4C
inline constexpr Color elysia_twilight_mist{ 46, 61, 124, 255 }; // #2E3D7C
inline constexpr Color elysia_focus_blue{ 77, 96, 167, 255 };     // #4D60A7
inline constexpr Color elysia_aurora_haze{ 86, 105, 181, 255 };  // #5669B5
inline constexpr Color elysia_starlight_lilac{ 198, 206, 255, 255 }; // #C6CEFF
inline constexpr Color elysia_moonlit_lavender{ 237, 231, 255, 255 }; // #EDE7FF
inline constexpr Color elysia_crystal_orchid{ 233, 170, 255, 255 }; // #E9AAFF
inline constexpr Color elysia_dream_rose{ 196, 123, 221, 255 };  // #C47BDD
inline constexpr Color elysia_silver_mist{ 136, 145, 178, 255 }; // #8891B2
inline constexpr Color elysia_corruption_blue{ 40, 93, 134, 255 }; // #285D86
inline constexpr Color elysia_corruption_blue_focus{ 52, 121, 164, 255 }; // #3479A4
inline constexpr Color elysia_corruption_blue_active{ 29, 72, 111, 255 }; // #1D486F
inline constexpr Color elysia_corruption_glow{ 104, 185, 208, 255 }; // #68B9D0
inline constexpr Color elysia_corruption_ice{ 184, 238, 255, 255 };  // #B8EEFF
inline constexpr Color elysia_rose_copper{ 142, 79, 85, 255 };       // #8E4F55
inline constexpr Color elysia_rose_copper_focus{ 163, 94, 98, 255 }; // #A35E62
inline constexpr Color elysia_rose_copper_active{ 113, 60, 69, 255 }; // #713C45
inline constexpr Color elysia_rose_gold{ 229, 181, 155, 255 };       // #E5B59B
inline constexpr Color elysia_rose_gold_focus{ 244, 210, 189, 255 }; // #F4D2BD
inline constexpr Color elysia_rose_gold_active{ 255, 240, 227, 255 }; // #FFF0E3
inline constexpr Color elysia_light_button_idle{ 255, 245, 251, 255 }; // #FFF5FB
inline constexpr Color elysia_light_button_focus{ 245, 217, 239, 255 }; // #F5D9EF
inline constexpr Color elysia_light_button_active{ 234, 193, 219, 255 }; // #EAC1DB
inline constexpr Color elysia_dark_button_idle{ 74, 75, 134, 255 }; // #4A4B86
inline constexpr Color elysia_dark_button_focus{ 98, 99, 163, 255 }; // #6263A3
inline constexpr Color elysia_dark_button_active{ 52, 54, 97, 255 }; // #343661
inline constexpr Color elysia_danger_crimson{ 122, 47, 72, 255 };   // #7A2F48
inline constexpr Color elysia_danger_crimson_focus{ 150, 58, 87, 255 }; // #963A57
inline constexpr Color elysia_danger_crimson_active{ 90, 35, 54, 255 }; // #5A2336

// Evangelion-inspired palette tokens used by the built-in EVA themes.
inline constexpr Color eva_unit00_rei_white{ 243, 246, 250, 255 };     // #F3F6FA
inline constexpr Color eva_unit00_button_idle{ 255, 255, 255, 255 }; // #FFFFFF
inline constexpr Color eva_unit00_primary_idle{ 195, 220, 239, 255 }; // #C3DCEF
inline constexpr Color eva_unit00_frost_blue{ 220, 234, 246, 255 };    // #DCEAF6
inline constexpr Color eva_unit00_focus_blue{ 195, 220, 239, 255 };    // #C3DCEF
inline constexpr Color eva_unit00_pale_blue{ 175, 200, 222, 255 };     // #AFC8DE
inline constexpr Color eva_unit00_warning_yellow{ 232, 188, 82, 255 }; // #E8BC52
inline constexpr Color eva_unit00_soft_graphite{ 104, 116, 132, 255 }; // #687484
inline constexpr Color eva_unit00_deep_graphite{ 52, 69, 87, 255 };    // #344557
inline constexpr Color eva_unit00_muted_ink{ 59, 80, 101, 255 };       // #3B5065
inline constexpr Color eva_unit00_border_blue{ 199, 215, 230, 255 };   // #C7D7E6
inline constexpr Color eva_unit00_mist_blue{ 134, 161, 191, 255 };     // #86A1BF
inline constexpr Color eva_unit00_caution_gold{ 244, 208, 116, 255 };  // #F4D074
inline constexpr Color eva_unit00_amber_ochre{ 131, 90, 27, 255 };     // #835A1B
inline constexpr Color eva_unit00_signal_orange{ 166, 61, 33, 255 };   // #A63D21
inline constexpr Color eva_unit00_danger_amber{ 118, 80, 27, 255 };    // #76501B
inline constexpr Color eva_unit00_danger_amber_focus{ 138, 93, 28, 255 }; // #8A5D1C
inline constexpr Color eva_unit00_danger_amber_active{ 86, 56, 18, 255 }; // #563812
inline constexpr Color eva_unit00_alert_crimson{ 143, 47, 61, 255 };   // #8F2F3D
inline constexpr Color eva_unit00_alert_crimson_focus{ 182, 59, 74, 255 }; // #B63B4A
inline constexpr Color eva_unit00_alert_crimson_active{ 103, 30, 43, 255 }; // #671E2B

inline constexpr Color eva_unit00_ivory = eva_unit00_rei_white;        // #F3F6FA
inline constexpr Color eva_unit00_cerulean = eva_unit00_frost_blue;    // #DCEAF6
inline constexpr Color eva_unit00_rei_blue = eva_unit00_pale_blue;     // #AFC8DE
inline constexpr Color eva_unit00_signal_amber = eva_unit00_warning_yellow; // #E8BC52
inline constexpr Color eva_unit00_graphite = eva_unit00_soft_graphite; // #687484

inline constexpr Color eva_unit01_deep_purple{ 49, 33, 76, 255 };      // #31214C
inline constexpr Color eva_unit01_button_idle{ 66, 48, 94, 255 };    // #42305E
inline constexpr Color eva_unit01_royal_purple{ 95, 62, 132, 255 };    // #5F3E84
inline constexpr Color eva_unit01_toxic_green{ 126, 222, 76, 255 };    // #7EDE4C
inline constexpr Color eva_unit01_lime_glow{ 176, 255, 92, 255 };      // #B0FF5C
inline constexpr Color eva_unit01_orange_core{ 230, 138, 54, 255 };    // #E68A36
inline constexpr Color eva_unit01_void_purple{ 34, 22, 53, 255 };      // #221635
inline constexpr Color eva_unit01_muted_lime{ 146, 182, 111, 255 };    // #92B66F
inline constexpr Color eva_unit01_mist_lime{ 197, 217, 168, 255 };     // #C5D9A8
inline constexpr Color eva_unit01_alert_amber{ 246, 179, 92, 255 };    // #F6B35C
inline constexpr Color eva_unit01_burnt_orange{ 208, 124, 52, 255 };   // #D07C34

inline constexpr Color eva_unit02_crimson{ 176, 38, 48, 255 };         // #B02630
inline constexpr Color eva_unit02_button_idle{ 133, 43, 53, 255 };    // #852B35
inline constexpr Color eva_unit02_vermilion{ 214, 72, 48, 255 };       // #D64830
inline constexpr Color eva_unit02_orange{ 237, 142, 46, 255 };         // #ED8E2E
inline constexpr Color eva_unit02_sun_yellow{ 246, 196, 74, 255 };     // #F6C44A
inline constexpr Color eva_unit02_bone_white{ 244, 240, 234, 255 };    // #F4F0EA
inline constexpr Color eva_unit02_deep_maroon{ 108, 24, 31, 255 };     // #6C181F
inline constexpr Color eva_unit02_pressed_maroon{ 77, 17, 24, 255 };   // #4D1118
inline constexpr Color eva_unit02_active_crimson{ 133, 28, 40, 255 };  // #851C28
inline constexpr Color eva_unit02_danger_black_red{ 43, 10, 15, 255 }; // #2B0A0F
inline constexpr Color eva_unit02_danger_black_red_focus{ 90, 16, 27, 255 }; // #5A101B
inline constexpr Color eva_unit02_danger_black_red_active{ 22, 3, 7, 255 }; // #160307
inline constexpr Color eva_unit02_glow_amber{ 251, 209, 99, 255 };     // #FBD163
inline constexpr Color eva_unit02_muted_orange{ 197, 121, 68, 255 };   // #C57944
inline constexpr Color eva_unit02_warm_mist{ 242, 207, 179, 255 };     // #F2CFB3

// Reserved, low-saturation neutral set for understated gray UI themes.
inline constexpr Color quiet_slate_white{ 236, 240, 244, 255 };        // #ECF0F4
inline constexpr Color quiet_slate_mist{ 210, 217, 224, 255 };         // #D2D9E0
inline constexpr Color quiet_slate_silver{ 179, 188, 198, 255 };       // #B3BCC6
inline constexpr Color quiet_slate_gray{ 110, 122, 136, 255 };         // #6E7A88
inline constexpr Color quiet_slate_steel{ 64, 75, 88, 255 };           // #404B58
inline constexpr Color quiet_slate_charcoal{ 54, 63, 74, 255 };        // #363F4A
inline constexpr Color quiet_slate_button_idle{ 66, 77, 90, 255 };    // #424D5A
inline constexpr Color quiet_slate_button_focus{ 86, 100, 117, 255 }; // #566475
inline constexpr Color quiet_slate_ink{ 32, 38, 46, 255 };             // #20262E
inline constexpr Color quiet_slate_danger{ 127, 63, 70, 255 };         // #7F3F46
inline constexpr Color quiet_slate_danger_focus{ 152, 74, 83, 255 };   // #984A53
inline constexpr Color quiet_slate_danger_active{ 100, 47, 53, 255 };  // #642F35

}

}
