#include "filesystem_segment_formatter.h"

#include <iomanip>
#include <sstream>

namespace elysia::resources
{
bool format_filesystem_segment(size_t segment_index, std::string& value)
{
	if (segment_index > 99) return false;
	std::ostringstream stream;
	stream << std::setfill('0') << std::setw(2) << segment_index;
	value = stream.str();
	return true;
}
}
