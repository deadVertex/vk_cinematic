internal void UploadHdrImageToGPU(
    VulkanRenderer *renderer, HdrImage image, u32 imageId, u32 dstBinding)
{
    Assert(imageId < MAX_IMAGES);

    // Create image
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
    renderer->images[imageId] = VulkanCreateImage(renderer->device,
        renderer->physicalDevice, image.width, image.height, format,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    // Map memory
    MemoryArena textureUploadArena = {};
    InitializeMemoryArena(&textureUploadArena,
        renderer->textureUploadBuffer.data, TEXTURE_UPLOAD_BUFFER_SIZE);

    // Copy data to upload buffer
    f32 *pixels =
        AllocateArray(&textureUploadArena, f32, image.width * image.height * 4);
    CopyMemory(
        pixels, image.pixels, image.width * image.height * sizeof(f32) * 4);

    // Update image
    VulkanTransitionImageLayout(renderer->images[imageId].handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, renderer->device,
        renderer->commandPool, renderer->graphicsQueue);

    VulkanCopyBufferToImage(renderer->device, renderer->commandPool,
        renderer->graphicsQueue, renderer->textureUploadBuffer.handle,
        renderer->images[imageId].handle, image.width, image.height, 0);

    VulkanTransitionImageLayout(renderer->images[imageId].handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, renderer->device,
        renderer->commandPool, renderer->graphicsQueue);


    // FIXME: This seems horrific!
    Assert(renderer->swapchain.imageCount == 2);
    for (u32 i = 0; i < renderer->swapchain.imageCount; ++i)
    {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = renderer->images[imageId].view;

        VkWriteDescriptorSet descriptorWrites[2] = {};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = renderer->descriptorSets[i];
        descriptorWrites[0].dstBinding = dstBinding;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = renderer->computeDescriptorSets[i];
        descriptorWrites[1].dstBinding = dstBinding;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(renderer->device, ArrayCount(descriptorWrites),
            descriptorWrites, 0, NULL);
    }
}

internal void UploadCubeMapToGPU(VulkanRenderer *renderer, HdrCubeMap cubeMap,
    u32 imageId, u32 dstBinding, u32 width, u32 height)
{
    u32 bytesPerPixel = sizeof(f32) * 4; // Using VK_FORMAT_R32G32B32A32_SFLOAT

    // Create image
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
    renderer->images[imageId] = VulkanCreateImage(renderer->device,
        renderer->physicalDevice, width, height, format,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT, true);

    // Allocate cube map from upload buffer
    MemoryArena textureUploadArena = {};
    InitializeMemoryArena(&textureUploadArena,
        renderer->textureUploadBuffer.data, TEXTURE_UPLOAD_BUFFER_SIZE);

    u32 layerCount = 6;
    void *pixels = AllocateBytes(
        &textureUploadArena, width * height * bytesPerPixel * layerCount);

    for (u32 layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        // Map layer index to basis vectors for cube map face
        BasisVectors basis =
            MapCubeMapLayerIndexToBasisVectors(layerIndex);

        HdrImage *srcImage = cubeMap.images + layerIndex;

        u32 layerOffset = layerIndex * width * height * bytesPerPixel;
        void *dst = (u8 *)pixels + layerOffset;

        CopyMemory(dst, srcImage->pixels, width * height * bytesPerPixel);
    }

    // Submit cube map data for upload to GPU
    VulkanTransitionImageLayout(renderer->images[imageId].handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, renderer->device,
        renderer->commandPool, renderer->graphicsQueue, true);

    VulkanCopyBufferToImage(renderer->device, renderer->commandPool,
        renderer->graphicsQueue, renderer->textureUploadBuffer.handle,
        renderer->images[imageId].handle, width, height, 0, true);

    VulkanTransitionImageLayout(renderer->images[imageId].handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, renderer->device,
        renderer->commandPool, renderer->graphicsQueue, true);

    for (u32 i = 0; i < renderer->swapchain.imageCount; ++i)
    {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = renderer->images[imageId].view;

        VkWriteDescriptorSet descriptorWrites[1] = {};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = renderer->descriptorSets[i];
        descriptorWrites[0].dstBinding = dstBinding;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(renderer->device, ArrayCount(descriptorWrites),
            descriptorWrites, 0, NULL);
    }
}

internal HdrImage CreateCheckerBoardImage(MemoryArena *tempArena)
{
    u32 width = 256;
    u32 height = 256;

    HdrImage result = {};
    result.width = width;
    result.height = height;
    result.pixels = AllocateArray(tempArena, f32, width * height * 4);

    u32 gridSize = 16;
    for (u32 y = 0; y < height; ++y)
    {
        for (u32 x = 0; x < width; ++x)
        {
            u32 pixelIndex = y * width + x;
            vec4 color = Vec4(0.18, 0.18, 0.18, 1);
            if (((x / gridSize) + (y / gridSize)) % 2)
            {
                color = Vec4(0.04, 0.04, 0.04, 1);
            }

            *(vec4 *)(result.pixels + (pixelIndex * 4)) = color;
        }
    }

    return result;
}
