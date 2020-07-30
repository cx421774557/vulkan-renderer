#include "inexor/vulkan-renderer/frame_graph.hpp"

#include "inexor/vulkan-renderer/wrapper/info.hpp"

#include <spdlog/spdlog.h>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace inexor::vulkan_renderer {

void BufferResource::add_vertex_attribute(VkFormat format, std::uint32_t offset) {
    VkVertexInputAttributeDescription vertex_attribute = {};
    vertex_attribute.format = format;
    vertex_attribute.location = m_vertex_attributes.size();
    vertex_attribute.offset = offset;
    m_vertex_attributes.push_back(vertex_attribute);
}

void RenderStage::writes_to(const RenderResource &resource) {
    m_writes.push_back(&resource);
}

void RenderStage::reads_from(const RenderResource &resource) {
    m_reads.push_back(&resource);
}

void GraphicsStage::bind_buffer(const BufferResource &buffer, std::uint32_t binding) {
    m_buffer_bindings.emplace(&buffer, binding);
}

void GraphicsStage::uses_shader(const wrapper::Shader &shader) {
    auto create_info = wrapper::make_info<VkPipelineShaderStageCreateInfo>();
    create_info.module = shader.get_module();
    create_info.stage = shader.get_type();
    create_info.pName = shader.get_entry_point().c_str();
    m_shaders.push_back(create_info);
}

PhysicalBuffer::~PhysicalBuffer() {
    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
}

PhysicalImage::~PhysicalImage() {
    vkDestroyImageView(m_device, m_image_view, nullptr);
    vmaDestroyImage(m_allocator, m_image, m_allocation);
}

PhysicalStage::~PhysicalStage() {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
}

PhysicalGraphicsStage::~PhysicalGraphicsStage() {
    vkDestroyRenderPass(device(), m_render_pass, nullptr);
}

void FrameGraph::build_image(const TextureResource *resource, PhysicalImage *phys, VmaAllocationCreateInfo *alloc_ci) {
    auto image_ci = wrapper::make_info<VkImageCreateInfo>();
    image_ci.imageType = VK_IMAGE_TYPE_2D;

    // TODO(): Support textures with dimensions not equal to back buffer size
    image_ci.extent.width = m_swapchain.get_extent().width;
    image_ci.extent.height = m_swapchain.get_extent().height;
    image_ci.extent.depth = 1;

    image_ci.arrayLayers = 1;
    image_ci.mipLevels = 1;
    image_ci.format = resource->m_format;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage = resource->m_usage == TextureUsage::DEPTH_STENCIL_BUFFER
                         ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                         : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VmaAllocationInfo alloc_info;
    if (vmaCreateImage(m_allocator, &image_ci, alloc_ci, &phys->m_image, &phys->m_allocation, &alloc_info) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }
}

void FrameGraph::build_image_view(const TextureResource *resource, PhysicalImage *phys) {
    auto image_view_ci = wrapper::make_info<VkImageViewCreateInfo>();
    image_view_ci.format = resource->m_format;
    image_view_ci.image = phys->m_image;
    image_view_ci.subresourceRange.aspectMask = resource->m_usage == TextureUsage::DEPTH_STENCIL_BUFFER
                                                    ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                                                    : VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_ci.subresourceRange.layerCount = 1;
    image_view_ci.subresourceRange.levelCount = 1;
    image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;

    if (vkCreateImageView(m_device, &image_view_ci, nullptr, &phys->m_image_view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view!");
    }
}

void FrameGraph::build_render_pass(const GraphicsStage *stage, PhysicalGraphicsStage *phys) {
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colour_refs;
    std::vector<VkAttachmentReference> depth_refs;

    // Build attachments
    // TODO(): Support multisampled attachments
    // TODO(): Use range-based for loop initialization statements when we switch to C++ 20
    for (std::size_t i = 0; i < stage->m_writes.size(); i++) {
        const auto *resource = stage->m_writes[i];
        const auto *texture = dynamic_cast<const TextureResource *>(resource);
        if (texture == nullptr) {
            continue;
        }

        VkAttachmentDescription attachment = {};
        attachment.format = texture->m_format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        switch (texture->m_usage) {
        case TextureUsage::BACK_BUFFER:
            attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            colour_refs.push_back({static_cast<std::uint32_t>(i), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
            break;
        case TextureUsage::DEPTH_STENCIL_BUFFER:
            attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_refs.push_back({static_cast<std::uint32_t>(i), attachment.finalLayout});
            break;
        default:
            attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colour_refs.push_back({static_cast<std::uint32_t>(i), attachment.finalLayout});
            break;
        }

        attachments.push_back(attachment);
    }

    VkSubpassDependency subpass_dependency = {};
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubpassDescription subpass_description = {};
    subpass_description.colorAttachmentCount = static_cast<std::uint32_t>(colour_refs.size());
    subpass_description.pColorAttachments = colour_refs.data();
    subpass_description.pDepthStencilAttachment = depth_refs.data();
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    auto render_pass_ci = wrapper::make_info<VkRenderPassCreateInfo>();
    render_pass_ci.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    render_pass_ci.dependencyCount = 1;
    render_pass_ci.subpassCount = 1;
    render_pass_ci.pAttachments = attachments.data();
    render_pass_ci.pDependencies = &subpass_dependency;
    render_pass_ci.pSubpasses = &subpass_description;

    if (vkCreateRenderPass(m_device, &render_pass_ci, nullptr, &phys->m_render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}

void FrameGraph::build_graphics_pipeline(const GraphicsStage *stage, PhysicalGraphicsStage *phys) {
    // Make pipeline layout
    phys->m_pipeline_layout =
        std::make_unique<wrapper::PipelineLayout>(m_device, stage->m_descriptor_layouts, "Default pipeline layout");

    std::vector<VkVertexInputAttributeDescription> attribute_bindings;
    std::vector<VkVertexInputBindingDescription> vertex_bindings;
    for (const auto *resource : stage->m_reads) {
        const auto *buffer_resource = dynamic_cast<const BufferResource *>(resource);
        if (buffer_resource == nullptr) {
            continue;
        }

        std::uint32_t binding = stage->m_buffer_bindings.at(buffer_resource);
        for (auto attribute_binding : buffer_resource->m_vertex_attributes) {
            attribute_binding.binding = binding;
            attribute_bindings.push_back(attribute_binding);
        }

        VkVertexInputBindingDescription vertex_binding = {};
        vertex_binding.binding = binding;
        vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertex_binding.stride = buffer_resource->m_element_size;
        vertex_bindings.push_back(vertex_binding);
    }

    auto vertex_input = wrapper::make_info<VkPipelineVertexInputStateCreateInfo>();
    vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attribute_bindings.size());
    vertex_input.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vertex_bindings.size());
    vertex_input.pVertexAttributeDescriptions = attribute_bindings.data();
    vertex_input.pVertexBindingDescriptions = vertex_bindings.data();

    // TODO(): Support primitives other than triangles
    auto input_assembly = wrapper::make_info<VkPipelineInputAssemblyStateCreateInfo>();
    input_assembly.primitiveRestartEnable = VK_FALSE;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    auto depth_stencil = wrapper::make_info<VkPipelineDepthStencilStateCreateInfo>();
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;

    // TODO(): Wireframe rendering
    auto rasterization_state = wrapper::make_info<VkPipelineRasterizationStateCreateInfo>();
    rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_state.lineWidth = 1.0F;
    rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;

    // TODO(): Support multisampling again
    auto multisample_state = wrapper::make_info<VkPipelineMultisampleStateCreateInfo>();
    multisample_state.minSampleShading = 1.0F;
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_attachment = {};
    blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    auto blend_state = wrapper::make_info<VkPipelineColorBlendStateCreateInfo>();
    blend_state.attachmentCount = 1;
    blend_state.pAttachments = &blend_attachment;

    VkRect2D scissor = {};
    scissor.extent = m_swapchain.get_extent();

    VkViewport viewport = {};
    viewport.width = static_cast<float>(m_swapchain.get_extent().width);
    viewport.height = static_cast<float>(m_swapchain.get_extent().height);
    viewport.maxDepth = 1.0F;

    // TODO(): Custom scissors?
    auto viewport_state = wrapper::make_info<VkPipelineViewportStateCreateInfo>();
    viewport_state.scissorCount = 1;
    viewport_state.viewportCount = 1;
    viewport_state.pScissors = &scissor;
    viewport_state.pViewports = &viewport;

    auto pipeline_ci = wrapper::make_info<VkGraphicsPipelineCreateInfo>();
    pipeline_ci.pVertexInputState = &vertex_input;
    pipeline_ci.pInputAssemblyState = &input_assembly;
    pipeline_ci.pDepthStencilState = &depth_stencil;
    pipeline_ci.pRasterizationState = &rasterization_state;
    pipeline_ci.pMultisampleState = &multisample_state;
    pipeline_ci.pColorBlendState = &blend_state;
    pipeline_ci.pViewportState = &viewport_state;
    pipeline_ci.layout = phys->m_pipeline_layout->get();
    pipeline_ci.renderPass = phys->m_render_pass;
    pipeline_ci.stageCount = static_cast<std::uint32_t>(stage->m_shaders.size());
    pipeline_ci.pStages = stage->m_shaders.data();

    // TODO(): Pipeline caching (basically load the frame graph from a file)
    if (vkCreateGraphicsPipelines(m_device, nullptr, 1, &pipeline_ci, nullptr, &phys->m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline!");
    }
}

void FrameGraph::alloc_command_buffers(const RenderStage *stage, PhysicalStage *phys) {
    m_log->trace("Allocating command buffers for stage '{}'", stage->m_name);
    for (std::uint32_t i = 0; i < m_swapchain.get_image_count(); i++) {
        phys->m_command_buffers.emplace_back(m_device, m_command_pool);
    }
}

void FrameGraph::record_command_buffers(const RenderStage *stage, PhysicalStage *phys) {
    m_log->trace("Recording command buffers for stage '{}'", stage->m_name);
    for (std::size_t i = 0; i < phys->m_command_buffers.size(); i++) {
        // TODO(): Remove simultaneous usage once we have proper max frames in flight control
        auto &cmd_buf = phys->m_command_buffers[i];
        cmd_buf.begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

        // Record render pass for graphics stages
        const auto *graphics_stage = dynamic_cast<const GraphicsStage *>(stage);
        if (graphics_stage != nullptr) {
            const auto *phys_graphics_stage = dynamic_cast<const PhysicalGraphicsStage *>(phys);
            assert(phys_graphics_stage != nullptr);

            // TODO(): Allow custom clear values (or no clearing at all)
            std::array<VkClearValue, 2> clear_values = {};
            clear_values[0].color = {0, 0, 0, 0};
            clear_values[1].depthStencil = {1.0F, 0};

            auto render_pass_bi = wrapper::make_info<VkRenderPassBeginInfo>();
            render_pass_bi.clearValueCount = clear_values.size();
            render_pass_bi.pClearValues = clear_values.data();
            render_pass_bi.framebuffer = phys_graphics_stage->m_framebuffers[i].get();
            render_pass_bi.renderArea.extent = m_swapchain.get_extent();
            render_pass_bi.renderPass = phys_graphics_stage->m_render_pass;
            cmd_buf.begin_render_pass(render_pass_bi);
        }

        std::vector<VkBuffer> bind_buffers;
        for (const auto *resource : stage->m_reads) {
            const auto *phys_resource = m_resource_map[resource].get();
            assert(phys_resource != nullptr);

            if (const auto *phys_buffer = dynamic_cast<const PhysicalBuffer *>(phys_resource)) {
                bind_buffers.push_back(phys_buffer->m_buffer);
            }
        }

        if (!bind_buffers.empty()) {
            cmd_buf.bind_vertex_buffers(bind_buffers);
        }

        cmd_buf.bind_graphics_pipeline(phys->m_pipeline);
        stage->m_on_record(phys, cmd_buf);

        if (graphics_stage != nullptr) {
            cmd_buf.end_render_pass();
        }
        cmd_buf.end();
    }
}

void FrameGraph::compile(const RenderResource &target) {
    // TODO(): Better logging and input validation
    // TODO(): Many opportunities for optimisation

    // Build a simple helper map to lookup resources writers
    std::unordered_map<const RenderResource *, std::vector<RenderStage *>> writers;
    for (auto &stage : m_stages) {
        for (const auto *resource : stage->m_writes) {
            writers[resource].push_back(stage.get());
        }
    }

    // Post order depth first search
    // NOTE: Doesn't do any colouring, only works on acyclic graphs!
    // TODO(): Stage graph validation (ensuring no cycles, etc.)
    // TODO(): Move away from recursive dfs algo
    std::function<void(RenderStage *)> dfs = [&](RenderStage *stage) {
        for (const auto *resource : stage->m_reads) {
            for (auto *writer : writers[resource]) {
                dfs(writer);
            }
        }
        m_stage_stack.push_back(stage);
    };

    // DFS starting from writer of target (initial stage executants)
    // TODO(): Will there be more than one writer to the target (back buffer), maybe with blending?
    assert(writers[&target].size() == 1);
    dfs(writers[&target][0]);

    m_log->debug("Final stage order:");
    for (auto *stage : m_stage_stack) {
        m_log->debug("  - {}", stage->m_name);
    }

    // Create physical resources
    // TODO(): Resource aliasing (i.e. reusing the same physical resource for multiple resources)
    for (const auto &resource : m_resources) {
        // Build allocation (using VMA for now)
        m_log->trace("Allocating physical resource for resource '{}'", resource->m_name);
        VmaAllocationCreateInfo alloc_ci = {};

        // TODO(): Use a constexpr bool
#if VMA_RECORDING_ENABLED
        alloc_ci.flags |= VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        alloc_ci.pUserData = const_cast<char *>(resource->m_name.data());
#endif

        if (const auto *buffer_resource = dynamic_cast<const BufferResource *>(resource.get())) {
            assert(buffer_resource->m_usage != BufferUsage::INVALID);
            auto *phys = create<PhysicalBuffer>(buffer_resource, m_allocator, m_device);

            bool is_uploading_data = buffer_resource->m_data != nullptr;
            alloc_ci.flags |= is_uploading_data ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0U;
            alloc_ci.usage = is_uploading_data ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_GPU_ONLY;

            auto buffer_ci = wrapper::make_info<VkBufferCreateInfo>();
            buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            buffer_ci.size = buffer_resource->m_data_size;
            switch (buffer_resource->m_usage) {
            case BufferUsage::VERTEX_BUFFER:
                buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                break;
            default:
                assert(false);
            }

            VmaAllocationInfo alloc_info;
            if (vmaCreateBuffer(m_allocator, &buffer_ci, &alloc_ci, &phys->m_buffer, &phys->m_allocation,
                                &alloc_info) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create buffer!");
            }

            if (is_uploading_data) {
                assert(alloc_info.pMappedData != nullptr);
                std::memcpy(alloc_info.pMappedData, buffer_resource->m_data, buffer_resource->m_data_size);
            }
        }

        if (const auto *texture_resource = dynamic_cast<const TextureResource *>(resource.get())) {
            assert(texture_resource->m_usage != TextureUsage::INVALID);

            // Back buffer gets special handling
            if (texture_resource->m_usage == TextureUsage::BACK_BUFFER) {
                // TODO(): Move image views from wrapper::Swapchain to PhysicalBackBuffer
                create<PhysicalBackBuffer>(texture_resource, m_allocator, m_device, m_swapchain);
            } else {
                auto *phys = create<PhysicalImage>(texture_resource, m_allocator, m_device);
                alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
                build_image(texture_resource, phys, &alloc_ci);
                build_image_view(texture_resource, phys);
            }
        }
    }

    // Create physical stages
    // NOTE: Each render stage, after merging and reordering, maps to a vulkan pipeline and list of command buffers
    // NOTE: Each graphics stage maps to a vulkan render pass and graphics pipeline
    for (const auto *stage : m_stage_stack) {
        if (const auto *graphics_stage = dynamic_cast<const GraphicsStage *>(stage)) {
            auto *phys = create<PhysicalGraphicsStage>(graphics_stage, m_device);
            build_render_pass(graphics_stage, phys);
            build_graphics_pipeline(graphics_stage, phys);

            // If we write to at least one texture, we need to make framebuffers
            if (!stage->m_writes.empty()) {
                // For every texture that this stage writes to, we need to attach it to the framebuffer
                std::vector<const PhysicalBackBuffer *> back_buffers;
                std::vector<const PhysicalImage *> images;
                for (const auto *resource : stage->m_writes) {
                    const auto *phys_resource = m_resource_map[resource].get();
                    if (const auto *back_buffer = dynamic_cast<const PhysicalBackBuffer *>(phys_resource)) {
                        back_buffers.push_back(back_buffer);
                    } else if (const auto *image = dynamic_cast<const PhysicalImage *>(phys_resource)) {
                        images.push_back(image);
                    }
                }

                std::vector<VkImageView> image_views;
                for (std::uint32_t i = 0; i < m_swapchain.get_image_count(); i++) {
                    image_views.clear();
                    for (const auto *back_buffer : back_buffers) {
                        image_views.push_back(back_buffer->m_swapchain.get_image_view(i));
                    }

                    for (const auto *image : images) {
                        image_views.push_back(image->m_image_view);
                    }

                    phys->m_framebuffers.emplace_back(m_device, phys->m_render_pass, image_views, m_swapchain);
                }
            }
        }
    }

    // Allocate and record command buffers
    for (const auto *stage : m_stage_stack) {
        auto *phys = m_stage_map[stage].get();
        alloc_command_buffers(stage, phys);
        record_command_buffers(stage, phys);
    }
}

void FrameGraph::render(int image_index, VkSemaphore signal_semaphore, VkSemaphore wait_semaphore,
                        VkQueue graphics_queue) const {
    auto submit_info = wrapper::make_info<VkSubmitInfo>();
    submit_info.commandBufferCount = 1;
    submit_info.signalSemaphoreCount = 1;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &signal_semaphore;
    submit_info.pWaitSemaphores = &wait_semaphore;

    std::array<VkPipelineStageFlags, 1> wait_stage_mask = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.pWaitDstStageMask = wait_stage_mask.data();

    // TODO(): Batch submit infos
    // TODO(): Loop over physical stages here
    for (const auto *stage : m_stage_stack) {
        auto *cmd_buf = m_stage_map.at(stage)->m_command_buffers[image_index].get();
        submit_info.pCommandBuffers = &cmd_buf;
        vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    }
}

} // namespace inexor::vulkan_renderer
