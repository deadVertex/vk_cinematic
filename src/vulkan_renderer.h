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

#define TEXTURE_UPLOAD_BUFFER_SIZE Megabytes(128)

//#define SHADER_PATH "src/shaders"
#define SHADER_PATH "shaders"

enum
{
    Image_CpuRayTracer,
    Image_CubeMapTest,
    Image_EnvMapTest,
    Image_Irradiance,
    Image_CheckerBoard,
    Image_IrradianceCubeMap,
    MAX_IMAGES,
};

enum
{
    Output_None = 0,
    Output_ShowDebugDrawing = 1,
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

struct VulkanImage
{
    VkImage handle;
    VkDeviceMemory memory;
    VkImageView view;
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

struct VulkanHdrSwapchainFramebuffer
{
    VulkanImage depth;
    VulkanImage color;
    VkFramebuffer handle;
};

struct VulkanHdrSwapchain
{
    VkRenderPass renderPass;
    VulkanHdrSwapchainFramebuffer framebuffers[2];
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

    VulkanHdrSwapchain hdrSwapchain;

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

    // FIXME: Should be combined with imageUploadBuffer but imageUploadBuffer
    // is currently being used in a strange way for uploading the CPU ray
    // tracing result.
    VulkanBuffer textureUploadBuffer;

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

    // Post processing stuff
    VkShaderModule fullscreenQuadVertexShader;
    VkShaderModule postProcessingFragmentShader;
    VkPipeline postProcessPipeline;
    VkDescriptorSet postProcessDescriptorSets[2];

    // Debug draw buffer
    VkShaderModule debugDrawVertexShader;
    VkShaderModule debugDrawFragmentShader;
    VkPipeline debugDrawPipeline;
    VkPipelineLayout debugDrawPipelineLayout;
    VkDescriptorSetLayout debugDrawDescriptorSetLayout;
    VkDescriptorSet debugDrawDescriptorSets[2];

    // Skybox stuff
    VkShaderModule skyboxFragmentShader;
    VkPipeline skyboxPipeline;

    u32 vertexDataUploadBufferSize;
    u32 indexUploadBufferSize;
    u32 debugDrawVertexCount;

    Mesh meshes[MAX_MESHES];
    VulkanImage images[MAX_IMAGES];
};
