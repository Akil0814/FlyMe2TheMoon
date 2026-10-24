#include "strict_json.h"

#include <fstream>
#include <set>
#include <vector>

namespace elysia::io
{
std::string json_pointer_escape(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value)
    {
        if (character == '~') escaped += "~0";
        else if (character == '/') escaped += "~1";
        else escaped += character;
    }
    return escaped;
}

std::expected<json,JsonFileFailure> load_strict_json(
    const std::filesystem::path& path,
    std::source_location origin)
{
    if (path.empty())
        return std::unexpected(JsonFileFailure{
            JsonFileError::EmptyPath,"JSON path is empty.",path,{},{},origin});

    std::error_code status_error;
    const bool regular_file = std::filesystem::is_regular_file(path,status_error);
    if (status_error && status_error != std::errc::no_such_file_or_directory)
        return std::unexpected(JsonFileFailure{
            JsonFileError::FilesystemAccess,
            "JSON file status could not be read: " + status_error.message(),
            path,{},{},origin});
    if (!regular_file)
        return std::unexpected(JsonFileFailure{
            JsonFileError::FileMissing,
            "JSON file does not exist or is not a regular file.",path,{},{},origin});

    std::ifstream input(path);
    if (!input.is_open())
        return std::unexpected(JsonFileFailure{
            JsonFileError::OpenFailed,"JSON file could not be opened.",path,{},{},origin});
    struct Container
    {
        bool array = false;
        std::string pointer;
        std::string pending_key;
        std::size_t next_index = 0;
        std::set<std::string> keys;
    };
    std::vector<Container> containers;
    std::string duplicate;
    std::string duplicate_pointer;
    const auto child_pointer = [&]()
    {
        if (containers.empty()) return std::string{};
        const auto& parent = containers.back();
        return parent.pointer + "/" + (parent.array
            ? std::to_string(parent.next_index)
            : json_pointer_escape(parent.pending_key));
    };
    const auto consume_value = [&]()
    {
        if (containers.empty()) return;
        if (containers.back().array) ++containers.back().next_index;
        else containers.back().pending_key.clear();
    };
    auto callback = [&](int,json::parse_event_t event,json& parsed)
    {
        if (event == json::parse_event_t::object_start)
            containers.push_back(Container{.pointer = child_pointer()});
        else if (event == json::parse_event_t::array_start)
            containers.push_back(Container{.array = true,.pointer = child_pointer()});
        else if (event == json::parse_event_t::key && !containers.empty())
        {
            const std::string key = parsed.get<std::string>();
            auto& object = containers.back();
            object.pending_key = key;
            if (!object.keys.insert(key).second && duplicate.empty())
            {
                duplicate = key;
                duplicate_pointer = object.pointer + "/" + json_pointer_escape(key);
            }
        }
        else if ((event == json::parse_event_t::object_end
            || event == json::parse_event_t::array_end) && !containers.empty())
        {
            containers.pop_back();
            consume_value();
        }
        else if (event == json::parse_event_t::value)
            consume_value();
        return true;
    };
    try
    {
        json result = json::parse(input,callback,true,true);
        if (input.bad())
            return std::unexpected(JsonFileFailure{
                JsonFileError::ReadFailed,"JSON file could not be read.",path,{},{},origin});
        if (!duplicate.empty())
            return std::unexpected(JsonFileFailure{
                JsonFileError::DuplicateProperty,
                "JSON object contains a duplicate property.",path,duplicate,
                std::move(duplicate_pointer),origin});
        return result;
    }
    catch (const json::parse_error& exception)
    {
        return std::unexpected(JsonFileFailure{
            input.bad() ? JsonFileError::ReadFailed : JsonFileError::ParseFailed,
            "JSON parsing failed: " + std::string(exception.what()),path,{},{},origin});
    }
    catch (const json::out_of_range& exception)
    {
        // The parser reports external numeric overflow as out_of_range.
        return std::unexpected(JsonFileFailure{
            JsonFileError::ParseFailed,exception.what(),path,{},{},origin});
    }
}
}
