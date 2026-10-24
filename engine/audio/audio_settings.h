#pragma once

namespace elysia::audio
{
struct AudioSettings
{
    int master_volume = 100;
    int music_volume = 100;
    int sound_volume = 100;

    friend bool operator==(const AudioSettings&,const AudioSettings&) = default;
};

}
