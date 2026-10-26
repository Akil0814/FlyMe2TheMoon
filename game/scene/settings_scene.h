#pragma once
#include "engine/scene/scene.h"
#include "engine/config/user_config_types.h"
#include "engine/ui/presets/settings_panel.h"
namespace game::scene_keys {inline constexpr elysia::scene::SceneKey Settings=6;}
namespace game::scene {
class SettingsScene final:public elysia::scene::Scene {
protected:
    void on_enter(const elysia::scene::ScenePayload&)override;
    void on_exit()override;
    void on_reset()override;
    void on_after_update(double)override;
private:
    void save(const elysia::ui::SettingsPanelDraft&);
    elysia::config::UserConfigRuntimeState _baseline;
    elysia::ui::SettingsPanel* _panel=nullptr;
    bool _saved=false;
};
}
