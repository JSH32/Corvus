#pragma once
#include "corvus/graphics/graphics.hpp"
#include "corvus/renderer/model.hpp"

namespace Corvus::Renderer {

/**
 * Utilities for generating procedural models
 */
namespace ModelGenerator {

    /**
     * Create a cube model
     */
    Model createCube(Graphics::GraphicsContext& ctx, float size = 1.0f);

    /**
     * Create a sphere model
     */
    Model createSphere(Graphics::GraphicsContext& ctx,
                       float                      radius = 1.0f,
                       uint32_t                   rings  = 16,
                       uint32_t                   slices = 16);

    /**
     * Create a plane model
     */
    Model createPlane(Graphics::GraphicsContext& ctx, float width = 1.0f, float length = 1.0f);

    /**
     * Create a cylinder model
     */
    Model createCylinder(Graphics::GraphicsContext& ctx,
                         float                      radius = 0.5f,
                         float                      height = 1.0f,
                         uint32_t                   slices = 16);

}

}
