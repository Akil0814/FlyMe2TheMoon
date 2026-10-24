#pragma once

#include "../typography/font_settings.h"
#include "../ui/style/ui_theme_id.h"

namespace elysia::application
{
enum class ApplicationTextureFilter
{
    Nearest,
    Linear
};

struct ApplicationRenderSettings
{
    ApplicationTextureFilter texture_filter =
        ApplicationTextureFilter::Nearest;
};

struct ApplicationUiPresentationSettings
{
    elysia::ui::UiBuiltinTheme default_theme =
        elysia::ui::UiBuiltinTheme::BlueGlassMoon;
};

enum class ApplicationEngineLogoVariant
{
    Default,
    Black,
    BlackAlphaInverse,
    LightEdge,
    White
};

struct ApplicationStartupPresentationSettings
{
    ApplicationEngineLogoVariant engine_logo =
        ApplicationEngineLogoVariant::White;
};

struct ApplicationPresentationSettings
{
    ApplicationRenderSettings render;
    elysia::typography::FontSettings fonts;
    ApplicationUiPresentationSettings ui;
    ApplicationStartupPresentationSettings startup;
};
}
