#include "engine/builtin/audio/builtin_music_player.h"
#include "elysia_realm_scene.h"

#include "../../builtin/resources/builtin_resources.h"
#include "../../scene/runtime/scene_runtime_context.h"
#include "../../ui/widgets/image/ui_image.h"
#include "../../ui/window/ui_window.h"

#include <memory>
#include <stdexcept>

namespace elysia::realm::detail
{
namespace
{
[[nodiscard]] bool is_valid_return_route(
    const elysia::scene::SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}
}

void ElysiaRealmScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const RealmContentPayload* realm_payload =
        elysia::scene::try_scene_payload<RealmContentPayload>(payload);
    if (!realm_payload || !is_valid_return_route(realm_payload->return_route))
    {
        throw std::logic_error(
            "ElysiaRealmScene requires RealmContentPayload with a valid return route.");
    }

    if (!elysia::builtin::BuiltinResources::instance()->is_initialized())
    {
        throw std::logic_error(
            "ElysiaRealmScene requires initialized BuiltinResources.");
    }

    _return_route = realm_payload->return_route;
    _paused = false;
    destroy_ui();
    build_ui();
    _root_window->set_visible(true);
    _root_window->set_active(true);
}

void ElysiaRealmScene::on_exit()
{
    elysia::builtin::BuiltinMusicPlayer::instance()->stop();

    _paused = false;
    if (_root_window && !_root_window->is_destroyed())
    {
        _root_window->set_active(false);
        _root_window->set_visible(false);
    }
}

void ElysiaRealmScene::on_reset()
{
    _paused = false;
    _return_route = {};
    destroy_ui();
}

void ElysiaRealmScene::build_ui()
{
    SDL_Texture* texture = elysia::builtin::BuiltinResources::instance()->find_texture(
        elysia::builtin::BuiltinTextureId::ElysiaDefault);
    if (!texture)
    {
        throw std::logic_error(
            "ElysiaRealmScene requires engine.brand.elysia.default.");
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
        throw std::runtime_error("ElysiaRealmScene could not create its UiWindow.");

    _root_window->set_on_cancel([this]() { return_to_caller(); });

    auto logo = std::make_unique<elysia::ui::UiImage>(
        texture,
        elysia::core::Rect{0,0,320,320});
    _root_window->add_child(
        std::move(logo),
        elysia::ui::UiLayoutChildOptions{
            ._anchor = elysia::ui::UiLayoutAnchor::Center});

    _elysia_theme.set_theme(elysia::ui::UiBuiltinTheme::ElysiaLight);
    _theme_registration = _elysia_theme.register_root(*_root_window);
}

void ElysiaRealmScene::destroy_ui() noexcept
{
    _theme_registration.reset();
    if (_root_window)
        _root_window->destroy();
    _root_window = nullptr;
}

void ElysiaRealmScene::return_to_caller()
{
    if (is_valid_return_route(_return_route))
        request_scene_switch(_return_route);
}
}
