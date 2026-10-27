#pragma once
#include "engine/core/game_object.h"
#include "engine/core/render/render_command.h"
#include "game/content/content.h"

namespace game::play {
// Visual continuation of existing outer walls; owns no physics body or collider.
class BoundaryRocks final:public elysia::core::GameObject {
public:
    explicit BoundaryRocks(const content::Json& level);
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out)const override;
private:
    std::vector<elysia::core::RenderCommand> _commands;
};
}
