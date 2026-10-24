#include "animation.h"

namespace elysia::animation
{
Animation::Animation()
{
	_timer.set_one_shot(false);
	_timer.set_wait_time(_interval_seconds);
	_timer.set_on_timeout(
		[this]()
		{
			if (!_atlas || _atlas->empty())
			{
				_frame_index = 0;
				return;
			}

			++_frame_index;
			if (_frame_index < _atlas->size())
				return;

			_frame_index = _is_loop ? 0 : _atlas->size() - 1;
			if (_is_loop)
				return;

			_is_finished = true;
			_timer.pause();
			if (_on_finished)
				_on_finished();
		});
}

bool Animation::build_render_command(const elysia::core::Rect& target_rect,double angle_degrees,
	elysia::core::SpriteFlip flip,elysia::core::RenderCommand& out_command) const
{
	const elysia::resources::FrameInfo* frame_info = current_frame();
	if (!frame_info || !frame_info->_texture)
		return false;

	out_command.texture = frame_info->_texture;
	out_command.command_rect = target_rect;
	out_command.alpha = 255;
	out_command.texture_color_modulation.reset();
	out_command.use_src_rect = frame_info->_source_rect.has_value();
	if (frame_info->_source_rect.has_value())
		out_command.src_rect = *frame_info->_source_rect;
	out_command.rotation_degrees = angle_degrees;
	out_command.rotation_origin = elysia::core::Vector2(0.5f, 0.5f);
	out_command.flip = flip;
	return true;
}

bool Animation::append_render_commands(const elysia::core::Rect& target_rect,double angle_degrees,
	elysia::core::SpriteFlip flip,const std::optional<elysia::core::Color>& color_overlay,
	std::vector<elysia::core::RenderCommand>& out_commands) const
{
	elysia::core::RenderCommand base_command;
	if (!build_render_command(target_rect,angle_degrees,flip,base_command))
		return false;

	const elysia::resources::FrameInfo* frame_info = current_frame();
	if (color_overlay && color_overlay->a > 0 && (!frame_info || !frame_info->_coverage_mask))
		return false;

	out_commands.push_back(base_command);

	if (!color_overlay || color_overlay->a == 0)
		return true;

	elysia::core::RenderCommand overlay_command = base_command;
	overlay_command.texture = frame_info->_coverage_mask;
	overlay_command.alpha = color_overlay->a;
	overlay_command.texture_color_modulation = 
		elysia::core::TextureColorModulation{
			.r = color_overlay->r,
			.g = color_overlay->g,
			.b = color_overlay->b
		};
	out_commands.push_back(overlay_command);
	return true;
}

void Animation::update(double delta_seconds)
{
	if (!_atlas || _atlas->empty() || _is_finished)
		return;

	_timer.update(delta_seconds);
}

void Animation::set_atlas(const elysia::resources::Atlas* atlas)
{
	_atlas = atlas;
	reset();
}

void Animation::set_loop(bool is_loop)
{
	_is_loop = is_loop;
}

void Animation::set_interval_seconds(double interval_seconds)
{
	_interval_seconds = interval_seconds > 0.0 ? interval_seconds : 0.1;
	_timer.set_wait_time(_interval_seconds);
}

void Animation::set_on_finished(PlayCallback on_finished)
{
	_on_finished = std::move(on_finished);
}

void Animation::reset()
{
	_timer.restart();
	_frame_index = 0;
	_is_finished = false;
}

void Animation::pause()
{
	_timer.pause();
}

void Animation::resume()
{
	_timer.resume();
}

bool Animation::is_finished() const
{
	return _is_finished;
}

bool Animation::is_paused() const
{
	return _timer.is_paused();
}

size_t Animation::current_frame_index() const
{
	return _frame_index;
}

const elysia::resources::FrameInfo* Animation::current_frame() const
{
	if (!_atlas)
		return nullptr;

	return _atlas->frame_at(_frame_index);
}

}
