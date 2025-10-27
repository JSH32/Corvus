#pragma once

#include "linp/asset/asset_manager.hpp"
#include <IconsFontAwesome6.h>
#include <concepts>
#include <imgui.h>
#include <linp/components/entity_info.hpp>
#include <linp/components/mesh_renderer.hpp>
#include <linp/components/transform.hpp>
#include <linp/entity.hpp>

namespace Linp::Editor {

/**
 * @brief Base descriptor that components must specialize to provide UI for the inspector.
 *
 * This template struct defines the contract for how a component exposes its
 * display name, removability, layout style (flat or normal), and a static
 * `draw` function for its rendering logic.
 *
 * @tparam T The component type.
 */
template <typename T>
struct ComponentInfo {
    using ComponentType = T; ///< The actual type of the component.

    /** @brief The display name of the component in the inspector. Can include icons. */
    static constexpr std::string_view name = "Unknown Component";

    /** @brief Whether this component can be removed from an entity via the inspector. */
    static constexpr bool removable = true;

    /** @brief If true, the component's UI is drawn directly without surrounding formatting.
     *         If false, it's drawn within a section (ImGui::TreeNodeEx).
     */
    static constexpr bool flat = false;

    /**
     * @brief Function to draw the ImGui UI for the component.
     *
     * @param component A reference to the component instance to be drawn/edited.
     */
    static void draw(T& component, Core::AssetManager* assetManager) = delete;
};

/**
 * @typedef DrawableComponents
 * @brief A std::tuple listing all component types that can be drawn in the inspector.
 *
 * The InspectorPanel will iterate over this tuple to find and draw components
 * present on a selected entity.
 */
template <typename T>
concept HasComponentInfo = requires(T component, Core::AssetManager* assetManager) {
    typename ComponentInfo<T>::ComponentType;
    { ComponentInfo<T>::name } -> std::convertible_to<std::string_view>;
    { ComponentInfo<T>::removable } -> std::convertible_to<bool>;
    { ComponentInfo<T>::flat } -> std::convertible_to<bool>;
    { ComponentInfo<T>::draw(component, assetManager) } -> std::same_as<void>;
};

/**
 * @brief A ilst of all component types that can be drawn in the inspector.
 *
 * The inspector will iterate over this to know what is renderable
 */
using DrawableComponents = std::tuple<Linp::Core::Components::EntityInfoComponent,
                                      Linp::Core::Components::TransformComponent,
                                      Linp::Core::Components::MeshRendererComponent>;

}
