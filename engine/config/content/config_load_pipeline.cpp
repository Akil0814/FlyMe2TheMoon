#include "config_load_pipeline.h"

#include "config_document_loader.h"
#include "config_manifest_loader.h"
#include "config_snapshot_builder.h"

namespace elysia::config
{
std::expected<std::shared_ptr<const ConfigSnapshot>,ConfigLoadFailure> ConfigLoadPipeline::load(
    const std::filesystem::path& manifest_path) const
{
    const auto manifest = ConfigManifestLoader{}.load(manifest_path);
    if (!manifest) return std::unexpected(manifest.error());
    std::vector<ConfigDocument> documents;
    documents.reserve(manifest->entries.size());
    ConfigDocumentLoader document_loader;
    for (const auto& entry : manifest->entries)
    {
        auto document = document_loader.load(entry);
        if (!document) return std::unexpected(document.error());
        documents.push_back(std::move(*document));
    }
    return ConfigSnapshotBuilder{}.build(documents);
}
}
