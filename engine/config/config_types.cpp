#include "config_types.h"

namespace elysia::config
{
std::string ConfigOrigin::describe() const
{
    return config_path + "#" + json_pointer + " namespace=" + key_namespace + " key=" + full_key;
}

elysia::core::FailureDiagnostic ConfigLoadFailure::diagnostic() const
{
    std::vector<elysia::core::FailureDiagnosticEntry> entries;
    const auto append = [&](const ConfigOrigin& value,std::string reason)
    {
        if (value.config_path.empty() && value.json_pointer.empty()
            && value.full_key.empty()) return;
        entries.push_back(elysia::core::make_failure_diagnostic_entry(
            "config",value.full_key,{},value.config_path,value.json_pointer,
            std::move(reason),origin));
    };
    append(first,message);
    append(second,"Conflicting declaration.");
    return elysia::core::make_failure_diagnostic(message,std::move(entries),origin);
}
}
