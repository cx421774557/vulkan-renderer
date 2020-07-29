#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include <vector>

namespace inexor::vulkan_renderer {

// TODO: Generalize this setup using a builder pattern!
class OctreeVertex {
public:
    glm::vec3 pos;
    glm::vec3 color;

    OctreeVertex(glm::vec3 pos, glm::vec3 color);

    [[nodiscard]] static VkVertexInputBindingDescription get_vertex_binding_description();

    /// @note You should use the format where the amount of color channels matches the number of components in the
    /// shader data type. It is allowed to use more channels than the number of components in the shader, but they will
    /// be silently discarded.
    [[nodiscard]] static std::vector<VkVertexInputAttributeDescription> get_attribute_binding_description();
};

} // namespace inexor::vulkan_renderer
