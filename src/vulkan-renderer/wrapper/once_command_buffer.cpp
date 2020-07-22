#include "inexor/vulkan-renderer/wrapper/once_command_buffer.hpp"

#include "inexor/vulkan-renderer/wrapper/info.hpp"

#include <spdlog/spdlog.h>

#include <cassert>
#include <stdexcept>
#include <utility>

namespace inexor::vulkan_renderer::wrapper {

OnceCommandBuffer::OnceCommandBuffer(OnceCommandBuffer &&other) noexcept
    : device(other.device), command_pool(std::move(other.command_pool)),
      command_buffer(std::exchange(other.command_buffer, nullptr)), data_transfer_queue(other.data_transfer_queue),
      recording_started(other.recording_started), command_buffer_created(other.command_buffer_created) {}

OnceCommandBuffer::OnceCommandBuffer(const VkDevice device, const VkQueue data_transfer_queue,
                                     const std::uint32_t data_transfer_queue_family_index)
    : device(device), data_transfer_queue(data_transfer_queue), command_pool(device, data_transfer_queue_family_index) {

    assert(device);
    assert(data_transfer_queue);
    command_buffer_created = false;
    recording_started = false;
}

OnceCommandBuffer::~OnceCommandBuffer() {
    command_buffer.reset();
    command_buffer_created = false;
    recording_started = false;
}

void OnceCommandBuffer::create_command_buffer() {
    assert(device);
    assert(command_pool.get());
    assert(data_transfer_queue);
    assert(!recording_started);
    assert(!command_buffer_created);

    command_buffer = std::make_unique<wrapper::CommandBuffer>(device, command_pool.get());

    command_buffer_created = true;
}

void OnceCommandBuffer::start_recording() {
    assert(device);
    assert(command_pool.get());
    assert(data_transfer_queue);
    assert(command_buffer_created);
    assert(!recording_started);

    spdlog::debug("Starting recording of once command buffer.");

    auto command_buffer_bi = make_info<VkCommandBufferBeginInfo>();

    // We're only going to use the command buffer once and wait with returning from the function until the copy
    // operation has finished executing. It's good practice to tell the driver about our intent using
    // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT.
    command_buffer_bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(command_buffer->get(), &command_buffer_bi) != VK_SUCCESS) {
        throw std::runtime_error("Error: vkBeginCommandBuffer failed for once command buffer!");
    }

    recording_started = true;

    // TODO: Set object name using Vulkan debug markers.
}

void OnceCommandBuffer::end_recording_and_submit_command() {
    assert(device);
    assert(command_pool.get());
    assert(command_buffer);
    assert(data_transfer_queue);
    assert(command_buffer_created);
    assert(recording_started);

    spdlog::debug("Ending recording of once command buffer.");

    if (vkEndCommandBuffer(command_buffer->get()) != VK_SUCCESS) {
        throw std::runtime_error("Error: VkEndCommandBuffer failed for once command buffer!");
    }

    spdlog::debug("Command buffer recording ended successfully.");

    spdlog::debug("Starting to submit command.");

    auto submit_info = make_info<VkSubmitInfo>();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = command_buffer->get_ptr();

    if (vkQueueSubmit(data_transfer_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Error: vkQueueSubmit failed for once command buffer!");
    }

    // TODO: Refactor! Introduce proper synchronisation using VkFence!
    if (vkQueueWaitIdle(data_transfer_queue) != VK_SUCCESS) {
        throw std::runtime_error("Error: vkQueueWaitIdle failed for once command buffer!");
    }

    spdlog::debug("Destroying once command buffer.");

    // Because we destroy the command buffer after submission, we have to allocate it every time.
    vkFreeCommandBuffers(device, command_pool.get(), 1, command_buffer->get_ptr());

    command_buffer_created = false;

    recording_started = false;
}

} // namespace inexor::vulkan_renderer::wrapper
