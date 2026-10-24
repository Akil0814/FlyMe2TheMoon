#include "user_config_store.h"

#include "user_config_json_fields.h"
#include "../../io/json/strict_json.h"

#include <chrono>
#include <fstream>
#include <set>

namespace elysia::config
{
namespace
{
using Data = UserConfigData;
using Json = elysia::io::json;
constexpr int kSchemaVersion = 2;

enum class ParseKind { Valid, Invalid, Future, Access };
struct ParseResult
{
    ParseKind kind = ParseKind::Invalid;
    Data data;
    std::string error;
    std::string pointer;
    std::source_location origin = std::source_location::current();
};

core::FailureDiagnostic parse_diagnostic(const ParseResult& parsed,const std::filesystem::path& path)
{
    return core::make_failure_diagnostic(parsed.error,
        {core::make_failure_diagnostic_entry("user-config",{},path,path,parsed.pointer,parsed.error,parsed.origin)},parsed.origin);
}

bool allowed_fields(const Json& object,std::initializer_list<std::string_view> fields,
    std::string_view path,std::string& error,std::string& pointer)
{
    if (!object.is_object()) { error = "UserConfig section must be an object."; pointer = path; return false; }
    std::set<std::string> allowed;
    for (auto field : fields) allowed.emplace(field);
    for (const auto& [key,value] : object.items())
        if (!allowed.contains(key)) { error = "Unknown UserConfig field."; pointer = std::string(path)+"/"+io::json_pointer_escape(key); return false; }
    for (const auto& key : allowed)
        if (!object.contains(key)) { error = "Missing UserConfig field."; pointer = std::string(path)+"/"+io::json_pointer_escape(key); return false; }
    return true;
}

ParseResult parse(const std::filesystem::path& path,const Data& defaults)
{
    const auto loaded = io::load_strict_json(path);
    if (!loaded)
    {
        const auto& error = loaded.error();
        const bool invalid = error.code == io::JsonFileError::ParseFailed || error.code == io::JsonFileError::DuplicateProperty;
        return {invalid ? ParseKind::Invalid : ParseKind::Access,{},error.message,error.json_pointer,error.origin};
    }
    const auto& root = *loaded;
    if (!root.is_object()) return {ParseKind::Invalid,{},"UserConfig root must be an object.",""};
    if (!root.contains("schema_version") || !root.at("schema_version").is_number_integer())
        return {ParseKind::Invalid,{},"UserConfig schema_version must be an integer.","/schema_version"};
    const auto& version_node = root.at("schema_version");
    const bool future = version_node.is_number_unsigned()
        ? version_node.get<std::uint64_t>() > kSchemaVersion : version_node.get<std::int64_t>() > kSchemaVersion;
    if (future) return {ParseKind::Future,{},"UserConfig schema_version is newer than this application.","/schema_version"};
    if (version_node != kSchemaVersion)
        return {ParseKind::Invalid,{},"Unsupported UserConfig schema_version.","/schema_version"};
    Data data = defaults;
    std::string error,pointer;
    if (!allowed_fields(root,{"schema_version","window","render","audio","localization"},"",error,pointer))
        return {ParseKind::Invalid,{},error,pointer};
    const auto& window = root.at("window");
    if (!allowed_fields(window,{"mode","windowed_size"},"/window",error,pointer))
        return {ParseKind::Invalid,{},error,pointer};
    if (!detail::parse_window_mode(window.at("mode"),data.window.mode,error))
        return {ParseKind::Invalid,{},error,"/window/mode"};
    const auto& size = window.at("windowed_size");
    if (!allowed_fields(size,{"width","height"},"/window/windowed_size",error,pointer))
        return {ParseKind::Invalid,{},error,pointer};
    if (!detail::parse_positive_int(size,"width",data.window.windowed_size.width,error))
        return {ParseKind::Invalid,{},error,"/window/windowed_size/width"};
    if (!detail::parse_positive_int(size,"height",data.window.windowed_size.height,error))
        return {ParseKind::Invalid,{},error,"/window/windowed_size/height"};
    const auto& render = root.at("render");
    if (!allowed_fields(render,{"fps","vsync"},"/render",error,pointer))
        return {ParseKind::Invalid,{},error,pointer};
    if (!detail::parse_positive_number(render,"fps",data.target_fps,error))
        return {ParseKind::Invalid,{},error,"/render/fps"};
    if (!detail::parse_boolean(render,"vsync",data.vsync,error))
        return {ParseKind::Invalid,{},error,"/render/vsync"};
    const auto& audio = root.at("audio");
    if (!allowed_fields(audio,{"master_volume","music_volume","sound_volume"},"/audio",error,pointer))
        return {ParseKind::Invalid,{},error,pointer};
    if (!detail::parse_volume(audio,"master_volume",data.audio.master_volume,error))
        return {ParseKind::Invalid,{},error,"/audio/master_volume"};
    if (!detail::parse_volume(audio,"music_volume",data.audio.music_volume,error))
        return {ParseKind::Invalid,{},error,"/audio/music_volume"};
    if (!detail::parse_volume(audio,"sound_volume",data.audio.sound_volume,error))
        return {ParseKind::Invalid,{},error,"/audio/sound_volume"};
    const auto& localization = root.at("localization");
    if (!allowed_fields(localization,{"language"},"/localization",error,pointer))
        return {ParseKind::Invalid,{},error,pointer};
    if (!detail::parse_non_empty_string(localization,"language",data.language,error))
        return {ParseKind::Invalid,{},error,"/localization/language"};
    return {ParseKind::Valid,std::move(data),{}, {}};
}

Json serialize(const Data& data)
{
    const char* mode = "invalid";
    switch (data.window.mode)
    {
    case WindowMode::Windowed:
        mode = "windowed";
        break;
    case WindowMode::BorderlessFullscreen:
        mode = "borderless_fullscreen";
        break;
    }
    return {{"schema_version",kSchemaVersion},
        {"window",{
            {"mode",mode},
            {"windowed_size",{
                {"width",data.window.windowed_size.width},
                {"height",data.window.windowed_size.height}
            }}
        }},
        {"render",{{"fps",data.target_fps},{"vsync",data.vsync}}},
        {"audio",{{"master_volume",data.audio.master_volume},{"music_volume",data.audio.music_volume},{"sound_volume",data.audio.sound_volume}}},
        {"localization",{{"language",data.language}}}};
}

UserConfigFailure failure(
    UserConfigError kind,std::string message,
    std::filesystem::path path = {},std::string pointer = {},
    std::source_location origin = std::source_location::current())
{
    const std::string reason = message;
    auto diagnostic = core::make_failure_diagnostic(message,
        {core::make_failure_diagnostic_entry("user-config",{},path,path,std::move(pointer),reason,origin)},origin);
    return make_user_config_failure(kind,{},std::move(diagnostic));
}
}

std::expected<void,UserConfigFailure> UserConfigStore::save(const std::filesystem::path& path,const Data& data) const
{
    if (path.empty()) return std::unexpected(failure(UserConfigError::SaveFailed,"UserConfig path must not be empty.",path));
    const auto content = serialize(data).dump(2) + "\n";
    const auto saved = io::detail::replace_json_file(path,content,_operations,
        [&](const std::filesystem::path& temporary) -> std::expected<void,core::FailureDiagnostic>
        {
            const auto parsed = parse(temporary,data);
            if (parsed.kind != ParseKind::Valid) return std::unexpected(parse_diagnostic(parsed,temporary));
            return {};
        },[&](const std::filesystem::path& path) -> std::expected<bool,core::FailureDiagnostic>
        {
            const auto parsed = parse(path,data);
            if (parsed.kind == ParseKind::Access || parsed.kind == ParseKind::Future)
            {
                return std::unexpected(parse_diagnostic(parsed,path));
            }
            return parsed.kind == ParseKind::Valid;
        });
    if (saved) return {};
    return std::unexpected(make_user_config_failure(UserConfigError::SaveFailed,{},saved.error().diagnostic,saved.error().context));
}

std::expected<UserConfigLoadResult,UserConfigFailure> UserConfigStore::load(
    const std::filesystem::path& path,const Data& defaults) const
{
    if (path.empty()) return std::unexpected(failure(UserConfigError::LoadFailed,"UserConfig path must not be empty.",path));
    UserConfigLoadResult result;
    const std::filesystem::path tmp = path.string() + ".tmp";
    const std::filesystem::path bak = path.string() + ".bak";
    const auto failed = [&](core::FailureDiagnostic diagnostic) -> std::expected<UserConfigLoadResult,UserConfigFailure>
    {
        if (result.warning)
        {
            auto original = result.warning->diagnostic;
            io::append_failure_context(original,diagnostic);
            diagnostic = std::move(original);
        }
        io::detail::PersistenceFailure failure{std::move(diagnostic),
            {.stage = "load",.primary = {path},.temporary = {tmp},.backup = {bak}}};
        io::detail::observe_files(_operations,failure);
        return std::unexpected(make_user_config_failure(UserConfigError::LoadFailed,{},std::move(failure.diagnostic),std::move(failure.context)));
    };
    const auto remember = [&](const ParseResult& parsed,const std::filesystem::path& candidate)
    {
        auto diagnostic = parse_diagnostic(parsed,candidate);
        if (!result.warning) result.warning = make_user_config_failure(UserConfigError::LoadFailed,{},std::move(diagnostic));
        else io::append_failure_context(result.warning->diagnostic,diagnostic);
    };
    const auto primary_exists = io::detail::file_exists(_operations,path);
    if (!primary_exists) return failed(primary_exists.error());
    if (*primary_exists)
    {
        if (auto error = io::detail::before_operation(_operations,"read",path))
            return failed(io::detail::operation_failure("read",path,error));
        const auto primary = parse(path,defaults);
        if (primary.kind == ParseKind::Future || primary.kind == ParseKind::Access)
            return failed(parse_diagnostic(primary,path));
        if (primary.kind == ParseKind::Valid) { result.settings = primary.data; return result; }
        remember(primary,path);
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::filesystem::path corrupt = path.string()+"."+std::to_string(stamp)+".corrupt";
        for (unsigned suffix = 1;; ++suffix)
        {
            const auto exists = io::detail::file_exists(_operations,corrupt);
            if (!exists) return failed(exists.error());
            if (!*exists) break;
            corrupt = path.string()+"."+std::to_string(stamp)+"."+std::to_string(suffix)+".corrupt";
        }
        if (auto archived = io::detail::rename_file(_operations,path,corrupt,"archive-corrupt"); !archived)
            return failed(archived.error());
        result.warning->diagnostic.entries.push_back(core::make_failure_diagnostic_entry(
            "user-config-archive",{},corrupt,path,{},"Corrupt primary archived."));
    }
    for (const auto& candidate : {tmp,bak})
    {
        const auto exists = io::detail::file_exists(_operations,candidate);
        if (!exists) return failed(exists.error());
        if (!*exists) continue;
        if (auto error = io::detail::before_operation(_operations,"read",candidate))
            return failed(io::detail::operation_failure("read",candidate,error));
        const auto recovered = parse(candidate,defaults);
        if (recovered.kind == ParseKind::Future || recovered.kind == ParseKind::Access)
            return failed(parse_diagnostic(recovered,candidate));
        if (recovered.kind != ParseKind::Valid) { remember(recovered,candidate); continue; }
        result.settings = recovered.data;
        result.recovered = true;
        if (!result.warning) result.warning = failure(UserConfigError::LoadFailed,"Primary missing; recovery copy selected.",path);
        result.warning->diagnostic.entries.push_back(core::make_failure_diagnostic_entry(
            "user-config-recovery",{},path,candidate,{},"Valid recovery copy selected."));
        if (candidate == tmp)
        {
            if (auto promoted = io::detail::rename_file(_operations,tmp,path,"promote-temporary"); !promoted)
                return failed(promoted.error());
        }
        else if (auto saved = save(path,result.settings); !saved)
        {
            auto error = saved.error();
            auto diagnostic = result.warning->diagnostic;
            io::append_failure_context(diagnostic,error.diagnostic);
            error.diagnostic = std::move(diagnostic);
            return std::unexpected(std::move(error));
        }
        return result;
    }
    result.settings = defaults;
    result.rebuilt = true;
    auto rebuild = core::make_failure_diagnostic("No valid recovery copy; defaults selected for rebuild.",
        {core::make_failure_diagnostic_entry("user-config-rebuild",{},path,{},{},"Defaults selected for rebuild.")});
    // A first run with no files is normal success, not a LoadFailed warning.
    if (result.warning) io::append_failure_context(result.warning->diagnostic,rebuild);
    if (auto saved = save(path,result.settings); !saved)
    {
        auto error = saved.error();
        if (result.warning)
        {
            auto diagnostic = result.warning->diagnostic;
            io::append_failure_context(diagnostic,error.diagnostic);
            error.diagnostic = std::move(diagnostic);
        }
        else io::append_failure_context(error.diagnostic,rebuild);
        return std::unexpected(std::move(error));
    }
    return result;
}
}
