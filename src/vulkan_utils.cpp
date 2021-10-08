#define VK_CHECK(CALL) \
    do { \
        VkResult result_ = CALL; \
        Assert(result_ == VK_SUCCESS); \
    } while (0)

internal u32 VulkanFindMemoryType(VkPhysicalDevice physicalDevice,
    u32 typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (u32 i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1 << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) ==
                properties)
        {
            return i;
        }
    }

    Assert(!"Unable to find suitable memory type");
    return U32_MAX;
}

internal VkSemaphore VulkanCreateSemaphore(VkDevice device)
{
    VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore;
    VK_CHECK(vkCreateSemaphore(device, &createInfo, NULL, &semaphore));
    return semaphore;
}

internal VkCommandPool VulkanCreateCommandPool(VkDevice device, u32 familyIndex)
{
    VkCommandPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    createInfo.queueFamilyIndex = familyIndex;

    VkCommandPool commandPool;
    VK_CHECK(vkCreateCommandPool(device, &createInfo, NULL, &commandPool));
    return commandPool;
}

internal VkCommandBuffer VulkanAllocateCommandBuffer(
    VkDevice device, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);

    return commandBuffer;
}

internal VulkanBuffer VulkanCreateBuffer(VkDevice device,
    VkPhysicalDevice physicalDevice, u32 size, VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags properties)
{
    VulkanBuffer buffer = {};

    VkBufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    createInfo.size = size;
    createInfo.usage = usageFlags;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &createInfo, NULL, &buffer.handle));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer.handle, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex =
        VulkanFindMemoryType(physicalDevice, memoryRequirements.memoryTypeBits,
                properties);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, NULL, &buffer.memory));
    vkBindBufferMemory(device, buffer.handle, buffer.memory, 0);

    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        vkMapMemory(device, buffer.memory, 0, size, 0, &buffer.data);
    }

    return buffer;
}

internal VulkanImage VulkanCreateImage(VkDevice device,
    VkPhysicalDevice physicalDevice, u32 width, u32 height, VkFormat format,
    VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
    b32 isCubeMap = false, u32 mipLevelCount = 1)
{
    VulkanImage image = {};

    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = mipLevelCount;
    createInfo.arrayLayers = isCubeMap ? 6 : 1;
    createInfo.format = format;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.flags = isCubeMap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

    VK_CHECK(vkCreateImage(device, &createInfo, NULL, &image.handle));

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, image.handle, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = VulkanFindMemoryType(
        physicalDevice, memoryRequirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, NULL, &image.memory));

    vkBindImageMemory(device, image.handle, image.memory, 0);

    return image;
}

internal void VulkanDestroyImage(VkDevice device, VulkanImage image)
{
    vkDestroyImage(device, image.handle, NULL);
    vkFreeMemory(device, image.memory, NULL);
}

internal VkImageView VulkanCreateImageView(VkDevice device, VkImage image,
    VkFormat format, VkImageAspectFlags aspectFlags, b32 isCubeMap = false,
    u32 mipLevelCount = 1)
{
    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    viewInfo.viewType = isCubeMap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevelCount;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = isCubeMap ? 6 : 1;

    VkImageView imageView;
    VK_CHECK(vkCreateImageView(device, &viewInfo, NULL, &imageView));
    return imageView;
}

internal VkFramebuffer VulkanCreateFramebuffer(VkDevice device,
    VkRenderPass renderPass, VkImageView imageView, VkImageView depthImageView,
    u32 framebufferWidth, u32 framebufferHeight)
{
    VkImageView attachments[] = {imageView, depthImageView};

    VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    createInfo.renderPass = renderPass;
    createInfo.attachmentCount = ArrayCount(attachments);
    createInfo.pAttachments = attachments;
    createInfo.width = framebufferWidth;
    createInfo.height = framebufferHeight;
    createInfo.layers = 1;

    VkFramebuffer framebuffer;
    VK_CHECK(vkCreateFramebuffer(device, &createInfo, NULL, &framebuffer));
    return framebuffer;
}

internal VkShaderModule LoadShader(VkDevice device, const char *path)
{
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", SHADER_PATH, path);

    DebugReadFileResult fileData = ReadEntireFile(fullPath);
    Assert(fileData.contents);
    Assert(fileData.length % 4 == 0);

    VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = fileData.length;
    createInfo.pCode = (u32 *)fileData.contents;

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, NULL, &shaderModule));
    return shaderModule;
}

internal void VulkanAllocateDescriptorSets(VkDevice device,
    VkDescriptorPool pool, VkDescriptorSetLayout *layouts, u32 count,
    VkDescriptorSet *sets)
{
    VkDescriptorSetAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = layouts;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, sets));
}

internal VkPipeline VulkanCreatePipeline(VkDevice device,
    VkPipelineCache pipelineCache, VkRenderPass renderPass,
    VkPipelineLayout layout, VkShaderModule vertexShader,
    VkShaderModule fragmentShader, VulkanPipelineDefinition *pipelineDefinition)
{
    VkGraphicsPipelineCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertexShader;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragmentShader;
    stages[1].pName = "main";

    createInfo.stageCount = ArrayCount(stages);
    createInfo.pStages = stages;

    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = pipelineDefinition->vertexStride;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInput = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    createInfo.pVertexInputState = &vertexInput;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    switch (pipelineDefinition->primitive)
    {
    case Primitive_Triangle:
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    case Primitive_Line:
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        break;
    default:
        InvalidCodePath();
        break;
    }
    createInfo.pInputAssemblyState = &inputAssembly;

    VkPipelineViewportStateCreateInfo viewportState = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    createInfo.pViewportState = &viewportState;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    switch (pipelineDefinition->polygonMode)
    {
    case PolygonMode_Fill:
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        break;
    case PolygonMode_Line:
        rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
        break;
    default:
        InvalidCodePath();
        break;
    }
    rasterizationState.lineWidth = 1.0f;

    switch (pipelineDefinition->cullMode)
    {
    case CullMode_None:
        rasterizationState.cullMode = VK_CULL_MODE_NONE;
        break;
    case CullMode_Front:
        rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
        break;
    case CullMode_Back:
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
        break;
    default:
        InvalidCodePath();
        break;
    }
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    createInfo.pRasterizationState = &rasterizationState;

    VkPipelineMultisampleStateCreateInfo multisampleState = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.pMultisampleState = &multisampleState;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilState.depthTestEnable =
        pipelineDefinition->depthTestEnabled ? VK_TRUE : VK_FALSE;
    depthStencilState.depthWriteEnable =
        pipelineDefinition->depthWriteEnabled ? VK_TRUE : VK_FALSE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;
    depthStencilState.stencilTestEnable = VK_FALSE;
    createInfo.pDepthStencilState = &depthStencilState;

    VkPipelineColorBlendAttachmentState colorAttachmentState = {};
    colorAttachmentState.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (pipelineDefinition->alphaBlendingEnabled)
    {
        colorAttachmentState.blendEnable = VK_TRUE;
        colorAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorAttachmentState.dstColorBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
        colorAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorAttachmentState;
    createInfo.pColorBlendState = &colorBlendState;

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = ArrayCount(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;
    createInfo.pDynamicState = &dynamicState;

    createInfo.layout = layout;
    createInfo.renderPass = renderPass;

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(
        device, pipelineCache, 1, &createInfo, NULL, &pipeline));
    return pipeline;
}

VkResult vkCreateDebugUtilsMessengerEXT(
    VkInstance                                  instance,
    const VkDebugUtilsMessengerCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugUtilsMessengerEXT*                   pMessenger)
{
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
    Assert(func);
    return func(instance, pCreateInfo, pAllocator, pMessenger);
}

internal VkBool32 
VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void *pUserData)
{
    // LogMessage is truncating the error message
    //LogMessage("%s", pCallbackData->pMessage);
#ifdef PLATFORM_WINDOWS
    OutputDebugString(pCallbackData->pMessage);
    OutputDebugString("\n");
#endif
    puts(pCallbackData->pMessage);

    //Assert(!(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT));
    //Assert(!(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT));

    return VK_FALSE;
}

internal VkDebugUtilsMessengerEXT VulkanRegisterDebugCallback(VkInstance instance)
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            //VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | Skip info for now
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = VulkanDebugCallback;

    VkDebugUtilsMessengerEXT messenger;
    vkCreateDebugUtilsMessengerEXT(instance, &createInfo, NULL, &messenger);
    return messenger;
}

// TODO: Handle multiple devices, dont just pick the first one!
internal VkPhysicalDevice VulkanGetPhysicalDevice(VkInstance instance)
{
    VkPhysicalDevice physicalDevice;
    u32 deviceCount = 1;
    VK_CHECK(
        vkEnumeratePhysicalDevices(instance, &deviceCount, &physicalDevice));
    Assert(deviceCount > 0);

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    LogMessage("API VERSION: %u.%u.%u\nDRIVER VERSION: %u\nVENDOR ID: %u\n"
            "DEVICE ID: %u\nDEVICE NAME: %s",
        VK_VERSION_MAJOR(properties.apiVersion),
        VK_VERSION_MINOR(properties.apiVersion),
        VK_VERSION_PATCH(properties.apiVersion),
        properties.driverVersion, properties.vendorID,
        properties.deviceID, properties.deviceName);

    return physicalDevice;
}

internal VkCommandBuffer VulkanBeginSingleTimeCommands(
    VkDevice device, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    return commandBuffer;
}

internal void VulkanEndSingleTimeCommands(VkCommandBuffer commandBuffer,
        VkDevice device, VkCommandPool commandPool, VkQueue queue)
{
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

internal void VulkanCopyBuffer(VkDevice device, VkCommandPool commandPool,
    VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer =
        VulkanBeginSingleTimeCommands(device, commandPool);

    VkBufferCopy copyRegion = {};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    VulkanEndSingleTimeCommands(commandBuffer, device, commandPool, queue);
}

internal void VulkanTransitionImageLayout(VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout, VkDevice device,
    VkCommandPool commandPool, VkQueue queue, b32 isCubeMap = false,
    u32 mipLevelCount = 1)
{
    VkCommandBuffer commandBuffer =
        VulkanBeginSingleTimeCommands(device, commandPool);

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevelCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = isCubeMap ? 6 : 1;

    VkPipelineStageFlags sourceStage = 0;
    VkPipelineStageFlags destinationStage = 0;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        InvalidCodePath();
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
        NULL, 0, NULL, 1, &barrier);

    VulkanEndSingleTimeCommands(commandBuffer, device, commandPool, queue);
}

// TODO: Can be much more efficient about this when copying multiple mip levels
internal void VulkanCopyBufferToImage(VkDevice device, VkCommandPool commandPool,
        VkQueue queue, VkBuffer buffer, VkImage image, u32 width, u32 height,
        VkDeviceSize offset, b32 isCubeMap = false, u32 mipLevel = 0)
{
    VkCommandBuffer commandBuffer =
        VulkanBeginSingleTimeCommands(device, commandPool);

    VkBufferImageCopy imageCopy = {};
    imageCopy.bufferOffset = offset;
    imageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopy.imageSubresource.mipLevel = mipLevel;
    imageCopy.imageSubresource.baseArrayLayer = 0;
    imageCopy.imageSubresource.layerCount = isCubeMap ? 6 : 1;

    imageCopy.imageExtent.width = width;
    imageCopy.imageExtent.height = height;
    imageCopy.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(commandBuffer, buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

    VulkanEndSingleTimeCommands(commandBuffer, device, commandPool, queue);
}
