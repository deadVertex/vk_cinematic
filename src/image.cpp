internal void UploadHdrImageToVulkan(
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


    Assert(renderer->swapchain.imageCount == 2);
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

internal HdrImage CreateDiffuseIrradianceTexture(
    HdrImage image, MemoryArena *tempArena)
{
    RandomNumberGenerator rng = {};
    rng.state = 0x45BA12F3;

    HdrImage dstImage = {};
    dstImage.width = 128;
    dstImage.height = 64;
    dstImage.pixels =
        AllocateArray(tempArena, f32, dstImage.width * dstImage.height * 4);

    for (u32 y = 0; y < dstImage.height; ++y)
    {
        for (u32 x = 0; x < dstImage.width; ++x)
        {
            vec2 uv = {};
            uv.x = (f32)x / (f32)dstImage.width;
            uv.y = (f32)y / (f32)dstImage.height;

            vec2 sphereCoords = MapEquirectangularToSphereCoordinates(uv);
            vec3 dir = MapSphericalToCartesianCoordinates(sphereCoords);

            vec4 irradiance = {};
            for (u32 sampleIndex = 0; sampleIndex < 512; ++sampleIndex)
            {
                // Compute random direction on hemi-sphere around
                // rayHit.normal
                vec3 offset = Vec3(RandomBilateral(&rng),
                        RandomBilateral(&rng), RandomBilateral(&rng));
                vec3 sampleDir = Normalize(dir + offset);
                if (Dot(sampleDir, dir) < 0.0f)
                {
                    sampleDir = -sampleDir;
                }

                f32 cosine = Max(Dot(dir, sampleDir), 0.0f);

                vec2 sphereCoords2 = ToSphericalCoordinates(sampleDir);
                vec2 uv2 = MapToEquirectangular(sphereCoords2);
                //uv2.y = 1.0f - uv2.y; // Flip Y axis as usual
                vec4 sample = SampleImage(image, uv2);
                irradiance += sample * (1.0f / 32.0f) * cosine;
            }

            irradiance.a = 1.0;

            u32 pixelIndex = (y * dstImage.width + x) * 4;
            *(vec4 *)(dstImage.pixels + pixelIndex) = irradiance;
        }
    }

    return dstImage;
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

internal void UploadTestCubeMapToGPU(VulkanRenderer *renderer,
    HdrImage equirectangularImage)
{
    // Allocate cube map from upload buffer
    MemoryArena textureUploadArena = {};
    InitializeMemoryArena(&textureUploadArena,
        renderer->textureUploadBuffer.data, TEXTURE_UPLOAD_BUFFER_SIZE);

    // NOTE: We don't actually use any of this since its broken
    // FIXME: Hardcoded in vulkan_renderer.cpp
    u32 width = 256;
    u32 height = 256;
    u32 bytesPerPixel = 4; // Using VK_FORMAT_R8G8B8A8_UNORM
    u32 layerCount = 6;
    void *pixels = AllocateBytes(
        &textureUploadArena, width * height * bytesPerPixel * layerCount);

    // Copy test data into upload buffer
    for (u32 layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        // Map layer index to basis vectors for cube map face
        BasisVectors basis =
            MapCubeMapLayerIndexToBasisVectors(layerIndex);

        u32 layerOffset = layerIndex * width * height * bytesPerPixel;
        for (u32 y = 0; y < height; ++y)
        {
            for (u32 x = 0; x < width; ++x)
            {
                // Convert pixel to cartesian direction vector
                f32 fx = (f32)x / (f32)width;
                f32 fy = (f32)y / (f32)height;

                // Map to -1 to 1
                fx = fx * 2.0f - 1.0f;
                fy = fy * 2.0f - 1.0f;

                vec3 dir = basis.forward + basis.right * fx + basis.up * fy;
                dir = Normalize(dir);

                // Sample equirectangular texture using direction vector
                vec4 sample =
                    SampleEquirectangularImage(equirectangularImage, dir);

                // Store sample for pixel
                u32 pixelOffset = (y * width + x) * bytesPerPixel;
                u32 index = layerOffset + pixelOffset;

                u32 *pixel = (u32 *)((u8 *)pixels + index);

                // FIXME: Need to do proper tone mapping but without SRGB conversion
                vec4 color = Clamp(sample, Vec4(0), Vec4(1));

                color *= 255.0f;
                *pixel =
                    (0xFF000000 | ((u32)color.z) << 16 | ((u32)color.y) << 8) |
                    (u32)color.x;
            }
        }
    }

    // Submit cube map data for upload to GPU
    VulkanTransitionImageLayout(renderer->images[Image_CubeMapTest].handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, renderer->device,
        renderer->commandPool, renderer->graphicsQueue, true);

    VulkanCopyBufferToImage(renderer->device, renderer->commandPool,
        renderer->graphicsQueue, renderer->textureUploadBuffer.handle,
        renderer->images[Image_CubeMapTest].handle, width, height, 0, true);

    VulkanTransitionImageLayout(renderer->images[Image_CubeMapTest].handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, renderer->device,
        renderer->commandPool, renderer->graphicsQueue, true);

    for (u32 i = 0; i < renderer->swapchain.imageCount; ++i)
    {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = renderer->images[Image_CubeMapTest].view;

        VkWriteDescriptorSet descriptorWrites[1] = {};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = renderer->descriptorSets[i];
        descriptorWrites[0].dstBinding = 6;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(renderer->device, ArrayCount(descriptorWrites),
            descriptorWrites, 0, NULL);
    }
}

internal void UploadIrradianceCubeMapToGPU(VulkanRenderer *renderer,
    HdrImage equirectangularImage, u32 imageId, u32 dstBinding)
{
    Assert(imageId < MAX_IMAGES);

    u32 width = 64;
    u32 height = 64;

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

    HdrCubeMap irradianceCubeMap = CreateIrradianceCubeMap(
        equirectangularImage, &textureUploadArena, width, height, 64);

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
