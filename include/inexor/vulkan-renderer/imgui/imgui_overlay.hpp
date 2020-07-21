#pragma once

#include <inexor/vulkan-renderer/wrapper/mesh_buffer.hpp>
#include <inexor/vulkan-renderer/wrapper/shader.hpp>
#include <inexor/vulkan-renderer/wrapper/texture.hpp>

// TODO: This was copied from Sascha Willem's repository. We should create our own wrappers.
#include "inexor/vulkan-renderer/vks.hpp"

#include <glm/glm.hpp>
#include <imgui.h>
#include <vma/vma_usage.h>
#include <vulkan/vulkan.h>

#include <assert.h>
#include <cassert>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

namespace inexor::vulkan_renderer::imgui {

class ImguiOverlay {
    VkDevice device;
    VkPhysicalDevice graphics_card;
    VkQueue data_transfer_queue;
    std::uint32_t data_transfer_queue_family_index;
    VmaAllocator vma_allocator;

    VkResult result;

    VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    uint32_t subpass = 0;

    int32_t vertexCount = 0;
    int32_t indexCount = 0;

    std::unique_ptr<wrapper::MeshBuffer> imgui_mesh;
    std::unique_ptr<wrapper::Texture> imgui_texture;
    std::unique_ptr<wrapper::Shader> imgui_vertex_shader;
    std::unique_ptr<wrapper::Shader> imgui_fragment_shader;

    std::vector<VkPipelineShaderStageCreateInfo> shaders;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    struct PushConstBlock {
        glm::vec2 scale;
        glm::vec2 translate;
    } pushConstBlock;

    bool visible = true;

    bool imgui_overlay_initialised = false;

public:
    // TODO: Sort public/private!
    float scale = 1.0f;
    bool updated = false;

    ImguiOverlay();

    /// Delete the copy constructor so shaders are move-only objects.
    ImguiOverlay(const ImguiOverlay &) = delete;
    ImguiOverlay(ImguiOverlay &&other) noexcept;

    /// Delete the copy assignment operator so shaders are move-only objects.
    ImguiOverlay &operator=(const ImguiOverlay &) = delete;
    ImguiOverlay &operator=(ImguiOverlay &&) noexcept = default;

    ~ImguiOverlay();

    /// @brief Initialises imgui overlay.
    /// @param device [in] The Vulkan device.
    /// @param graphics_card [in] The associated graphics card.
    ///
    ///
    ///
    VkResult init(const VkDevice device, const VkPhysicalDevice graphics_card, const VkQueue data_transfer_queue,
                  const std::uint32_t data_transfer_queue_family_index, const VmaAllocator vma_allocator);

    /// @brief Prepare a separate pipeline for the UI overlay rendering decoupled from the main application.
    /// @param pipelineCache [in] ?
    /// @param renderPass [in] ?
    VkResult preparePipeline(const VkPipelineCache pipelineCache, const VkRenderPass renderPass);

    /// @brief Prepare all vulkan resources required to render the UI overlay.
    VkResult prepareResources();

    /// @brief Update vertex and index buffer containing the imGui elements when required.
    bool update();

    /// @brief
    /// @param
    VkResult draw(const VkCommandBuffer commandBuffer);

    /// @brief
    /// @param
    VkResult resize(uint32_t width, uint32_t height);

    VkResult freeResources();

    bool header(const char *caption);
    bool checkBox(const char *caption, bool *value);
    bool checkBox(const char *caption, int32_t *value);
    bool inputFloat(const char *caption, float *value, float step, uint32_t precision);
    bool sliderFloat(const char *caption, float *value, float min, float max);
    bool sliderInt(const char *caption, int32_t *value, int32_t min, int32_t max);
    bool comboBox(const char *caption, int32_t *itemindex, std::vector<std::string> items);
    bool button(const char *caption);
    void text(const char *formatstr, ...);
};

}; // namespace inexor::vulkan_renderer::imgui
