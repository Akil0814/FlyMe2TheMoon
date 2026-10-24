#include "engine/builtin/audio/builtin_music_player.h"
#include "elysia_intro_scene.h"

#include "realm_content_payload.h"
#include "realm_scene_keys.h"

#include "../../builtin/resources/builtin_resources.h"
#include "../../scene/runtime/scene_runtime_context.h"
#include "../../ui/containers/ui_list_container.h"
#include "../../ui/widgets/image/ui_fade_image.h"
#include "../../ui/widgets/label/ui_label.h"
#include "../../ui/window/ui_window.h"

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace elysia::realm::detail
{
namespace
{
[[nodiscard]] bool is_valid_return_route(
    const elysia::scene::SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}

constexpr std::array<std::string_view,9> kCodeLines = {
    "int main(int argc, char const *argv[])",
    "{",
    "    RestoreEgo();",
    "    RestorePurePinkHeart();",
    "    RestructureHerrscherOfHuman();",
    "    RestoreThirteenFlameChasers();",
    "    RebuildIncarnation();",
    "    return 0;",
    "}"
};

constexpr float kCodeListWidth = 280.0f;
constexpr float kCodeLineHeight = 20.0f;
constexpr float kCodeLineSpacing = 3.0f;
constexpr float kCodeListMargin = 16.0f;
constexpr double kCodeLineIntervalSeconds = 0.5;
}

void ElysiaIntroScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const ElysiaRealmPayload* entry_payload =
        elysia::scene::try_scene_payload<ElysiaRealmPayload>(payload);
    if (!entry_payload || !is_valid_return_route(entry_payload->return_route))
    {
        throw std::logic_error(
            "ElysiaIntroScene requires ElysiaRealmPayload with a valid return route.");
    }

    if (!elysia::builtin::BuiltinResources::instance()->is_initialized())
    {
        throw std::logic_error(
            "ElysiaIntroScene requires initialized BuiltinResources.");
    }

    _return_route = entry_payload->return_route;
    _paused = false;
    stop_playback();
    _current_line = 0;
    _logo_finished = false;
    _code_finished = false;
    _transition_requested = false;
    _music_handed_off = false;

    destroy_ui();
    build_ui();
    _root_window->set_visible(true);
    _root_window->set_active(true);

    if (!elysia::builtin::BuiltinMusicPlayer::instance()->play(
            elysia::builtin::BuiltinMusicId::ElysianRealm, -1, std::chrono::milliseconds{1500}))
    {
        destroy_ui();
        throw std::logic_error("ElysiaIntroScene could not play Realm music.");
    }

    _code_timer.set_one_shot(false);
    _code_timer.set_wait_time(kCodeLineIntervalSeconds);
    _code_timer.set_on_timeout([this]() { reveal_next_code_line(); });
    _code_timer.restart();
}

void ElysiaIntroScene::on_before_update(double delta)
{
    _code_timer.update(delta);
}

void ElysiaIntroScene::on_after_update(double delta)
{
    (void)delta;
    request_realm_transition();
}

void ElysiaIntroScene::on_exit()
{
    stop_playback();
    if (!_music_handed_off)
        elysia::builtin::BuiltinMusicPlayer::instance()->stop();

    _music_handed_off = false;
    _paused = false;
    if (_root_window && !_root_window->is_destroyed())
    {
        _root_window->set_active(false);
        _root_window->set_visible(false);
    }
}

void ElysiaIntroScene::on_reset()
{
    stop_playback();
    _current_line = 0;
    _logo_finished = false;
    _code_finished = false;
    _transition_requested = false;
    _music_handed_off = false;
    _paused = false;
    _return_route = {};
    destroy_ui();
}

void ElysiaIntroScene::build_ui()
{
    SDL_Texture* texture = elysia::builtin::BuiltinResources::instance()->find_texture(
        elysia::builtin::BuiltinTextureId::ElysiaDefault);
    if (!texture)
    {
        throw std::logic_error(
            "ElysiaIntroScene requires engine.brand.elysia.default.");
    }

    const auto& context = runtime_context();
    _root_window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{
            0,
            0,
            static_cast<float>(context.logical_width()),
            static_cast<float>(context.logical_height())},
        100);
    if (!_root_window)
        throw std::runtime_error("ElysiaIntroScene could not create its UiWindow.");

    auto logo = std::make_unique<elysia::ui::UiFadeImage>(
        texture,
        elysia::core::Rect{0,0,320,320});
    logo->configure_playback(
        elysia::ui::effects::UiOpacityFadeMode::FadeInOut,
        1.5,
        1.5,
        1.5);
    logo->set_on_end([this]() { _logo_finished = true; });
    logo->play();
    _root_window->add_child(
        std::move(logo),
        elysia::ui::UiLayoutChildOptions{
            ._anchor = elysia::ui::UiLayoutAnchor::Center});

    _code_list = _root_window->create_child<elysia::ui::UiListContainer>(
        elysia::ui::UiLayoutChildOptions{
            ._anchor = elysia::ui::UiLayoutAnchor::BottomRight,
            ._margin = elysia::ui::UiLayoutMargin{
                0.0f,0.0f,kCodeListMargin,kCodeListMargin}},
        elysia::core::Rect{0,0,kCodeListWidth,0});
    _code_list->set_direction(elysia::ui::UiListDirection::Vertical);
    _code_list->set_cross_align(elysia::ui::UiLayoutAlign::Start);
    _code_list->set_item_spacing(kCodeLineSpacing);

    _elysia_theme.set_theme(elysia::ui::UiBuiltinTheme::ElysiaLight);
    _theme_registration = _elysia_theme.register_root(*_root_window);
}

void ElysiaIntroScene::destroy_ui() noexcept
{
    _theme_registration.reset();
    _code_list = nullptr;
    if (_root_window)
        _root_window->destroy();
    _root_window = nullptr;
}

void ElysiaIntroScene::reveal_next_code_line()
{
    if (_current_line >= kCodeLines.size())
    {
        _code_finished = true;
        _code_timer.pause();
        return;
    }

    add_label(kCodeLines[_current_line]);
    ++_current_line;

    if (_current_line >= kCodeLines.size())
    {
        _code_finished = true;
        _code_timer.pause();
    }
}

void ElysiaIntroScene::add_label(std::string_view code_line)
{
    if (!_code_list)
        throw std::runtime_error("ElysiaIntroScene code list is null.");

    auto label = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{0,0,kCodeListWidth,kCodeLineHeight},
        0,
        elysia::ui::ui_raw_text(std::string(code_line)));
    label->set_font_source_override(
        elysia::typography::FontSource::EngineBuiltIn);
    _code_list->add_back(std::move(label));
    _code_list->set_size(_code_list->content_extent());
}

void ElysiaIntroScene::request_realm_transition()
{
    if (!_logo_finished || !_code_finished || _transition_requested)
        return;

    request_scene_switch(elysia::scene::SceneRoute{
        .target = SceneKeys::RealmContent,
        .payload = RealmContentPayload{.return_route = _return_route},
        .reload_mode = elysia::scene::SceneReloadMode::Reuse});
    _transition_requested = true;
    _music_handed_off = true;
}

void ElysiaIntroScene::stop_playback() noexcept
{
    _code_timer.pause();
    _code_timer.set_on_timeout({});
}
}
