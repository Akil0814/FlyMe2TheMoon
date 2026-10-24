#pragma once

#include "../effect_types.h"
#include "floating_number_glyph_cache.h"

#include <memory>

struct SDL_Renderer;

namespace elysia::typography
{
class FontResolver;
}

namespace elysia::effects
{
class FloatingNumberEffectFactory
{
public:
	void set_runtime_dependencies(
		SDL_Renderer* renderer,
		const elysia::typography::FontResolver* font_resolver) noexcept;
	void clear_cache() noexcept;

	[[nodiscard]] std::unique_ptr<FloatingNumberEffect> create(
		const FloatingNumberEffectSpawnRequest& request);

private:
	FloatingNumberGlyphCache _glyph_cache;
	SDL_Renderer* _renderer = nullptr;
	const elysia::typography::FontResolver* _font_resolver = nullptr;
};
}
