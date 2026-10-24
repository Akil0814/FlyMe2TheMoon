#include "config_snapshot_builder.h"

#include "config_load_utils.h"
#include "config_snapshot_internal.h"
#include "../../core/validation/dotted_key_validator.h"

namespace elysia::config
{
namespace
{
std::expected<void,ConfigLoadFailure> index_value(const elysia::io::json& value,
    const std::string& key,const std::string& pointer,const ConfigDocument& document,
    ConfigSnapshot::Impl& snapshot)
{
    ConfigOrigin origin{document.origin.config_path,pointer,document.key_namespace,key};
    if (value.is_null())
        return std::unexpected(make_config_load_failure(ConfigLoadError::InvalidValue,
            "JSON null is not allowed: " + key,origin));
    if (const auto existing = snapshot.nodes.find(key); existing != snapshot.nodes.end())
        return std::unexpected(make_config_load_failure(ConfigLoadError::DuplicateKey,
            "Duplicate config key: " + key,existing->second.origin,origin));
    snapshot.nodes.emplace(key,ConfigSnapshotNode{value,origin});
    if (value.is_object())
    {
        for (const auto& [component,child] : value.items())
        {
            const std::string child_pointer = pointer + "/" + config_pointer_component(component);
            if (auto key_result = elysia::core::DottedKeyValidator::validate_component(component);
                !key_result)
                return std::unexpected(make_config_load_failure(ConfigLoadError::InvalidKey,
                    key_result.error().message,
                    ConfigOrigin{document.origin.config_path,child_pointer,document.key_namespace,key+"."+component}));
            auto indexed = index_value(child,key+"."+component,child_pointer,document,snapshot);
            if (!indexed) return indexed;
        }
    }
    else if (value.is_array())
    {
        for (size_t index = 0; index < value.size(); ++index)
        {
            const std::string component = std::to_string(index);
            auto indexed = index_value(value[index],key+"."+component,pointer+"/"+component,document,snapshot);
            if (!indexed) return indexed;
        }
    }
    return {};
}
}

std::expected<std::shared_ptr<const ConfigSnapshot>,ConfigLoadFailure> ConfigSnapshotBuilder::build(
    const std::vector<ConfigDocument>& documents) const
{
    auto implementation = std::make_shared<ConfigSnapshot::Impl>();
    for (const auto& document : documents)
    {
        auto indexed = index_value(document.root,document.key_namespace,"",document,*implementation);
        if (!indexed) return std::unexpected(indexed.error());
    }
    return std::shared_ptr<const ConfigSnapshot>(new ConfigSnapshot(std::move(implementation)));
}
}
