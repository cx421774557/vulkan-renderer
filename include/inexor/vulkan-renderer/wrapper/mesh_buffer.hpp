﻿#pragma once

#include "inexor/vulkan-renderer/wrapper/gpu_memory_buffer.hpp"

#include <vma/vma_usage.h>
#include <vulkan/vulkan_core.h>

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>

namespace inexor::vulkan_renderer::wrapper {

/// @brief A structure which bundles vertex buffer and index buffer (if existent).
/// It contains all data which are related to memory allocations for these buffers.
/// @todo Driver developers recommend that you store multiple
/// buffers, like the vertex and index buffer, into a single VkBuffer and use offsets
/// in commands like vkCmdBindVertexBuffers. The advantage is that your data
/// is more cache friendly in that case, because it’s closer together. It is even possible
/// to reuse the same chunk of memory for multiple resources if they are not
/// used during the same render operations, provided that their data is refreshed,
/// of course. This is known as aliasing and some Vulkan functions have explicit
/// flags to specify that you want to do this.
class MeshBuffer {

private:
    std::string name = "";

    GPUMemoryBuffer vertex_buffer;

    // Index buffer, if available.
    std::optional<GPUMemoryBuffer> index_buffer;

    std::uint32_t number_of_vertices = 0;

    std::uint32_t number_of_indices = 0;

    // Don't forget that index buffers are optional!
    bool index_buffer_available = false;

public:
    // Delete the copy constructor so mesh buffers are move-only objects.
    MeshBuffer(const MeshBuffer &) = delete;
    MeshBuffer(MeshBuffer &&buffer) noexcept;

    // Delete the copy assignment operator so uniform buffers are move-only objects.
    MeshBuffer &operator=(const MeshBuffer &) = delete;
    MeshBuffer &operator=(MeshBuffer &&) noexcept = default;

    /// @brief Creates a new vertex buffer and an associated index buffer.
    MeshBuffer(const VkDevice device, VkQueue data_transfer_queue, const std::uint32_t data_transfer_queue_family_index,
               const VmaAllocator vma_allocator, const std::string &name, const VkDeviceSize size_of_vertex_structure,
               const std::size_t number_of_vertices, void *vertices, const VkDeviceSize size_of_index_structure,
               const std::size_t number_of_indices, void *indices);

    /// @brief Creates a vertex buffer without index buffer.
    MeshBuffer(const VkDevice device, VkQueue data_transfer_queue, const std::uint32_t data_transfer_queue_family_index,
               const VmaAllocator vma_allocator, const std::string &name, const VkDeviceSize size_of_vertex_structure,
               const std::size_t number_of_vertices, void *vertices);

    ~MeshBuffer();

    [[nodiscard]] VkBuffer get_vertex_buffer() const {
        return vertex_buffer.get_buffer();
    }

    [[nodiscard]] bool has_index_buffer() const {
        return index_buffer.has_value();
    }

    [[nodiscard]] std::optional<VkBuffer> get_index_buffer() const {
        assert(index_buffer);
        return index_buffer->get_buffer();
    }

    [[nodiscard]] const std::uint32_t get_vertex_count() const {
        return number_of_vertices;
    }

    [[nodiscard]] const std::uint32_t get_index_cound() const {
        return number_of_indices;
    }

    // TODO: Update data!
    // void update_vertex_buffer();
    // void update_index_buffer();
};

} // namespace inexor::vulkan_renderer::wrapper
