#pragma once

#include "../core/ui_element.h"
#include "../../core/interface/updatable.h"

#include <memory>

namespace elysia::ui
{
class UiWindow;

// Passive window-level hint composite. The trigger is borrowed; displayed content is owned.
class UiTooltip final : public UiElement, public elysia::core::Updatable
{
public:
    explicit UiTooltip(int order = 0) noexcept;
    ~UiTooltip() override;

    void reset() noexcept override;
    void update(double delta) override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    void bind_trigger(UiElement& trigger) noexcept;
    void clear_trigger() noexcept;
    [[nodiscard]] UiElement* trigger() noexcept { return _trigger; }
    [[nodiscard]] const UiElement* trigger() const noexcept { return _trigger; }

    UiElement* set_content(std::unique_ptr<UiElement> content);
    [[nodiscard]] std::unique_ptr<UiElement> release_content() noexcept;
    void clear_content() noexcept;
    [[nodiscard]] UiElement* content() noexcept { return _content.get(); }
    [[nodiscard]] const UiElement* content() const noexcept { return _content.get(); }

    void set_show_delay(double seconds) noexcept;
    [[nodiscard]] double show_delay() const noexcept { return _show_delay; }
    void open() noexcept;
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept { return _open; }

    void register_with_window(UiWindow& window);
    void unregister_from_window() noexcept;

private:
    friend class UiWindow;
    void observe_pointer(int mouse_x,int mouse_y) noexcept;
    void submit_tooltip_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const;
    void sync_content_position() noexcept;
    [[nodiscard]] bool trigger_is_active() const noexcept;

    UiElement* _trigger = nullptr;
    std::unique_ptr<UiElement> _content;
    UiWindow* _window = nullptr;
    double _show_delay = 0.4;
    double _hover_time = 0.0;
    int _mouse_x = 0;
    int _mouse_y = 0;
    bool _has_pointer = false;
    bool _open = false;
};
}
