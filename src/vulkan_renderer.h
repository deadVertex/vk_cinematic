#pragma once

#include "math_lib.h"
#include "mesh.h"

#define VERTEX_BUFFER_SIZE Megabytes(512)
#define INDEX_BUFFER_SIZE Megabytes(2)
#define UNIFORM_BUFFER_SIZE Kilobytes(4)
#define IMAGE_BUFFER_SIZE Megabytes(64)
#define DEBUG_VERTEX_BUFFER_SIZE Megabytes(256)
#define MODEL_MATRICES_BUFFER_SIZE Megabytes(4)
#define MATERIAL_BUFFER_SIZE Kilobytes(4) // TODO: Combine this UBO?

//#define SHADER_PATH "src/shaders"
#define SHADER_PATH "shaders"

enum
{
    Output_None = 0,
    Output_VulkanRenderer = 1,
    Output_CpuRayTracer = 2,
};

struct DrawCommand
{
    u32 mesh;
    u32 material;
};

struct Mesh
{
    u32 indexCount;
    u32 indexDataOffset;
    u32 vertexDataOffset;
};

struct UniformBufferObject
{
    mat4 viewMatrices[16];
    mat4 projectionMatrices[16];
    vec3 cameraPosition;
    u32 showComparision;
};

struct VulkanQueueFamilyIndices
{
    u32 graphicsQueue;
    u32 presentQueue;
};

// TODO: Add ImageView to VulkanImage struct
struct VulkanImage
{
    VkImage handle;
    VkDeviceMemory memory;
};

struct VulkanBuffer
{
    VkBuffer handle;
    VkDeviceMemory memory;
    void *data;
};

struct VulkanSwapchain
{
    VkSwapchainKHR handle;
    VkImageView imageViews[2]; // TODO: Store as array of VulkanImages
    VkImage images[2];
    VkFramebuffer framebuffers[2];
    u32 imageCount;
    u32 width;
    u32 height;

    VulkanImage depthImage;
    VkImageView depthImageView;
};

enum
{
    Primitive_Triangle,
    Primitive_Line,
};

enum
{
    PolygonMode_Fill,
    PolygonMode_Line,
};

enum
{
    CullMode_None,
    CullMode_Front,
    CullMode_Back,
};

struct VulkanPipelineDefinition
{
    u32 vertexStride;
    u32 primitive;
    u32 polygonMode;
    u32 cullMode;
    b32 depthTestEnabled;
    b32 depthWriteEnabled;
    b32 alphaBlendingEnabled;
};

struct MeshPushConstants
{
    u32 modelMatrixIndex;
    u32 vertexDataOffset;
    u32 materialIndex;
};

struct VulkanRenderer
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT messenger;
    VkPhysicalDevice physicalDevice;
    VkSurfaceKHR surface;

    VulkanQueueFamilyIndices queueFamilyIndices;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkRenderPass renderPass;
    VulkanSwapchain swapchain;

    VkSemaphore acquireSemaphore;
    VkSemaphore releaseSemaphore;

    VkCommandPool commandPool;
    VkDescriptorPool descriptorPool;

    VkCommandBuffer commandBuffer;
    VulkanBuffer vertexDataUploadBuffer;
    VulkanBuffer vertexDataBuffer;
    VulkanBuffer uniformBuffer;
    VulkanBuffer indexUploadBuffer;
    VulkanBuffer indexBuffer;
    VulkanBuffer imageUploadBuffer;
    VulkanBuffer modelMatricesBuffer;
    VulkanBuffer materialBuffer;

    VulkanBuffer debugVertexDataBuffer;

    VkSampler defaultSampler;

    // Triangle stuff
    VkShaderModule testVertexShader;
    VkShaderModule testFragmentShader;

    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;

    VkPipeline pipeline;
    VkPipelineCache pipelineCache;
    VkDescriptorSet descriptorSets[2];

    // Fullscreen quad stuff
    VkShaderModule fullscreenQuadVertexShader;
    VkShaderModule fullscreenQuadFragmentShader;
    VkPipeline fullscreenQuadPipeline;
    VulkanImage image;
    VkImageView imageView;

    // Debug draw buffer
    VkShaderModule debugDrawVertexShader;
    VkShaderModule debugDrawFragmentShader;
    VkPipeline debugDrawPipeline;
    VkPipelineLayout debugDrawPipelineLayout;
    VkDescriptorSetLayout debugDrawDescriptorSetLayout;
    VkDescriptorSet debugDrawDescriptorSets[2];

    u32 vertexDataUploadBufferSize;
    u32 indexUploadBufferSize;
    u32 debugDrawVertexCount;

    Mesh meshes[MAX_MESHES];
};
