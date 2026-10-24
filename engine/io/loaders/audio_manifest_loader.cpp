#include "audio_manifest_loader.h"

#include "../json/json_loader.h"
#include "../../resources/pipeline/resource_key_builder.h"

#include <utility>

namespace elysia::io
{
namespace
{
std::expected<std::vector<AudioManifestEntry>,ManifestLoadFailure> load_group(
    const json& node,std::string group,const std::filesystem::path& manifest_path)
{
    const auto fail = [&](ManifestLoadError error,std::string message,
        std::string key = {},std::string pointer = {},
        std::source_location origin = std::source_location::current())
        -> std::expected<std::vector<AudioManifestEntry>,ManifestLoadFailure>
    {
        return std::unexpected(make_manifest_load_failure(
            error,std::move(message),group == "sounds" ? "sound" : "music",
            std::move(key),manifest_path,manifest_path,std::move(pointer),origin));
    };
    if (!node.is_object())
        return fail(ManifestLoadError::InvalidSchema,
            "Load audio manifest failed: " + group + " is not an object.",{},"/" + group);
    std::vector<AudioManifestEntry> entries;
    for (auto item = node.begin();item != node.end();++item)
    {
        const std::string pointer = "/" + group + "/" + json_pointer_escape(item.key());
        if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_key(item.key());
            !key_result)
            return fail(ManifestLoadError::InvalidResourceKey,
                "Load audio manifest failed: " + key_result.error().message,item.key(),
                pointer,key_result.error().origin);
        if (!item.value().is_object())
            return fail(ManifestLoadError::InvalidSchema,
                "Load audio manifest failed: entry is not an object.",item.key(),pointer);
        const json& entry_node = item.value();
        if (!entry_node.contains("path") || !entry_node.at("path").is_string())
            return fail(ManifestLoadError::MissingField,
                "Load audio manifest failed: path is missing or not a string.",item.key(),pointer + "/path");
        for (auto field = entry_node.begin();field != entry_node.end();++field)
            if (field.key() != "path")
                return fail(ManifestLoadError::UnknownField,
                    "Load audio manifest failed: unknown field: " + field.key(),item.key(),
                    pointer + "/" + json_pointer_escape(field.key()));
        AudioManifestEntry entry;
        entry.key = item.key();
        entry.file_path = entry_node.at("path").get<std::string>();
        if (entry.file_path.empty())
            return fail(ManifestLoadError::InvalidValue,
                "Load audio manifest failed: path must not be empty.",item.key(),
                pointer + "/path");
        entry.origin = elysia::resources::make_resource_origin(
            manifest_path,pointer,{},group,{},item.key());
        entries.push_back(std::move(entry));
    }
    return entries;
}
}

std::expected<AudioManifest,ManifestLoadFailure> AudioManifestLoader::load(
    const std::filesystem::path& manifest_path) const
{
    const auto fail = [&manifest_path](ManifestLoadError error,std::string message,
        std::string pointer = {},
        std::source_location origin = std::source_location::current())
        -> std::expected<AudioManifest,ManifestLoadFailure>
    {
        return std::unexpected(make_manifest_load_failure(
            error,std::move(message),"audio-manifest",{},manifest_path,manifest_path,
            std::move(pointer),origin));
    };
    JsonLoader loader;
    const auto read = loader.open_file(manifest_path);
    if (!read)
        return std::unexpected(manifest_failure_from_json(
            read.error(),"audio-manifest","Load audio manifest failed: "));
    if (!loader.root().is_object())
        return fail(ManifestLoadError::InvalidSchema,
            "Load audio manifest failed: root is not an object.","/");
    for (auto field = loader.root().begin(); field != loader.root().end(); ++field)
        if (field.key() != "sounds" && field.key() != "music")
            return fail(ManifestLoadError::UnknownField,
                "Load audio manifest failed: unknown root field.",
                "/" + json_pointer_escape(field.key()));
    if (!loader.root().contains("sounds") || !loader.root().contains("music"))
        return fail(ManifestLoadError::MissingField,
            "Load audio manifest failed: sounds or music is missing.","/");
    auto sounds = load_group(loader.root().at("sounds"),"sounds",manifest_path);
    if (!sounds) return std::unexpected(std::move(sounds.error()));
    auto music = load_group(loader.root().at("music"),"music",manifest_path);
    if (!music) return std::unexpected(std::move(music.error()));
    return AudioManifest{ .sounds = std::move(*sounds),.music = std::move(*music) };
}
}
