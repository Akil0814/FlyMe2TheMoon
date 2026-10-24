#pragma once

#include "ui_theme.h"

#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace elysia::ui
{
class UiElement;

enum class UiThemeStyleTraversal
{
    PublicChildren,
    Stop,
    AdapterDefinedChildren
};

struct UiThemeStyleResolution
{
    bool applied = false;
    UiThemeStyleTraversal traversal = UiThemeStyleTraversal::PublicChildren;
    std::vector<UiElement*> adapter_children;
};

class UiThemeStyleResolver
{
public:
    template<class Element>
    bool register_adapter(
        std::function<void(Element&,const UiTheme&)> apply,
        UiThemeStyleTraversal traversal = UiThemeStyleTraversal::PublicChildren,
        std::function<std::vector<UiElement*>(Element&)> children = {})
    {
        const std::type_index key(typeid(Element));
        if (_adapters.contains(key))
            return false;
        _adapters.emplace(key,std::make_unique<Adapter<Element>>(std::move(apply),traversal,std::move(children)));
        return true;
    }

    [[nodiscard]] UiThemeStyleResolution apply(UiElement& element,const UiTheme& theme) const;

private:
    struct AdapterBase
    {
        virtual ~AdapterBase() = default;
        virtual UiThemeStyleResolution apply(UiElement&,const UiTheme&) const = 0;
    };

    template<class Element>
    struct Adapter final : AdapterBase
    {
        Adapter(std::function<void(Element&,const UiTheme&)> fn,UiThemeStyleTraversal policy,
            std::function<std::vector<UiElement*>(Element&)> child_fn)
            : apply_fn(std::move(fn)),traversal(policy),children_fn(std::move(child_fn)) {}

        UiThemeStyleResolution apply(UiElement& element,const UiTheme& theme) const override
        {
            auto& exact = static_cast<Element&>(element);
            apply_fn(exact,theme);
            return { true,traversal,children_fn ? children_fn(exact) : std::vector<UiElement*>{} };
        }
        std::function<void(Element&,const UiTheme&)> apply_fn;
        UiThemeStyleTraversal traversal;
        std::function<std::vector<UiElement*>(Element&)> children_fn;
    };

    std::unordered_map<std::type_index,std::unique_ptr<AdapterBase>> _adapters;
};

void register_builtin_ui_theme_adapters(UiThemeStyleResolver& resolver);
}
