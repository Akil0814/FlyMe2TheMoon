#include "texture_manifest_loader.h"

#include "../json/json_loader.h"
#include "../../resources/pipeline/resource_key_builder.h"

#include <utility>

namespace elysia::io
{
std::expected<TextureManifest,ManifestLoadFailure> TextureManifestLoader::load(
    const std::filesystem::path& manifest_path) const
{
    const auto fail = [&manifest_path](ManifestLoadError error,std::string message,
        std::string key = {},std::string pointer = {},
        std::source_location origin = std::source_location::current())
        -> std::expected<TextureManifest,ManifestLoadFailure>
    {
        return std::unexpected(make_manifest_load_failure(
            error,std::move(message),"texture",std::move(key),manifest_path,
            manifest_path,std::move(pointer),origin));
    };
    JsonLoader loader;
    const auto read = loader.open_file(manifest_path);
    if (!read)
        return std::unexpected(manifest_failure_from_json(
            read.error(),"texture-manifest","Load texture manifest failed: "));
    if (!loader.root().is_object())
        return fail(ManifestLoadError::InvalidSchema,
            "Load texture manifest failed: root is not an object.",{},"/");
    if (!loader.root().contains("textures") || !loader.root().at("textures").is_object())
        return fail(ManifestLoadError::MissingField,
            "Load texture manifest failed: textures is missing or not an object.",{},"/textures");
    for (auto field = loader.root().begin(); field != loader.root().end(); ++field)
        if (field.key() != "textures")
            return fail(ManifestLoadError::UnknownField,
                "Load texture manifest failed: unknown root field.",{},
                "/" + json_pointer_escape(field.key()));

    TextureManifest manifest;
    for (auto texture = loader.root().at("textures").begin();
        texture != loader.root().at("textures").end();++texture)
    {
        const std::string pointer = "/textures/" + json_pointer_escape(texture.key());
        if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_key(texture.key());
            !key_result)
            return fail(ManifestLoadError::InvalidResourceKey,
                "Load texture manifest failed: " + key_result.error().message,texture.key(),
                pointer,key_result.error().origin);
        if (!texture.value().is_object())
            return fail(ManifestLoadError::InvalidSchema,
                "Load texture manifest failed: texture entry is not an object.",texture.key(),pointer);
        const json& node = texture.value();
        if (!node.contains("path") || !node.at("path").is_string())
            return fail(ManifestLoadError::MissingField,
                "Load texture manifest failed: path is missing or not a string.",texture.key(),pointer + "/path");
        for (auto field = node.begin();field != node.end();++field)
            if (field.key() != "path")
                return fail(ManifestLoadError::UnknownField,
                    "Load texture manifest failed: unknown field: " + field.key(),texture.key(),
                    pointer + "/" + json_pointer_escape(field.key()));

        TextureManifestEntry entry;
        entry.key = texture.key();
        entry.file_path = node.at("path").get<std::string>();
        if (entry.file_path.empty())
            return fail(ManifestLoadError::InvalidValue,
                "Load texture manifest failed: path must not be empty.",entry.key,
                pointer + "/path");
        entry.origin = elysia::resources::make_resource_origin(
            manifest_path,pointer,{},"textures",{},texture.key());
        manifest.textures.push_back(std::move(entry));
    }
    return manifest;
}
}
