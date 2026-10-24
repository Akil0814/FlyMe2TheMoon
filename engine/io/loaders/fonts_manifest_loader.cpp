#include "fonts_manifest_loader.h"

#include "../json/json_loader.h"
#include "../../resources/pipeline/resource_key_builder.h"

#include <utility>

namespace elysia::io
{
std::expected<FontManifest,ManifestLoadFailure> FontsManifestLoader::load(
    const std::filesystem::path& manifest_path) const
{
    const auto fail = [&manifest_path](ManifestLoadError error,std::string message,
        std::string key = {},std::string pointer = {},
        std::source_location origin = std::source_location::current())
        -> std::expected<FontManifest,ManifestLoadFailure>
    {
        return std::unexpected(make_manifest_load_failure(
            error,std::move(message),"font-manifest",std::move(key),manifest_path,
            manifest_path,std::move(pointer),origin));
    };
    JsonLoader loader;
    const auto read = loader.open_file(manifest_path);
    if (!read)
        return std::unexpected(manifest_failure_from_json(
            read.error(),"font-manifest","Load fonts manifest failed: "));
    if (!loader.root().is_object())
        return fail(ManifestLoadError::InvalidSchema,
            "Load fonts manifest failed: root is not an object.",{},"/");
    if (!loader.root().contains("fonts") || !loader.root().at("fonts").is_array())
        return fail(ManifestLoadError::MissingField,
            "Load fonts manifest failed: fonts is missing or not an array.",{},"/fonts");
    for (auto field = loader.root().begin();field != loader.root().end();++field)
        if (field.key() != "fonts")
            return fail(ManifestLoadError::UnknownField,
                "Load fonts manifest failed: unknown root field: " + field.key(),
                {},"/" + json_pointer_escape(field.key()));

    FontManifest manifest;
    std::size_t index = 0;
    for (const json& node : loader.root().at("fonts"))
    {
        const std::string pointer = "/fonts/" + std::to_string(index++);
        if (!node.is_object())
            return fail(ManifestLoadError::InvalidSchema,
                "Load fonts manifest failed: font entry is not an object.",{},pointer);
        if (!node.contains("key") || !node.at("key").is_string()
            || !node.contains("file") || !node.at("file").is_string())
            return fail(ManifestLoadError::MissingField,
                "Load fonts manifest failed: key or file is missing or invalid.",{},pointer);
        FontManifestEntry entry;
        entry.key = node.at("key").get<std::string>();
        entry.file_path = node.at("file").get<std::string>();
        if (entry.key.empty() || entry.file_path.empty())
            return fail(ManifestLoadError::InvalidValue,
                "Load fonts manifest failed: font key and file must not be empty.",entry.key,pointer);
        for (auto field = node.begin();field != node.end();++field)
            if (field.key() != "key" && field.key() != "file")
                return fail(ManifestLoadError::UnknownField,
                    "Load fonts manifest failed: unknown field: " + field.key(),entry.key,
                    pointer + "/" + json_pointer_escape(field.key()));
        if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_key(entry.key);
            !key_result)
            return fail(ManifestLoadError::InvalidResourceKey,
                "Load fonts manifest failed: " + key_result.error().message,entry.key,
                pointer + "/key",key_result.error().origin);
        entry.origin = elysia::resources::make_resource_origin(
            manifest_path,pointer,{},"fonts",{},entry.key);
        manifest.fonts.push_back(std::move(entry));
    }
    if (manifest.fonts.empty())
        return fail(ManifestLoadError::MissingContent,
            "Load fonts manifest failed: fonts must not be empty.",{},"/fonts");
    return manifest;
}
}
