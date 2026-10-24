#pragma once

#include <cstddef>
#include <string>

namespace elysia::resources
{
[[nodiscard]] bool format_filesystem_segment(size_t segment_index, std::string& value);
}
