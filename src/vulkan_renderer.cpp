#include "vulkan_renderer.h"

#include "vulkan_utils.cpp"

#define VALIDATION_LAYER_NAME "VK_LAYER_KHRONOS_validation"

internal b32 HasValidationLayerSupport()
{
    b32 found = false;

    VkLayerProperties layers[256];
    u32 layerCount = ArrayCount(layers);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, layers));

    for (u32 layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        if (strcmp(VALIDATION_LAYER_NAME,
                layers[layerIndex].layerName) == 0)
        {
            found = true;
            break;
        }
    }

    return found;
}

internal VkInstance VulkanCreateInstance()
{
    const char *validationLayers[] = {VALIDATION_LAYER_NAME};
    const char *requestedExtensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

    u32 glfwExtensionsCount = 0;
    const char **glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

    const char *extensions[16];
    u32 extensionCount = 0;
    for (u32 i = 0; i < ArrayCount(requestedExtensions); ++i)
    {
        Assert(extensionCount < ArrayCount(extensions));
        extensions[extensionCount++] = requestedExtensions[i];
    }

    for (u32 i = 0; i < glfwExtensionsCount; ++i)
    {
        Assert(extensionCount < ArrayCount(extensions));
        extensions[extensionCount++] = glfwExtensions[i];
    }

    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instanceCreateInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.enabledExtensionCount = extensionCount;
    instanceCreateInfo.ppEnabledExtensionNames = extensions;
#ifdef ENABLE_VALIDATION_LAYERS
    LogMessage("Enabling Vulkan validation layers");
    if (HasValidationLayerSupport())
    {
        instanceCreateInfo.enabledLayerCount = ArrayCount(validationLayers);
        instanceCreateInfo.ppEnabledLayerNames = validationLayers;
    }
    else
    {
        LogMessage("Vulkan instance does not support validation layers");
    }
#endif

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instanceCreateInfo, NULL, &instance));

    return instance;
}

internal VulkanQueueFamilyIndices VulkanGetQueueFamilies(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    VulkanQueueFamilyIndices result = {};
    result.graphicsQueue = U32_MAX;
    result.presentQueue = U32_MAX;

    VkQueueFamilyProperties queueFamilies[64];
    u32 queueFamilyCount = ArrayCount(queueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice, &queueFamilyCount, queueFamilies);

    for (u32 i = 0; i < queueFamilyCount; ++i)
    {
        if (result.graphicsQueue != U32_MAX &&
            result.presentQueue != U32_MAX)
        {
            break;
        }

        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            result.graphicsQueue = i;
        }

        b32 supported = false;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
                    physicalDevice, i, surface, &supported));
        if (supported)
        {
            result.presentQueue = i;
        }
    }
    Assert(result.graphicsQueue != U32_MAX);
    Assert(result.presentQueue != U32_MAX);

    return result;
}

internal VkDevice VulkanCreateDevice(VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VulkanQueueFamilyIndices queueFamilyIndices)
{
    float queuePriorities[] = { 1.0f };

    u32 queueCreateInfoCount = 1;
    VkDeviceQueueCreateInfo queueCreateInfos[2] = {};
    queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[0].queueFamilyIndex = queueFamilyIndices.graphicsQueue;
    queueCreateInfos[0].queueCount = 1;
    queueCreateInfos[0].pQueuePriorities = queuePriorities;

    if (queueFamilyIndices.presentQueue != queueFamilyIndices.graphicsQueue)
    {
        queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[1].queueFamilyIndex = queueFamilyIndices.presentQueue;
        queueCreateInfos[1].queueCount = 1;
        queueCreateInfos[1].pQueuePriorities = queuePriorities;
        queueCreateInfoCount = 2;
    }

    const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // TODO: Not sure if we need to test if the feature is supported?
    VkPhysicalDeviceFeatures enabledFeatures = {};
    enabledFeatures.fillModeNonSolid = VK_TRUE;
    enabledFeatures.samplerAnisotropy = VK_TRUE;
    enabledFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.ppEnabledExtensionNames = extensions;
    deviceCreateInfo.enabledExtensionCount = ArrayCount(extensions);
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

    VkDevice device;
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device));

    return device;
}


internal VkSwapchainKHR VulkanCreateSwapchain(VkDevice device,
    VkSurfaceKHR surface, u32 imageCount, u32 framebufferWidth,
    u32 framebufferHeight, VulkanQueueFamilyIndices queueFamilyIndices)
{
    u32 queueFamilyIndicesArray[2] = {
        queueFamilyIndices.graphicsQueue, queueFamilyIndices.presentQueue};

    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; // FIXME: Supported?
    createInfo.imageExtent.width = framebufferWidth;
    createInfo.imageExtent.height = framebufferHeight;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (queueFamilyIndices.graphicsQueue != queueFamilyIndices.presentQueue)
    {
        // VK_SHARING_MODE_EXCLUSIVE is defined as 0, so we don't need to
        // handle it
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = ArrayCount(queueFamilyIndicesArray);
        createInfo.pQueueFamilyIndices = queueFamilyIndicesArray;
    }
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; //VK_PRESENT_MODE_IMMEDIATE_KHR; // FIXME: Always supported?
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkSwapchainKHR swapchain;
    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, NULL, &swapchain));
    return swapchain;
}

internal VulkanSwapchain VulkanSetupSwapchain(VkDevice device,
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    VulkanQueueFamilyIndices queueFamilyIndices, VkRenderPass renderPass,
    u32 imageCount, u32 width, u32 height)
{
    VulkanSwapchain swapchain = {};
    swapchain.width = width;
    swapchain.height = height;
    swapchain.imageCount = imageCount;

    Assert(imageCount <= ArrayCount(swapchain.images));

    swapchain.handle = VulkanCreateSwapchain(device, surface,
        imageCount, width, height, queueFamilyIndices);

    VK_CHECK(vkGetSwapchainImagesKHR(
        device, swapchain.handle, &swapchain.imageCount, NULL));

    // TODO: Probably don't need to assert this, rather just check that we can
    // store the returned imageCount
    Assert(swapchain.imageCount == imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(
        device, swapchain.handle, &swapchain.imageCount, swapchain.images));

    swapchain.depthImage =
        VulkanCreateImage(device, physicalDevice, width, height,
            VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    for (u32 i = 0; i < swapchain.imageCount; ++i)
    {
        // TODO: Get supported image formats for the surface?
        swapchain.imageViews[i] =
            VulkanCreateImageView(device, swapchain.images[i],
                VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
        swapchain.framebuffers[i] = VulkanCreateFramebuffer(device, renderPass,
            swapchain.imageViews[i], swapchain.depthImage.view, width, height);
    }

    return swapchain;
}

internal void VulkanDestroySwapchain(VulkanSwapchain swapchain, VkDevice device)
{
    for (u32 i = 0; i < swapchain.imageCount; ++i)
    {
        vkDestroyFramebuffer(device, swapchain.framebuffers[i], NULL);
        vkDestroyImageView(device, swapchain.imageViews[i], NULL);
    }
    VulkanDestroyImage(device, swapchain.depthImage);

    vkDestroySwapchainKHR(device, swapchain.handle, NULL);
}

// TODO: Define constants for this
internal VkDescriptorPool VulkanCreateDescriptorPool(VkDevice device)
{
    VkDescriptorPoolSize poolSizes[5];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 64;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 64;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[2].descriptorCount = 64;
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[3].descriptorCount = 256;
    poolSizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[4].descriptorCount = 64;

    VkDescriptorPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.poolSizeCount = ArrayCount(poolSizes);
    createInfo.pPoolSizes = poolSizes;
    createInfo.maxSets = 64;

    VkDescriptorPool descriptorPool;
    VK_CHECK(vkCreateDescriptorPool(device, &createInfo, NULL, &descriptorPool));
    return descriptorPool;
}

internal VkDescriptorSetLayout VulkanCreateDescriptorSetLayout(
    VkDevice device, VkSampler sampler)
{
    VkDescriptorSetLayoutBinding layoutBindings[12] = {};
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutBindings[2].binding = 2;
    layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    layoutBindings[2].descriptorCount = 1;
    layoutBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[2].pImmutableSamplers = &sampler;
    layoutBindings[3].binding = 3;
    layoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[3].descriptorCount = 1;
    layoutBindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[4].binding = 4;
    layoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[4].descriptorCount = 1;
    layoutBindings[4].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutBindings[5].binding = 5;
    layoutBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[5].descriptorCount = 1;
    layoutBindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[6].binding = 6;
    layoutBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[6].descriptorCount = 1;
    layoutBindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[7].binding = 7;
    layoutBindings[7].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[7].descriptorCount = 1;
    layoutBindings[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[8].binding = 8;
    layoutBindings[8].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[8].descriptorCount = 1;
    layoutBindings[8].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[9].binding = 9;
    layoutBindings[9].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[9].descriptorCount = 1;
    layoutBindings[9].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[10].binding = 10;
    layoutBindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[10].descriptorCount = 1;
    layoutBindings[10].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[11].binding = 11;
    layoutBindings[11].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[11].descriptorCount = 1;
    layoutBindings[11].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.bindingCount = ArrayCount(layoutBindings);
    createInfo.pBindings = layoutBindings;

    VkDescriptorSetLayout descriptorSetLayout;

    VK_CHECK(vkCreateDescriptorSetLayout(
        device, &createInfo, NULL, &descriptorSetLayout));

    return descriptorSetLayout;
}

internal VkDescriptorSetLayout VulkanCreateDebugDrawDescriptorSetLayout(
    VkDevice device)
{
    VkDescriptorSetLayoutBinding layoutBindings[2] = {};
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.bindingCount = ArrayCount(layoutBindings);
    createInfo.pBindings = layoutBindings;

    VkDescriptorSetLayout descriptorSetLayout;

    VK_CHECK(vkCreateDescriptorSetLayout(
        device, &createInfo, NULL, &descriptorSetLayout));

    return descriptorSetLayout;
}

internal VkPipelineLayout VulkanCreatePipelineLayout(VkDevice device,
    VkDescriptorSetLayout descriptorSetLayout, b32 usePushConstants)
{
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(MeshPushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &descriptorSetLayout;
    if (usePushConstants)
    {
        createInfo.pPushConstantRanges = &pushConstantRange;
        createInfo.pushConstantRangeCount = 1;
    }

    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, 0, &layout));
    return layout;
}

internal void VulkanCopyImageFromCPU(VulkanRenderer *renderer)
{
    u32 width = RAY_TRACER_WIDTH;
    u32 height = RAY_TRACER_HEIGHT;

    VulkanTransitionImageLayout(renderer->images[Image_CpuRayTracer].handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, renderer->device,
        renderer->commandPool, renderer->graphicsQueue);

    VulkanCopyBufferToImage(renderer->device, renderer->commandPool,
        renderer->graphicsQueue, renderer->imageUploadBuffer.handle,
        renderer->images[Image_CpuRayTracer].handle, width, height, 0);

    VulkanTransitionImageLayout(renderer->images[Image_CpuRayTracer].handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, renderer->device,
        renderer->commandPool, renderer->graphicsQueue);
}

internal void VulkanCopyMeshDataToGpu(VulkanRenderer *renderer)
{
    VulkanCopyBuffer(renderer->device, renderer->commandPool,
        renderer->graphicsQueue, renderer->vertexDataUploadBuffer.handle,
        renderer->vertexDataBuffer.handle,
        renderer->vertexDataUploadBufferSize);

    VulkanCopyBuffer(renderer->device, renderer->commandPool,
        renderer->graphicsQueue, renderer->indexUploadBuffer.handle,
        renderer->indexBuffer.handle, 
        renderer->indexUploadBufferSize);
}

internal void UploadSceneDataToGpu(VulkanRenderer *renderer, Scene scene)
{
    mat4 *modelMatrices = (mat4 *)renderer->modelMatricesBuffer.data;
    mat4 *invModelMatrices = (mat4 *)renderer->computeInvModelMatricesBuffer.data;
    modelMatrices[0] = Identity(); // 0 slot reserved for skybox
    invModelMatrices[0] = Identity(); // 0 slot reserved for skybox

    for (u32 i = 0; i < scene.count; ++i)
    {
        Entity *entity = scene.entities + i;
        modelMatrices[i + 1] = Translate(entity->position) *
                               Rotate(entity->rotation) * Scale(entity->scale);
        invModelMatrices[i + 1] = Scale(Inverse(entity->scale)) * 
                                Rotate(Conjugate(entity->rotation)) *
                                Translate(-entity->position);
    }
}

// FIXME: Why does this share the same descriptor sets as the mesh shader!
// Because we use the mesh.vert.glsl shader
internal void UpdatePostProcessingDescriptorSets(VulkanRenderer *renderer)
{
    // Post processing descriptor sets
    for (u32 i = 0; i < renderer->swapchain.imageCount; ++i)
    {
        VkDescriptorBufferInfo uniformBufferInfo = {};
        uniformBufferInfo.buffer = renderer->uniformBuffer.handle;
        uniformBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo vertexDataBufferInfo = {};
        vertexDataBufferInfo.buffer =
            renderer->vertexDataBuffer.handle;
        vertexDataBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo modelMatrixBufferInfo = {};
        modelMatrixBufferInfo.buffer = renderer->modelMatricesBuffer.handle;
        modelMatrixBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorImageInfo vulkanImageInfo = {};
        vulkanImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vulkanImageInfo.imageView = renderer->hdrSwapchain.framebuffers[i].color.view;

        VkDescriptorImageInfo rayTracerImageInfo = {};
        rayTracerImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        rayTracerImageInfo.imageView = renderer->images[Image_CpuRayTracer].view;

        VkDescriptorImageInfo computeShaderImageInfo = {};
        computeShaderImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        computeShaderImageInfo.imageView = renderer->images[Image_ComputeShader].view;

        VkWriteDescriptorSet descriptorWrites[6] = {};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = renderer->postProcessDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uniformBufferInfo;
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = renderer->postProcessDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &vertexDataBufferInfo;
        // Binding 2 is for the defaultSampler
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = renderer->postProcessDescriptorSets[i];
        descriptorWrites[2].dstBinding = 3;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &vulkanImageInfo;
        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = renderer->postProcessDescriptorSets[i];
        descriptorWrites[3].dstBinding = 4;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &modelMatrixBufferInfo;
        // Binding 5 is for the material data
        // Binding 6 is for the test cube map
        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = renderer->postProcessDescriptorSets[i];
        descriptorWrites[4].dstBinding = 9;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pImageInfo = &rayTracerImageInfo;
        // Binding 10 is the lighting data
        descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[5].dstSet = renderer->postProcessDescriptorSets[i];
        descriptorWrites[5].dstBinding = 11;
        descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pImageInfo = &computeShaderImageInfo;
        vkUpdateDescriptorSets(renderer->device, ArrayCount(descriptorWrites),
            descriptorWrites, 0, NULL);
    }

}

internal VkPipeline VulkanCreateComputePipeline(VkDevice device,
        VkPipelineCache pipelineCache, VkPipelineLayout pipelineLayout,
        VkShaderModule shaderModule)
{
    VkPipeline pipeline = VK_NULL_HANDLE;

    VkComputePipelineCreateInfo computePipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};

    computePipelineCreateInfo.stage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineCreateInfo.stage.module = shaderModule;
    computePipelineCreateInfo.stage.pName = "main";
    computePipelineCreateInfo.layout = pipelineLayout;

    VK_CHECK(vkCreateComputePipelines(
        device, pipelineCache, 1, &computePipelineCreateInfo, NULL, &pipeline));

    return pipeline;
}

internal VkPipelineLayout VulkanCreateComputePipelineLayout(
    VkDevice device, VkDescriptorSetLayout descriptorSetLayout)
{
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ComputePushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &descriptorSetLayout;
    createInfo.pPushConstantRanges = &pushConstantRange;
    createInfo.pushConstantRangeCount = 1;

    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, 0, &layout));
    return layout;
}

internal VkDescriptorSetLayout VulkanCreateComputeDescriptorSetLayout(
    VkDevice device, VkSampler sampler)
{
    VkDescriptorSetLayoutBinding layoutBindings[10] = {};
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[2].binding = 2;
    layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    layoutBindings[2].descriptorCount = 1;
    layoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[2].pImmutableSamplers = &sampler;
    layoutBindings[3].binding = 8;
    layoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[3].descriptorCount = 1;
    layoutBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[4].binding = 3;
    layoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[4].descriptorCount = 1;
    layoutBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[5].binding = 4;
    layoutBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[5].descriptorCount = 1;
    layoutBindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[6].binding = 5;
    layoutBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[6].descriptorCount = 1;
    layoutBindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[7].binding = 6;
    layoutBindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[7].descriptorCount = 1;
    layoutBindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[8].binding = 7;
    layoutBindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[8].descriptorCount = 1;
    layoutBindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[9].binding = 9;
    layoutBindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[9].descriptorCount = 1;
    layoutBindings[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.bindingCount = ArrayCount(layoutBindings);
    createInfo.pBindings = layoutBindings;

    VkDescriptorSetLayout descriptorSetLayout;

    VK_CHECK(vkCreateDescriptorSetLayout(
        device, &createInfo, NULL, &descriptorSetLayout));

    return descriptorSetLayout;
}

internal void VulkanUpdateComputeDescriptorSets(VkDevice device,
    VkDescriptorSet set, VulkanImage image, VulkanBuffer uniformBuffer,
    VulkanBuffer vertexDataBuffer, VulkanBuffer indexBuffer,
    VulkanBuffer meshBuffer, VulkanBuffer tileQueueBuffer,
    VulkanBuffer modelMatricesBuffer, VulkanBuffer sceneBuffer,
    VulkanBuffer invModelMatricesBuffer)
{
    VkDescriptorImageInfo outputImageInfo = {};
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputImageInfo.imageView = image.view;

    VkDescriptorBufferInfo uniformBufferInfo = {};
    uniformBufferInfo.buffer = uniformBuffer.handle;
    uniformBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo vertexDataBufferInfo = {};
    vertexDataBufferInfo.buffer = vertexDataBuffer.handle;
    vertexDataBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo indexBufferInfo = {};
    indexBufferInfo.buffer = indexBuffer.handle;
    indexBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo meshBufferInfo = {};
    meshBufferInfo.buffer = meshBuffer.handle;
    meshBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo modelMatricsBufferInfo = {};
    modelMatricsBufferInfo.buffer = modelMatricesBuffer.handle;
    modelMatricsBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo sceneBufferInfo = {};
    sceneBufferInfo.buffer = sceneBuffer.handle;
    sceneBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo invModelMatricesBufferInfo = {};
    invModelMatricesBufferInfo.buffer = invModelMatricesBuffer.handle;
    invModelMatricesBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrites[8] = {};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = set;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &outputImageInfo;
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = set;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &uniformBufferInfo;
    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = set;
    descriptorWrites[2].dstBinding = 3;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &vertexDataBufferInfo;
    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = set;
    descriptorWrites[3].dstBinding = 4;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &indexBufferInfo;
    descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[4].dstSet = set;
    descriptorWrites[4].dstBinding = 5;
    descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[4].descriptorCount = 1;
    descriptorWrites[4].pBufferInfo = &meshBufferInfo;
    descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[5].dstSet = set;
    descriptorWrites[5].dstBinding = 6;
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pBufferInfo = &modelMatricsBufferInfo;
    descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[6].dstSet = set;
    descriptorWrites[6].dstBinding = 7;
    descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[6].descriptorCount = 1;
    descriptorWrites[6].pBufferInfo = &sceneBufferInfo;
    descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[7].dstSet = set;
    descriptorWrites[7].dstBinding = 9;
    descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[7].descriptorCount = 1;
    descriptorWrites[7].pBufferInfo = &invModelMatricesBufferInfo;

    vkUpdateDescriptorSets(
        device, ArrayCount(descriptorWrites), descriptorWrites, 0, NULL);
}

struct ComputeMesh
{
    u32 indexCount;
    u32 indicesOffset;
    u32 vertexDataOffset;
};

internal void VulkanUploadComputeMeshData(VulkanRenderer *renderer)
{
    ComputeMesh *meshBuffer = (ComputeMesh *)renderer->computeMeshBuffer.data;
    for (u32 i = 0; i < ArrayCount(renderer->meshes); ++i)
    {
        Mesh in = renderer->meshes[i];
        ComputeMesh *out = meshBuffer + i;
        out->indexCount = in.indexCount;
        out->indicesOffset = in.indexDataOffset / sizeof(u32);
        out->vertexDataOffset = in.vertexDataOffset;
    }
}

internal void VulkanUploadComputeTileQueue(VulkanRenderer *renderer)
{
    // FIXME: Don't duplicate these constants!
    u32 tileWidth = 16;
    u32 tileHeight = 16;
    u32 imageWidth = RAY_TRACER_WIDTH;
    u32 imageHeight = RAY_TRACER_HEIGHT;
    u32 tileCountX = imageWidth / tileWidth;
    u32 tileCountY = imageHeight / tileHeight;

    renderer->computeTileQueueTail = 0;
    renderer->computeTileQueueHead = 0;
    for (u32 y = 0; y < tileCountY; y++)
    {
        for (u32 x = 0; x < tileCountX; x++)
        {
            Assert(renderer->computeTileQueueTail <
                   ArrayCount(renderer->computeTileQueue));
            renderer->computeTileQueue[renderer->computeTileQueueTail++] =
                y * tileCountX + x;
        }
    }
}

internal void VulkanUploadComputeSceneBuffer(
    VulkanRenderer *renderer, Scene scene)
{
    ComputeSceneBuffer *buffer =
        (ComputeSceneBuffer *)renderer->computeSceneBuffer.data;
    buffer->count = scene.count;

    for (u32 i = 0; i < scene.count; ++i)
    {
        ComputeEntity *entity = buffer->entities + i;
        entity->meshIndex = scene.entities[i].mesh;
        entity->materialId = scene.entities[i].material;

        // HACK copied from mesh rendering
        entity->modelMatrixIndex = i + 1; // FIXME: 0 is reserved for skybox
    }
}

// TODO: Handle errors gracefully?
internal void VulkanInit(VulkanRenderer *renderer, GLFWwindow *window)
{
    // Create instance, debug callback
    renderer->instance = VulkanCreateInstance();
    renderer->messenger = VulkanRegisterDebugCallback(renderer->instance);

    // Select physical device
    renderer->physicalDevice = VulkanGetPhysicalDevice(renderer->instance);

    // Create surface
    VK_CHECK(glfwCreateWindowSurface(
        renderer->instance, window, NULL, &renderer->surface));

    // Create logical device
    renderer->queueFamilyIndices =
        VulkanGetQueueFamilies(renderer->physicalDevice, renderer->surface);
    renderer->device = VulkanCreateDevice(renderer->instance,
        renderer->physicalDevice, renderer->queueFamilyIndices);

    // Retrieve graphics and present queue
    vkGetDeviceQueue(renderer->device,
        renderer->queueFamilyIndices.graphicsQueue, 0,
        &renderer->graphicsQueue);
    vkGetDeviceQueue(renderer->device,
        renderer->queueFamilyIndices.presentQueue, 0, &renderer->presentQueue);

    // FIXME: Query surface formats and present modes
    VkSurfaceCapabilitiesKHR surfaceCapabilities = {
        VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        renderer->physicalDevice, renderer->surface, &surfaceCapabilities));
    Assert(surfaceCapabilities.currentTransform &
           VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);

    VkSurfaceFormatKHR surfaceFormats[8];
    u32 surfaceFormatCount = ArrayCount(surfaceFormats);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physicalDevice,
        renderer->surface, &surfaceFormatCount, surfaceFormats));
    Assert(surfaceFormats[0].format == VK_FORMAT_B8G8R8A8_UNORM);
    Assert(surfaceFormats[0].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);

    // Create render pass
    VulkanRenderPassSpec displayRenderPassSpec = {};
    displayRenderPassSpec.colorAttachmentFormat = surfaceFormats[0].format;
    displayRenderPassSpec.colorAttachmentFinalLayout =
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    displayRenderPassSpec.useDepth = true;
    renderer->renderPass =
        VulkanCreateRenderPass(renderer->device, displayRenderPassSpec);

    // TODO: Config option to support 16-bit floats?
    VkFormat linearFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    VulkanRenderPassSpec hdrRenderPassSpec = {};
    hdrRenderPassSpec.colorAttachmentFormat = linearFormat;
    hdrRenderPassSpec.colorAttachmentFinalLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdrRenderPassSpec.useDepth = true;
    renderer->hdrRenderPass =
        VulkanCreateRenderPass(renderer->device, hdrRenderPassSpec);

    renderer->hdrSwapchain = CreateHdrSwapchain(renderer->device,
        renderer->physicalDevice, linearFormat,
        surfaceCapabilities.currentExtent.width,
        surfaceCapabilities.currentExtent.height, 2, renderer->hdrRenderPass);

    // Create swapchain
    renderer->swapchain =
        VulkanSetupSwapchain(renderer->device, renderer->physicalDevice,
            renderer->surface, renderer->queueFamilyIndices,
            renderer->renderPass, 2, surfaceCapabilities.currentExtent.width,
            surfaceCapabilities.currentExtent.height);

    renderer->acquireSemaphore = VulkanCreateSemaphore(renderer->device);
    renderer->releaseSemaphore = VulkanCreateSemaphore(renderer->device);

    // Create command pool
    renderer->commandPool = VulkanCreateCommandPool(
        renderer->device, renderer->queueFamilyIndices.graphicsQueue);

    // Create descriptor pool
    renderer->descriptorPool = VulkanCreateDescriptorPool(renderer->device);

    // Create command buffer
    renderer->commandBuffer =
        VulkanAllocateCommandBuffer(renderer->device, renderer->commandPool);

    // Create vertex data upload buffer
    renderer->vertexDataUploadBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            VERTEX_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Create vertex data buffer
    renderer->vertexDataBuffer = VulkanCreateBuffer(renderer->device,
        renderer->physicalDevice, VERTEX_BUFFER_SIZE,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    renderer->uniformBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            UNIFORM_BUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    renderer->indexUploadBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            INDEX_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    renderer->indexBuffer = VulkanCreateBuffer(renderer->device,
        renderer->physicalDevice, INDEX_BUFFER_SIZE,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    renderer->imageUploadBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            IMAGE_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    renderer->debugVertexDataBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            DEBUG_VERTEX_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    renderer->modelMatricesBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            MODEL_MATRICES_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    renderer->materialBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            MATERIAL_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // TODO: Combine this with materialBuffer?
    renderer->lightBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            LIGHT_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    renderer->textureUploadBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            TEXTURE_UPLOAD_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Compute shader uniform buffer
    renderer->computeUniformBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            COMPUTE_UNIFORM_BUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Compute shader buffer of mesh data (vertex + index offsets)
    renderer->computeMeshBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            COMPUTE_MESH_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    renderer->computeTileQueueBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            COMPUTE_TILE_QUEUE_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    renderer->computeSceneBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            COMPUTE_SCENE_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    renderer->computeInvModelMatricesBuffer =
        VulkanCreateBuffer(renderer->device, renderer->physicalDevice,
            COMPUTE_INV_MODEL_MATRICES_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Load shaders
    renderer->testVertexShader = LoadShader(renderer->device, "mesh.vert.spv");
    renderer->testFragmentShader = LoadShader(renderer->device, "mesh.frag.spv");

    // Load fullscreen quad shaders
    renderer->fullscreenQuadVertexShader = LoadShader(renderer->device, "fullscreen_quad.vert.spv");

    // Debug draw shaders
    renderer->debugDrawVertexShader = LoadShader(renderer->device, "debug_draw.vert.spv");
    renderer->debugDrawFragmentShader = LoadShader(renderer->device, "debug_draw.frag.spv");

    // Post processing shader
    renderer->postProcessingFragmentShader = LoadShader(renderer->device, "post_processing.frag.spv");

    // Skybox shader
    renderer->skyboxFragmentShader = LoadShader(renderer->device, "skybox.frag.spv");

    // Compute shader
    renderer->computeShader = LoadShader(renderer->device, "test_compute.comp.spv");

    // TODO: CLAMP_TO_EDGE sampler 
    VkSamplerCreateInfo samplerCreateInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = 16.0f;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 1000.0f;
    VK_CHECK(vkCreateSampler(
        renderer->device, &samplerCreateInfo, NULL, &renderer->defaultSampler));

    // Create descriptor set layout
    renderer->descriptorSetLayout = VulkanCreateDescriptorSetLayout(
        renderer->device, renderer->defaultSampler);

    renderer->debugDrawDescriptorSetLayout =
        VulkanCreateDebugDrawDescriptorSetLayout(renderer->device);

    renderer->computeDescriptorSetLayout =
        VulkanCreateComputeDescriptorSetLayout(
            renderer->device, renderer->defaultSampler);

    // Create pipeline layout
    renderer->pipelineLayout = VulkanCreatePipelineLayout(
        renderer->device, renderer->descriptorSetLayout, true);

    renderer->debugDrawPipelineLayout = VulkanCreatePipelineLayout(
        renderer->device, renderer->debugDrawDescriptorSetLayout, false);

    renderer->computePipelineLayout = VulkanCreateComputePipelineLayout(
        renderer->device, renderer->computeDescriptorSetLayout);

    // Create pipeline
    VulkanPipelineDefinition pipelineDefinition = {};
    pipelineDefinition.vertexStride = sizeof(VertexPNT);
    pipelineDefinition.primitive = Primitive_Triangle;
    pipelineDefinition.polygonMode = PolygonMode_Fill;
    pipelineDefinition.cullMode = CullMode_Back;
    pipelineDefinition.depthTestEnabled = true;
    pipelineDefinition.depthWriteEnabled = true;

    renderer->pipeline = VulkanCreatePipeline(renderer->device,
        renderer->pipelineCache, renderer->hdrRenderPass,
        renderer->pipelineLayout, renderer->testVertexShader,
        renderer->testFragmentShader, &pipelineDefinition);

    VulkanPipelineDefinition postProcessingPipelineDefinition = {};
    postProcessingPipelineDefinition.vertexStride = sizeof(VertexPNT);
    postProcessingPipelineDefinition.primitive = Primitive_Triangle;
    postProcessingPipelineDefinition.polygonMode = PolygonMode_Fill;
    postProcessingPipelineDefinition.cullMode = CullMode_None;
    postProcessingPipelineDefinition.depthTestEnabled = false;
    postProcessingPipelineDefinition.depthWriteEnabled = false;

    renderer->postProcessPipeline = VulkanCreatePipeline(renderer->device,
        renderer->pipelineCache, renderer->renderPass, renderer->pipelineLayout,
        renderer->fullscreenQuadVertexShader,
        renderer->postProcessingFragmentShader,
        &postProcessingPipelineDefinition);

    // Create debug draw pipeline
    VulkanPipelineDefinition debugDrawPipelineDefinition = {};
    debugDrawPipelineDefinition.vertexStride = sizeof(VertexPC);
    debugDrawPipelineDefinition.primitive = Primitive_Line;
    debugDrawPipelineDefinition.polygonMode = PolygonMode_Fill;
    debugDrawPipelineDefinition.cullMode = CullMode_None;
    debugDrawPipelineDefinition.depthTestEnabled = false;
    debugDrawPipelineDefinition.depthWriteEnabled = false;

    renderer->debugDrawPipeline = VulkanCreatePipeline(renderer->device,
        renderer->pipelineCache, renderer->hdrRenderPass,
        renderer->debugDrawPipelineLayout, renderer->debugDrawVertexShader,
        renderer->debugDrawFragmentShader, &debugDrawPipelineDefinition);


    // Skybox pipeline
    VulkanPipelineDefinition skyboxPipelineDefinition = {};
    skyboxPipelineDefinition.vertexStride = sizeof(VertexPNT);
    skyboxPipelineDefinition.primitive = Primitive_Triangle;
    skyboxPipelineDefinition.polygonMode = PolygonMode_Fill;
    skyboxPipelineDefinition.cullMode = CullMode_Front;
    skyboxPipelineDefinition.depthTestEnabled = false;
    skyboxPipelineDefinition.depthWriteEnabled = false;

    renderer->skyboxPipeline = VulkanCreatePipeline(renderer->device,
        renderer->pipelineCache, renderer->hdrRenderPass,
        renderer->pipelineLayout, renderer->testVertexShader,
        renderer->skyboxFragmentShader, &skyboxPipelineDefinition);

    // Create compute shader pipeline
    renderer->computePipeline =
        VulkanCreateComputePipeline(renderer->device, renderer->pipelineCache,
            renderer->computePipelineLayout, renderer->computeShader);

    // Allocate descriptor sets
    {
        VkDescriptorSetLayout layouts[2] = {
            renderer->descriptorSetLayout, renderer->descriptorSetLayout};
        Assert(ArrayCount(layouts) == ArrayCount(renderer->descriptorSets));
        VulkanAllocateDescriptorSets(renderer->device, renderer->descriptorPool,
            layouts, ArrayCount(layouts), renderer->descriptorSets);
    }

    {
        VkDescriptorSetLayout layouts[2] = {
            renderer->descriptorSetLayout, renderer->descriptorSetLayout};
        Assert(ArrayCount(layouts) ==
               ArrayCount(renderer->postProcessDescriptorSets));
        VulkanAllocateDescriptorSets(renderer->device, renderer->descriptorPool,
            layouts, ArrayCount(layouts), renderer->postProcessDescriptorSets);
    }

    // Allocate debug draw descriptor sets
    {
        VkDescriptorSetLayout layouts[2] = {
            renderer->debugDrawDescriptorSetLayout,
            renderer->debugDrawDescriptorSetLayout};
        Assert(ArrayCount(layouts) == ArrayCount(renderer->debugDrawDescriptorSets));
        VulkanAllocateDescriptorSets(renderer->device, renderer->descriptorPool,
            layouts, ArrayCount(layouts), renderer->debugDrawDescriptorSets);
    }

    // Alocate compute shader descriptor sets
    {
        VkDescriptorSetLayout layouts[2] = {
            renderer->computeDescriptorSetLayout,
            renderer->computeDescriptorSetLayout};
        Assert(ArrayCount(layouts) == ArrayCount(renderer->computeDescriptorSets));
        VulkanAllocateDescriptorSets(renderer->device, renderer->descriptorPool,
            layouts, ArrayCount(layouts), renderer->computeDescriptorSets);
    }

    // Populate vertex data buffer
    VertexPNT *vertices = (VertexPNT *)renderer->vertexDataUploadBuffer.data;
    vertices[0].position = Vec3(-0.5, -0.5, 0.0);
    vertices[0].normal = Vec3(0.0, 0.0, 1.0);
    vertices[0].textureCoord = Vec2(0.0, 0.0);
    vertices[1].position = Vec3(0.5, -0.5, 0.0);
    vertices[1].normal = Vec3(0.0, 0.0, 1.0);
    vertices[1].textureCoord = Vec2(1.0, 0.0);
    vertices[2].position = Vec3(0.0, 0.5, 0.0);
    vertices[2].normal = Vec3(0.0, 0.0, 1.0);
    vertices[2].textureCoord = Vec2(0.0, 1.0);

    VulkanCopyBuffer(renderer->device, renderer->commandPool,
        renderer->graphicsQueue, renderer->vertexDataUploadBuffer.handle,
        renderer->vertexDataBuffer.handle, sizeof(VertexPNT) * 3);

    // Populate index buffer
    u32 *indices = (u32 *)renderer->indexUploadBuffer.data;
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;

    VulkanCopyBuffer(renderer->device, renderer->commandPool,
        renderer->graphicsQueue, renderer->indexUploadBuffer.handle,
        renderer->indexBuffer.handle, sizeof(u32) * 3);

    // Create image from CPU ray tracer
    {
        u32 width = RAY_TRACER_WIDTH;
        u32 height = RAY_TRACER_HEIGHT;
        VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        renderer->images[Image_CpuRayTracer] = VulkanCreateImage(
            renderer->device, renderer->physicalDevice, width, height, format,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        VulkanTransitionImageLayout(renderer->images[Image_CpuRayTracer].handle,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            renderer->device, renderer->commandPool, renderer->graphicsQueue);
    }

    // Create image for compute shader
    {
        u32 width = RAY_TRACER_WIDTH;
        u32 height = RAY_TRACER_HEIGHT;
        VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        renderer->images[Image_ComputeShader] = VulkanCreateImage(
            renderer->device, renderer->physicalDevice, width, height, format,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        VulkanTransitionImageLayout(renderer->images[Image_ComputeShader].handle,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
            renderer->device, renderer->commandPool, renderer->graphicsQueue);
    }

    // Update descriptor sets
    Assert(renderer->swapchain.imageCount == 2);
    for (u32 i = 0; i < renderer->swapchain.imageCount; ++i)
    {
        VkDescriptorBufferInfo uniformBufferInfo = {};
        uniformBufferInfo.buffer = renderer->uniformBuffer.handle;
        uniformBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo vertexDataBufferInfo = {};
        vertexDataBufferInfo.buffer =
            renderer->vertexDataBuffer.handle;
        vertexDataBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo modelMatrixBufferInfo = {};
        modelMatrixBufferInfo.buffer = renderer->modelMatricesBuffer.handle;
        modelMatrixBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo materialBufferInfo = {};
        materialBufferInfo.buffer = renderer->materialBuffer.handle;
        materialBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo lightBufferInfo = {};
        lightBufferInfo.buffer = renderer->lightBuffer.handle;
        lightBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = renderer->images[Image_CpuRayTracer].view;

        VkWriteDescriptorSet descriptorWrites[6] = {};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = renderer->descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uniformBufferInfo;
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = renderer->descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &vertexDataBufferInfo;
        // Binding 2 is for the defaultSampler
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = renderer->descriptorSets[i];
        descriptorWrites[2].dstBinding = 3;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &imageInfo;
        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = renderer->descriptorSets[i];
        descriptorWrites[3].dstBinding = 4;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &modelMatrixBufferInfo;
        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = renderer->descriptorSets[i];
        descriptorWrites[4].dstBinding = 5;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pBufferInfo = &materialBufferInfo;
        descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[5].dstSet = renderer->descriptorSets[i];
        descriptorWrites[5].dstBinding = 10;
        descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pBufferInfo = &lightBufferInfo;

        vkUpdateDescriptorSets(renderer->device, ArrayCount(descriptorWrites),
            descriptorWrites, 0, NULL);
    }

    UpdatePostProcessingDescriptorSets(renderer);

    // Update debug draw descriptor sets
    for (u32 i = 0; i < renderer->swapchain.imageCount; ++i)
    {
        VkDescriptorBufferInfo uniformBufferInfo = {};
        uniformBufferInfo.buffer = renderer->uniformBuffer.handle;
        uniformBufferInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo vertexDataBufferInfo = {};
        vertexDataBufferInfo.buffer =
            renderer->debugVertexDataBuffer.handle;
        vertexDataBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet descriptorWrites[2] = {};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = renderer->debugDrawDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uniformBufferInfo;
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = renderer->debugDrawDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &vertexDataBufferInfo;
        vkUpdateDescriptorSets(renderer->device, ArrayCount(descriptorWrites),
            descriptorWrites, 0, NULL);
    }

    for (u32 i = 0; i < renderer->swapchain.imageCount; ++i)
    {
        VulkanUpdateComputeDescriptorSets(renderer->device,
            renderer->computeDescriptorSets[i],
            renderer->images[Image_ComputeShader],
            renderer->computeUniformBuffer,
            renderer->vertexDataBuffer,
            renderer->indexBuffer,
            renderer->computeMeshBuffer,
            renderer->computeTileQueueBuffer,
            renderer->modelMatricesBuffer,
            renderer->computeSceneBuffer,
            renderer->computeInvModelMatricesBuffer);
    }
}

internal void VulkanRender(
    VulkanRenderer *renderer, u32 outputFlags, Scene scene)
{
    UploadSceneDataToGpu(renderer, scene);

    u32 imageIndex;
    VK_CHECK(vkAcquireNextImageKHR(renderer->device, renderer->swapchain.handle,
        0, renderer->acquireSemaphore, VK_NULL_HANDLE, &imageIndex));

    VK_CHECK(vkResetCommandPool(renderer->device, renderer->commandPool, 0));

    VkCommandBufferBeginInfo beginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(renderer->commandBuffer, &beginInfo));

    if (outputFlags & Output_RunPathTracingComputeShader)
    {
        // Execution barrier
        vkCmdPipelineBarrier(renderer->commandBuffer,
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 0, NULL);

        vkCmdBindPipeline(renderer->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                renderer->computePipeline);
        vkCmdBindDescriptorSets(renderer->commandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE, renderer->computePipelineLayout, 0, 1,
                &renderer->computeDescriptorSets[imageIndex], 0, NULL);

        //u32 tileCountX = RAY_TRACER_WIDTH / 64;
        //u32 tileCountY = RAY_TRACER_HEIGHT / 64;
        // TODO: Figure out max number of tiles to render per frame
        u32 maxTilesPerFrame = COMPUTE_MAX_TILES_PER_FRAME;

        u32 queueLength = renderer->computeTileQueueTail - renderer->computeTileQueueHead;
        if (queueLength > 0)
        {
            u32 tilesToDispatch = MinU32(queueLength, maxTilesPerFrame);

            // FIXME: THIS IS TERRIBLE NEEDS SYNC!!!
            // Copy from CPU queue to GPU queue
            //CopyMemory(renderer->computeTileQueueBuffer.data,
                //renderer->computeTileQueue + renderer->computeTileQueueHead,
                //tilesToDispatch * sizeof(u32));

            ComputePushConstants pushConstants = {};
            CopyMemory(pushConstants.tileIndices,
                renderer->computeTileQueue + renderer->computeTileQueueHead,
                tilesToDispatch * sizeof(u32));

#if 0
            for (u32 i = 0; i < tilesToDispatch; ++i)
            {
                LogMessage("Rendering tile %u",
                        pushConstants.tileIndices[i]);
            }
#endif

            // Advance queue head
            renderer->computeTileQueueHead += tilesToDispatch;
            Assert(renderer->computeTileQueueHead <= renderer->computeTileQueueTail);

            vkCmdPushConstants(renderer->commandBuffer, renderer->computePipelineLayout,
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants),
                    &pushConstants);

            vkCmdDispatch(
                    renderer->commandBuffer, tilesToDispatch, 1, 1);
        }
    }

    // Execution barrier
    vkCmdPipelineBarrier(renderer->commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0, 0, NULL, 0, NULL, 0, NULL);

    // Begin render pass
    VkClearValue clearValues[2] = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassBegin = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassBegin.renderPass = renderer->hdrRenderPass;
    renderPassBegin.framebuffer = renderer->hdrSwapchain.framebuffers[imageIndex].handle;
    renderPassBegin.renderArea.extent.width = renderer->swapchain.width;
    renderPassBegin.renderArea.extent.height = renderer->swapchain.height;
    renderPassBegin.clearValueCount = ArrayCount(clearValues);
    renderPassBegin.pClearValues = clearValues;
    vkCmdBeginRenderPass(
        renderer->commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

    // Set dynamic pipeline state
    VkViewport viewport = {0, 0, (f32)renderer->swapchain.width,
        (f32)renderer->swapchain.height, 0.0f, 1.0f};
    VkRect2D scissor = {
        0, 0, renderer->swapchain.width, renderer->swapchain.height};
    vkCmdSetViewport(renderer->commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(renderer->commandBuffer, 0, 1, &scissor);

    // Perform scene render pass
    // Bind pipeline
    vkCmdBindDescriptorSets(renderer->commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->pipelineLayout, 0, 1,
        &renderer->descriptorSets[imageIndex], 0, NULL);

#if 1
    // Draw skybox
    {
        u32 meshIndex = Mesh_Cube;
        u32 materialIndex = scene.backgroundMaterial;
        Mesh mesh = renderer->meshes[meshIndex];
        vkCmdBindPipeline(renderer->commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->skyboxPipeline);

        // FIXME: Code dup
        vkCmdBindIndexBuffer(renderer->commandBuffer,
            renderer->indexBuffer.handle, mesh.indexDataOffset,
            VK_INDEX_TYPE_UINT32);

        MeshPushConstants pushConstants = {};
        pushConstants.modelMatrixIndex = 0;
        pushConstants.vertexDataOffset = mesh.vertexDataOffset;
        pushConstants.materialIndex = materialIndex;
        pushConstants.cameraIndex = 1; // Skybox camera

        vkCmdPushConstants(renderer->commandBuffer, renderer->pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants),
            &pushConstants);

        vkCmdDrawIndexed(renderer->commandBuffer, mesh.indexCount, 1, 0, 0, 0);
    }
#endif

    // Draw mesh
    for (u32 i = 0; i < scene.count; ++i)
    {
        u32 meshIndex = scene.entities[i].mesh;
        Mesh mesh = renderer->meshes[meshIndex];
        u32 materialIndex = scene.entities[i].material;

        vkCmdBindPipeline(renderer->commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->pipeline);

        // Bind index buffer
        vkCmdBindIndexBuffer(renderer->commandBuffer,
            renderer->indexBuffer.handle, mesh.indexDataOffset,
            VK_INDEX_TYPE_UINT32);

        MeshPushConstants pushConstants = {};
        pushConstants.modelMatrixIndex = i + 1; // FIXME: 0 is reserved for skybox
        pushConstants.vertexDataOffset = mesh.vertexDataOffset;
        pushConstants.materialIndex = materialIndex;

        vkCmdPushConstants(renderer->commandBuffer, renderer->pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants),
            &pushConstants);

        vkCmdDrawIndexed(renderer->commandBuffer, mesh.indexCount, 1, 0, 0, 0);
    }

    // Draw debug buffer
    if (outputFlags & Output_ShowDebugDrawing)
    {
        vkCmdBindDescriptorSets(renderer->commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->debugDrawPipelineLayout,
            0, 1, &renderer->debugDrawDescriptorSets[imageIndex], 0, NULL);

        vkCmdBindPipeline(renderer->commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->debugDrawPipeline);

        vkCmdDraw(
            renderer->commandBuffer, renderer->debugDrawVertexCount, 1, 0, 0);
    }

    vkCmdEndRenderPass(renderer->commandBuffer);

    // Perform post-processing pass
    VkRenderPassBeginInfo renderPassPresentBegin = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassPresentBegin.renderPass = renderer->renderPass;
    renderPassPresentBegin.framebuffer =
        renderer->swapchain.framebuffers[imageIndex];
    renderPassPresentBegin.renderArea.extent.width = renderer->swapchain.width;
    renderPassPresentBegin.renderArea.extent.height =
        renderer->swapchain.height;
    renderPassPresentBegin.clearValueCount = ArrayCount(clearValues);
    renderPassPresentBegin.pClearValues = clearValues;
    vkCmdBeginRenderPass(renderer->commandBuffer, &renderPassPresentBegin,
        VK_SUBPASS_CONTENTS_INLINE);

    // Set dynamic pipeline state
    VkViewport viewportPresent = {0, 0, (f32)renderer->swapchain.width,
        (f32)renderer->swapchain.height, 0.0f, 1.0f};
    VkRect2D scissorPresent = {
        0, 0, renderer->swapchain.width, renderer->swapchain.height};
    vkCmdSetViewport(renderer->commandBuffer, 0, 1, &viewportPresent);
    vkCmdSetScissor(renderer->commandBuffer, 0, 1, &scissorPresent);

    // Bind pipeline
    vkCmdBindDescriptorSets(renderer->commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->pipelineLayout, 0, 1,
        &renderer->postProcessDescriptorSets[imageIndex], 0, NULL);

    vkCmdBindPipeline(renderer->commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->postProcessPipeline);

    // Draw mesh
    vkCmdDraw(renderer->commandBuffer, 6, 1, 0, 0);

    vkCmdEndRenderPass(renderer->commandBuffer);

    VK_CHECK(vkEndCommandBuffer(renderer->commandBuffer));

    VkPipelineStageFlags submitStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &renderer->acquireSemaphore;
    submitInfo.pWaitDstStageMask = &submitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &renderer->commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderer->releaseSemaphore;
    VK_CHECK(vkQueueSubmit(renderer->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

    VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.swapchainCount = 1;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderer->releaseSemaphore;
    presentInfo.pSwapchains = &renderer->swapchain.handle;
    presentInfo.pImageIndices = &imageIndex;
    VK_CHECK(vkQueuePresentKHR(renderer->presentQueue, &presentInfo));

    // FIXME: Remove this to allow CPU and GPU to run in parallel
    VK_CHECK(vkDeviceWaitIdle(renderer->device));
}

internal void VulkanFramebufferResize(
    VulkanRenderer *renderer, u32 framebufferWidth, u32 framebufferHeight)
{
    // Make doubly sure no resources are still in use
    vkDeviceWaitIdle(renderer->device);

    // Destroy swapchain
    VulkanDestroySwapchain(renderer->swapchain, renderer->device);
    DestroyHdrSwapchain(renderer->device, renderer->hdrSwapchain);

    // Recreate swapchain
    renderer->swapchain =
        VulkanSetupSwapchain(renderer->device, renderer->physicalDevice,
            renderer->surface, renderer->queueFamilyIndices,
            renderer->renderPass, 2, framebufferWidth, framebufferHeight);

    // TODO: Config option to support 16-bit floats?
    VkFormat linearFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    renderer->hdrSwapchain = CreateHdrSwapchain(renderer->device,
        renderer->physicalDevice, linearFormat, framebufferWidth,
        framebufferHeight, 2, renderer->hdrRenderPass);

    // Update descriptor sets to point to new image views
    UpdatePostProcessingDescriptorSets(renderer);
}
