#include "save_store.h"

#include "../../io/json/strict_json.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <utility>

namespace elysia::save::detail
{
namespace
{
using Json = elysia::io::json;
constexpr std::int64_t kFormatVersion = 1;

enum class ParseKind
{
    Valid,
    Invalid,
    Future,
    Access
};

struct ParseResult
{
    ParseKind kind = ParseKind::Invalid;
    SaveData data;
    std::string error;
    std::string pointer;
    std::source_location origin = std::source_location::current();
};

core::FailureDiagnostic parse_diagnostic(const ParseResult& parsed,const std::filesystem::path& path)
{
    return core::make_failure_diagnostic(parsed.error,
        {core::make_failure_diagnostic_entry("save",{},path,path,parsed.pointer,parsed.error,parsed.origin)},parsed.origin);
}

SaveFailure failure(
    SaveError error,
    std::string_view save_name,
    std::string message,
    std::string key = {},
    std::source_location origin = std::source_location::current())
{
    return make_save_failure(error,std::string(save_name),std::move(key),std::move(message),origin);
}

bool has_exact_fields(
    const Json& object,
    std::initializer_list<std::string_view> expected)
{
    if (!object.is_object() || object.size() != expected.size()) return false;
    return std::ranges::all_of(expected,[&object](std::string_view field)
    {
        return object.contains(field);
    });
}

std::expected<std::int64_t,std::string> read_int64(const Json& value)
{
    if (!value.is_number_integer())
        return std::unexpected("value must be an int64");
    if (value.is_number_unsigned())
    {
        const auto number = value.get<std::uint64_t>();
        if (number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            return std::unexpected("integer is outside the int64 range");
        return static_cast<std::int64_t>(number);
    }
    return value.get<std::int64_t>();
}

template<typename Value,typename Reader>
std::expected<std::vector<Value>,std::string> read_array(
    const Json& value,
    Reader&& reader,std::string& pointer_suffix)
{
    if (!value.is_array()) return std::unexpected("value must be an array");
    std::vector<Value> result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        auto parsed = reader(value[index]);
        if (!parsed) { pointer_suffix = "/"+std::to_string(index); return std::unexpected(parsed.error()); }
        result.push_back(std::move(*parsed));
    }
    return result;
}

std::expected<bool,std::string> read_bool(const Json& value)
{
    if (!value.is_boolean()) return std::unexpected("value must be a bool");
    return value.get<bool>();
}

std::expected<double,std::string> read_double(const Json& value)
{
    if (!value.is_number_float()) return std::unexpected("value must be a double");
    const double number = value.get<double>();
    if (!std::isfinite(number)) return std::unexpected("double must be finite");
    return number;
}

std::expected<std::string,std::string> read_string(const Json& value)
{
    if (!value.is_string()) return std::unexpected("value must be a string");
    return value.get<std::string>();
}

std::expected<bool,std::string> add_typed_value(
    SaveData& data,
    const std::string& key,
    std::string_view type,
    const Json& value,std::string& pointer_suffix)
{
    auto store = [&data,&key](auto parsed) -> std::expected<bool,std::string>
    {
        if (!parsed) return std::unexpected(parsed.error());
        auto stored = data.set(key,std::move(*parsed));
        if (!stored) return std::unexpected(stored.error().diagnostic.message);
        return *stored;
    };

    if (type == "bool") return store(read_bool(value));
    if (type == "int64") return store(read_int64(value));
    if (type == "double") return store(read_double(value));
    if (type == "string") return store(read_string(value));
    if (type == "bool_array")
        return store(read_array<bool>(value,read_bool,pointer_suffix));
    if (type == "int64_array")
        return store(read_array<std::int64_t>(value,read_int64,pointer_suffix));
    if (type == "double_array")
        return store(read_array<double>(value,read_double,pointer_suffix));
    if (type == "string_array")
        return store(read_array<std::string>(value,read_string,pointer_suffix));
    return std::unexpected("unknown SaveData type tag: " + std::string(type));
}

ParseResult parse_document(const std::filesystem::path& path)
{
    const auto loaded = elysia::io::load_strict_json(path);
    if (!loaded)
    {
        const auto& error = loaded.error();
        const bool invalid = error.code == io::JsonFileError::ParseFailed
            || error.code == io::JsonFileError::DuplicateProperty;
        return {invalid ? ParseKind::Invalid : ParseKind::Access,{},error.message,error.json_pointer,error.origin};
    }
    const Json& root = *loaded;
    if (!root.is_object() || !root.contains("format_version")
        || !root.at("format_version").is_number_integer())
        return {ParseKind::Invalid,{},"Save format_version must be an integer.","/format_version"};

    if (root.at("format_version").is_number_unsigned()
        && root.at("format_version").get<std::uint64_t>() > kFormatVersion)
        return {ParseKind::Future,{},"Save format_version is newer than this engine.","/format_version"};
    const auto version = read_int64(root.at("format_version"));
    if (!version)
        return {ParseKind::Invalid,{},"Save format_version " + version.error() + ".","/format_version"};
    if (*version > kFormatVersion)
        return {ParseKind::Future,{},"Save format_version is newer than this engine.","/format_version"};
    if (*version != kFormatVersion)
        return {ParseKind::Invalid,{},"Unsupported Save format_version.","/format_version"};

    if (!has_exact_fields(root,{"format_version","types","values"}))
        return {ParseKind::Invalid,{},"Save root must contain exactly format_version, types, and values."};
    const Json& types = root.at("types");
    const Json& values = root.at("values");
    if (!types.is_object() || !values.is_object())
        return {ParseKind::Invalid,{},"Save types and values must be objects.",!types.is_object() ? "/types" : "/values"};
    if (types.size() != values.size())
        return {ParseKind::Invalid,{},"Save types and values must contain the same keys."};

    SaveData data;
    for (const auto& [key,type_node] : types.items())
    {
        if (key.empty())
            return {ParseKind::Invalid,{},"SaveData keys must not be empty."};
        if (!values.contains(key))
            return {ParseKind::Invalid,{},"Save types and values must contain the same keys."};
        if (!type_node.is_string())
            return {ParseKind::Invalid,{},"Save type tag for '" + key + "' must be a string.","/types/" + io::json_pointer_escape(key)};
        const auto type = type_node.get<std::string>();
        if (type != "bool" && type != "int64" && type != "double" && type != "string"
            && type != "bool_array" && type != "int64_array" && type != "double_array" && type != "string_array")
            return {ParseKind::Invalid,{},"Unknown SaveData type tag.","/types/"+io::json_pointer_escape(key)};
        std::string pointer_suffix;
        const auto added = add_typed_value(
            data,
            key,
            type,
            values.at(key),pointer_suffix);
        if (!added)
            return {ParseKind::Invalid,{},"Invalid SaveData value '" + key + "': " + added.error(),"/values/" + io::json_pointer_escape(key)+pointer_suffix};
    }

    return {ParseKind::Valid,std::move(data),{}};
}

std::string_view type_tag(const SaveValue& value)
{
    return std::visit([](const auto& stored) -> std::string_view
    {
        using Value = std::remove_cvref_t<decltype(stored)>;
        if constexpr (std::same_as<Value,bool>) return "bool";
        else if constexpr (std::same_as<Value,std::int64_t>) return "int64";
        else if constexpr (std::same_as<Value,double>) return "double";
        else if constexpr (std::same_as<Value,std::string>) return "string";
        else if constexpr (std::same_as<Value,std::vector<bool>>) return "bool_array";
        else if constexpr (std::same_as<Value,std::vector<std::int64_t>>) return "int64_array";
        else if constexpr (std::same_as<Value,std::vector<double>>) return "double_array";
        else return "string_array";
    },value);
}

Json serialize_document(const SaveData& data)
{
    Json types = Json::object();
    Json values = Json::object();
    for (const auto& [key,value] : data.entries())
    {
        types[key] = type_tag(value);
        std::visit([&values,&key](const auto& stored)
        {
            values[key] = stored;
        },value);
    }
    return Json{
        {"format_version",kFormatVersion},
        {"types",std::move(types)},
        {"values",std::move(values)}
    };
}

std::expected<std::filesystem::path,core::FailureDiagnostic> make_corrupt_path(
    const std::filesystem::path& primary,const io::detail::PersistenceOperations& operations)
{
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::filesystem::path candidate = primary.string() + "."
        + std::to_string(stamp) + ".corrupt";
    int suffix = 1;
    for (;;)
    {
        const auto exists = io::detail::file_exists(operations,candidate);
        if (!exists) return std::unexpected(exists.error());
        if (!*exists) break;
        candidate = primary.string() + "." + std::to_string(stamp)
            + "." + std::to_string(suffix++) + ".corrupt";
    }
    return candidate;
}

bool is_portable_save_character(unsigned char character)
{
    return (character >= 'a' && character <= 'z')
        || (character >= 'A' && character <= 'Z')
        || (character >= '0' && character <= '9')
        || character == '_'
        || character == '-';
}

bool is_windows_reserved_name(std::string_view name)
{
    std::string uppercase(name);
    std::ranges::transform(uppercase,uppercase.begin(),[](unsigned char character)
    {
        return static_cast<char>(std::toupper(character));
    });
    if (uppercase == "CON" || uppercase == "PRN" || uppercase == "AUX"
        || uppercase == "NUL")
        return true;
    if (uppercase.size() == 4
        && (uppercase.starts_with("COM") || uppercase.starts_with("LPT"))
        && uppercase[3] >= '1' && uppercase[3] <= '9')
        return true;
    return false;
}
}

SaveStore::SaveStore(std::filesystem::path save_directory,io::detail::PersistenceOperations operations)
    : _save_directory(std::move(save_directory)),_operations(std::move(operations))
{
}

std::expected<void,SaveFailure> SaveStore::initialize() const
{
    if (_save_directory.empty())
        return std::unexpected(failure(SaveError::IoFailure,{},"Save directory must not be empty."));
    auto error = io::detail::before_operation(_operations,"create-directory",_save_directory);
    if (!error) std::filesystem::create_directories(_save_directory,error);
    bool directory = false;
    if (!error) directory = std::filesystem::is_directory(_save_directory,error);
    if (!error && !directory) error = std::make_error_code(std::errc::not_a_directory);
    if (!error) return {};
    return std::unexpected(make_save_failure(SaveError::IoFailure,{},{},
        io::detail::operation_failure("initialize-directory",_save_directory,error),
        io::PersistenceFailureContext{.stage = "initialize-directory",.primary = {_save_directory}}));
}

std::expected<void,SaveFailure> SaveStore::validate_save_name(
    std::string_view save_name)
{
    if (save_name.empty() || save_name.size() > 64
        || !std::ranges::all_of(save_name,is_portable_save_character)
        || is_windows_reserved_name(save_name))
    {
        return std::unexpected(failure(
            SaveError::InvalidSaveName,
            save_name,
            "Save name must contain 1-64 ASCII letters, digits, '_' or '-', and must not be a reserved file name."));
    }
    return {};
}

std::filesystem::path SaveStore::primary_path(std::string_view save_name) const
{
    return _save_directory / (std::string(save_name) + ".json");
}

std::filesystem::path SaveStore::temporary_path(std::string_view save_name) const
{
    return primary_path(save_name).string() + ".tmp";
}

std::filesystem::path SaveStore::backup_path(std::string_view save_name) const
{
    return primary_path(save_name).string() + ".bak";
}

std::expected<SaveStoreLoadResult,SaveFailure> SaveStore::load(
    std::string_view save_name) const
{
    if (auto valid = validate_save_name(save_name); !valid)
        return std::unexpected(valid.error());

    const auto primary = primary_path(save_name);
    const auto temporary = temporary_path(save_name);
    const auto backup = backup_path(save_name);
    std::optional<SaveFailure> warning;
    const auto failed = [&](SaveError code,core::FailureDiagnostic diagnostic,const std::filesystem::path& path)
        -> std::expected<SaveStoreLoadResult,SaveFailure>
    {
        auto error = make_save_failure(code,std::string(save_name),{},std::move(diagnostic));
        error.diagnostic.entries.push_back(core::make_failure_diagnostic_entry("save",std::string(save_name),path));
        if (warning)
        {
            auto diagnostic = warning->diagnostic;
            io::append_failure_context(diagnostic,error.diagnostic);
            error.diagnostic = std::move(diagnostic);
        }
        io::detail::PersistenceFailure details{error.diagnostic,
            {.stage = "load",.primary = {primary},.temporary = {temporary},.backup = {backup}}};
        io::detail::observe_files(_operations,details);
        error.diagnostic = std::move(details.diagnostic);
        error.persistence = std::move(details.context);
        return std::unexpected(std::move(error));
    };
    const auto remember = [&](const ParseResult& parsed,const std::filesystem::path& path)
    {
        auto diagnostic = parse_diagnostic(parsed,path);
        if (!warning) warning = make_save_failure(SaveError::InvalidDocument,std::string(save_name),{},std::move(diagnostic));
        else io::append_failure_context(warning->diagnostic,diagnostic);
    };
    const auto exists = io::detail::file_exists(_operations,primary);
    if (!exists) return failed(SaveError::IoFailure,exists.error(),primary);
    if (*exists)
    {
        if (auto error = io::detail::before_operation(_operations,"read",primary))
            return failed(SaveError::IoFailure,io::detail::operation_failure("read",primary,error),primary);
        auto parsed = parse_document(primary);
        if (parsed.kind == ParseKind::Future || parsed.kind == ParseKind::Access)
            return failed(parsed.kind == ParseKind::Future ? SaveError::UnsupportedFormatVersion : SaveError::IoFailure,
                parse_diagnostic(parsed,primary),primary);
        if (parsed.kind == ParseKind::Valid) return SaveStoreLoadResult{std::move(parsed.data),false,{}};
        remember(parsed,primary);
        const auto corrupt = make_corrupt_path(primary,_operations);
        if (!corrupt) return failed(SaveError::IoFailure,corrupt.error(),primary);
        if (auto archived = io::detail::rename_file(_operations,primary,*corrupt,"archive-corrupt"); !archived)
            return failed(SaveError::IoFailure,archived.error(),primary);
        warning->diagnostic.entries.push_back(core::make_failure_diagnostic_entry(
            "save-archive",std::string(save_name),*corrupt,primary,{},"Corrupt primary archived."));
    }
    for (const auto& candidate : {temporary,backup})
    {
        const auto candidate_exists = io::detail::file_exists(_operations,candidate);
        if (!candidate_exists) return failed(SaveError::IoFailure,candidate_exists.error(),candidate);
        if (!*candidate_exists) continue;
        if (auto error = io::detail::before_operation(_operations,"read",candidate))
            return failed(SaveError::IoFailure,io::detail::operation_failure("read",candidate,error),candidate);
        auto parsed = parse_document(candidate);
        if (parsed.kind == ParseKind::Future || parsed.kind == ParseKind::Access)
            return failed(parsed.kind == ParseKind::Future ? SaveError::UnsupportedFormatVersion : SaveError::IoFailure,
                parse_diagnostic(parsed,candidate),candidate);
        if (parsed.kind != ParseKind::Valid) { remember(parsed,candidate); continue; }
        if (candidate == temporary)
        {
            if (auto promoted = io::detail::rename_file(_operations,temporary,primary,"promote-temporary"); !promoted)
                return failed(SaveError::IoFailure,promoted.error(),temporary);
        }
        else if (auto promoted = save(save_name,parsed.data); !promoted)
        {
            auto error = promoted.error();
            if (warning)
            {
                auto diagnostic = warning->diagnostic;
                io::append_failure_context(diagnostic,error.diagnostic);
                error.diagnostic = std::move(diagnostic);
            }
            return std::unexpected(std::move(error));
        }
        if (!warning) warning = failure(SaveError::InvalidDocument,save_name,"Missing primary recovered from a recovery copy.");
        warning->diagnostic.entries.push_back(core::make_failure_diagnostic_entry(
            "save-recovery",std::string(save_name),primary,candidate,{},"Recovery copy promoted."));
        return SaveStoreLoadResult{std::move(parsed.data),true,std::move(warning)};
    }
    return failed(warning ? SaveError::InvalidDocument : SaveError::NotFound,
        core::make_failure_diagnostic(warning ? "No valid recovery save exists." : "Save file was not found.",std::string(save_name),primary),primary);
}

std::expected<void,SaveFailure> SaveStore::save(
    std::string_view save_name,
    const SaveData& data) const
{
    if (auto valid = validate_save_name(save_name); !valid)
        return std::unexpected(valid.error());

    if (_save_directory.empty()) return std::unexpected(failure(SaveError::IoFailure,save_name,"Save directory must not be empty."));
    const auto primary = primary_path(save_name);
    const auto content = serialize_document(data).dump(2) + "\n";
    SaveError error_code = SaveError::IoFailure;
    const auto saved = io::detail::replace_json_file(primary,content,_operations,
        [&](const std::filesystem::path& path) -> std::expected<void,core::FailureDiagnostic>
        {
            const auto parsed = parse_document(path);
            if (parsed.kind != ParseKind::Valid)
            {
                if (parsed.kind != ParseKind::Access) error_code = SaveError::InvalidDocument;
                return std::unexpected(parse_diagnostic(parsed,path));
            }
            return {};
        },[&](const std::filesystem::path& path) -> std::expected<bool,core::FailureDiagnostic>
        {
            const auto parsed = parse_document(path);
            if (parsed.kind == ParseKind::Access || parsed.kind == ParseKind::Future)
            {
                if (parsed.kind == ParseKind::Future) error_code = SaveError::UnsupportedFormatVersion;
                return std::unexpected(parse_diagnostic(parsed,path));
            }
            return parsed.kind == ParseKind::Valid;
        });
    if (saved) return {};
    auto diagnostic = saved.error().diagnostic;
    diagnostic.entries.push_back(core::make_failure_diagnostic_entry("save",std::string(save_name),primary));
    return std::unexpected(make_save_failure(error_code,std::string(save_name),{},
        std::move(diagnostic),saved.error().context));
}

std::expected<bool,SaveFailure> SaveStore::exists(
    std::string_view save_name) const
{
    if (auto valid = validate_save_name(save_name); !valid)
        return std::unexpected(valid.error());
    for (const auto& path : {primary_path(save_name),temporary_path(save_name),backup_path(save_name)})
    {
        const auto exists = io::detail::file_exists(_operations,path);
        if (!exists)
        {
            io::detail::PersistenceFailure details{exists.error(),
                {.stage = "status",.primary = {primary_path(save_name)},
                    .temporary = {temporary_path(save_name)},.backup = {backup_path(save_name)}}};
            io::detail::observe_files(_operations,details);
            return std::unexpected(make_save_failure(SaveError::IoFailure,std::string(save_name),{},std::move(details.diagnostic),std::move(details.context)));
        }
        if (*exists) return true;
    }
    return false;
}

std::expected<std::vector<std::string>,SaveFailure>
SaveStore::list_save_names() const
{
    try
    {
        std::set<std::string> names;
        for (const auto& entry : std::filesystem::directory_iterator(_save_directory))
        {
            if (!entry.is_regular_file()) continue;
            std::string filename = entry.path().filename().string();
            std::string name;
            if (filename.ends_with(".json.tmp"))
                name = filename.substr(0,filename.size() - 9);
            else if (filename.ends_with(".json.bak"))
                name = filename.substr(0,filename.size() - 9);
            else if (filename.ends_with(".json"))
                name = filename.substr(0,filename.size() - 5);
            else
                continue;

            if (validate_save_name(name)) names.insert(std::move(name));
        }
        return std::vector<std::string>(names.begin(),names.end());
    }
    catch (const std::filesystem::filesystem_error& exception)
    {
        auto diagnostic = io::detail::operation_failure("list-save-names",
            exception.path1().empty() ? _save_directory : exception.path1(),exception.code());
        return std::unexpected(make_save_failure(SaveError::IoFailure,{},{},std::move(diagnostic),
            io::PersistenceFailureContext{.stage = "list-save-names",.primary = {_save_directory}}));
    }
}

std::expected<void,SaveFailure> SaveStore::remove(
    std::string_view save_name) const
{
    if (auto valid = validate_save_name(save_name); !valid)
        return std::unexpected(valid.error());
    bool removed = false;
    for (const auto& path : {primary_path(save_name),temporary_path(save_name),backup_path(save_name)})
    {
        auto error = io::detail::before_operation(_operations,"remove",path);
        if (!error) removed = std::filesystem::remove(path,error) || removed;
        if (error)
        {
            io::detail::PersistenceFailure details{io::detail::operation_failure("remove",path,error),
                {.stage = "remove",.primary = {primary_path(save_name)},
                    .temporary = {temporary_path(save_name)},.backup = {backup_path(save_name)}}};
            io::detail::observe_files(_operations,details);
            return std::unexpected(make_save_failure(SaveError::IoFailure,std::string(save_name),{},std::move(details.diagnostic),std::move(details.context)));
        }
    }
    if (!removed) return std::unexpected(failure(SaveError::NotFound,save_name,"Save file was not found."));
    return {};
}
}
