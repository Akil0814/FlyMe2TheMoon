#include "builtin_asset_catalog.h"

#include <array>
#include <system_error>
#include <utility>

namespace elysia::builtin
{
namespace
{
const std::array<BuiltinFontDescriptor, 5> kFontDescriptors = {
    BuiltinFontDescriptor{BuiltinFontId::Latin, BuiltinLocaleId::English, "fonts/NotoSans-Regular.ttf"},
    BuiltinFontDescriptor{BuiltinFontId::SimplifiedChinese, BuiltinLocaleId::SimplifiedChinese, "fonts/NotoSansSC-Regular.ttf"},
    BuiltinFontDescriptor{BuiltinFontId::TraditionalChinese, BuiltinLocaleId::TraditionalChinese, "fonts/NotoSansTC-Regular.ttf"},
    BuiltinFontDescriptor{BuiltinFontId::Japanese, BuiltinLocaleId::Japanese, "fonts/NotoSansJP-Regular.ttf"},
    BuiltinFontDescriptor{BuiltinFontId::Korean, BuiltinLocaleId::Korean, "fonts/NotoSansKR-Regular.ttf"},
};

const std::array<BuiltinTextureDescriptor, 7> kTextureDescriptors = {
    BuiltinTextureDescriptor{BuiltinTextureId::ElysiaDefault, "textures/elysia.png"},
    BuiltinTextureDescriptor{BuiltinTextureId::ElysiaBlack, "textures/elysia_black.png"},
    BuiltinTextureDescriptor{BuiltinTextureId::ElysiaBlackAlphaInverse,"textures/elysia_black_alpha_inverse.png"},
    BuiltinTextureDescriptor{BuiltinTextureId::ElysiaLightEdge, "textures/elysia_light_edge.png"},
    BuiltinTextureDescriptor{BuiltinTextureId::ElysiaWhite,"textures/elysia_white.png"},
    BuiltinTextureDescriptor{BuiltinTextureId::EngineCharacterMove, "textures/character_move.png"},
    BuiltinTextureDescriptor{BuiltinTextureId::EngineCharacterIdle, "textures/character_idle.png"},
};

const std::array<BuiltinLocaleDescriptor, 5> kLocaleDescriptors = {
    BuiltinLocaleDescriptor{BuiltinLocaleId::English, "i18n/en/engine.json"},
    BuiltinLocaleDescriptor{BuiltinLocaleId::SimplifiedChinese, "i18n/zh-Hans/engine.json"},
    BuiltinLocaleDescriptor{BuiltinLocaleId::TraditionalChinese, "i18n/zh-Hant/engine.json"},
    BuiltinLocaleDescriptor{BuiltinLocaleId::Japanese, "i18n/ja/engine.json"},
    BuiltinLocaleDescriptor{BuiltinLocaleId::Korean, "i18n/ko/engine.json"},
};

const std::array<BuiltinAnimationDescriptor, 2> kAnimationDescriptors = {
    BuiltinAnimationDescriptor{
        .id = BuiltinAnimationId::EngineCharacterIdle,
        .texture_id = BuiltinTextureId::EngineCharacterIdle,
        .frame_width = 32,
        .frame_height = 32,
        .frame_count = 8,
        .fps = 8.0,
        .loop = true
    },
    BuiltinAnimationDescriptor{
        .id = BuiltinAnimationId::EngineCharacterMove,
        .texture_id = BuiltinTextureId::EngineCharacterMove,
        .frame_width = 32,
        .frame_height = 32,
        .frame_count = 8,
        .fps = 8.0,
        .loop = true
    }
};

const std::array<BuiltinSoundDescriptor, 0> kSoundDescriptors = {};

const std::array<BuiltinMusicDescriptor, 1> kMusicDescriptors = {
    BuiltinMusicDescriptor{BuiltinMusicId::ElysianRealm,"audio/Elysian_Realm.ogg"}
};

constexpr std::string_view kRequiredMarkerFileName = ".elysia_engine_required";
}

BuiltinAssetCatalog::BuiltinAssetCatalog(std::filesystem::path project_root)
    : _root(std::move(project_root) / "assets" / "engine"){}

BuiltinAssetCatalog::BuiltinAssetCatalog(const elysia::io::PathManager& path_manager)
    : _root(path_manager.assets() / "engine"){}

const std::filesystem::path& BuiltinAssetCatalog::root() const noexcept
{
    return _root;
}

std::filesystem::path BuiltinAssetCatalog::required_marker_path() const
{
    return _root / kRequiredMarkerFileName;
}

std::span<const BuiltinFontDescriptor> BuiltinAssetCatalog::fonts() const noexcept
{
    return kFontDescriptors;
}

std::span<const BuiltinTextureDescriptor> BuiltinAssetCatalog::textures() const noexcept
{
    return kTextureDescriptors;
}

std::span<const BuiltinLocaleDescriptor> BuiltinAssetCatalog::locales() const noexcept
{
    return kLocaleDescriptors;
}

std::span<const BuiltinAnimationDescriptor> BuiltinAssetCatalog::animations() const noexcept
{
    return kAnimationDescriptors;
}

std::span<const BuiltinSoundDescriptor> BuiltinAssetCatalog::sounds() const noexcept
{
    return kSoundDescriptors;
}

std::span<const BuiltinMusicDescriptor> BuiltinAssetCatalog::music() const noexcept
{
    return kMusicDescriptors;
}

std::filesystem::path BuiltinAssetCatalog::resolve(const std::filesystem::path& relative_path) const
{
    return _root / relative_path;
}

std::expected<void, BuiltinAssetValidationError>
BuiltinAssetCatalog::validate_required_files() const
{
    std::error_code error;
    if (!std::filesystem::is_directory(_root, error))
    {
        return std::unexpected(BuiltinAssetValidationError{
            .code = BuiltinAssetValidationErrorCode::RootMissing,
            .path = _root
        });
    }

    const std::filesystem::path required_marker = required_marker_path();
    if (!std::filesystem::is_regular_file(required_marker, error))
    {
        return std::unexpected(BuiltinAssetValidationError{
            .code = BuiltinAssetValidationErrorCode::RequiredMarkerMissing,
            .path = required_marker
        });
    }

    const auto validate_asset = [this](const auto& descriptor)-> std::expected<void, BuiltinAssetValidationError>
    {
        const std::filesystem::path path = resolve(descriptor.relative_path);
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error))
        {
            return std::unexpected(BuiltinAssetValidationError{
                .code = BuiltinAssetValidationErrorCode::RequiredFileMissing,
                .path = path
            });
        }

        return {};
    };

    for (const BuiltinFontDescriptor& descriptor : kFontDescriptors)
    {
        if (const auto result = validate_asset(descriptor); !result)
            return result;
    }
    for (const BuiltinTextureDescriptor& descriptor : kTextureDescriptors)
    {
        if (const auto result = validate_asset(descriptor); !result)
            return result;
    }
    for (const BuiltinLocaleDescriptor& descriptor : kLocaleDescriptors)
    {
        if (const auto result = validate_asset(descriptor); !result)
            return result;
    }
    for (const BuiltinSoundDescriptor& descriptor : kSoundDescriptors)
    {
        if (const auto result = validate_asset(descriptor); !result)
            return result;
    }
    for (const BuiltinMusicDescriptor& descriptor : kMusicDescriptors)
    {
        if (const auto result = validate_asset(descriptor); !result)
            return result;
    }

    for (const BuiltinAnimationDescriptor& descriptor : kAnimationDescriptors)
    {
        if (builtin_resource_name(descriptor.id).empty()
            || builtin_resource_name(descriptor.texture_id).empty()
            || descriptor.frame_width <= 0 || descriptor.frame_height <= 0
            || descriptor.frame_count == 0 || descriptor.fps <= 0.0)
        {
            return std::unexpected(BuiltinAssetValidationError{
                .code = BuiltinAssetValidationErrorCode::RequiredFileMissing,
                .path = _root
            });
        }
    }

    return {};
}
}
