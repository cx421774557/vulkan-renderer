#include "inexor/vulkan-renderer/imgui/imgui_overlay.hpp"

#include <optional>

namespace inexor::vulkan_renderer::imgui {

ImguiOverlay::ImguiOverlay(ImguiOverlay &&other) noexcept {
    // TODO: Implement!
}

ImguiOverlay::ImguiOverlay() {

    // Init ImGui
    ImGui::CreateContext();

    // Color scheme
    ImGuiStyle &style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);

    // Dimensions
    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = scale;
}

ImguiOverlay::~ImguiOverlay() {}

VkResult ImguiOverlay::init(const VkDevice device, const VkPhysicalDevice graphics_card,
                            const VkQueue data_transfer_queue, const std::uint32_t data_transfer_queue_family_index,
                            const VmaAllocator vma_allocator) {
    assert(device);
    assert(graphics_card);

    this->device = device;
    this->graphics_card = graphics_card;
    this->vma_allocator = vma_allocator;
    this->data_transfer_queue = data_transfer_queue;
    this->data_transfer_queue_family_index = data_transfer_queue_family_index;

    imgui_overlay_initialised = true;
    return VK_SUCCESS;
}

VkResult ImguiOverlay::prepareResources() {
    ImGuiIO &io = ImGui::GetIO();

    // Create font texture
    unsigned char *fontData;
    int texWidth, texHeight;

    io.Fonts->AddFontFromFileTTF("assets/fonts/vegur/vegur.otf", 16.0f);
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
    VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

    // Create the texture for imgui from memory.
    imgui_texture = std::make_unique<wrapper::Texture>(device, graphics_card, vma_allocator, fontData, texWidth,
                                                       texHeight, uploadSize, "imgui_overlay", data_transfer_queue,
                                                       data_transfer_queue_family_index);

    VkPipelineShaderStageCreateInfo shader_stage_create_info = {};

    // Load imgui shaders.
    imgui_vertex_shader = std::make_unique<wrapper::Shader>(device, VK_SHADER_STAGE_VERTEX_BIT, "imgui_vertex_shader",
                                                            "shaders/ui.vert.spv");

    shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_info.pNext = nullptr;
    shader_stage_create_info.flags = 0;
    shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stage_create_info.module = imgui_vertex_shader->get_module();
    shader_stage_create_info.pName = imgui_vertex_shader->get_entry_point().c_str();
    shader_stage_create_info.pSpecializationInfo = nullptr;

    shaders.push_back(shader_stage_create_info);

    //
    imgui_fragment_shader = std::make_unique<wrapper::Shader>(device, VK_SHADER_STAGE_FRAGMENT_BIT,
                                                              "imgui_fragment_shader", "shaders/ui.frag.spv");

    shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_info.pNext = nullptr;
    shader_stage_create_info.flags = 0;
    shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_create_info.module = imgui_fragment_shader->get_module();
    shader_stage_create_info.pName = imgui_fragment_shader->get_entry_point().c_str();
    shader_stage_create_info.pSpecializationInfo = nullptr;

    shaders.push_back(shader_stage_create_info);

    // Descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)};

    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);

    if (vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Error: vkCreateDescriptorPool failed for imgui overlay!");
    }

    // Descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayout =
        vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);

    if (vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout)) {
        throw std::runtime_error("Error: vkCreateDescriptorSetLayout failed for imgui overlay!");
    }

    // Descriptor set
    VkDescriptorSetAllocateInfo allocInfo =
        vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet)) {
        throw std::runtime_error("Error: vkAllocateDescriptorSets failed for imgui overlay!");
    }

    VkDescriptorImageInfo fontDescriptor = vks::initializers::descriptorImageInfo(
        imgui_texture->get_sampler(), imgui_texture->get_image_view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {vks::initializers::writeDescriptorSet(
        descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)};

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0,
                           nullptr);

    return VK_SUCCESS;
}

VkResult ImguiOverlay::preparePipeline(const VkPipelineCache pipelineCache, const VkRenderPass renderPass) {
    // Pipeline layout
    // Push constants for UI rendering parameters
    VkPushConstantRange pushConstantRange =
        vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
        vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    result = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

    // Setup graphics pipeline for UI rendering
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(
        VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    // Enable blending
    VkPipelineColorBlendAttachmentState blendAttachmentState{};
    blendAttachmentState.blendEnable = VK_TRUE;
    blendAttachmentState.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisampleState =
        vks::initializers::pipelineMultisampleStateCreateInfo(rasterizationSamples);

    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState =
        vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);

    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaders.size());
    pipelineCreateInfo.pStages = shaders.data();
    pipelineCreateInfo.subpass = subpass;

    // Vertex bindings an attributes based on ImGui vertex definition
    std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
        vks::initializers::vertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
        vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT,
                                                           offsetof(ImDrawVert, pos)), // Location 0: Position
        vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT,
                                                           offsetof(ImDrawVert, uv)), // Location 1: UV
        vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM,
                                                           offsetof(ImDrawVert, col)), // Location 0: Color
    };
    VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

    pipelineCreateInfo.pVertexInputState = &vertexInputState;

    if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Error: vkCreateGraphicsPipelines failed for imgui overlay!");
    }

    return VK_SUCCESS;
}

bool ImguiOverlay::update() {
    ImDrawData *imDrawData = ImGui::GetDrawData();
    bool updateCmdBuffers = false;

    if (!imDrawData) {
        return false;
    };

    // Note: Alignment is done inside buffer creation
    VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    // Update buffers only if vertex or index count has been changed compared to current buffer size
    if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
        return false;
    }

    if (!imgui_mesh) {
        imgui_mesh.reset();

        auto meshbuffer = wrapper::MeshBuffer(device, data_transfer_queue, data_transfer_queue_family_index,
                                              vma_allocator, "imgui_mesh_buffer", sizeof(ImDrawVert),
                                              imDrawData->TotalVtxCount, sizeof(ImDrawIdx), imDrawData->TotalIdxCount);

        imgui_mesh = std::make_unique<wrapper::MeshBuffer>(
            device, data_transfer_queue, data_transfer_queue_family_index, vma_allocator, "imgui_mesh_buffer",
            sizeof(ImDrawVert), imDrawData->TotalVtxCount, sizeof(ImDrawIdx), imDrawData->TotalIdxCount);
    }

    // Vertex buffer
    if ((imgui_mesh->get_vertex_buffer() == VK_NULL_HANDLE) || (vertexCount != imDrawData->TotalVtxCount)) {

        imgui_mesh.reset();

        // @TODO: Implement .resize() method!
        // TODO: Fix constructor for no index buffer
        imgui_mesh = std::make_unique<wrapper::MeshBuffer>(
            device, data_transfer_queue, data_transfer_queue_family_index, vma_allocator, "imgui_mesh_buffer",
            sizeof(ImDrawVert), imDrawData->TotalVtxCount, sizeof(ImDrawIdx), imDrawData->TotalIdxCount);

        vertexCount = imDrawData->TotalVtxCount;
        updateCmdBuffers = true;
    }

    // Index buffer
    VkDeviceSize indexSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    if ((imgui_mesh->get_index_buffer() == VK_NULL_HANDLE) || (indexCount < imDrawData->TotalIdxCount)) {

        imgui_mesh.reset();

        imgui_mesh = std::make_unique<wrapper::MeshBuffer>(
            device, data_transfer_queue, data_transfer_queue_family_index, vma_allocator, "imgui_mesh_buffer",
            sizeof(ImDrawVert), imDrawData->TotalVtxCount, sizeof(ImDrawIdx), imDrawData->TotalIdxCount);

        indexCount = imDrawData->TotalIdxCount;
        updateCmdBuffers = true;
    }

    // Upload data
    ImDrawVert *vtxDst = (ImDrawVert *)imgui_mesh->get_vertex_buffer_address();
    ImDrawIdx *idxDst = (ImDrawIdx *)imgui_mesh->get_index_buffer_address();
    ;

    for (int n = 0; n < imDrawData->CmdListsCount; n++) {
        const ImDrawList *cmd_list = imDrawData->CmdLists[n];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }

    // TODO: Flush to make writes visible to GPU?

    return updateCmdBuffers;
}

VkResult ImguiOverlay::draw(const VkCommandBuffer commandBuffer) {
    ImDrawData *imDrawData = ImGui::GetDrawData();
    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;

    if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) {
        return VK_SUCCESS;
    }

    ImGuiIO &io = ImGui::GetIO();

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0,
                            NULL);

    pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    pushConstBlock.translate = glm::vec2(-1.0f);
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock),
                       &pushConstBlock);

    VkDeviceSize offsets[1] = {0};

    // TODO: Refactor this!
    auto vertex_buffer = imgui_mesh->get_vertex_buffer();

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex_buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, imgui_mesh->get_index_buffer(), 0, VK_INDEX_TYPE_UINT16);

    for (int32_t i = 0; i < imDrawData->CmdListsCount; i++) {
        const ImDrawList *cmd_list = imDrawData->CmdLists[i];
        for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[j];
            VkRect2D scissorRect;
            scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
            scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
            scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
            scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
            vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
            indexOffset += pcmd->ElemCount;
        }
        vertexOffset += cmd_list->VtxBuffer.Size;
    }
    return VK_SUCCESS;
}

VkResult ImguiOverlay::resize(uint32_t width, uint32_t height) {
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)(width), (float)(height));

    return VK_SUCCESS;
}

VkResult ImguiOverlay::freeResources() {
    ImGui::DestroyContext();
    // Other resources will be shut down by manager classes!
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);

    return VK_SUCCESS;
}

bool ImguiOverlay::header(const char *caption) {
    return ImGui::CollapsingHeader(caption, ImGuiTreeNodeFlags_DefaultOpen);
}

bool ImguiOverlay::checkBox(const char *caption, bool *value) {
    bool res = ImGui::Checkbox(caption, value);
    if (res) {
        updated = true;
    };
    return res;
}

bool ImguiOverlay::checkBox(const char *caption, int32_t *value) {
    bool val = (*value == 1);
    bool res = ImGui::Checkbox(caption, &val);
    *value = val;
    if (res) {
        updated = true;
    };
    return res;
}

bool ImguiOverlay::inputFloat(const char *caption, float *value, float step, uint32_t precision) {
    bool res = ImGui::InputFloat(caption, value, step, step * 10.0f, precision);
    if (res) {
        updated = true;
    };
    return res;
}

bool ImguiOverlay::sliderFloat(const char *caption, float *value, float min, float max) {
    bool res = ImGui::SliderFloat(caption, value, min, max);
    if (res) {
        updated = true;
    };
    return res;
}

bool ImguiOverlay::sliderInt(const char *caption, int32_t *value, int32_t min, int32_t max) {
    bool res = ImGui::SliderInt(caption, value, min, max);
    if (res) {
        updated = true;
    };
    return res;
}

bool ImguiOverlay::comboBox(const char *caption, int32_t *itemindex, std::vector<std::string> items) {
    if (items.empty()) {
        return false;
    }
    std::vector<const char *> charitems;
    charitems.reserve(items.size());
    for (size_t i = 0; i < items.size(); i++) {
        charitems.push_back(items[i].c_str());
    }
    uint32_t itemCount = static_cast<uint32_t>(charitems.size());
    bool res = ImGui::Combo(caption, itemindex, &charitems[0], itemCount, itemCount);
    if (res) {
        updated = true;
    };
    return res;
}

bool ImguiOverlay::button(const char *caption) {
    bool res = ImGui::Button(caption);
    if (res) {
        updated = true;
    };
    return res;
}

void ImguiOverlay::text(const char *formatstr, ...) {
    va_list args;
    va_start(args, formatstr);
    ImGui::TextV(formatstr, args);
    va_end(args);
}
} // namespace inexor::vulkan_renderer::imgui
