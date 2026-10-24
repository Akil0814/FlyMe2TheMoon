#pragma once

#include "action_input_frame.h"
#include "../raw_input_frame.h"
#include "../input_snapshot.h"

#include <span>
#include <set>
#include <vector>

namespace elysia::input
{
struct ActionInputResult
{
    ActionInputFrame frame;
    std::vector<ActionInputEvent> events;
    std::unordered_map<InputActionId, InputActionValue, InputActionIdHash> deltas;
};

class InputActionMap
{
  public:
    [[nodiscard]] bool register_action(InputActionDescriptor descriptor,
                                       std::vector<InputBinding> default_bindings = {});
    [[nodiscard]] bool add_binding(InputBinding binding);
    [[nodiscard]] bool replace_bindings(const InputActionId &action, std::vector<InputBinding> bindings);
    [[nodiscard]] bool clear_bindings(const InputActionId &action);
    void reset_defaults();
    void reset_state();

    [[nodiscard]] bool valid() const;
    [[nodiscard]] std::set<RawInputControl> keyboard_controls() const
    {
        std::set<RawInputControl> keys;
        auto add=[&](RawInputControl key) { if(is_keyboard_control(key)) keys.insert(key); };
        for (const auto &registration:_registrations)
            for (const auto &binding:registration.current_bindings)
            {
                if (auto *button=std::get_if<ButtonInputBinding>(&binding.source)) add(button->control);
                if (auto *buttons=std::get_if<Button2DInputBinding>(&binding.source))
                    for(auto key:{buttons->left,buttons->right,buttons->up,buttons->down}) add(key);
            }
        return keys;
    }
    [[nodiscard]] bool contains(const InputActionId &action) const;
    [[nodiscard]] std::span<const InputBinding> bindings(const InputActionId &action) const;
    [[nodiscard]] ActionInputResult resolve(const InputSnapshot &input);

  private:
    ActionInputResult evaluate_state(const RawInputFrame &raw_input);
    struct Registration
    {
        InputActionDescriptor descriptor;
        std::vector<InputBinding> default_bindings;
        std::vector<InputBinding> current_bindings;
        InputActionValue previous_value;
    };

    [[nodiscard]] Registration *find(const InputActionId &action);
    [[nodiscard]] const Registration *find(const InputActionId &action) const;
    [[nodiscard]] static bool binding_valid_for(const InputBinding &binding,
                                                const InputActionDescriptor &descriptor);
    [[nodiscard]] static InputActionValue resolve_value(const Registration &registration,
                                                        const RawInputState &raw_state);

    std::vector<Registration> _registrations;
};
} // namespace elysia::input
