#include "floating_number_effect_factory.h"

#include "../../tools/logger.h"
#include "../../typography/font_resolver.h"

#include <algorithm>
#include <cmath>

namespace elysia::effects
{
namespace
{
bool is_finite_vector(const elysia::core::Vector2& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y);
}
}

void FloatingNumberEffectFactory::set_runtime_dependencies(
	SDL_Renderer* renderer,
	const elysia::typography::FontResolver* font_resolver) noexcept
{
	if (_renderer == renderer && _font_resolver == font_resolver)
		return;

	_glyph_cache.reset();
	_renderer = renderer;
	_font_resolver = font_resolver;
}

void FloatingNumberEffectFactory::clear_cache() noexcept
{
	_glyph_cache.reset();
}

std::unique_ptr<FloatingNumberEffect> FloatingNumberEffectFactory::create(
	const FloatingNumberEffectSpawnRequest& request)
{
	if (request.text.empty())
	{
		ELYSIA_LOG_WARN("effects","Create floating number effect failed: text is empty.");
		return nullptr;
	}

	if (!is_finite_vector(request.position)
		|| !std::isfinite(request.target_height) || request.target_height <= 0.0f
		|| !std::isfinite(request.start_delay_seconds)
		|| !std::isfinite(request.time_scale) || request.time_scale < 0.0
		|| !std::isfinite(request.lifetime_seconds) || request.lifetime_seconds <= 0.0
		|| !FloatingNumberEffect::is_valid_effects(request.effects))
	{
		ELYSIA_LOG_WARN("effects","Create floating number effect failed: request parameters are invalid.");
		return nullptr;
	}

	for (const char ch : request.text)
	{
		if (!FloatingNumberGlyphCache::supports(ch))
		{
			ELYSIA_LOG_WARN("effects","Create floating number effect failed: text contains an unsupported character.");
			return nullptr;
		}
	}

	if (!_renderer)
	{
		ELYSIA_LOG_WARN("effects","Create floating number effect failed: renderer is unavailable.");
		return nullptr;
	}
	if (!_font_resolver)
	{
		ELYSIA_LOG_WARN("effects","Create floating number effect failed: FontResolver is unavailable.");
		return nullptr;
	}

	const auto resolved_font = _font_resolver->resolve_effect(
		elysia::typography::EffectTypographyRole::FloatingNumber);
	if (!resolved_font)
	{
		ELYSIA_LOG_WARN("effects","Create floating number effect failed: "
			<< resolved_font.error().message);
		return nullptr;
	}

	if (!_glyph_cache.configure(_renderer,resolved_font->font,resolved_font->generation))
	{
		ELYSIA_LOG_WARN("effects","Create floating number effect failed: glyph cache dependencies are unavailable.");
		return nullptr;
	}

	std::optional<std::vector<FloatingNumberGlyph>> glyphs =
		_glyph_cache.resolve(request.text,request.color);
	if (!glyphs.has_value())
	{
		ELYSIA_LOG_WARN("effects","Create floating number effect failed: digit glyph is unavailable.");
		return nullptr;
	}

	std::unique_ptr<FloatingNumberEffect> effect = std::make_unique<FloatingNumberEffect>(
		std::move(*glyphs),request.position,request.alignment,request.target_height,
		request.lifetime_seconds,request.effects,request.on_finished);
	effect->set_start_delay(std::max(0.0,request.start_delay_seconds));
	effect->set_time_scale(request.time_scale);
	return effect;
}
}
