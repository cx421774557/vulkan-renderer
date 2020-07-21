#pragma once

#include "inexor/vulkan-renderer/wrapper/gpu_memory_buffer.hpp"

#include <vma/vma_usage.h>
#include <vulkan/vulkan_core.h>

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <stdexcept>

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

    /// @brief Creates a new vertex buffer with an associated index buffer and copies memory into it.
    MeshBuffer(const VkDevice device, const VkQueue data_transfer_queue, const std::uint32_t data_transfer_queue_family_index,
               const VmaAllocator vma_allocator, const std::string &name, const VkDeviceSize size_of_vertex_structure,
               const std::size_t number_of_vertices, void *vertices, const VkDeviceSize size_of_index_structure,
               const std::size_t number_of_indices, void *indices);

    /// @brief Creates a new vertex buffer with an associated index buffer but does not copy memory into it.
    /// This is useful when you know the size of the buffer but you don't know it's data values yet.
    MeshBuffer(const VkDevice device, const VkQueue data_transfer_queue, const std::uint32_t data_transfer_queue_family_index,
               const VmaAllocator vma_allocator, const std::string &name, const VkDeviceSize size_of_vertex_structure,
               const std::size_t number_of_vertices, const VkDeviceSize size_of_index_structure,
               const std::size_t number_of_indices);

    /// @brief Creates a vertex buffer without index buffer, but copies the vertex data into it.
    MeshBuffer(const VkDevice device, const VkQueue data_transfer_queue, const std::uint32_t data_transfer_queue_family_index,
               const VmaAllocator vma_allocator, const std::string &name, const VkDeviceSize size_of_vertex_structure,
               const std::size_t number_of_vertices, void *vertices);

    /// @brief Creates a vertex buffer without index buffer and copies no vertex data into it.
    MeshBuffer(const VkDevice device, const VkQueue data_transfer_queue, const std::uint32_t data_transfer_queue_family_index,
               const VmaAllocator vma_allocator, const std::string &name, const VkDeviceSize size_of_vertex_structure,
               const std::size_t number_of_vertices);

    ~MeshBuffer();

    [[nodiscard]] VkBuffer get_vertex_buffer() const {
        return vertex_buffer.get_buffer();
    }

    [[nodiscard]] bool has_index_buffer() const {
        return (index_buffer) ? true : false;
    }

    [[nodiscard]] VkBuffer get_index_buffer() const {
        if(!index_buffer) {
            throw std::runtime_error(std::string("Error: No index buffer for mesh "+ name +"!"));
        }

        return index_buffer.value().get_buffer();
    }

    [[nodiscard]] const std::uint32_t get_vertex_count() const {
        return number_of_vertices;
    }

    [[nodiscard]] const std::uint32_t get_index_cound() const {
        return number_of_indices;
    }

    [[nodiscard]] auto get_vertex_buffer_address() const {
        return vertex_buffer.get_allocation_info().pMappedData;
    }
    
    [[nodiscard]] auto get_index_buffer_address() const {
        if(!index_buffer) {
            throw std::runtime_error(std::string("Error: No index buffer for mesh "+ name +"!"));
        }

        return index_buffer.value().get_allocation_info().pMappedData;
    }
    
    /// 
    /// 
    /// 
    /// 
    void update_vertices(const void* source, const std::size_t vertex_struct_size, const std::size_t vertex_count, const std::size_t vertex_offset);

    
    /// 
    /// 
    /// 
    /// 
    void update_indices(const void* source, const std::size_t index_structure_size, const std::size_t index_count, const std::size_t index_offset);


};

} // namespace inexor::vulkan_renderer::wrapper
