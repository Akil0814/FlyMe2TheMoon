#pragma once

#include "builtin_resource_ids.h"
#include "../../io/path/path_manager.h"

#include <expected>
#include <filesystem>
#include <cstddef>
#include <span>
#include <string_view>

namespace elysia::builtin
{
struct BuiltinFontDescriptor
{
    BuiltinFontId id = BuiltinFontId::Count;
    BuiltinLocaleId locale = BuiltinLocaleId::Count;
    std::filesystem::path relative_path;
};

struct BuiltinTextureDescriptor
{
    BuiltinTextureId id = BuiltinTextureId::Count;
    std::filesystem::path relative_path;
};

struct BuiltinLocaleDescriptor
{
    BuiltinLocaleId id = BuiltinLocaleId::Count;
    std::filesystem::path relative_path;
};

struct BuiltinSoundDescriptor
{
    BuiltinSoundId id = BuiltinSoundId::Count;
    std::filesystem::path relative_path;
};

struct BuiltinMusicDescriptor
{
    BuiltinMusicId id = BuiltinMusicId::Count;
    std::filesystem::path relative_path;
};

struct BuiltinAnimationDescriptor
{
    BuiltinAnimationId id = BuiltinAnimationId::Count;
    BuiltinTextureId texture_id = BuiltinTextureId::Count;
    int frame_width = 0;
    int frame_height = 0;
    std::size_t frame_count = 0;
    double fps = 0.0;
    bool loop = false;

    [[nodiscard]] bool has_expected_texture_dimensions(int width, int height) const noexcept
    {
        return frame_width > 0
            && frame_height > 0
            && frame_count > 0
            && width == frame_width * static_cast<int>(frame_count)
            && height == frame_height;
    }
};

enum class BuiltinAssetValidationErrorCode
{
    RootMissing,
    RequiredMarkerMissing,
    RequiredFileMissing
};

struct BuiltinAssetValidationError
{
    BuiltinAssetValidationErrorCode code;
    std::filesystem::path path;
};

class BuiltinAssetCatalog
{
public:
    explicit BuiltinAssetCatalog(std::filesystem::path project_root);
    explicit BuiltinAssetCatalog(const elysia::io::PathManager& path_manager);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] std::filesystem::path required_marker_path() const;
    [[nodiscard]] std::span<const BuiltinFontDescriptor> fonts() const noexcept;
    [[nodiscard]] std::span<const BuiltinTextureDescriptor> textures() const noexcept;
    [[nodiscard]] std::span<const BuiltinLocaleDescriptor> locales() const noexcept;
    [[nodiscard]] std::span<const BuiltinAnimationDescriptor> animations() const noexcept;
    [[nodiscard]] std::span<const BuiltinSoundDescriptor> sounds() const noexcept;
    [[nodiscard]] std::span<const BuiltinMusicDescriptor> music() const noexcept;
    [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& relative_path) const;
    [[nodiscard]] std::expected<void, BuiltinAssetValidationError>validate_required_files() const;

private:
    std::filesystem::path _root;
};
}
