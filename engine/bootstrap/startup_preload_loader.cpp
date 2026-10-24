#include "startup_preload_loader.h"

#include "../builtin/resources/builtin_resource_ids.h"
#include "../resources/texture/surface_loader.h"
#include "../resources/texture/texture_loader.h"
#include "../io/path/path_manager.h"
#include "../resources/pipeline/resource_key_builder.h"

#include <unordered_set>
#include <utility>

namespace elysia::bootstrap
{
namespace
{
BootstrapFailure preload_failure(
    std::string message,std::string type = {},std::string key = {},
    std::filesystem::path expected_path = {},
    std::filesystem::path declaration_path = {},std::string pointer = {},
    std::source_location origin = std::source_location::current())
{
    if (type.empty() && key.empty() && expected_path.empty()
        && declaration_path.empty() && pointer.empty())
        return BootstrapFailure{BootstrapFailure::Code::Preload,std::move(message),origin};
    const std::string reason = message;
    return BootstrapFailure{
        BootstrapFailure::Code::Preload,
        elysia::core::make_failure_diagnostic(
            std::move(message),
            {elysia::core::make_failure_diagnostic_entry(
                std::move(type),std::move(key),std::move(expected_path),
                std::move(declaration_path),std::move(pointer),reason,origin)},origin)};
}
}

void StartupPreloadLoader::set_manifest_path(const std::filesystem::path& preload_manifest_path)
{
    reset();
    _manifest_path = preload_manifest_path;
}

void StartupPreloadLoader::reset()
{
    release_textures();
    _manifest_path.clear();
    _manifest_loader.reset();
    _project_textures.clear();
}

void StartupPreloadLoader::release_textures() noexcept
{
    _texture_cache.clear();
    _renderer = nullptr;
    _is_loaded = false;
}

std::expected<void,BootstrapFailure>
StartupPreloadLoader::load(SDL_Renderer* renderer)
{
    if (_is_loaded && renderer == _renderer)
        return {};

    if (!renderer)
        return std::unexpected(preload_failure(
            "Bootstrapper phase2 failed: renderer is null.","renderer"));

    if (_manifest_path.empty())
        return std::unexpected(preload_failure(
            "Bootstrapper phase2 failed: preload manifest path is not prepared.",
            "preload-manifest"));

    if (auto manifest_result = load_manifest(); !manifest_result)
        return manifest_result;

    BootstrapTextureCache prepared_cache;
    if (auto texture_result = load_textures(renderer,prepared_cache);
        !texture_result)
        return texture_result;

    _texture_cache = std::move(prepared_cache);
    _renderer = renderer;
    _is_loaded = true;
    return {};
}

SDL_Texture* StartupPreloadLoader::find_texture(
    std::string_view key) const noexcept
{
    return _texture_cache.find(key);
}

std::expected<void,BootstrapFailure> StartupPreloadLoader::load_manifest()
{
    _project_textures.clear();
    const auto result = _manifest_loader.open_file(_manifest_path);
    if (!result)
        return std::unexpected(preload_failure(
            "Load preload manifest failed: " + result.error().message,
            "preload-manifest",result.error().duplicate_property,
            _manifest_path,_manifest_path,result.error().json_pointer,
            result.error().origin));

    const elysia::io::json& root = _manifest_loader.root();
    if (!root.is_object() || root.size() != 1 || !root.contains("textures")
        || !root.at("textures").is_array())
        return std::unexpected(preload_failure(
            "Load preload manifest failed: root must contain only a textures array.",
            "preload-manifest",{},_manifest_path,_manifest_path,"/textures"));

    std::unordered_set<std::string> keys;
    std::vector<TextureEntry> parsed_entries;
    const elysia::io::json& textures = root.at("textures");
    parsed_entries.reserve(textures.size());

    std::size_t texture_index = 0;
    for (const elysia::io::json& node : textures)
    {
        const std::string pointer = "/textures/" + std::to_string(texture_index++);
        if (!node.is_object() || node.size() != 2
            || !node.contains("key") || !node.at("key").is_string()
            || !node.contains("file") || !node.at("file").is_string())
            return std::unexpected(preload_failure(
                "Load preload manifest failed: every texture must contain "
                "string key and file fields.","preload-texture",{},
                _manifest_path,_manifest_path,pointer));

        TextureEntry entry;
        entry.key = node.at("key").get<std::string>();
        entry.file = node.at("file").get<std::string>();

        if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_key(entry.key);
            !key_result)
            return std::unexpected(preload_failure(
                "Load preload manifest failed: invalid texture key: "
                    + key_result.error().message,
                "preload-texture",entry.key,_manifest_path,_manifest_path,
                pointer + "/key",key_result.error().origin));
        if (entry.key == elysia::builtin::builtin_resource_name(
                elysia::builtin::BuiltinTextureId::ElysiaWhite))
            return std::unexpected(preload_failure(
                "Load preload manifest failed: project texture uses the "
                "engine-reserved key: " + entry.key,"preload-texture",entry.key,
                _manifest_path,_manifest_path,pointer + "/key"));
        if (!keys.insert(entry.key).second)
            return std::unexpected(preload_failure(
                "Load preload manifest failed: duplicate texture key: " + entry.key,
                "preload-texture",entry.key,_manifest_path,_manifest_path,pointer + "/key"));
        if (entry.file.empty() || entry.file.is_absolute())
            return std::unexpected(preload_failure(
                "Load preload manifest failed: texture file must be a non-empty "
                "relative path: " + entry.key,"preload-texture",entry.key,
                entry.file,_manifest_path,pointer + "/file"));

        entry.file = entry.file.lexically_normal();
        for (const std::filesystem::path& component : entry.file)
        {
            if (component == "..")
                return std::unexpected(preload_failure(
                    "Load preload manifest failed: texture file escapes the "
                    "preload root: " + entry.key,"preload-texture",entry.key,
                    entry.file,_manifest_path,pointer + "/file"));
        }

        entry.origin = elysia::resources::make_resource_origin(
            _manifest_path,pointer,{},"preload",{},entry.key);

        parsed_entries.push_back(std::move(entry));
    }

    _project_textures = std::move(parsed_entries);
    return {};
}

std::expected<void,BootstrapFailure> StartupPreloadLoader::load_textures(
    SDL_Renderer* renderer,
    BootstrapTextureCache& destination
)
{
    for (const TextureEntry& entry : _project_textures)
    {
        const std::filesystem::path file =
            elysia::io::PathManager::instance()->preload() / entry.file;
        if (auto result = load_texture(
                renderer,
                entry.key,
                file,
                entry.origin,
                destination);
            !result)
            return result;
    }

    return {};
}

std::expected<void,BootstrapFailure> StartupPreloadLoader::load_texture(
    SDL_Renderer* renderer,
    std::string_view key,
    const std::filesystem::path& file,
    const elysia::resources::ResourceOrigin& origin,
    BootstrapTextureCache& destination)
{
    elysia::resources::SurfaceLoadRequest surface_request;
    surface_request._asset_key = std::string(key);
    surface_request._subject_type = "preload-texture";
    surface_request._frame_path = file;
    surface_request._frame_index = 0;
    surface_request._origin = origin;

    elysia::resources::SurfaceLoader surface_loader;
    auto surface_result =
        surface_loader.load_surface(surface_request);
    if (!surface_result)
        return std::unexpected(BootstrapFailure{
            BootstrapFailure::Code::Preload,
            std::move(surface_result.error().diagnostic)
        });

    elysia::resources::TextureLoader texture_loader;
    auto texture_result = texture_loader.load_texture(renderer,*surface_result);
    if (!texture_result)
        return std::unexpected(BootstrapFailure{
            BootstrapFailure::Code::Preload,
            std::move(texture_result.error().diagnostic)
        });

    if (!destination.store(std::string(key),std::move(texture_result->_texture)))
        return std::unexpected(preload_failure(
            "Load preload texture failed: duplicate or invalid cache key: "
                + std::string(key),"preload-texture",std::string(key),file,
            origin.config_path,origin.json_pointer));

    return {};
}

}
