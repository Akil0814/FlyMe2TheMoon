#pragma once
#include "../resource_sub_manager.h"
#include "../resource_failure.h"
#include "../resource_types.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>

namespace elysia::resources
{
using FontPool = std::unordered_map<std::string, TTF_Font*>;

class FontManager : public ResourceSubManager
{
public:
	~FontManager() override;

	[[nodiscard]] std::expected<void,ResourceFailure> load_font(
		const std::string& key,
		const std::filesystem::path& file_path,
		int point_size
	);
	[[nodiscard]] std::expected<void,ResourceFailure> load_font(
		const FontLoadRequest& request);
	[[nodiscard]] std::expected<void,ResourceFailure> store_font(
		const std::string& key,TTF_Font* font);
	bool has_font(std::string_view key) const noexcept;
	TTF_Font* find_font(const std::string_view& key) const;

	void clear() override;
	size_t resource_count() const override;

private:
	FontPool _font_pool;
};

}
