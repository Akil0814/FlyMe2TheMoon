#pragma once

#include "../audio/audio_settings.h"

#include <string>

namespace elysia::config
{
enum class WindowMode
{
    Windowed,
    BorderlessFullscreen
};

struct WindowSize
{
    int width = 1280;
    int height = 720;

    friend bool operator==(const WindowSize&,const WindowSize&) = default;
};

struct WindowSettings
{
    WindowMode mode = WindowMode::Windowed;
    WindowSize windowed_size{};

    friend bool operator==(const WindowSettings&,const WindowSettings&) = default;
};

struct UserConfigData
{
    WindowSettings window;
    double target_fps = 60.0;
    bool vsync = true;
    std::string language;
    elysia::audio::AudioSettings audio;

    friend bool operator==(const UserConfigData&,const UserConfigData&) = default;
};
}
