#include "app_config_loader.h"

#include "../io/json/strict_json.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <set>

namespace elysia::bootstrap
{
namespace
{
using Json = elysia::io::json;

BootstrapFailure app_failure(
    const std::filesystem::path& path,std::string message,std::string pointer,
    std::source_location origin = std::source_location::current())
{
    const std::string reason = message;
    return BootstrapFailure{BootstrapFailure::Code::AppConfig,
        elysia::core::make_failure_diagnostic(
            std::move(message),{elysia::core::make_failure_diagnostic_entry(
                "app-config",{},path,path,std::move(pointer),reason,origin)},origin)};
}

std::expected<void,BootstrapFailure> exact_fields(
    const Json& node,std::initializer_list<std::string_view> expected,
    const std::filesystem::path& file_path,std::string pointer)
{
    if (!node.is_object())
        return std::unexpected(app_failure(
            file_path,"AppConfig value must be an object.",pointer));
    std::set<std::string> allowed;
    for (const auto key : expected) allowed.emplace(key);
    for (const auto& [key,value] : node.items())
        if (!allowed.contains(key))
            return std::unexpected(app_failure(file_path,
                "AppConfig contains an unknown field.",pointer + "/"
                    + elysia::io::json_pointer_escape(key)));
    for (const auto& key : allowed)
        if (!node.contains(key))
            return std::unexpected(app_failure(file_path,
                "AppConfig is missing a required field.",pointer + "/"
                    + elysia::io::json_pointer_escape(key)));
    return {};
}

template<typename Value,typename Predicate>
std::expected<Value,BootstrapFailure> read_value(
    const Json& node,std::string_view key,const std::filesystem::path& path,
    std::string parent_pointer,Predicate predicate,std::string message)
{
    const std::string field(key);
    const std::string pointer = parent_pointer + "/" + elysia::io::json_pointer_escape(field);
    if (!node.contains(field) || !predicate(node.at(field)))
        return std::unexpected(app_failure(path,std::move(message),pointer));
    try { return node.at(field).get<Value>(); }
    catch (const Json::type_error& exception)
    {
        return std::unexpected(app_failure(path,
            "AppConfig value conversion failed: " + std::string(exception.what()),pointer));
    }
    catch (const Json::out_of_range& exception)
    {
        return std::unexpected(app_failure(path,
            "AppConfig value conversion failed: " + std::string(exception.what()),pointer));
    }
}

}

std::expected<AppConfig,BootstrapFailure> AppConfigLoader::load(
    const std::filesystem::path& path) const
{
    const auto parsed = elysia::io::load_strict_json(path);
    if (!parsed)
        return std::unexpected(BootstrapFailure{
            BootstrapFailure::Code::AppConfig,
            elysia::core::make_failure_diagnostic(
                parsed.error().message,
                {elysia::core::make_failure_diagnostic_entry(
                    "app-config",parsed.error().duplicate_property,path,path,
                    parsed.error().json_pointer,parsed.error().message,
                    parsed.error().origin)},parsed.error().origin)});
    const Json& root = *parsed;
    if (auto fields = exact_fields(root,
        {"schema_version","window","render","audio","localization"},path,""); !fields)
        return std::unexpected(std::move(fields.error()));
    if (!root.at("schema_version").is_number_integer()
        || root.at("schema_version").get<int>() != 2)
        return std::unexpected(app_failure(path,
            "AppConfig schema_version must be 2.","/schema_version"));
    if (auto fields = exact_fields(root.at("window"),
            {"title","mode","windowed_size"},path,"/window"); !fields)
        return std::unexpected(std::move(fields.error()));
    if (auto fields = exact_fields(root.at("window").at("windowed_size"),
            {"width","height"},path,"/window/windowed_size"); !fields)
        return std::unexpected(std::move(fields.error()));
    if (auto fields = exact_fields(root.at("render"),{"fps","vsync"},path,"/render"); !fields)
        return std::unexpected(std::move(fields.error()));
    if (auto fields = exact_fields(root.at("audio"),
            {"master_volume","music_volume","sound_volume"},path,"/audio"); !fields)
        return std::unexpected(std::move(fields.error()));
    if (auto fields = exact_fields(root.at("localization"),{"language"},path,"/localization"); !fields)
        return std::unexpected(std::move(fields.error()));

    AppConfig result;
    const Json& window = root.at("window");
    auto title = read_value<std::string>(window,"title",path,"/window",
        [](const Json& value) { return value.is_string() && !value.get<std::string>().empty(); },
        "AppConfig window title must be a non-empty string.");
    if (!title) return std::unexpected(std::move(title.error()));
    result.window_title = std::move(*title);

    auto mode = read_value<std::string>(window,"mode",path,"/window",
        [](const Json& value) { return value.is_string(); },
        "AppConfig window mode must be a string.");
    if (!mode) return std::unexpected(std::move(mode.error()));
    if (*mode == "windowed") result.user_defaults.window.mode = elysia::config::WindowMode::Windowed;
    else if (*mode == "borderless_fullscreen")
        result.user_defaults.window.mode = elysia::config::WindowMode::BorderlessFullscreen;
    else return std::unexpected(app_failure(path,
        "AppConfig window mode is invalid.","/window/mode"));

    const Json& windowed_size = window.at("windowed_size");
    const auto positive_int = [](const Json& value)
    {
        if (!value.is_number_integer()) return false;
        const auto number = value.get<std::int64_t>();
        return number > 0 && number <= std::numeric_limits<int>::max();
    };
    auto width = read_value<int>(windowed_size,"width",path,"/window/windowed_size",
        positive_int,"AppConfig window width must be a positive integer.");
    if (!width) return std::unexpected(std::move(width.error()));
    auto height = read_value<int>(windowed_size,"height",path,"/window/windowed_size",
        positive_int,"AppConfig window height must be a positive integer.");
    if (!height) return std::unexpected(std::move(height.error()));
    result.user_defaults.window.windowed_size = {*width,*height};

    const Json& render = root.at("render");
    auto fps = read_value<double>(render,"fps",path,"/render",
        [](const Json& value)
        {
            if (!value.is_number()) return false;
            const double number = value.get<double>();
            return std::isfinite(number) && number > 0.0;
        },"AppConfig fps must be finite and positive.");
    if (!fps) return std::unexpected(std::move(fps.error()));
    auto vsync = read_value<bool>(render,"vsync",path,"/render",
        [](const Json& value) { return value.is_boolean(); },
        "AppConfig vsync must be boolean.");
    if (!vsync) return std::unexpected(std::move(vsync.error()));
    result.user_defaults.target_fps = *fps;
    result.user_defaults.vsync = *vsync;

    const Json& audio = root.at("audio");
    const auto volume = [](const Json& value)
    {
        if (!value.is_number_integer()) return false;
        const auto number = value.get<std::int64_t>();
        return number >= 0 && number <= 100;
    };
    auto master = read_value<int>(audio,"master_volume",path,"/audio",volume,
        "AppConfig master volume must be within 0..100.");
    if (!master) return std::unexpected(std::move(master.error()));
    auto music = read_value<int>(audio,"music_volume",path,"/audio",volume,
        "AppConfig music volume must be within 0..100.");
    if (!music) return std::unexpected(std::move(music.error()));
    auto sound = read_value<int>(audio,"sound_volume",path,"/audio",volume,
        "AppConfig sound volume must be within 0..100.");
    if (!sound) return std::unexpected(std::move(sound.error()));
    result.user_defaults.audio = {*master,*music,*sound};

    const Json& localization = root.at("localization");
    auto language = read_value<std::string>(localization,"language",path,"/localization",
        [](const Json& value) { return value.is_string() && !value.get<std::string>().empty(); },
        "AppConfig language must be a non-empty string.");
    if (!language) return std::unexpected(std::move(language.error()));
    result.user_defaults.language = std::move(*language);
    return result;
}
}
