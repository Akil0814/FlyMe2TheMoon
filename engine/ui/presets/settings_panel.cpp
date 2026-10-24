#include "settings_panel.h"

#include "../composites/ui_dropdown.h"
#include "../composites/ui_labeled_checkbox.h"
#include "../containers/ui_scroll_container.h"
#include "../containers/ui_panel.h"
#include "../layout/ui_layout_types.h"
#include "../text/ui_text_content.h"
#include "../text/ui_typography.h"
#include "../widgets/label/ui_label.h"
#include "../widgets/ui_button.h"
#include "../widgets/ui_slider.h"
#include "../window/ui_window.h"

#include "../../tools/logger.h"
#include "../../localization/locale.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string_view>
#include <utility>

namespace elysia::ui
{
using elysia::typography::UiTypographyRole;

namespace
{
constexpr float kRowHeight = 48.0f;
constexpr float kTitleHeight = 56.0f;
constexpr float kSectionHeadingHeight = 32.0f;
constexpr float kStatusHeight = 32.0f;
constexpr float kActionHeight = 48.0f;
constexpr float kHintHeight = 32.0f;
constexpr float kItemSpacing = 6.0f;
constexpr float kHorizontalPadding = 28.0f;
constexpr float kVerticalPadding = 24.0f;
constexpr float kFieldSpacing = 20.0f;
constexpr float kButtonWidth = 160.0f;
constexpr float kCompactSliderValueHeight = 20.0f;
constexpr std::size_t kNotFound = std::numeric_limits<std::size_t>::max();

std::unique_ptr<UiLabel> make_label(UiTextContent content,float width,float height = kRowHeight,UiTypographyRole role = UiTypographyRole::Label)
{
    auto label = std::make_unique<UiLabel>(elysia::core::Rect{ 0,0,width,height },0,std::move(content));
    label->set_typography_role(role);
    label->set_horizontal_align(TextHorizontalAlign::Left);
    label->set_vertical_align(TextVerticalAlign::Center);
    return label;
}

std::unique_ptr<UiListContainer> make_field_row(UiTextContent label_content,float field_width,float label_width,std::unique_ptr<UiElement> control)
{
    auto row = std::make_unique<UiListContainer>(elysia::core::Rect{ 0,0,field_width,kRowHeight });
    row->set_direction(UiListDirection::Horizontal);
    row->set_item_spacing(kFieldSpacing);
    row->add_back(make_label(std::move(label_content),label_width));
    row->add_back(std::move(control));
    return row;
}

std::unique_ptr<UiListContainer> make_hint_row(
    UiTextContent content,
    float field_width)
{
    auto row = std::make_unique<UiListContainer>(
        elysia::core::Rect{ 0,0,field_width,kHintHeight });
    row->set_direction(UiListDirection::Horizontal);
    row->set_item_spacing(kFieldSpacing);
    auto hint = make_label(
        std::move(content),
        field_width,
        kHintHeight,
        UiTypographyRole::LabelMuted);
    hint->set_visual_role(UiLabelVisualRole::Muted);
    row->add_back(std::move(hint));
    return row;
}

std::unique_ptr<UiSlider> make_volume_slider(float width)
{
    // Settings rows use a compact numeric readout so the percentage remains subordinate
    // to the field label and keeps clear space inside the slider's trailing edge.
    UiSliderConfig config{};
    config.min_value = 0.0f;
    config.max_value = 100.0f;
    config.value = 100.0f;
    config.step = 1.0f;
    config.value_display = UiSliderValueDisplay::Percent;
    config.value_target_height = kCompactSliderValueHeight;
    return std::make_unique<UiSlider>(elysia::core::Rect{ 0,0,width,kRowHeight },config);
}

std::vector<SettingsWindowSize> normalized_window_sizes(std::vector<SettingsWindowSize> window_sizes)
{
    std::erase_if(window_sizes,[](const SettingsWindowSize& window_size)
    {
        return window_size.width <= 0 || window_size.height <= 0;
    });

    std::sort(window_sizes.begin(),window_sizes.end(),[](const auto& left,const auto& right)
    {
        if (left.width != right.width)
            return left.width < right.width;
        return left.height < right.height;
    });

    window_sizes.erase(std::unique(window_sizes.begin(),window_sizes.end()),window_sizes.end());

    return window_sizes;
}

std::vector<std::string> normalized_languages(std::vector<std::string> languages)
{
    std::erase_if(languages,[](const std::string& language) { return language.empty(); });
    std::vector<std::string> result;
    result.reserve(languages.size());
    for (std::string& language : languages)
    {
        if (std::find(result.begin(),result.end(),language) == result.end())
            result.push_back(std::move(language));
    }
    return result;
}

std::vector<double> normalized_target_fps_values(
    std::vector<double> values)
{
    std::erase_if(values,[](double value)
    {
        return !std::isfinite(value) || value <= 0.0;
    });
    std::sort(values.begin(),values.end());
    values.erase(std::unique(values.begin(),values.end()),values.end());
    return values;
}

std::string target_fps_label(double value)
{
    std::ostringstream stream;
    stream << value << " FPS";
    return stream.str();
}

UiTextContent language_content(const std::string& language)
{
    const std::string_view key_segment =
        elysia::localization::locale_key_segment(language);
    if (!key_segment.empty())
    {
        return ui_text_key("engine.settings.languages." + std::string(key_segment));
    }
    else
    {
        ELYSIA_LOG_ERROR("UiSettingPanel","cant find language i18n:"+language);
        return ui_raw_text(language);
    }
}
}

std::vector<SettingsWindowSize> make_settings_window_size_options(
    std::optional<SettingsWindowSize> usable_size,
    SettingsWindowSize current_size)
{
    constexpr std::array<SettingsWindowSize,6> presets{{
        { 960,540 },
        { 1280,720 },
        { 1600,900 },
        { 1920,1080 },
        { 2560,1440 },
        { 3840,2160 }
    }};

    std::vector<SettingsWindowSize> result;
    result.reserve(presets.size() + 1u);
    for (const SettingsWindowSize& preset : presets)
    {
        if (!usable_size
            || (preset.width <= usable_size->width
                && preset.height <= usable_size->height))
            result.push_back(preset);
    }
    if (!usable_size || (current_size.width <= usable_size->width
            && current_size.height <= usable_size->height))
        result.push_back(current_size);
    if (result.empty() && usable_size)
        result.push_back(*usable_size);
    return normalized_window_sizes(std::move(result));
}

std::vector<double> make_settings_target_fps_options(double current_fps)
{
    std::vector<double> result{ 30.0,60.0,120.0,240.0 };
    result.push_back(current_fps);
    return normalized_target_fps_values(std::move(result));
}

SettingsPanel::SettingsPanel(const elysia::core::Rect& rect,int order)
    : SettingsPanel(rect,SettingsPanelVisibility{},order)
{
}

SettingsPanel::SettingsPanel(
    const elysia::core::Rect& rect,
    SettingsPanelVisibility visibility,
    int order)
    : UiListContainer(rect,order)
    , _visibility(visibility)
{
    reset();
}

SettingsPanel::~SettingsPanel()
{
    unregister_from_window();
}

void SettingsPanel::reset() noexcept
{
    unregister_from_window();
    UiListContainer::reset();
    _options = {};
    _draft = {};
    _on_save = {};
    _on_back = {};
    _content_scroll = nullptr;
    _window_mode_dropdown = nullptr;
    _window_size_dropdown = nullptr;
    _target_fps_dropdown = nullptr;
    _vsync_checkbox = nullptr;
    _master_volume_slider = nullptr;
    _music_volume_slider = nullptr;
    _sound_volume_slider = nullptr;
    _language_dropdown = nullptr;
    _status_label = nullptr;
    _window = nullptr;
    _syncing_controls = false;
    build_controls();
}

void SettingsPanel::set_options(SettingsPanelOptions options)
{
    options.window_sizes =normalized_window_sizes(std::move(options.window_sizes));
    options.target_fps_values =
        normalized_target_fps_values(std::move(options.target_fps_values));
    options.languages = normalized_languages(std::move(options.languages));

    if (_draft.window_size.width > 0 && _draft.window_size.height > 0
        && std::find(options.window_sizes.begin(),options.window_sizes.end(), _draft.window_size)
        == options.window_sizes.end())
    {
        options.window_sizes.push_back(_draft.window_size);
        options.window_sizes =normalized_window_sizes(std::move(options.window_sizes));
    }

    if (std::isfinite(_draft.target_fps) && _draft.target_fps > 0.0
        && std::find(
            options.target_fps_values.begin(),
            options.target_fps_values.end(),
            _draft.target_fps) == options.target_fps_values.end())
    {
        options.target_fps_values.push_back(_draft.target_fps);
        options.target_fps_values = normalized_target_fps_values(
            std::move(options.target_fps_values));
    }

    if (!_draft.language.empty()
        && std::find(options.languages.begin(),options.languages.end(),_draft.language)
            == options.languages.end())
    {
        options.languages.push_back(_draft.language);
    }

    _options = std::move(options);
    if (_options.usable_window_size
        && _options.usable_window_size->width > 0
        && _options.usable_window_size->height > 0)
    {
        const auto limit = *_options.usable_window_size;
        std::erase_if(_options.window_sizes, [limit](const auto& size)
        {
            return size.width > limit.width || size.height > limit.height;
        });
        if (_options.window_sizes.empty())
            _options.window_sizes.push_back(limit);
        // Keep hidden settings intact; visible window choices must match the draft.
        if (_visibility.window_mode && find_window_size_index(_draft.window_size) == kNotFound)
            _draft.window_size = _options.window_sizes.back();
    }
    rebuild_window_options();
    rebuild_target_fps_options();
    rebuild_language_options();
    sync_controls_from_draft();
}

const SettingsPanelOptions& SettingsPanel::options() const noexcept
{
    return _options;
}

void SettingsPanel::set_draft(const SettingsPanelDraft& draft)
{
    _draft = draft;

    SettingsPanelOptions options = _options;
    bool options_changed = false;

    if (_draft.window_size.width > 0 && _draft.window_size.height > 0
        && find_window_size_index(_draft.window_size) == kNotFound)
    {
        options.window_sizes.push_back(_draft.window_size);
        options_changed = true;
    }

    if (std::isfinite(_draft.target_fps) && _draft.target_fps > 0.0
        && find_target_fps_index(_draft.target_fps) == kNotFound)
    {
        options.target_fps_values.push_back(_draft.target_fps);
        options_changed = true;
    }

    if (!_draft.language.empty() && find_language_index(_draft.language) == kNotFound)
    {
        options.languages.push_back(_draft.language);
        options_changed = true;
    }

    if (options_changed)
        set_options(std::move(options));
    else
        sync_controls_from_draft();
}

const SettingsPanelDraft& SettingsPanel::draft() const noexcept
{
    return _draft;
}

const SettingsPanelVisibility& SettingsPanel::visibility() const noexcept
{
    return _visibility;
}

void SettingsPanel::reset_navigation_state()
{
    if (_content_scroll)
        _content_scroll->scroll_to_top();
}

void SettingsPanel::set_on_save(SettingsPanelSaveCallback on_save)
{
    _on_save = std::move(on_save);
}

void SettingsPanel::set_on_back(SettingsPanelBackCallback on_back)
{
    _on_back = std::move(on_back);
}

void SettingsPanel::set_status_content(
    UiTextContent content,bool is_error)
{
    if (!_status_label)
        return;

    _status_label->set_text_content(std::move(content));
    _status_label->set_visual_role(
        is_error ? UiLabelVisualRole::Default : UiLabelVisualRole::Muted);
    _status_label->set_visible(true);
}

void SettingsPanel::set_status_message(std::string message,bool is_error)
{
    set_status_content(ui_raw_text(std::move(message)),is_error);
}

void SettingsPanel::clear_status_message()
{
    if (!_status_label)
        return;

    _status_label->set_text_content({});
    _status_label->set_visible(false);
}

void SettingsPanel::register_with_window(UiWindow& window)
{
    if (_window == &window)
        return;

    unregister_from_window();

    _window = &window;

    if (_window_mode_dropdown)
        _window_mode_dropdown->register_with_window(window);
    if (_window_size_dropdown)
        _window_size_dropdown->register_with_window(window);

    if (_target_fps_dropdown)
        _target_fps_dropdown->register_with_window(window);

    if (_language_dropdown)
        _language_dropdown->register_with_window(window);
}

void SettingsPanel::unregister_from_window() noexcept
{
    if (_window_mode_dropdown)
        _window_mode_dropdown->unregister_from_window();
    if (_window_size_dropdown)
        _window_size_dropdown->unregister_from_window();

    if (_target_fps_dropdown)
        _target_fps_dropdown->unregister_from_window();

    if (_language_dropdown)
        _language_dropdown->unregister_from_window();
    _window = nullptr;
}

void SettingsPanel::build_controls()
{
    set_direction(UiListDirection::Vertical);
    set_item_spacing(kItemSpacing);
    set_padding(UiLayoutPadding{
        kHorizontalPadding,
        kVerticalPadding,
        kHorizontalPadding,
        kVerticalPadding
    });

    const float field_width = std::max(
        240.0f,size().x - 2.0f * kHorizontalPadding);

    constexpr float scrollbar_space = 12.0f + 20.0f;
    const float content_width = field_width - scrollbar_space;

    constexpr float page_padding = 16.0f;
    const float page_field_width =
        content_width - page_padding * 2.0f;

    const float label_width = std::clamp(
        page_field_width * 0.32f,80.0f,190.0f);
    const float control_width = page_field_width - label_width - kFieldSpacing;

    auto title = make_label(
        ui_text_key("engine.settings.title"),
        field_width,kTitleHeight,UiTypographyRole::Title);
    title->set_horizontal_align(TextHorizontalAlign::Left);
    title->set_visual_role(UiLabelVisualRole::Title);
    add_back(std::move(title));

    auto content_scroll = std::make_unique<UiScrollContainer>();
    _content_scroll = content_scroll.get();
    _content_scroll->set_scroll_axis(UiScrollAxis::Vertical);
    _content_scroll->set_scrollbar_visibility(UiScrollBarVisibility::Auto);
    _content_scroll->set_scroll_step({ 0.0f,kRowHeight });
    _content_scroll->set_style_overrides(
        UiScrollContainerStyleOverrides{
            .scrollbar = {
                .thickness = 12.0f,
                .margin = 20.0f
            },
            .draw_border = false
        });

    auto content = std::make_unique<UiListContainer>(
        elysia::core::Rect{ 0,0,content_width,0 });
    content->set_direction(UiListDirection::Vertical);
    content->set_item_spacing(20.0f);

    const bool has_display = _visibility.window_mode
        || _visibility.target_fps || _visibility.vsync;
    const bool has_audio = _visibility.master_volume
        || _visibility.music_volume || _visibility.sound_volume;
    const bool has_general = _visibility.language;
    const int visible_section_count = static_cast<int>(has_display)
        + static_cast<int>(has_audio) + static_cast<int>(has_general);
    std::unique_ptr<UiListContainer> section;
    std::unique_ptr<UiListContainer> fields;
    const auto finish_section = [&]()
    {
        if (!section)
            return;
        // Reuse the existing panel palette; only its geometry is customized.
        const auto extent = fields->content_extent();
        fields->set_size(extent);
        auto surface = std::make_unique<UiPanel>(
            elysia::core::Rect{ 0,0,extent.x,extent.y });
        surface->set_style_overrides(UiPanelStyleOverrides{
            .corner_radius = 10.0f,
            .draw_border = false
        });
        surface->add_child(std::move(fields));
        section->add_back(std::move(surface));
        content->add_back(std::move(section));
    };
    const auto add_section_heading = [&](std::string_view key)
    {
        finish_section();
        section = std::make_unique<UiListContainer>(
            elysia::core::Rect{ 0,0,content_width,0 });
        section->set_item_spacing(8.0f);
        if (visible_section_count > 1)
        {
            section->add_back(make_label(
                ui_text_key(std::string(key)),content_width,
                kSectionHeadingHeight,UiTypographyRole::Heading));
        }
        fields = std::make_unique<UiListContainer>(
            elysia::core::Rect{ 0,0,content_width,0 });
        fields->set_item_spacing(kItemSpacing);
        fields->set_padding({ page_padding,12.0f,page_padding,12.0f });
    };

    if (has_display)
    {
        add_section_heading("engine.settings.sections.display");

        if (_visibility.window_mode)
        {
            auto window_mode = std::make_unique<UiDropdown>(
                elysia::core::Rect{ 0,0,control_width,kRowHeight });
            _window_mode_dropdown = window_mode.get();
            _window_mode_dropdown->set_options({
                { ui_text_key("engine.settings.window_modes.windowed") },
                { ui_text_key("engine.settings.window_modes.borderless_fullscreen") }
            });
            _window_mode_dropdown->set_on_selection_changed(
                [this](std::size_t index)
            {
                if (_syncing_controls)
                    return;
                _draft.window_mode = index == 0
                    ? SettingsWindowMode::Windowed
                    : SettingsWindowMode::BorderlessFullscreen;
                sync_controls_from_draft();
            });
            fields->add_back(make_field_row(
                ui_text_key("engine.settings.fields.window_mode"),
                page_field_width,
                label_width,
                std::move(window_mode)));

            auto window_size = std::make_unique<UiDropdown>(
                elysia::core::Rect{ 0,0,control_width,kRowHeight });
            _window_size_dropdown = window_size.get();
            _window_size_dropdown->set_on_selection_changed([this](std::size_t index)
            {
                if (!_syncing_controls && index < _options.window_sizes.size())
                    _draft.window_size = _options.window_sizes[index];
            });
            fields->add_back(make_field_row(
                ui_text_key("engine.settings.fields.window_size"),
                page_field_width,label_width,std::move(window_size)));
        }

        if (_visibility.target_fps)
        {
            auto target_fps = std::make_unique<UiDropdown>(
                elysia::core::Rect{ 0,0,control_width,kRowHeight });
            _target_fps_dropdown = target_fps.get();
            _target_fps_dropdown->set_on_selection_changed(
                [this](std::size_t index)
            {
                if (!_syncing_controls
                    && index < _options.target_fps_values.size())
                {
                    _draft.target_fps = _options.target_fps_values[index];
                }
            });
            fields->add_back(make_field_row(
                ui_text_key("engine.settings.fields.target_fps"),
                page_field_width,
                label_width,
                std::move(target_fps)));
        }

        if (_visibility.vsync)
        {
            auto vsync = std::make_unique<UiLabeledCheckbox>(
                elysia::core::Rect{ 0,0,kRowHeight,kRowHeight });
            _vsync_checkbox = vsync.get();
            _vsync_checkbox->set_on_toggled([this](UiCheckboxState state)
            {
                if (!_syncing_controls)
                    _draft.vsync = state == UiCheckboxState::Checked;
            });
            fields->add_back(make_field_row(
                ui_text_key("engine.settings.fields.vsync"),
                page_field_width,
                page_field_width - kRowHeight - kFieldSpacing,
                std::move(vsync)));
            fields->add_back(make_hint_row(
                ui_text_key("engine.settings.hints.vsync_restart"),
                page_field_width));
        }
    }

    if (has_audio)
    {
        add_section_heading("engine.settings.sections.audio");

        if (_visibility.master_volume)
        {
            auto master = make_volume_slider(control_width);
            _master_volume_slider = master.get();
            _master_volume_slider->set_on_value_changed([this](float value)
            {
                if (!_syncing_controls)
                {
                    _draft.master_volume =
                        static_cast<int>(std::lround(value));
                }
            });
            fields->add_back(make_field_row(
                ui_text_key("engine.settings.fields.master_volume"),
                page_field_width,
                label_width,
                std::move(master)));
        }

        if (_visibility.music_volume)
        {
            auto music = make_volume_slider(control_width);
            _music_volume_slider = music.get();
            _music_volume_slider->set_on_value_changed([this](float value)
            {
                if (!_syncing_controls)
                {
                    _draft.music_volume =
                        static_cast<int>(std::lround(value));
                }
            });
            fields->add_back(make_field_row(
                ui_text_key("engine.settings.fields.music_volume"),
                page_field_width,
                label_width,
                std::move(music)));
        }

        if (_visibility.sound_volume)
        {
            auto sound = make_volume_slider(control_width);
            _sound_volume_slider = sound.get();
            _sound_volume_slider->set_on_value_changed([this](float value)
            {
                if (!_syncing_controls)
                {
                    _draft.sound_volume =
                        static_cast<int>(std::lround(value));
                }
            });
            fields->add_back(make_field_row(
                ui_text_key("engine.settings.fields.sound_volume"),
                page_field_width,
                label_width,
                std::move(sound)));
        }
    }

    if (has_general)
    {
        add_section_heading("engine.settings.sections.general");

        auto language = std::make_unique<UiDropdown>(
            elysia::core::Rect{ 0,0,control_width,kRowHeight });
        _language_dropdown = language.get();
        _language_dropdown->set_on_selection_changed(
            [this](std::size_t index)
        {
            if (!_syncing_controls && index < _options.languages.size())
                _draft.language = _options.languages[index];
        });
        fields->add_back(make_field_row(
            ui_text_key("engine.settings.fields.language"),
            page_field_width,
            label_width,
            std::move(language)));
    }

    finish_section();
    _content_scroll->set_content(std::move(content));

    const float content_height = std::max(
        80.0f,
        size().y - 2.0f * kVerticalPadding - kTitleHeight
            - kStatusHeight - kActionHeight - 3.0f * kItemSpacing);
    add_child(std::move(content_scroll),UiLayoutChildOptions{
        ._size_override = { field_width,content_height },
        ._use_size_override = true
    });

    auto status = make_label({},field_width,kStatusHeight);
    status->set_visual_role(UiLabelVisualRole::Muted);
    _status_label = status.get();
    _status_label->set_visible(false);
    add_back(std::move(status));

    const float button_width = std::min(kButtonWidth,(field_width - 16.0f) * 0.5f);
    auto actions = std::make_unique<UiListContainer>(
        elysia::core::Rect{ 0,0,button_width * 2.0f + 16.0f,kActionHeight });
    actions->set_direction(UiListDirection::Horizontal);
    actions->set_cross_align(UiLayoutAlign::Center);
    actions->set_item_spacing(16.0f);

    auto save = std::make_unique<UiButton>(
        elysia::core::Rect{ 0,0,button_width,kActionHeight });
    save->set_text_content(ui_text_key("engine.settings.actions.save"));
    save->set_visual_role(UiButtonVisualRole::Primary);
    save->set_on_click([this]()
    {
        clear_status_message();
        const SettingsPanelSaveCallback callback = _on_save;
        if (callback)
            callback(_draft);
    });

    auto back = std::make_unique<UiButton>(
        elysia::core::Rect{ 0,0,button_width,kActionHeight });
    back->set_text_content(ui_text_key("engine.settings.actions.back"));
    back->set_on_click([this]()
    {
        const SettingsPanelBackCallback callback = _on_back;
        if (callback)
            callback();
    });
    actions->add_back(std::move(back));
    actions->add_back(std::move(save));
    add_child(std::move(actions),UiLayoutChildOptions{
        ._cross_align = UiLayoutAlign::End,
        ._use_custom_cross_align = true
    });

    reset_navigation_state();
}

void SettingsPanel::rebuild_window_options()
{
    if (!_window_size_dropdown)
        return;

    std::vector<UiDropdownOption> options;
    options.reserve(_options.window_sizes.size());
    for (const SettingsWindowSize& window_size : _options.window_sizes)
    {
        options.push_back(UiDropdownOption{
            ui_raw_text(
                std::to_string(window_size.width)
                + " x "
                + std::to_string(window_size.height))
        });
    }
    _window_size_dropdown->set_options(std::move(options));
}

void SettingsPanel::rebuild_target_fps_options()
{
    if (!_target_fps_dropdown)
        return;

    std::vector<UiDropdownOption> options;
    options.reserve(_options.target_fps_values.size());
    for (const double value : _options.target_fps_values)
    {
        options.push_back(UiDropdownOption{
            ui_raw_text(target_fps_label(value))
        });
    }
    _target_fps_dropdown->set_options(std::move(options));
}

void SettingsPanel::rebuild_language_options()
{
    if (!_language_dropdown)
        return;
    std::vector<UiDropdownOption> options;
    options.reserve(_options.languages.size());
    for (const std::string& language : _options.languages)
        options.push_back(UiDropdownOption{ language_content(language) });
    _language_dropdown->set_options(std::move(options));
}

void SettingsPanel::sync_controls_from_draft()
{
    _syncing_controls = true;

    if (_window_mode_dropdown)
    {
        (void)_window_mode_dropdown->set_selected_index(
            _draft.window_mode == SettingsWindowMode::Windowed ? 0u : 1u);
    }
    if (_window_size_dropdown)
    {
        const auto index = find_window_size_index(_draft.window_size);
        if (index != kNotFound)
            (void)_window_size_dropdown->set_selected_index(index);
        _window_size_dropdown->set_enabled(
            _draft.window_mode == SettingsWindowMode::Windowed
            && !_options.window_sizes.empty());
    }

    if (_master_volume_slider)
        _master_volume_slider->set_value(static_cast<float>(_draft.master_volume));
    if (_music_volume_slider)
        _music_volume_slider->set_value(static_cast<float>(_draft.music_volume));
    if (_sound_volume_slider)
        _sound_volume_slider->set_value(static_cast<float>(_draft.sound_volume));

    const std::size_t target_fps_index =
        find_target_fps_index(_draft.target_fps);
    if (_target_fps_dropdown && target_fps_index != kNotFound)
    {
        (void)_target_fps_dropdown->set_selected_index(target_fps_index);
    }
    if (_vsync_checkbox)
        _vsync_checkbox->set_checked(_draft.vsync);

    const std::size_t language_index = find_language_index(_draft.language);
    if (_language_dropdown && language_index != kNotFound)
        (void)_language_dropdown->set_selected_index(language_index);
    _syncing_controls = false;
}

std::size_t SettingsPanel::find_window_size_index(
    const SettingsWindowSize& window_size) const noexcept
{
    const auto iterator = std::find(
        _options.window_sizes.begin(),
        _options.window_sizes.end(),
        window_size);
    if (iterator == _options.window_sizes.end())
        return kNotFound;
    return static_cast<std::size_t>(
        std::distance(_options.window_sizes.begin(),iterator));
}

std::size_t SettingsPanel::find_target_fps_index(double target_fps) const noexcept
{
    const auto iterator = std::find(
        _options.target_fps_values.begin(),
        _options.target_fps_values.end(),
        target_fps);
    if (iterator == _options.target_fps_values.end())
        return kNotFound;
    return static_cast<std::size_t>(
        std::distance(_options.target_fps_values.begin(),iterator));
}

std::size_t SettingsPanel::find_language_index(
    const std::string& language) const noexcept
{
    const auto iterator = std::find(
        _options.languages.begin(),
        _options.languages.end(),
        language);
    if (iterator == _options.languages.end())
        return kNotFound;
    return static_cast<std::size_t>(
        std::distance(_options.languages.begin(),iterator));
}
}
