#include "main_menu_scene.h"

#include "../../engine/ui/containers/ui_list_container.h"
#include "../../engine/ui/widgets/label/ui_label.h"
#include "../../engine/ui/widgets/ui_button.h"
#include "../../engine/ui/window/ui_window.h"

#include <memory>

namespace game::scene
{
void MainMenuScene::on_enter(const elysia::scene::ScenePayload&)
{
    clear_scene_objects();

    const auto& context = runtime_context();
    auto* window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{0, 0,
            static_cast<float>(context.logical_width()),
            static_cast<float>(context.logical_height())});
    window->set_on_cancel([this] { request_quit(); });

    auto content = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{0, 0, 480, 144});
    content->set_cross_align(elysia::ui::UiLayoutAlign::Center);
    content->set_item_spacing(32);

    auto title = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{0, 0, 480, 56}, 0,
        elysia::ui::ui_raw_text("FlyMe2TheMoon"));
    title->set_typography_role(elysia::typography::UiTypographyRole::Title);
    title->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    title->set_vertical_align(elysia::ui::TextVerticalAlign::Center);
    content->add_back(std::move(title));

    auto exit = std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{0, 0, 200, 56});
    exit->set_text_content(elysia::ui::ui_raw_text("Exit"));
    exit->set_on_click([this] { request_quit(); });
    content->add_back(std::move(exit));

    auto* focus_scope = content.get();
    window->add_child(std::move(content),
        elysia::ui::UiLayoutChildOptions{._anchor = elysia::ui::UiLayoutAnchor::Center});
    window->register_focus_scope(*focus_scope);
    window->focus_first_available_scope();
}

void MainMenuScene::on_exit()
{
    clear_scene_objects();
}

void MainMenuScene::on_reset()
{
    clear_scene_objects();
}
}
