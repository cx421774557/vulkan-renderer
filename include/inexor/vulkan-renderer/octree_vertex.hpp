#pragma once

#include <glm/vec3.hpp>

namespace inexor::vulkan_renderer {

struct OctreeVertex {
    glm::vec3 position;
    glm::vec3 color;

    OctreeVertex(glm::vec3 position, glm::vec3 color) : position(position), color(color) {}
};

} // namespace inexor::vulkan_renderer
