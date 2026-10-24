#include "ui_child_host.h"
#include "ui_render_command_range_utils.h"
#include "../layout/ui_layout_geometry.h"
#include "../style/ui_theme_manager.h"

#include <algorithm>
#include <cstddef>
#include <exception>

namespace elysia::ui
{
void UiElement::notify_layout_parent_of_intrinsic_layout_invalidation() noexcept
{
    if (_layout_parent)
        _layout_parent->on_child_intrinsic_layout_invalidated(*this);
}

UiChildHost::UiChildHost(const elysia::core::Rect& rect,int order) noexcept
    : UiElement(rect,order) {}

UiChildHost::UiChildHost(const elysia::core::Vector2& position,const elysia::core::Vector2& size,int order) noexcept
    : UiElement(position,size,order) {}

UiChildHost::UiChildHost(const elysia::core::Vector2& center,const elysia::core::Vector2& size,UiFromCenterTag,int order) noexcept
    : UiElement(center,size,from_center,order) {}

UiChildHost::~UiChildHost()
{
    if (_theme_manager)
        _theme_manager->on_host_destroying(*this);
}

void UiChildHost::reset() noexcept
{
    if (_theme_manager)
        for (ChildEntry& entry : _children)
            if (entry.element) _theme_manager->detach_subtree(*entry.element);
    while (!_external_style_children.empty())
    {
        UiElement* external = _external_style_children.back();
        if (external)
            detach_external_style_subtree(*external);
        else
            _external_style_children.pop_back();
    }
    detach_all_children_from_layout_tree();
    for (ChildEntry& entry : _children)
        invalidate_child_lifetime(entry);
    UiElement::reset();
    _children.clear();
    invalidate_child_order_cache();
    _padding = UiLayoutPadding{};
    _clip_children = false;
    _layout_dirty = true;
    _is_rebuilding_layout = false;
    _layout_dirty_after_rebuild = false;
    _last_layout_rect = elysia::core::Rect::zero();
}

UiElement* UiChildHost::add_child(std::unique_ptr<UiElement> child,UiLayoutChildOptions options)
{
    return insert_child(std::move(child),_children.size(),options);
}

UiElement* UiChildHost::insert_child(std::unique_ptr<UiElement> child,std::size_t index,UiLayoutChildOptions options)
{
    if (!child)
        return nullptr;

    UiElement* child_ptr = child.get();
    const std::size_t target_index = std::min(index,_children.size());
    auto lifetime = std::make_shared<ChildLifetime>();
    lifetime->element = child_ptr;
    ChildEntry entry{};
    entry.element = std::move(child);
    entry.layout = options;
    entry.lifetime = std::move(lifetime);
    _children.insert(_children.begin() + static_cast<std::ptrdiff_t>(target_index),std::move(entry));
    attach_child_to_layout_tree(*child_ptr);
    if (_theme_manager)
        _theme_manager->attach_and_apply_subtree(*child_ptr);
    invalidate_child_order_cache();
    invalidate_intrinsic_layout();
    return child_ptr;
}

void UiChildHost::clear_children()
{
    if (_children.empty())
        return;

    if (_theme_manager)
        for (ChildEntry& entry : _children)
            if (entry.element) _theme_manager->detach_subtree(*entry.element);
    detach_all_children_from_layout_tree();
    for (ChildEntry& entry : _children)
        invalidate_child_lifetime(entry);
    _children.clear();
    invalidate_child_order_cache();
    invalidate_intrinsic_layout();
}

std::unique_ptr<UiElement> UiChildHost::extract_child(std::size_t index)
{
    if (index >= _children.size())
        return nullptr;
    ChildEntry& entry = _children[index];
    if (_theme_manager && entry.element)
        _theme_manager->detach_subtree(*entry.element);
    detach_child_from_layout_tree(entry.element.get());
    invalidate_child_lifetime(entry);
    std::unique_ptr<UiElement> result = std::move(entry.element);
    _children.erase(_children.begin() + static_cast<std::ptrdiff_t>(index));
    invalidate_child_order_cache();
    mark_layout_dirty();
    notify_layout_parent_of_intrinsic_layout_invalidation();
    return result;
}

std::size_t UiChildHost::child_count() const noexcept
{
    return _children.size();
}

UiElement* UiChildHost::child_at(std::size_t index) noexcept
{
    return index < _children.size() ? _children[index].element.get() : nullptr;
}

const UiElement* UiChildHost::child_at(std::size_t index) const noexcept
{
    return index < _children.size() ? _children[index].element.get() : nullptr;
}

void UiChildHost::set_child_layout_options(std::size_t index,const UiLayoutChildOptions& options)
{
    if (index >= _children.size())
        return;
    _children[index].layout = options;
    invalidate_intrinsic_layout();
}

const UiLayoutChildOptions* UiChildHost::child_layout_options(std::size_t index) const noexcept
{
    return index < _children.size() ? &_children[index].layout : nullptr;
}

bool UiChildHost::move_child(std::size_t from,std::size_t to)
{
    if (from >= _children.size() || to >= _children.size() || from == to)
        return from == to && from < _children.size();
    ChildEntry moved = std::move(_children[from]);
    _children.erase(_children.begin() + static_cast<std::ptrdiff_t>(from));
    _children.insert(_children.begin() + static_cast<std::ptrdiff_t>(to),std::move(moved));
    invalidate_child_order_cache();
    invalidate_intrinsic_layout();
    return true;
}

void UiChildHost::set_padding(const UiLayoutPadding& padding) noexcept
{
    _padding = padding;
    invalidate_intrinsic_layout();
}

const UiLayoutPadding& UiChildHost::padding() const noexcept
{
    return _padding;
}

void UiChildHost::set_clip_children(bool clip_children) noexcept
{
    _clip_children = clip_children;
}

bool UiChildHost::clips_children() const noexcept
{
    return _clip_children;
}

void UiChildHost::mark_layout_dirty() noexcept
{
    if (_is_rebuilding_layout)
        _layout_dirty_after_rebuild = true;
    _layout_dirty = true;
}

void UiChildHost::invalidate_intrinsic_layout() noexcept
{
    mark_layout_dirty();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiChildHost::on_child_intrinsic_layout_invalidated(UiElement& child) noexcept
{
    (void)child;
    if (_is_rebuilding_layout)
        return;

    mark_layout_dirty();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiChildHost::on_child_base_style_invalidated(UiElement& child) noexcept
{
    UiElement* target = &child;
    for (const ChildEntry& entry : _children)
    {
        if (entry.element.get() == &child && entry.style_relation == UiChildStyleRelation::CompositeImplementation)
        {
            target = entry.style_owner ? entry.style_owner : this;
            break;
        }
    }
    if (_theme_manager)
        _theme_manager->refresh_element(*target);
    else
        notify_base_style_invalidated();
}

void UiChildHost::notify_host_base_style_invalidated() noexcept
{
    if (_theme_manager)
        _theme_manager->refresh_element(*this);
    else
        notify_base_style_invalidated();
}

void UiChildHost::attach_external_style_subtree(UiElement& element)
{
    if (std::find(_external_style_children.begin(),_external_style_children.end(),&element)
        == _external_style_children.end())
        _external_style_children.push_back(&element);
    if (_theme_manager)
        _theme_manager->attach_and_apply_subtree(element);
}

void UiChildHost::detach_external_style_subtree(UiElement& element) noexcept
{
    if (_theme_manager)
        _theme_manager->detach_subtree(element);
    std::erase(_external_style_children,&element);
}

void UiChildHost::mark_child_as_composite_implementation(UiElement& child,UiElement& style_owner) noexcept
{
    for (ChildEntry& entry : _children)
    {
        if (entry.element.get() != &child)
            continue;
        entry.style_relation = UiChildStyleRelation::CompositeImplementation;
        entry.style_owner = &style_owner;
        return;
    }
}

void UiChildHost::update_layout_if_dirty()
{
    if (_is_rebuilding_layout)
        return;

    if (!needs_layout_rebuild())
        return;

    _is_rebuilding_layout = true;
    _layout_dirty_after_rebuild = false;
    rebuild_layout();
    _is_rebuilding_layout = false;
    _last_layout_rect = screen_rect();
    _layout_dirty = _layout_dirty_after_rebuild;
    _layout_dirty_after_rebuild = false;
}

void UiChildHost::update(double delta)
{
    cleanup_destroyed_children();
    update_layout_if_dirty();
    update_child_objects(delta);
}

void UiChildHost::update_presentation_animations(double delta)
{
    UiElement::update_presentation_animations(delta);
    ensure_child_order_cache();
    for (const UiChildHandle& handle : _logical_child_order)
    {
        UiElement* child = handle.resolve();
        if (!child || child->is_destroyed() || !child->is_active())
            continue;
        child->update_presentation_animations(delta);
    }
}

void UiChildHost::on_ui_input_frame(const UiInputFrame& input)
{
    cleanup_destroyed_children();
    update_layout_if_dirty();
    dispatch_frame_to_children(input);
}

bool UiChildHost::on_ui_input_event(const UiInputEvent& event)
{
    cleanup_destroyed_children();
    update_layout_if_dirty();
    return dispatch_input_to_children(event);
}

void UiChildHost::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    const_cast<UiChildHost*>(this)->update_layout_if_dirty();
    submit_child_render_commands(out_commands);
}

void UiChildHost::rebuild_layout()
{
}

elysia::core::Rect UiChildHost::content_rect() const noexcept
{
    return layout::padded_content_rect(screen_rect(),_padding);
}

std::vector<UiChildHost::ChildEntry>& UiChildHost::children() noexcept
{
    invalidate_child_order_cache();
    return _children;
}

const std::vector<UiChildHost::ChildEntry>& UiChildHost::children() const noexcept
{
    return _children;
}

void UiChildHost::submit_child_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    const elysia::core::Rect clip_rect = clips_children() ? content_rect() : elysia::core::Rect::zero();
    ensure_child_order_cache();
    for (const UiChildHandle& handle : _visual_child_order)
    {
        const UiElement* child = handle.resolve();
        if (!child || child->is_destroyed() || !child->is_visible())
            continue;
        const elysia::core::Vector2 translation = child->presentation_translation();
        const std::size_t begin = out_commands.size();
        child->submit_ui_render_commands(out_commands);
        render_command_range_utils::apply_translation_to_range(out_commands,begin,translation);
        finalize_child_command_range(out_commands,begin,clip_rect);
    }
}

void UiChildHost::apply_opacity_to_range(std::vector<elysia::core::UiRenderCommand>& out_commands,std::size_t begin) const
{
    render_command_range_utils::apply_opacity_to_range(out_commands,begin,opacity());
}

void UiChildHost::apply_child_presentation_translation_to_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    const UiElement& child
) const
{
    render_command_range_utils::apply_translation_to_range(out_commands,begin,child.presentation_translation());
}

void UiChildHost::apply_clip_to_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    const elysia::core::Rect& clip_rect
) const
{
    render_command_range_utils::apply_clip_to_range(out_commands,begin,clip_rect);
}

void UiChildHost::finalize_child_command_range(
    std::vector<elysia::core::UiRenderCommand>& out_commands,
    std::size_t begin,
    const elysia::core::Rect& clip_rect
) const
{
    render_command_range_utils::finalize_child_command_range(out_commands,begin,opacity(),clips_children(),clip_rect);
}

void UiChildHost::update_child_objects(double delta)
{
    ensure_child_order_cache();
    for (const UiChildHandle& handle : _logical_child_order)
    {
        UiElement* child = handle.resolve();
        if (!child || child->is_destroyed() || !child->is_active())
            continue;
        if (elysia::core::Updatable* updatable = dynamic_cast<elysia::core::Updatable*>(child))
            updatable->update(delta);
    }
}

void UiChildHost::dispatch_frame_to_children(const UiInputFrame& input)
{
    ensure_child_order_cache();
    for (const UiChildHandle& handle : _logical_child_order)
    {
        UiElement* child = handle.resolve();
        if (!child || child->is_destroyed() || !child->is_active())
            continue;
        if (UiInputFrameReceiver* receiver = dynamic_cast<UiInputFrameReceiver*>(child))
            receiver->on_ui_input_frame(input);
    }
}

bool UiChildHost::dispatch_input_to_children(const UiInputEvent& event)
{
    ensure_child_order_cache();
    for (auto iter = _visual_child_order.rbegin(); iter != _visual_child_order.rend(); ++iter)
    {
        UiElement* child = iter->resolve();
        if (!child || child->is_destroyed() || !child->is_active())
            continue;
        if (UiInputEventReceiver* receiver = dynamic_cast<UiInputEventReceiver*>(child))
        {
            if (receiver->on_ui_input_event(event))
                return true;
        }
    }

    return false;
}

void UiChildHost::cleanup_destroyed_children()
{
    const std::size_t previous_count = _children.size();
    std::erase_if(_children,[this](ChildEntry& entry)
    {
        if (entry.element && !entry.element->is_destroyed())
            return false;

        detach_child_from_layout_tree(entry.element.get());
        if (_theme_manager && entry.element)
            _theme_manager->detach_subtree(*entry.element);
        invalidate_child_lifetime(entry);
        return true;
    });
    if (_children.size() != previous_count)
    {
        invalidate_child_order_cache();
        invalidate_intrinsic_layout();
    }
}

bool UiChildHost::needs_layout_rebuild() const noexcept
{
    return _layout_dirty || !_last_layout_rect.nearly_equals(screen_rect());
}

void UiChildHost::attach_child_to_layout_tree(UiElement& child) noexcept
{
    child.set_layout_parent(this);
}

void UiChildHost::detach_child_from_layout_tree(UiElement* child) noexcept
{
    if (child)
        child->set_layout_parent(nullptr);
}

void UiChildHost::detach_all_children_from_layout_tree() noexcept
{
    for (ChildEntry& entry : _children)
        detach_child_from_layout_tree(entry.element.get());
}

void UiChildHost::on_child_order_changed(UiElement& child) noexcept
{
    (void)child;
    invalidate_child_order_cache();
}

void UiChildHost::ensure_child_order_cache() const
{
    if (!_child_order_cache_dirty)
        return;

    _logical_child_order.clear();
    _visual_child_order.clear();
    _logical_child_order.reserve(_children.size());
    _visual_child_order.reserve(_children.size());
    for (const ChildEntry& entry : _children)
    {
        if (!entry.element)
            continue;
        UiChildHandle handle = make_child_handle(entry);
        _logical_child_order.push_back(handle);
        _visual_child_order.push_back(std::move(handle));
    }
    std::stable_sort(_visual_child_order.begin(),_visual_child_order.end(),[](const UiChildHandle& left,const UiChildHandle& right)
    {
        return left.resolve()->order() < right.resolve()->order();
    });
    _child_order_cache_dirty = false;
}

void UiChildHost::invalidate_child_order_cache() noexcept
{
    _child_order_cache_dirty = true;
}

void UiChildHost::invalidate_child_lifetime(ChildEntry& entry) noexcept
{
    entry.invalidate_lifetime();
}

UiChildHost::UiChildHandle UiChildHost::make_child_handle(const ChildEntry& entry) const noexcept
{
    return UiChildHandle{ entry.lifetime,entry.lifetime ? entry.lifetime->generation : 0 };
}

void UiChildHost::attach_theme_manager(UiThemeManager& manager)
{
    _theme_manager = &manager;
}

void UiChildHost::detach_theme_manager(UiThemeManager& manager) noexcept
{
    if (_theme_manager == &manager)
        _theme_manager = nullptr;
}
}

elysia::input::InputCapture elysia::ui::UiChildHost::input_capture() const noexcept
{
    auto result = elysia::input::InputCapture::None;
    for (const auto &child : _children)
        if (child.element && child.element->is_active() && child.element->is_visible() &&
            !child.element->is_destroyed())
            result = result | child.element->input_capture();
    return result;
}

void elysia::ui::UiChildHost::cancel_input_interaction()
{
    ensure_child_order_cache();
    const auto snapshot = _logical_child_order;
    std::exception_ptr first_failure;
    for (const auto& handle : snapshot)
        if (auto* child = handle.resolve())
        {
            try { child->cancel_input_interaction(); }
            catch (...) { if (!first_failure) first_failure = std::current_exception(); }
        }
    if (first_failure)
        std::rethrow_exception(first_failure);
}
