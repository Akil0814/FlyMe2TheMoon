#include "engine/core/render/sdl_texture_size.h"
#include "ui_text_input.h"

#include "../focus/ui_control_focus_scope_host.h"
#include "../style/ui_style_defaults.h"
#include "../window/ui_window.h"

#include "../../core/render/render_command.h"
#include "../../localization/localization_service.h"
#include "../../localization/localized_text_style.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>

namespace elysia::ui
{
using elysia::typography::UiTypographyRole;

namespace
{
[[nodiscard]] bool is_utf8_continuation_byte(unsigned char value) noexcept
{
    return (value & 0xC0U) == 0x80U;
}

[[nodiscard]] std::size_t utf8_codepoint_count(std::string_view text) noexcept
{
    std::size_t count = 0;
    for (unsigned char value : text)
    {
        if (!is_utf8_continuation_byte(value))
            ++count;
    }
    return count;
}

[[nodiscard]] std::size_t utf8_byte_offset_from_codepoint_index(std::string_view text,std::size_t codepoint_index) noexcept
{
    if (codepoint_index == 0)
        return 0;

    std::size_t current_index = 0;
    for (std::size_t byte_index = 0; byte_index < text.size(); ++byte_index)
    {
        if (is_utf8_continuation_byte(static_cast<unsigned char>(text[byte_index])))
            continue;
        if (current_index == codepoint_index)
            return byte_index;
        ++current_index;
    }

    return text.size();
}

[[nodiscard]] std::string utf8_truncate_to_codepoint_limit(std::string_view text,std::optional<std::size_t> max_length)
{
    if (!max_length.has_value())
        return std::string(text);
    return std::string(text.substr(0,utf8_byte_offset_from_codepoint_index(text,*max_length)));
}

[[nodiscard]] std::string sanitize_single_line_text(std::string_view text)
{
    std::string sanitized;
    sanitized.reserve(text.size());
    for (char ch : text)
    {
        if (ch == '\r' || ch == '\n')
            continue;
        sanitized.push_back(ch);
    }
    return sanitized;
}

[[nodiscard]] float clamp_non_negative(float value) noexcept
{
    return std::max(0.0f,value);
}

const UiTextInput* s_text_input_owner = nullptr;
SDL_Window* s_text_input_window = nullptr;
}

struct UiTextInput::TextLayout
{
    elysia::core::Rect content_rect = elysia::core::Rect::zero();
    std::string display_text{};
    std::size_t visible_caret_codepoint_index = 0;
    std::size_t composition_display_start_codepoint_index = 0;
    std::size_t composition_display_length = 0;
    float text_x = 0.0f;
    float text_y = 0.0f;
    float scroll_x = 0.0f;
    float caret_x = 0.0f;
    float text_height = 0.0f;
    float composition_highlight_start_x = 0.0f;
    float composition_highlight_end_x = 0.0f;
};

struct UiTextInput::EditingTextTexture
{
    elysia::localization::CachedTexturePtr texture;
    std::string display_text;
    std::string language;
    UiTypographyRole typography_role = UiTypographyRole::Input;
    std::optional<elysia::typography::FontSource> font_source_override;
    std::uint64_t font_generation = 0;
    elysia::core::Color color{};
};

UiTextInput::UiTextInput(const elysia::core::Rect& rect,int order) noexcept
    : UiControl(rect,order)
{
    reset();
}

UiTextInput::UiTextInput(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiTextInput(elysia::core::Rect(position.x,position.y,size.x,size.y),order) {}

UiTextInput::UiTextInput(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiTextInput(elysia::core::Rect::from_center(center,size),order) {}

UiTextInput::~UiTextInput()
{
    release_text_input_ownership();
}

void UiTextInput::reset() noexcept
{
    release_text_input_ownership();
    UiControl::reset();

    _on_text_changed = nullptr;
    _on_submit = nullptr;
    _text.clear();
    _placeholder_content = UiTextContent{};
    _composition_text.clear();
    _editing_text_texture.reset();
    _caret_codepoint_index = 0;
    _composition_insert_codepoint_index = 0;
    _composition_start = 0;
    _composition_length = 0;
    _max_length.reset();
    _style_state.reset(UiStyleDefaults::text_input());
    _typography_role = UiTypographyRole::Input;
    _placeholder_typography_role = UiTypographyRole::InputPlaceholder;
    _font_source_override.reset();
    _padding = 10;
    _is_pushed = false;
}

void UiTextInput::set_enabled(bool enabled)
{
    UiControl::set_enabled(enabled);
    if (!enabled)
    {
        clear_pushed_state();
        clear_composition();
        release_text_input_ownership();
    }
}

void UiTextInput::set_focused(bool focused)
{
    UiControl::set_focused(focused);
    if (is_focused() && is_enabled() && is_active() && is_visible())
        acquire_text_input_ownership();
    else
    {
        clear_pushed_state();
        clear_composition();
        release_text_input_ownership();
    }
}

bool UiTextInput::on_ui_input_event(const UiInputEvent& event)
{
    if (event.type == UiInputEventType::MouseMoved)
        return false;

    if (event.type == UiInputEventType::PointerPressed)
    {
        if (!is_primary_pointer_event(event) || !can_receive_pointer())
            return false;
        if (!contains_pointer(event.mouse_x,event.mouse_y))
            return false;

        set_focused(true);
        clear_composition();
        _caret_codepoint_index = codepoint_index_at_x(event.mouse_x);
        clear_composition();
        _is_pushed = true;
        return true;
    }

    if (event.type == UiInputEventType::PointerReleased)
    {
        if (!is_primary_pointer_event(event))
            return false;

        const bool was_pushed = _is_pushed;
        clear_pushed_state();
        return was_pushed;
    }

    if (!can_interact())
        return false;

    if (event.type == UiInputEventType::TextInput)
        return insert_text_at_caret(event.text);

    if (event.type == UiInputEventType::TextEditing)
    {
        _composition_insert_codepoint_index = _caret_codepoint_index;
        _composition_text = sanitize_single_line_text(event.text);
        const std::size_t composition_codepoints = utf8_codepoint_count(_composition_text);
        _composition_start = std::clamp(event.composition_start,0,static_cast<int>(composition_codepoints));
        _composition_length = std::clamp(event.composition_length,0,static_cast<int>(composition_codepoints) - _composition_start);
        return true;
    }

    if (event.type == UiInputEventType::ActionPressed)
    {
        switch (event.action)
        {
        case UiAction::Confirm:
            _is_pushed = true;
            return true;
        case UiAction::Backspace:
            return erase_previous_codepoint();
        case UiAction::DeleteKey:
            return erase_next_codepoint();
        case UiAction::NavigateLeft:
            move_caret_left();
            return true;
        case UiAction::NavigateRight:
            move_caret_right();
            return true;
        case UiAction::Home:
            move_caret_home();
            return true;
        case UiAction::End:
            move_caret_end();
            return true;
        default:
            return false;
        }
    }

    if (event.type == UiInputEventType::ActionReleased && event.action == UiAction::Confirm)
    {
        const bool should_submit = _is_pushed;
        clear_pushed_state();
        const UiTextInputSubmitCallback callback = should_submit ? _on_submit : nullptr;
        const std::string text = _text;
        if (callback)
            callback(text);
        return should_submit;
    }

    return false;
}

void UiTextInput::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    const elysia::core::Rect& input_rect = screen_rect();
    if (input_rect.is_empty())
        return;

    const UiTextInputStyle& style = _style_state.effective_style();
    if (style.chrome.draw_background)
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(input_rect,apply_opacity(current_background_color()),style.chrome.corner_radius));
    if (style.chrome.draw_border)
        out_commands.push_back(elysia::core::make_ui_draw_rect_command(
            input_rect,
            apply_opacity(current_border_color()),
            style.chrome.corner_radius,
            style.chrome.border_width));

    const TextLayout layout = compute_text_layout();
    if (layout.content_rect.is_empty())
        return;

    const bool show_placeholder = _text.empty() && _composition_text.empty();
    if (show_placeholder)
        _editing_text_texture.reset();

    elysia::localization::LocalizationService* localization_service = ELYSIA_LOCALIZATION;

    if (show_placeholder ? !_placeholder_content.empty() : !layout.display_text.empty())
    {
        const UiResolvedTextStyle typography = resolve_ui_typography(
            show_placeholder ? _placeholder_typography_role : _typography_role);
        elysia::localization::LocalizedTextStyle text_style;
        text_style.typography_role =
            show_placeholder ? _placeholder_typography_role : _typography_role;
        text_style.font_source_override = _font_source_override;
        text_style.color = show_placeholder ? current_placeholder_color() : current_text_color();
        text_style.wrap_width = 0;

        SDL_Texture* text_texture = nullptr;
        if (show_placeholder)
        {
            if (_placeholder_content.kind == UiTextContentKind::TextKey)
                text_texture = localization_service->get_text_texture(_placeholder_content.value,text_style);
            else if (_placeholder_content.kind == UiTextContentKind::RawText)
                text_texture = localization_service->get_raw_text_texture(_placeholder_content.value,text_style);
        }
        else
        {
            const std::string& language = localization_service->current_language();
            const bool has_matching_texture = _editing_text_texture
                && _editing_text_texture->display_text == layout.display_text
                && _editing_text_texture->language == language
                && _editing_text_texture->typography_role
                    == text_style.typography_role
                && _editing_text_texture->font_source_override
                    == text_style.font_source_override
                && _editing_text_texture->font_generation
                    == localization_service->font_generation()
                && _editing_text_texture->color == text_style.color;

            if (!has_matching_texture)
            {
                elysia::localization::CachedTexturePtr texture =
                    localization_service->create_uncached_raw_text_texture(layout.display_text,text_style);
                if (texture)
                {
                    auto next_texture = std::make_unique<EditingTextTexture>();
                    next_texture->texture = std::move(texture);
                    next_texture->display_text = layout.display_text;
                    next_texture->language = language;
                    next_texture->typography_role = text_style.typography_role;
                    next_texture->font_source_override =
                        text_style.font_source_override;
                    next_texture->font_generation =
                        localization_service->font_generation();
                    next_texture->color = text_style.color;
                    _editing_text_texture = std::move(next_texture);
                }
                else
                    _editing_text_texture.reset();
            }

            if (_editing_text_texture)
                text_texture = _editing_text_texture->texture.get();
        }
        if (text_texture)
        {
            int texture_width = 0;
            int texture_height = 0;
            if (elysia::core::texture_pixel_size(text_texture,&texture_width,&texture_height)
                && texture_width > 0
                && texture_height > 0)
            {
                const float text_y = show_placeholder
                    ? (layout.content_rect.center().y - static_cast<float>(texture_height) * 0.5f)
                    : layout.text_y;
                const elysia::core::Rect text_rect(
                    layout.text_x,
                    text_y,
                    static_cast<float>(texture_width),
                    static_cast<float>(texture_height)
                );
                elysia::core::UiRenderCommand command = elysia::core::make_ui_texture_command(text_texture,text_rect,layout.content_rect);
                apply_opacity(command);
                out_commands.push_back(command);
            }
        }
    }

    if (!_composition_text.empty() && layout.composition_display_length > 0)
    {
        const float underline_y = layout.text_y + layout.text_height;
            out_commands.push_back(elysia::core::make_ui_draw_line_command(
                elysia::core::Vector2(layout.composition_highlight_start_x,underline_y),
                elysia::core::Vector2(layout.composition_highlight_end_x,underline_y),
                apply_opacity(style.caret),
                layout.content_rect));
    }

    if (is_focused())
    {
        const std::uint64_t ticks = SDL_GetTicks();
        if (((ticks / 500U) % 2U) == 0U)
        {
            const float caret_top = layout.text_y;
            const float caret_bottom = layout.text_y + std::max(layout.text_height,1.0f);
            out_commands.push_back(elysia::core::make_ui_draw_line_command(
                elysia::core::Vector2(layout.caret_x,caret_top),
                elysia::core::Vector2(layout.caret_x,caret_bottom),
                apply_opacity(style.caret),
                layout.content_rect));
        }

        sync_text_input_rect();
    }
}

void UiTextInput::set_text(std::string text)
{
    (void)set_text_internal(std::move(text),true,true);
}

const std::string& UiTextInput::text() const noexcept
{
    return _text;
}

void UiTextInput::clear_text()
{
    set_text({});
}

void UiTextInput::set_placeholder_content(UiTextContent placeholder_content)
{
    _placeholder_content = std::move(placeholder_content);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

const UiTextContent& UiTextInput::placeholder_content() const noexcept
{
    return _placeholder_content;
}

void UiTextInput::set_on_text_changed(UiTextInputChangedCallback on_text_changed)
{
    _on_text_changed = std::move(on_text_changed);
}

void UiTextInput::set_on_submit(UiTextInputSubmitCallback on_submit)
{
    _on_submit = std::move(on_submit);
}

void UiTextInput::set_max_length(std::optional<std::size_t> max_length)
{
    _max_length = max_length;
    (void)set_text_internal(_text,true,false);
}

const std::optional<std::size_t>& UiTextInput::max_length() const noexcept
{
    return _max_length;
}

void UiTextInput::set_base_style(const UiTextInputStyle& style) noexcept
{
    _style_state.set_base_style(style);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiTextInput::set_style_overrides(const UiTextInputStyleOverrides& overrides) noexcept
{
    _style_state.set_style_overrides(overrides);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

const UiTextInputStyle& UiTextInput::style() const noexcept
{
    return _style_state.effective_style();
}

const UiTextInputStyleOverrides& UiTextInput::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiTextInput::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiTextInput::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiTextInput::set_typography_role(UiTypographyRole role) noexcept
{
    _typography_role = role;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

UiTypographyRole UiTextInput::typography_role() const noexcept
{
    return _typography_role;
}

void UiTextInput::set_placeholder_typography_role(UiTypographyRole role) noexcept
{
    _placeholder_typography_role = role;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

UiTypographyRole UiTextInput::placeholder_typography_role() const noexcept
{
    return _placeholder_typography_role;
}

void UiTextInput::set_font_source_override(
    elysia::typography::FontSource source) noexcept
{
    if (_font_source_override == source)
        return;
    _font_source_override = source;
    _editing_text_texture.reset();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiTextInput::clear_font_source_override() noexcept
{
    if (!_font_source_override)
        return;
    _font_source_override.reset();
    _editing_text_texture.reset();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

std::optional<elysia::typography::FontSource>
UiTextInput::font_source_override() const noexcept
{
    return _font_source_override;
}

void UiTextInput::set_padding(int padding) noexcept
{
    _padding = std::max(0,padding);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

int UiTextInput::padding() const noexcept
{
    return _padding;
}

bool UiTextInput::can_interact() const noexcept
{
    return is_enabled() && is_focused() && is_active() && is_visible();
}

bool UiTextInput::can_receive_pointer() const noexcept
{
    return is_enabled() && is_active() && is_visible();
}

bool UiTextInput::contains_pointer(int mouse_x,int mouse_y) const noexcept
{
    return presentation_screen_rect().contains(elysia::core::Vector2(static_cast<float>(mouse_x),static_cast<float>(mouse_y)));
}

bool UiTextInput::is_primary_pointer_event(const UiInputEvent& event) const noexcept
{
    return event.device == elysia::input::InputDevice::Mouse
        && event.control == elysia::input::RawInputControl::MouseLeft;
}

void UiTextInput::clear_pushed_state() noexcept
{
    _is_pushed = false;
}

void UiTextInput::clear_composition() noexcept
{
    _composition_text.clear();
    _composition_start = 0;
    _composition_length = 0;
    _composition_insert_codepoint_index = _caret_codepoint_index;
}

bool UiTextInput::set_text_internal(std::string text,bool notify_text_changed,bool move_caret_to_end)
{
    std::string sanitized = utf8_truncate_to_codepoint_limit(sanitize_single_line_text(text),_max_length);
    const std::string previous_text = _text;
    _text = std::move(sanitized);

    const std::size_t text_codepoint_count = utf8_codepoint_count(_text);
    if (move_caret_to_end)
        _caret_codepoint_index = text_codepoint_count;
    else
        _caret_codepoint_index = std::min(_caret_codepoint_index,text_codepoint_count);

    clear_composition();

    const bool changed = previous_text != _text;
    if (changed)
        notify_layout_parent_of_intrinsic_layout_invalidation();
    if (notify_text_changed)
        notify_text_changed_if_needed(previous_text);
    return changed;
}

bool UiTextInput::insert_text_at_caret(std::string_view text)
{
    std::string sanitized = sanitize_single_line_text(text);
    if (sanitized.empty())
        return false;

    const std::string previous_text = _text;
    const std::size_t base_codepoint_count = utf8_codepoint_count(_text);
    const std::size_t insert_at = _composition_text.empty() ? _caret_codepoint_index : _composition_insert_codepoint_index;
    const std::size_t allowed_codepoints = _max_length.has_value()
        ? (_max_length.value() > base_codepoint_count ? _max_length.value() - base_codepoint_count : 0U)
        : utf8_codepoint_count(sanitized);
    sanitized = utf8_truncate_to_codepoint_limit(sanitized,_max_length.has_value() ? std::optional<std::size_t>(allowed_codepoints) : std::nullopt);
    if (sanitized.empty())
    {
        clear_composition();
        return false;
    }

    const std::size_t insert_byte = utf8_byte_offset_from_codepoint_index(_text,insert_at);
    _text.insert(insert_byte,sanitized);
    _caret_codepoint_index = insert_at + utf8_codepoint_count(sanitized);
    clear_composition();
    const bool changed = previous_text != _text;
    if (changed)
        notify_layout_parent_of_intrinsic_layout_invalidation();
    notify_text_changed_if_needed(previous_text);
    return changed;
}

bool UiTextInput::erase_previous_codepoint()
{
    if (_caret_codepoint_index == 0 || _text.empty())
        return false;

    const std::string previous_text = _text;
    const std::size_t end_byte = utf8_byte_offset_from_codepoint_index(_text,_caret_codepoint_index);
    const std::size_t start_byte = utf8_byte_offset_from_codepoint_index(_text,_caret_codepoint_index - 1);
    _text.erase(start_byte,end_byte - start_byte);
    --_caret_codepoint_index;
    clear_composition();
    const bool changed = previous_text != _text;
    if (changed)
        notify_layout_parent_of_intrinsic_layout_invalidation();
    notify_text_changed_if_needed(previous_text);
    return changed;
}

bool UiTextInput::erase_next_codepoint()
{
    const std::size_t text_codepoint_count = utf8_codepoint_count(_text);
    if (_caret_codepoint_index >= text_codepoint_count || _text.empty())
        return false;

    const std::string previous_text = _text;
    const std::size_t start_byte = utf8_byte_offset_from_codepoint_index(_text,_caret_codepoint_index);
    const std::size_t end_byte = utf8_byte_offset_from_codepoint_index(_text,_caret_codepoint_index + 1);
    _text.erase(start_byte,end_byte - start_byte);
    clear_composition();
    const bool changed = previous_text != _text;
    if (changed)
        notify_layout_parent_of_intrinsic_layout_invalidation();
    notify_text_changed_if_needed(previous_text);
    return changed;
}

std::size_t UiTextInput::codepoint_index_at_x(int mouse_x) const
{
    const std::size_t codepoint_count = utf8_codepoint_count(_text);
    if (codepoint_count == 0)
        return 0;

    elysia::localization::LocalizationService* localization_service = ELYSIA_LOCALIZATION;

    const TextLayout layout = compute_text_layout();
    if (layout.content_rect.is_empty())
        return codepoint_count;

    elysia::localization::LocalizedTextStyle style;
    style.typography_role = _typography_role;
    style.font_source_override = _font_source_override;
    style.color = current_text_color();
    style.wrap_width = 0;

    const float target_text_x = presentation_to_layout_point(
        elysia::core::Vector2(static_cast<float>(mouse_x),0.0f)).x - layout.text_x;
    if (target_text_x <= 0.0f)
        return 0;

    int previous_width = 0;
    for (std::size_t index = 1; index <= codepoint_count; ++index)
    {
        const std::size_t byte_offset = utf8_byte_offset_from_codepoint_index(_text,index);
        const std::string prefix = _text.substr(0,byte_offset);

        int current_width = 0;
        int current_height = 0;
        if (!localization_service->measure_raw_text(prefix,style,current_width,current_height))
            return codepoint_count;

        const float midpoint = (static_cast<float>(previous_width) + static_cast<float>(current_width)) * 0.5f;
        if (target_text_x < midpoint)
            return index - 1;

        previous_width = current_width;
    }

    return codepoint_count;
}

void UiTextInput::move_caret_left() noexcept
{
    if (_caret_codepoint_index > 0)
        --_caret_codepoint_index;
    clear_composition();
}

void UiTextInput::move_caret_right() noexcept
{
    _caret_codepoint_index = std::min(_caret_codepoint_index + 1,utf8_codepoint_count(_text));
    clear_composition();
}

void UiTextInput::move_caret_home() noexcept
{
    _caret_codepoint_index = 0;
    clear_composition();
}

void UiTextInput::move_caret_end() noexcept
{
    _caret_codepoint_index = utf8_codepoint_count(_text);
    clear_composition();
}

elysia::core::Rect UiTextInput::content_rect() const noexcept
{
    const elysia::core::Rect& control_rect = screen_rect();
    const float width = std::max(0.0f,control_rect.width());
    const float height = std::max(0.0f,control_rect.height());
    const float padding = static_cast<float>(_padding);
    const float pad_x = std::min(padding,width * 0.5f);
    const float pad_y = std::min(padding,height * 0.5f);

    elysia::core::Rect content = control_rect;
    content.set_x(control_rect.x() + pad_x);
    content.set_y(control_rect.y() + pad_y);
    content.set_width(width - pad_x * 2.0f);
    content.set_height(height - pad_y * 2.0f);
    return content;
}

UiTextInput::TextLayout UiTextInput::compute_text_layout() const
{
    TextLayout layout;
    layout.content_rect = content_rect();
    if (layout.content_rect.is_empty())
        return layout;

    const std::size_t insert_byte = utf8_byte_offset_from_codepoint_index(_text,_composition_insert_codepoint_index);
    layout.display_text = _text.substr(0,insert_byte) + _composition_text + _text.substr(insert_byte);
    const std::size_t composition_codepoints = utf8_codepoint_count(_composition_text);
    const std::size_t clamped_composition_start = std::min<std::size_t>(static_cast<std::size_t>(std::max(_composition_start,0)),composition_codepoints);
    const std::size_t clamped_composition_length = std::min<std::size_t>(
        static_cast<std::size_t>(std::max(_composition_length,0)),
        composition_codepoints - clamped_composition_start);

    layout.visible_caret_codepoint_index = _composition_text.empty()
        ? _caret_codepoint_index
        : _composition_insert_codepoint_index + clamped_composition_start;
    layout.composition_display_start_codepoint_index = _composition_insert_codepoint_index + clamped_composition_start;
    layout.composition_display_length = clamped_composition_length;

    elysia::localization::LocalizationService* localization_service = ELYSIA_LOCALIZATION;

    elysia::localization::LocalizedTextStyle style;
    style.typography_role = _typography_role;
    style.font_source_override = _font_source_override;
    style.color = current_text_color();
    style.wrap_width = 0;

    int total_text_width = 0;
    int total_text_height = 0;
    (void)localization_service->measure_raw_text(layout.display_text,style,total_text_width,total_text_height);

    const std::string caret_prefix = layout.display_text.substr(
        0,
        utf8_byte_offset_from_codepoint_index(layout.display_text,layout.visible_caret_codepoint_index));
    int caret_prefix_width = 0;
    int caret_prefix_height = 0;
    (void)localization_service->measure_raw_text(caret_prefix,style,caret_prefix_width,caret_prefix_height);

    int highlight_prefix_width = 0;
    int highlight_prefix_height = 0;
    const std::string highlight_prefix = layout.display_text.substr(
        0,
        utf8_byte_offset_from_codepoint_index(layout.display_text,layout.composition_display_start_codepoint_index));
    (void)localization_service->measure_raw_text(highlight_prefix,style,highlight_prefix_width,highlight_prefix_height);

    int highlight_full_width = highlight_prefix_width;
    int highlight_full_height = 0;
    const std::string highlight_full_prefix = layout.display_text.substr(
        0,
        utf8_byte_offset_from_codepoint_index(
            layout.display_text,
            layout.composition_display_start_codepoint_index + layout.composition_display_length));
    (void)localization_service->measure_raw_text(highlight_full_prefix,style,highlight_full_width,highlight_full_height);

    layout.text_height = static_cast<float>(std::max({
        total_text_height,
        caret_prefix_height,
        highlight_prefix_height,
        highlight_full_height,
        1
    }));
    const float available_width = std::max(0.0f,layout.content_rect.width());
    layout.scroll_x = std::max(0.0f,static_cast<float>(caret_prefix_width) - available_width + 8.0f);
    layout.text_x = layout.content_rect.x() - layout.scroll_x;
    layout.text_y = layout.content_rect.center().y - layout.text_height * 0.5f;
    layout.caret_x = layout.text_x + static_cast<float>(caret_prefix_width);
    layout.composition_highlight_start_x = layout.text_x + static_cast<float>(highlight_prefix_width);
    layout.composition_highlight_end_x = layout.text_x + static_cast<float>(highlight_full_width);

    return layout;
}

elysia::core::Color UiTextInput::current_background_color() const noexcept
{
    return resolve_interactive_color(style().chrome.background,is_enabled(),is_focused(),_is_pushed);
}

elysia::core::Color UiTextInput::current_border_color() const noexcept
{
    return resolve_interactive_color(style().chrome.border,is_enabled(),is_focused(),_is_pushed);
}

elysia::core::Color UiTextInput::current_text_color() const noexcept
{
    return resolve_enabled_disabled_color(style().text,is_enabled());
}

elysia::core::Color UiTextInput::current_placeholder_color() const noexcept
{
    return resolve_enabled_disabled_color(style().placeholder,is_enabled());
}

void UiTextInput::sync_text_input_rect() const
{
    const TextLayout layout = compute_text_layout();
    SDL_Rect ime_rect{
        static_cast<int>(layout.caret_x),
        static_cast<int>(layout.text_y),
        1,
        static_cast<int>(std::max(layout.text_height,1.0f))
    };
    SDL_Window* window = s_text_input_window;
    if (!window) return;
    if (SDL_Renderer* renderer = SDL_GetRenderer(window))
    {
        float x=0,y=0,right=0,bottom=0;
        SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(ime_rect.x),static_cast<float>(ime_rect.y),&x,&y);
        SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(ime_rect.x+ime_rect.w),static_cast<float>(ime_rect.y+ime_rect.h),&right,&bottom);
        ime_rect = {static_cast<int>(std::floor(x)),static_cast<int>(std::floor(y)),
            std::max(1,static_cast<int>(std::ceil(right)-std::floor(x))),std::max(1,static_cast<int>(std::ceil(bottom)-std::floor(y)))};
    }
    SDL_SetTextInputArea(window,&ime_rect,0);
}

elysia::input::InputDevice UiTextInput::resolve_focus_input_device() const noexcept
{
    const UiChildHost* ancestor = layout_parent();
    while (ancestor)
    {
        if (const auto* focus_scope = dynamic_cast<const UiControlFocusScopeHost*>(ancestor))
            return focus_scope->focus_input_device();
        if (const auto* window = dynamic_cast<const UiWindow*>(ancestor))
            return window->focus_input_device();
        ancestor = ancestor->layout_parent();
    }
    return elysia::input::InputDevice::Unknown;
}

bool UiTextInput::should_try_show_screen_keyboard() const noexcept
{
    const elysia::input::InputDevice device = resolve_focus_input_device();
    return device != elysia::input::InputDevice::Keyboard
        && device != elysia::input::InputDevice::Mouse;
}

void UiTextInput::acquire_text_input_ownership() const
{
    if (s_text_input_window && s_text_input_window != SDL_GetKeyboardFocus()) SDL_StopTextInput(s_text_input_window);
    s_text_input_window = SDL_GetKeyboardFocus();
    s_text_input_owner = this;
    sync_text_input_rect();
    if (should_try_show_screen_keyboard())
        SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD,"1");
    if (s_text_input_window && !SDL_TextInputActive(s_text_input_window))
        SDL_StartTextInput(s_text_input_window);
}

void UiTextInput::release_text_input_ownership() const
{
    if (s_text_input_owner != this)
        return;

    s_text_input_owner = nullptr;
    if (s_text_input_window) SDL_StopTextInput(s_text_input_window);
    s_text_input_window = nullptr;
}

void UiTextInput::notify_text_changed_if_needed(const std::string& previous_text) const
{
    if (previous_text == _text || !_on_text_changed)
        return;
    const UiTextInputChangedCallback callback = _on_text_changed;
    const std::string text = _text;
    callback(text);
}

}


