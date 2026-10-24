#pragma once

namespace elysia::loading
{
// Clears every registry populated by a content-loading cycle. Scene ownership
// must already have released objects that reference these resources.
void clear_loaded_content() noexcept;
}
