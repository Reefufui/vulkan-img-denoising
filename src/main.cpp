#include <vulkan/vulkan.h>

#include <vector>
#include <cstring>
#include <string>
#include <cassert>
#include <stdexcept>
#include <cmath>
#include <chrono>

#include "Bitmap.h"
#include "cpptqdm/tqdm.h"

#include <iostream>

#define FOREGROUND_COLOR "\033[38;2;0;0;0m"
#define BACKGROUND_COLOR "\033[48;2;0;255;0m"
#define CLEAR_COLOR      "\033[0m"

const int WORKGROUP_SIZE = 16;

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

#include "vk_utils.h"

class Timer
{
    private:
        using clock_t = std::chrono::high_resolution_clock;
        using second_t = std::chrono::duration<double, std::ratio<1> >;

        std::chrono::time_point<clock_t> m_beg;

    public:
        Timer() : m_beg(clock_t::now())
    {
    }

        void reset()
        {
            m_beg = clock_t::now();
        }

        double elapsed() const
        {
            return std::chrono::duration_cast<second_t>(clock_t::now() - m_beg).count();
        }
};

class CustomVulkanTexture
{
    private:
        VkDeviceMemory m_imagesMemoryGPU{}; 
        VkImage        m_imageGPU{};
        VkSampler      m_imageSampler{};
        VkImageView    m_imageView{};
        VkDevice       m_device{};

        bool m_used{};

    public:

        VkDeviceMemory getDeviceMemory() { return m_imagesMemoryGPU; }
        VkImage        getImage()        { return m_imageGPU; }
        VkImage*       getpImage()       { return &m_imageGPU; }
        VkSampler      getSampler()      { return m_imageSampler; }
        VkImageView    getImageView()    { return m_imageView; }

        CustomVulkanTexture()
            : m_imagesMemoryGPU(0), m_imageGPU(0), m_imageSampler(0), m_imageView(0), m_device(0)
        {
        }

        void create(VkDevice a_device, VkPhysicalDevice a_physDevice, const int a_width, const int a_height)
        {
            m_device = a_device;
            m_used = true;

            VkImageCreateInfo imgCreateInfo{};
            imgCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imgCreateInfo.pNext         = nullptr;
            imgCreateInfo.flags         = 0;
            imgCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
            imgCreateInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
            imgCreateInfo.extent        = VkExtent3D{uint32_t(a_width), uint32_t(a_height), 1};
            imgCreateInfo.mipLevels     = 1;
            imgCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            imgCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            imgCreateInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imgCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
            imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgCreateInfo.arrayLayers   = 1;
            VK_CHECK_RESULT(vkCreateImage(a_device, &imgCreateInfo, nullptr, &m_imageGPU));

            VkMemoryRequirements memoryRequirements{};
            vkGetImageMemoryRequirements(a_device, m_imageGPU, &memoryRequirements);
            VkMemoryAllocateInfo allocateInfo{};
            allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocateInfo.allocationSize  = memoryRequirements.size;
            allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, a_physDevice);
            VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, NULL, &m_imagesMemoryGPU));
            VK_CHECK_RESULT(vkBindImageMemory(a_device, m_imageGPU, m_imagesMemoryGPU, 0));

            VkSamplerCreateInfo samplerInfo = {};
            {
                samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                samplerInfo.pNext        = nullptr;
                samplerInfo.flags        = 0;
                samplerInfo.magFilter    = VK_FILTER_LINEAR;
                samplerInfo.minFilter    = VK_FILTER_LINEAR;
                samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                samplerInfo.mipLodBias   = 0.0f;
                samplerInfo.compareOp    = VK_COMPARE_OP_NEVER;
                samplerInfo.minLod           = 0;
                samplerInfo.maxLod           = 0;
                samplerInfo.maxAnisotropy    = 1.0;
                samplerInfo.anisotropyEnable = VK_FALSE;
                samplerInfo.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
                samplerInfo.unnormalizedCoordinates = VK_FALSE;
            }

            VK_CHECK_RESULT(vkCreateSampler(a_device, &samplerInfo, nullptr, &m_imageSampler));

            VkImageViewCreateInfo imageViewInfo = {};
            {
                imageViewInfo.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                imageViewInfo.flags      = 0;
                imageViewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
                imageViewInfo.format     = VK_FORMAT_R8G8B8A8_UNORM;
                imageViewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
                imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                imageViewInfo.subresourceRange.baseMipLevel   = 0;
                imageViewInfo.subresourceRange.baseArrayLayer = 0;
                imageViewInfo.subresourceRange.layerCount     = 1;
                imageViewInfo.subresourceRange.levelCount     = 1;
                imageViewInfo.image = m_imageGPU;
            }
            VK_CHECK_RESULT(vkCreateImageView(a_device, &imageViewInfo, nullptr, &m_imageView));
        }

        void release()
        {
            if (m_used)
            {
                vkFreeMemory      (m_device, m_imagesMemoryGPU, NULL);
                vkDestroyImage    (m_device, m_imageGPU,        NULL);
                vkDestroyImageView(m_device, m_imageView,       NULL);
                vkDestroySampler  (m_device, m_imageSampler,    NULL);
                m_used = false;
            }
        }

};

class ComputeApplication
{
    private:

        struct Pixel {
            float r, g, b, a;
        };

        struct NLM { //debug
            Pixel weightedColor;
            Pixel norm;
        };

        VkInstance                m_instance{};
        VkDebugReportCallbackEXT  m_debugReportCallback{};
        VkPhysicalDevice          m_physicalDevice{};
        VkDevice                  m_device{};
        VkPipeline                m_pipeline{},            m_pipeline2{};
        VkPipelineLayout          m_pipelineLayout{},      m_pipelineLayout2{};
        VkShaderModule            m_computeShaderModule{}, m_computeShaderModule2{};
        VkCommandBuffer           m_commandBuffer{},       m_commandBuffer2{};
        VkQueue                   m_queue{},               m_queue2{};
        VkDescriptorSet           m_descriptorSet{},       m_descriptorSet2{};
        VkDescriptorSetLayout     m_descriptorSetLayout{}, m_descriptorSetLayout2{};
        VkDescriptorPool          m_descriptorPool{},      m_descriptorPool2{};
        VkCommandPool             m_commandPool{},         m_commandPool2{};
        VkBuffer                  m_bufferGPU{};
        VkBuffer                  m_bufferDynamic{};
        VkBuffer                  m_bufferStaging{};
        VkBuffer                  m_bufferTexel{};
        VkBuffer                  m_bufferNLM{};
        VkDeviceMemory            m_bufferMemoryGPU{}, m_bufferMemoryStaging{}, m_bufferMemoryTexel{}, m_bufferMemoryNLM{}, m_bufferMemoryDynamic{};
        VkBufferView              m_texelBufferView{};
        bool                      m_linear{};
        bool                      m_nlmFilter{}; // if false then bialteral (default)
        bool                      m_multiframe{}; // works only with nlm
        CustomVulkanTexture       m_targetImage{};
        CustomVulkanTexture       m_neighbourImage{};

        std::vector<const char *> m_enabledLayers{};

    public:

        ComputeApplication()
            : m_bufferDynamic(NULL), m_bufferMemoryDynamic(NULL){ }

        static void GetImageFromGPU(VkDevice a_device, VkDeviceMemory a_stagingMem, int a_w, int a_h, uint32_t *a_imageData)
        {
            void *mappedMemory = nullptr;
            vkMapMemory(a_device, a_stagingMem, 0, a_w * a_h * sizeof(float) * 4, 0, &mappedMemory);
            Pixel* pmappedMemory = (Pixel *)mappedMemory;

            for (int i = 0; i < a_w * a_h; ++i)
            {
                const uint32_t r = ((uint32_t) (255.0f * (pmappedMemory[i].r)));
                const uint32_t g = ((uint32_t) (255.0f * (pmappedMemory[i].g)));
                const uint32_t b = ((uint32_t) (255.0f * (pmappedMemory[i].b)));
                a_imageData[i] = (r << 0) | (g << 8) | (b << 16);
            }

            vkUnmapMemory(a_device, a_stagingMem);
        }

        static void PutImageToGPU(VkDevice a_device, VkDeviceMemory a_dynamicMem, int a_w, int a_h, const uint32_t *a_imageData)
        {
            void *mappedMemory = nullptr;
            vkMapMemory(a_device, a_dynamicMem, 0, a_w * a_h * sizeof(float) * 4, 0, &mappedMemory);
            float* pmappedMemory = (float*)mappedMemory;
            for (int i = 0; i < (a_w * a_h); i ++)
            {
                const uint32_t b = (a_imageData[i] & 0x00FF0000) >> 16;
                const uint32_t g = (a_imageData[i] & 0x0000FF00) >> 8;
                const uint32_t r = (a_imageData[i] & 0x000000FF);

                pmappedMemory[i*4+0] = float(r)*(1.0f/255.0f);
                pmappedMemory[i*4+1] = float(g)*(1.0f/255.0f);
                pmappedMemory[i*4+2] = float(b)*(1.0f/255.0f);
                pmappedMemory[i*4+3] = 0.0f;
            }

            vkUnmapMemory(a_device, a_dynamicMem);
        }


        static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFn(
                VkDebugReportFlagsEXT                       flags,
                VkDebugReportObjectTypeEXT                  objectType,
                uint64_t                                    object,
                size_t                                      location,
                int32_t                                     messageCode,
                const char*                                 pLayerPrefix,
                const char*                                 pMessage,
                void*                                       pUserData)
        {
            printf("Debug Report: %s: %s\n", pLayerPrefix, pMessage);
            return VK_FALSE;
        }


        // CPU (this buffer takes data from GPU)
        static void CreateStagingBuffer(VkDevice a_device, VkPhysicalDevice a_physDevice, const size_t a_bufferSize,
                VkBuffer *a_pBuffer, VkDeviceMemory *a_pBufferMemory)
        {
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size        = a_bufferSize;

            bufferCreateInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VK_CHECK_RESULT(vkCreateBuffer(a_device, &bufferCreateInfo, NULL, a_pBuffer));

            //----

            VkMemoryRequirements memoryRequirements{};
            vkGetBufferMemoryRequirements(a_device, (*a_pBuffer), &memoryRequirements);

            VkMemoryAllocateInfo allocateInfo{};
            allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocateInfo.allocationSize  = memoryRequirements.size;
            allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(
                    memoryRequirements.memoryTypeBits,
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    a_physDevice);

            VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, NULL, a_pBufferMemory));

            VK_CHECK_RESULT(vkBindBufferMemory(a_device, (*a_pBuffer), (*a_pBufferMemory), 0));
        }

        static void CreateDynamicBuffer(VkDevice a_device, VkPhysicalDevice a_physDevice, const size_t a_bufferSize,
                VkBuffer *a_pBuffer, VkDeviceMemory *a_pBufferMemory)
        {
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size        = a_bufferSize;
            bufferCreateInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VK_CHECK_RESULT(vkCreateBuffer(a_device, &bufferCreateInfo, NULL, a_pBuffer));

            VkMemoryRequirements memoryRequirements;
            vkGetBufferMemoryRequirements(a_device, (*a_pBuffer), &memoryRequirements);

            VkMemoryAllocateInfo allocateInfo{};
            allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocateInfo.allocationSize  = memoryRequirements.size;
            allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(
                    memoryRequirements.memoryTypeBits,
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    a_physDevice);

            VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, NULL, a_pBufferMemory));

            VK_CHECK_RESULT(vkBindBufferMemory(a_device, (*a_pBuffer), (*a_pBufferMemory), 0));
        }

        static void CreateTexelBuffer(VkDevice a_device, VkPhysicalDevice a_physDevice, const size_t a_bufferSize,
                VkBuffer *a_pBuffer, VkDeviceMemory *a_pBufferMemory)
        {
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size        = a_bufferSize;
            bufferCreateInfo.usage       = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
            bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VK_CHECK_RESULT(vkCreateBuffer(a_device, &bufferCreateInfo, NULL, a_pBuffer));

            VkMemoryRequirements memoryRequirements;
            vkGetBufferMemoryRequirements(a_device, (*a_pBuffer), &memoryRequirements);

            // This texel buffer is coherent and mappable
            VkMemoryAllocateInfo allocateInfo = {};
            allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocateInfo.allocationSize  = memoryRequirements.size;
            allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(
                    memoryRequirements.memoryTypeBits,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    a_physDevice);

            VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, NULL, a_pBufferMemory));

            VK_CHECK_RESULT(vkBindBufferMemory(a_device, (*a_pBuffer), (*a_pBufferMemory), 0));
        }

        static void CreateTexelBufferView(VkDevice a_device, const size_t a_bufferSize, VkBuffer a_buffer, VkBufferView *a_pBufferView)
        {
            VkBufferViewCreateInfo bufferViewCreateInfo{};
            bufferViewCreateInfo.sType   = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
            bufferViewCreateInfo.pNext   = nullptr;
            bufferViewCreateInfo.flags   = 0;
            bufferViewCreateInfo.buffer  = a_buffer;
            bufferViewCreateInfo.format  = VK_FORMAT_R8G8B8A8_UNORM;
            bufferViewCreateInfo.offset  = 0;
            bufferViewCreateInfo.range   = a_bufferSize;

            VK_CHECK_RESULT(vkCreateBufferView(a_device, &bufferViewCreateInfo, NULL, a_pBufferView));
        }

        static void CreateWriteOnlyBuffer(VkDevice a_device, VkPhysicalDevice a_physDevice, const size_t a_bufferSize,
                VkBuffer *a_pBuffer, VkDeviceMemory *a_pBufferMemory)
        {
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size        = a_bufferSize;
            bufferCreateInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VK_CHECK_RESULT(vkCreateBuffer(a_device, &bufferCreateInfo, NULL, a_pBuffer));

            VkMemoryRequirements memoryRequirements;
            vkGetBufferMemoryRequirements(a_device, (*a_pBuffer), &memoryRequirements);

            VkMemoryAllocateInfo allocateInfo = {};
            allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocateInfo.allocationSize  = memoryRequirements.size;
            allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(
                    memoryRequirements.memoryTypeBits,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    a_physDevice);

            VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, NULL, a_pBufferMemory));

            VK_CHECK_RESULT(vkBindBufferMemory(a_device, (*a_pBuffer), (*a_pBufferMemory), 0));
        }

        static void CreateNLMWeightBuffer(VkDevice a_device, VkPhysicalDevice a_physDevice, size_t a_bufferSize,
                VkBuffer *a_pBuffer, VkDeviceMemory *a_pBufferMemory)
        {
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size        = a_bufferSize;
            bufferCreateInfo.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VK_CHECK_RESULT(vkCreateBuffer(a_device, &bufferCreateInfo, NULL, a_pBuffer));

            VkMemoryRequirements memoryRequirements;
            vkGetBufferMemoryRequirements(a_device, (*a_pBuffer), &memoryRequirements);

            VkMemoryAllocateInfo allocateInfo = {};
            allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocateInfo.allocationSize  = memoryRequirements.size;
            allocateInfo.memoryTypeIndex = vk_utils::FindMemoryType(
                    memoryRequirements.memoryTypeBits,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    a_physDevice);

            VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, NULL, a_pBufferMemory));

            VK_CHECK_RESULT(vkBindBufferMemory(a_device, (*a_pBuffer), (*a_pBufferMemory), 0));
        }

        static void CreateDescriptorSetLayoutBialteral(VkDevice a_device, VkDescriptorSetLayout *a_pDSLayout, bool a_linear = false)
        {
            VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[2];

            // Compute shader output image storage
            descriptorSetLayoutBinding[0].binding            = 0;
            descriptorSetLayoutBinding[0].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorSetLayoutBinding[0].descriptorCount    = 1;
            descriptorSetLayoutBinding[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptorSetLayoutBinding[0].pImmutableSamplers = nullptr;

            // Compute shader input image storage
            descriptorSetLayoutBinding[1].binding            = 1;
            descriptorSetLayoutBinding[1].descriptorType     = (a_linear) ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
                : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorSetLayoutBinding[1].descriptorCount    = 1;
            descriptorSetLayoutBinding[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptorSetLayoutBinding[1].pImmutableSamplers = nullptr;

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
            descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = 2;
            descriptorSetLayoutCreateInfo.pBindings    = descriptorSetLayoutBinding;

            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(a_device, &descriptorSetLayoutCreateInfo, NULL, a_pDSLayout));
        }

        static void CreateDescriptorSetLayoutNLM(VkDevice a_device, VkDescriptorSetLayout *a_pDSLayout, bool a_linear = false, bool a_buildImage = false)
        {
            VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[(a_buildImage)? 2 : 3];

            // (O) Compute shader output image storage (or NLM weights buffer)
            descriptorSetLayoutBinding[0].binding            = 0;
            descriptorSetLayoutBinding[0].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorSetLayoutBinding[0].descriptorCount    = 1;
            descriptorSetLayoutBinding[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
            descriptorSetLayoutBinding[0].pImmutableSamplers = nullptr;

            if (!a_buildImage)
            {
                // (I) Compute shader input target image storage
                descriptorSetLayoutBinding[1].binding            = 1;
                descriptorSetLayoutBinding[1].descriptorType     = (a_linear) ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
                    : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorSetLayoutBinding[1].descriptorCount    = 1;
                descriptorSetLayoutBinding[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
                descriptorSetLayoutBinding[1].pImmutableSamplers = nullptr;

                // (I) Compute shader input neihbour image storage
                descriptorSetLayoutBinding[2].binding            = 2;
                descriptorSetLayoutBinding[2].descriptorType     = (a_linear) ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
                    : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorSetLayoutBinding[2].descriptorCount    = 1;
                descriptorSetLayoutBinding[2].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
                descriptorSetLayoutBinding[2].pImmutableSamplers = nullptr;
            }
            else
            {
                // (I) Compute shader NLM weights buffer
                descriptorSetLayoutBinding[1].binding            = 1;
                descriptorSetLayoutBinding[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                descriptorSetLayoutBinding[1].descriptorCount    = 1;
                descriptorSetLayoutBinding[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
                descriptorSetLayoutBinding[1].pImmutableSamplers = nullptr;
            }

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
            descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = (a_buildImage) ? 2 : 3;
            descriptorSetLayoutCreateInfo.pBindings    = descriptorSetLayoutBinding;

            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(a_device, &descriptorSetLayoutCreateInfo, NULL, a_pDSLayout));
        }

        void CreateDescriptorSetNLM(VkDevice a_device, VkBuffer a_bufferNLM, size_t a_bufferSize, const VkDescriptorSetLayout *a_pDSLayout,
                CustomVulkanTexture a_targetImage, CustomVulkanTexture a_neighbourImage,
                VkDescriptorPool *a_pDSPool, VkDescriptorSet *a_pDS)
        {
            // 0: NLM buffer (W/R)
            // 1: Texture/texbuffer #1 (R)
            // 2: Texture/texbuffer #2 (R)

            VkDescriptorPoolSize descriptorPoolSize[3];
            descriptorPoolSize[0].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorPoolSize[0].descriptorCount = 1;
            descriptorPoolSize[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorPoolSize[1].descriptorCount = 1;
            descriptorPoolSize[2].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorPoolSize[2].descriptorCount = 1;

            VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
            descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptorPoolCreateInfo.maxSets       = 1;
            descriptorPoolCreateInfo.poolSizeCount = 3;
            descriptorPoolCreateInfo.pPoolSizes    = descriptorPoolSize;

            VK_CHECK_RESULT(vkCreateDescriptorPool(a_device, &descriptorPoolCreateInfo, NULL, a_pDSPool));

            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
            descriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool     = (*a_pDSPool);
            descriptorSetAllocateInfo.descriptorSetCount = 1;
            descriptorSetAllocateInfo.pSetLayouts        = a_pDSLayout;

            VK_CHECK_RESULT(vkAllocateDescriptorSets(a_device, &descriptorSetAllocateInfo, a_pDS));

            // OUTPUT NLM BUFFER
            VkDescriptorBufferInfo descriptorBufferInfo{};
            descriptorBufferInfo.buffer = a_bufferNLM;
            descriptorBufferInfo.offset = 0;
            descriptorBufferInfo.range  = a_bufferSize;
            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet          = *a_pDS;
            writeDescriptorSet.dstBinding      = 0;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeDescriptorSet.pBufferInfo     = &descriptorBufferInfo;

            vkUpdateDescriptorSets(a_device, 1, &writeDescriptorSet, 0, NULL);

            // INPUT (two 2d tiled optimal images)

            VkDescriptorImageInfo descriptorTargetImageInfo{};
            descriptorTargetImageInfo.sampler     = a_targetImage.getSampler();
            descriptorTargetImageInfo.imageView   = a_targetImage.getImageView();
            descriptorTargetImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkWriteDescriptorSet writeDescriptorSet2{};
            writeDescriptorSet2.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet2.dstSet          = *a_pDS;
            writeDescriptorSet2.dstBinding      = 1;
            writeDescriptorSet2.descriptorCount = 1;
            writeDescriptorSet2.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorSet2.pImageInfo      = &descriptorTargetImageInfo;

            VkDescriptorImageInfo descriptorNeighbourImageInfo{};
            descriptorNeighbourImageInfo.sampler     = a_neighbourImage.getSampler();
            descriptorNeighbourImageInfo.imageView   = a_neighbourImage.getImageView();
            descriptorNeighbourImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkWriteDescriptorSet writeDescriptorSet3{};
            writeDescriptorSet3.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet3.dstSet          = *a_pDS;
            writeDescriptorSet3.dstBinding      = 2;
            writeDescriptorSet3.descriptorCount = 1;
            writeDescriptorSet3.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorSet3.pImageInfo      = &descriptorNeighbourImageInfo;

            vkUpdateDescriptorSets(a_device, 1, &writeDescriptorSet2, 0, NULL);
            vkUpdateDescriptorSets(a_device, 1, &writeDescriptorSet3, 0, NULL);
        }

        void CreateDescriptorSetNLM2(VkDevice a_device, VkBuffer a_bufferGPU, size_t a_bufferSize, const VkDescriptorSetLayout *a_pDSLayout,
                VkBuffer a_bufferNLM, size_t a_bufferNLMSize, VkDescriptorPool *a_pDSPool, VkDescriptorSet *a_pDS)
        {
            // 0: GPU buffer (W)
            // 1: NLM weights (R)

            VkDescriptorPoolSize descriptorPoolSize[2];
            descriptorPoolSize[0].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorPoolSize[0].descriptorCount = 1;
            descriptorPoolSize[1].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorPoolSize[1].descriptorCount = 1;

            VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
            descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptorPoolCreateInfo.maxSets       = 1;
            descriptorPoolCreateInfo.poolSizeCount = 2;
            descriptorPoolCreateInfo.pPoolSizes    = descriptorPoolSize;

            VK_CHECK_RESULT(vkCreateDescriptorPool(a_device, &descriptorPoolCreateInfo, NULL, a_pDSPool));

            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
            descriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool     = (*a_pDSPool);
            descriptorSetAllocateInfo.descriptorSetCount = 1;
            descriptorSetAllocateInfo.pSetLayouts        = a_pDSLayout;

            VK_CHECK_RESULT(vkAllocateDescriptorSets(a_device, &descriptorSetAllocateInfo, a_pDS));

            // OUTPUT BUFFER [for resut image]
            VkDescriptorBufferInfo descriptorBufferInfo{};
            descriptorBufferInfo.buffer = a_bufferGPU;
            descriptorBufferInfo.offset = 0;
            descriptorBufferInfo.range  = a_bufferSize;

            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet          = *a_pDS;
            writeDescriptorSet.dstBinding      = 0;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeDescriptorSet.pBufferInfo     = &descriptorBufferInfo;

            vkUpdateDescriptorSets(a_device, 1, &writeDescriptorSet, 0, NULL);

            // INPUT BUFFER [for NLM weights]
            VkDescriptorBufferInfo descriptorBufferNLMInfo{};
            descriptorBufferNLMInfo.buffer = a_bufferNLM;
            descriptorBufferNLMInfo.offset = 0;
            descriptorBufferNLMInfo.range  = a_bufferNLMSize;

            VkWriteDescriptorSet writeDescriptorSet2{};
            writeDescriptorSet2.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet2.dstSet          = *a_pDS;
            writeDescriptorSet2.dstBinding      = 1;
            writeDescriptorSet2.descriptorCount = 1;
            writeDescriptorSet2.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeDescriptorSet2.pBufferInfo     = &descriptorBufferNLMInfo;

            vkUpdateDescriptorSets(a_device, 1, &writeDescriptorSet2, 0, NULL);
        }

        void CreateDescriptorSetBialteral(VkDevice a_device, VkBuffer a_buffer, size_t a_bufferSize, const VkDescriptorSetLayout *a_pDSLayout, CustomVulkanTexture a_image,
                VkBuffer a_texelBuffer, VkBufferView *a_texelBufferView, VkDescriptorPool *a_pDSPool, VkDescriptorSet *a_pDS, bool a_linear = false)
        {
            VkDescriptorPoolSize descriptorPoolSize[2];
            descriptorPoolSize[0].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorPoolSize[0].descriptorCount = 1;
            descriptorPoolSize[1].type            = (a_linear) ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
                : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorPoolSize[1].descriptorCount = 1;

            VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
            descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptorPoolCreateInfo.maxSets       = 1;
            descriptorPoolCreateInfo.poolSizeCount = 2;
            descriptorPoolCreateInfo.pPoolSizes    = descriptorPoolSize;

            VK_CHECK_RESULT(vkCreateDescriptorPool(a_device, &descriptorPoolCreateInfo, NULL, a_pDSPool));

            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
            descriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool     = (*a_pDSPool);
            descriptorSetAllocateInfo.descriptorSetCount = 1;
            descriptorSetAllocateInfo.pSetLayouts        = a_pDSLayout;

            VK_CHECK_RESULT(vkAllocateDescriptorSets(a_device, &descriptorSetAllocateInfo, a_pDS));

            // OUTPUT
            VkDescriptorBufferInfo descriptorBufferInfo{};
            descriptorBufferInfo.buffer = a_buffer;
            descriptorBufferInfo.offset = 0;
            descriptorBufferInfo.range  = a_bufferSize;

            VkWriteDescriptorSet writeDescriptorSet{};
            writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet.dstSet          = *(a_pDS+0);
            writeDescriptorSet.dstBinding      = 0;
            writeDescriptorSet.descriptorCount = 1;
            writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeDescriptorSet.pBufferInfo     = &descriptorBufferInfo;

            vkUpdateDescriptorSets(a_device, 1, &writeDescriptorSet, 0, NULL);

            // INPUT (depends on a_linear: image or linear buffer)
            VkDescriptorImageInfo descriptorImageInfo{};
            VkDescriptorBufferInfo descriptorTexelBufferInfo{};

            descriptorImageInfo.sampler     = a_image.getSampler();
            descriptorImageInfo.imageView   = a_image.getImageView();
            descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            descriptorTexelBufferInfo.buffer = a_texelBuffer;
            descriptorTexelBufferInfo.offset = 0;
            descriptorTexelBufferInfo.range  = a_bufferSize;

            VkWriteDescriptorSet writeDescriptorSet2{};
            writeDescriptorSet2.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorSet2.dstSet          = *(a_pDS+0);
            writeDescriptorSet2.dstBinding      = 1;
            writeDescriptorSet2.descriptorCount = 1;
            writeDescriptorSet2.descriptorType  = (a_linear) ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
                : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

            if (a_linear)
            {
                writeDescriptorSet2.pBufferInfo = &descriptorTexelBufferInfo;
                writeDescriptorSet2.pTexelBufferView = a_texelBufferView;
            }
            else
            {
                writeDescriptorSet2.pImageInfo = &descriptorImageInfo;
            }

            vkUpdateDescriptorSets(a_device, 1, &writeDescriptorSet2, 0, NULL);
        }

        static void CreateComputePipelines(VkDevice a_device, const VkDescriptorSetLayout &a_dsLayout,
                VkShaderModule *a_pShaderModule, VkPipeline *a_pPipeline, VkPipelineLayout *a_pPipelineLayout,
                const char *a_shaderFileName)
        {
            std::vector<uint32_t> code = vk_utils::ReadFile(a_shaderFileName);
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.pCode    = code.data();
            createInfo.codeSize = code.size()*sizeof(uint32_t);

            VK_CHECK_RESULT(vkCreateShaderModule(a_device, &createInfo, NULL, a_pShaderModule));

            VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
            shaderStageCreateInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageCreateInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
            shaderStageCreateInfo.module = (*a_pShaderModule);
            shaderStageCreateInfo.pName  = "main";

            VkPushConstantRange pcRange{};
            pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            pcRange.offset     = 0;
            pcRange.size       = 2 * sizeof(int);

            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
            pipelineLayoutCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCreateInfo.setLayoutCount         = 1;
            pipelineLayoutCreateInfo.pSetLayouts            = &a_dsLayout;
            pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
            pipelineLayoutCreateInfo.pPushConstantRanges    = &pcRange;
            VK_CHECK_RESULT(vkCreatePipelineLayout(a_device, &pipelineLayoutCreateInfo, NULL, a_pPipelineLayout));

            VkComputePipelineCreateInfo pipelineCreateInfo = {};
            pipelineCreateInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipelineCreateInfo.stage  = shaderStageCreateInfo;
            pipelineCreateInfo.layout = (*a_pPipelineLayout);

            VK_CHECK_RESULT(vkCreateComputePipelines(a_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, a_pPipeline));
        }

        static void CreateCommandBuffer(VkDevice a_device, uint32_t a_queueFamilyIndex, VkPipeline a_pipeline, VkPipelineLayout a_layout,
                VkCommandPool *a_pool, VkCommandBuffer *a_pCmdBuff)
        {
            VkCommandPoolCreateInfo commandPoolCreateInfo{};
            commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            commandPoolCreateInfo.queueFamilyIndex = a_queueFamilyIndex;
            VK_CHECK_RESULT(vkCreateCommandPool(a_device, &commandPoolCreateInfo, NULL, a_pool));

            VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
            commandBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocateInfo.commandPool        = (*a_pool);
            commandBufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            commandBufferAllocateInfo.commandBufferCount = 1;
            VK_CHECK_RESULT(vkAllocateCommandBuffers(a_device, &commandBufferAllocateInfo, a_pCmdBuff));
        }

        static VkImageMemoryBarrier imBarTransfer(VkImage a_image, const VkImageSubresourceRange& a_range, VkImageLayout before, VkImageLayout after)
        {
            VkImageMemoryBarrier moveToGeneralBar{};
            moveToGeneralBar.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            moveToGeneralBar.pNext               = nullptr;
            moveToGeneralBar.srcAccessMask       = 0;
            moveToGeneralBar.dstAccessMask       = VK_PIPELINE_STAGE_TRANSFER_BIT;
            moveToGeneralBar.oldLayout           = before;
            moveToGeneralBar.newLayout           = after;
            moveToGeneralBar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            moveToGeneralBar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            moveToGeneralBar.image               = a_image;
            moveToGeneralBar.subresourceRange    = a_range;
            return moveToGeneralBar;
        }

        static VkImageSubresourceRange WholeImageRange()
        {
            VkImageSubresourceRange rangeWholeImage{};
            rangeWholeImage.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            rangeWholeImage.baseMipLevel   = 0;
            rangeWholeImage.levelCount     = 1;
            rangeWholeImage.baseArrayLayer = 0;
            rangeWholeImage.layerCount     = 1;
            return rangeWholeImage;
        }

        static void RecordCommandsOfExecuteAndTransfer(VkCommandBuffer a_cmdBuff, VkPipeline a_pipeline,VkPipelineLayout a_layout, const VkDescriptorSet &a_ds,
                size_t a_bufferSize, VkBuffer a_bufferGPU, VkBuffer a_bufferStaging, int a_w, int a_h)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

            vkCmdBindPipeline      (a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, a_pipeline);
            vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, a_layout, 0, 1, &a_ds, 0, NULL);

            int wh[2]{ a_w, a_h };
            vkCmdPushConstants(a_cmdBuff, a_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int) * 2, wh);

            vkCmdDispatch(a_cmdBuff, (uint32_t)ceil(a_w / float(WORKGROUP_SIZE)), (uint32_t)ceil(a_h / float(WORKGROUP_SIZE)), 1);

            VkBufferMemoryBarrier bufBarr{};
            bufBarr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufBarr.pNext = nullptr;
            bufBarr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufBarr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufBarr.size                = VK_WHOLE_SIZE;
            bufBarr.offset              = 0;
            bufBarr.buffer              = a_bufferGPU;
            bufBarr.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
            bufBarr.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(a_cmdBuff,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,
                    1, &bufBarr,
                    0, nullptr);

            VkBufferCopy copyInfo{};
            copyInfo.dstOffset = 0;
            copyInfo.srcOffset = 0;
            copyInfo.size      = a_bufferSize;

            vkCmdCopyBuffer(a_cmdBuff, a_bufferGPU, a_bufferStaging, 1, &copyInfo);

            VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
        }

        static void RecordCommandsOfExecuteNLM(VkCommandBuffer a_cmdBuff, VkPipeline a_pipeline,VkPipelineLayout a_layout, const VkDescriptorSet &a_ds,
                int a_w, int a_h)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

            vkCmdBindPipeline      (a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, a_pipeline);
            vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, a_layout, 0, 1, &a_ds, 0, NULL);

            int wh[2]{ a_w, a_h };
            vkCmdPushConstants(a_cmdBuff, a_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int) * 2, wh);

            vkCmdDispatch(a_cmdBuff, (uint32_t)ceil(a_w / float(WORKGROUP_SIZE)), (uint32_t)ceil(a_h / float(WORKGROUP_SIZE)), 1);

            VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
        }

        static void RecordCommandsOfTransferNLM(VkCommandBuffer a_cmdBuff, VkPipeline a_pipeline,VkPipelineLayout a_layout, const VkDescriptorSet &a_ds,
                size_t a_bufferSize, VkBuffer a_bufferGPU, VkBuffer a_bufferStaging, int a_w, int a_h)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

            vkCmdFillBuffer(a_cmdBuff, a_bufferStaging, 0, a_bufferSize, 0);

            vkCmdBindPipeline      (a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, a_pipeline);
            vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, a_layout, 0, 1, &a_ds, 0, NULL);

            int wh[2]{ a_w, a_h };
            vkCmdPushConstants(a_cmdBuff, a_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int) * 2, wh);

            vkCmdDispatch(a_cmdBuff, (uint32_t)ceil(a_w / float(WORKGROUP_SIZE)), (uint32_t)ceil(a_h / float(WORKGROUP_SIZE)), 1);

            VkBufferMemoryBarrier bufBarr{};
            bufBarr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufBarr.pNext = nullptr;
            bufBarr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufBarr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufBarr.size                = VK_WHOLE_SIZE;
            bufBarr.offset              = 0;
            bufBarr.buffer              = a_bufferGPU;
            bufBarr.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
            bufBarr.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(a_cmdBuff,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,
                    1, &bufBarr,
                    0, nullptr);

            VkBufferCopy copyInfo{};
            copyInfo.dstOffset = 0;
            copyInfo.srcOffset = 0;
            copyInfo.size      = a_bufferSize;

            vkCmdCopyBuffer(a_cmdBuff, a_bufferGPU, a_bufferStaging, 1, &copyInfo);

            VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
        }

        static void RecordCommandsOfCopyImageDataToTexture(VkCommandBuffer a_cmdBuff, int a_width, int a_height, VkBuffer a_bufferDynamic,
                VkImage *a_images)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

            VkImageSubresourceRange rangeWholeImage = WholeImageRange();

            VkImageSubresourceLayers shittylayers{};
            shittylayers.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            shittylayers.mipLevel       = 0;
            shittylayers.baseArrayLayer = 0;
            shittylayers.layerCount     = 1;

            VkBufferImageCopy wholeRegion = {};
            wholeRegion.bufferOffset      = 0;
            wholeRegion.bufferRowLength   = uint32_t(a_width);
            wholeRegion.bufferImageHeight = uint32_t(a_height);
            wholeRegion.imageExtent       = VkExtent3D{uint32_t(a_width), uint32_t(a_height), 1};
            wholeRegion.imageOffset       = VkOffset3D{0,0,0};
            wholeRegion.imageSubresource  = shittylayers;

            VkImageMemoryBarrier moveToGeneralBar = imBarTransfer(a_images[0],
                    rangeWholeImage,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            vkCmdPipelineBarrier(a_cmdBuff,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,            // general memory barriers
                    0, nullptr,            // buffer barriers
                    1, &moveToGeneralBar); // image  barriers

            VkClearColorValue clearVal = {};
            clearVal.float32[0] = 1.0f;
            clearVal.float32[1] = 1.0f;
            clearVal.float32[2] = 1.0f;
            clearVal.float32[3] = 1.0f;

            vkCmdClearColorImage(a_cmdBuff, a_images[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearVal, 1, &rangeWholeImage);

            vkCmdCopyBufferToImage(a_cmdBuff, a_bufferDynamic, a_images[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &wholeRegion);

            VkImageMemoryBarrier imgBar{};
            {
                imgBar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imgBar.pNext = nullptr;
                imgBar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imgBar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                imgBar.srcAccessMask       = 0;
                imgBar.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
                imgBar.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imgBar.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imgBar.image               = a_images[0];

                imgBar.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                imgBar.subresourceRange.baseMipLevel   = 0;
                imgBar.subresourceRange.levelCount     = 1;
                imgBar.subresourceRange.baseArrayLayer = 0;
                imgBar.subresourceRange.layerCount     = 1;
            };

            vkCmdPipelineBarrier(a_cmdBuff,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &imgBar);

            //VkImageMemoryBarrier barForCopy[2];
            //barForCopy[0] = imBarTransfer(a_images[0], rangeWholeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            //barForCopy[1] = imBarTransfer(a_images[1], rangeWholeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            //
            //vkCmdPipelineBarrier(a_cmdBuff,
            //                     VK_PIPELINE_STAGE_TRANSFER_BIT,
            //                     VK_PIPELINE_STAGE_TRANSFER_BIT,
            //                     0,
            //                     0, nullptr,     // general memory barriers
            //                     0, nullptr,     // buffer barriers
            //                     2, barForCopy); // image  barriers
            //
            //vkCmdCopyImageToBuffer(a_cmdBuff, a_images[1], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, a_bufferStaging, 1, &wholeRegion);


            VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
        }

        static void RunCommandBuffer(VkCommandBuffer a_cmdBuff, VkQueue a_queue, VkDevice a_device)
        {
            VkSubmitInfo submitInfo{};
            submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers    = &a_cmdBuff;

            VkFence fence{};
            VkFenceCreateInfo fenceCreateInfo{};
            fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceCreateInfo.flags = 0;
            VK_CHECK_RESULT(vkCreateFence(a_device, &fenceCreateInfo, NULL, &fence));
            VK_CHECK_RESULT(vkQueueSubmit(a_queue, 1, &submitInfo, fence));
            VK_CHECK_RESULT(vkWaitForFences(a_device, 1, &fence, VK_TRUE, 100000000000));
            vkDestroyFence(a_device, fence, NULL);
        }

        static void LoadImageDataToBuffer(VkDevice a_device, VkPhysicalDevice a_physDevice, std::vector<unsigned int> a_imageData,
                int a_w, int a_h, VkDeviceMemory a_bufferMemoryTexel, VkDeviceMemory a_bufferMemoryDynamic, bool a_linear)
        {
            void *mappedMemory = nullptr;

            if (a_linear)
            {
                vkMapMemory(a_device, a_bufferMemoryTexel, 0, a_w * a_h * sizeof(int), 0, &mappedMemory);
                memcpy(mappedMemory, a_imageData.data(), a_w * a_h * sizeof(int));
                vkUnmapMemory(a_device, a_bufferMemoryTexel);
            }
            else
            {
                vkMapMemory(a_device, a_bufferMemoryDynamic, 0, a_w * a_h * sizeof(int), 0, &mappedMemory);
                memcpy(mappedMemory, a_imageData.data(), a_w * a_h * sizeof(int));
                vkUnmapMemory(a_device, a_bufferMemoryDynamic);
            }
        }

        void Cleanup()
        {
            if (enableValidationLayers)
            {
                auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
                if (func == nullptr) {
                    throw std::runtime_error("Could not load vkDestroyDebugReportCallbackEXT");
                }
                func(m_instance, m_debugReportCallback, NULL);
            }

            // Destroy buffers and device memory allocated for them
            {
                if (m_bufferDynamic != VK_NULL_HANDLE)
                {
                    vkFreeMemory   (m_device, m_bufferMemoryDynamic, NULL);
                    vkDestroyBuffer(m_device, m_bufferDynamic, NULL);
                    m_bufferMemoryDynamic = VK_NULL_HANDLE;
                    m_bufferDynamic = VK_NULL_HANDLE;
                }

                if (m_bufferStaging != VK_NULL_HANDLE)
                {
                    vkFreeMemory   (m_device, m_bufferMemoryStaging, NULL);
                    vkDestroyBuffer(m_device, m_bufferStaging, NULL);
                    m_bufferStaging = VK_NULL_HANDLE;
                    m_bufferMemoryStaging = VK_NULL_HANDLE;
                }

                if (m_bufferGPU != VK_NULL_HANDLE)
                {
                    vkFreeMemory   (m_device, m_bufferMemoryGPU, NULL);
                    vkDestroyBuffer(m_device, m_bufferGPU, NULL);
                    m_bufferGPU = VK_NULL_HANDLE;
                    m_bufferMemoryGPU = VK_NULL_HANDLE;
                }

                if (m_bufferNLM != VK_NULL_HANDLE)
                {
                    vkFreeMemory   (m_device, m_bufferMemoryNLM, NULL);
                    vkDestroyBuffer(m_device, m_bufferNLM, NULL);
                    m_bufferNLM = VK_NULL_HANDLE;
                    m_bufferMemoryNLM = VK_NULL_HANDLE;
                }

                if (m_bufferTexel != VK_NULL_HANDLE)
                {
                    vkFreeMemory   (m_device, m_bufferMemoryTexel, NULL);
                    vkDestroyBuffer(m_device, m_bufferTexel, NULL);
                    vkDestroyBufferView(m_device, m_texelBufferView, NULL);
                    m_bufferMemoryTexel = VK_NULL_HANDLE;
                    m_bufferTexel = VK_NULL_HANDLE;
                    m_texelBufferView = VK_NULL_HANDLE;
                }

            }

            // Delete images
            m_targetImage.release();
            m_neighbourImage.release();

            // Delete shader related resourses
            {
                if (m_descriptorPool != VK_NULL_HANDLE)
                {
                    vkDestroyDescriptorPool(m_device, m_descriptorPool, NULL);
                    m_descriptorPool = VK_NULL_HANDLE;
                }

                if (m_descriptorPool2 != VK_NULL_HANDLE)
                {
                    vkDestroyDescriptorPool(m_device, m_descriptorPool2, NULL);
                    m_descriptorPool2 = VK_NULL_HANDLE;
                }

                if (m_computeShaderModule != VK_NULL_HANDLE)
                {
                    vkDestroyShaderModule(m_device, m_computeShaderModule, NULL);
                    m_computeShaderModule = VK_NULL_HANDLE;
                }

                if (m_computeShaderModule2 != VK_NULL_HANDLE)
                {
                    vkDestroyShaderModule(m_device, m_computeShaderModule2, NULL);
                    m_computeShaderModule2 = VK_NULL_HANDLE;
                }

                if (m_descriptorSetLayout != VK_NULL_HANDLE)
                {
                    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, NULL);
                    m_descriptorSetLayout = VK_NULL_HANDLE;
                }

                if (m_descriptorSetLayout2 != VK_NULL_HANDLE)
                {
                    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout2, NULL);
                    m_descriptorSetLayout2 = VK_NULL_HANDLE;
                }

                if (m_pipelineLayout != VK_NULL_HANDLE)
                {
                    vkDestroyPipelineLayout(m_device, m_pipelineLayout, NULL);
                    m_pipelineLayout = VK_NULL_HANDLE;
                }

                if (m_pipelineLayout2 != VK_NULL_HANDLE)
                {
                    vkDestroyPipelineLayout(m_device, m_pipelineLayout2, NULL);
                    m_pipelineLayout2 = VK_NULL_HANDLE;
                }

                if (m_pipeline != VK_NULL_HANDLE)
                {
                    vkDestroyPipeline(m_device, m_pipeline, NULL);
                    m_pipeline = VK_NULL_HANDLE;
                }

                if (m_pipeline2 != VK_NULL_HANDLE)
                {
                    vkDestroyPipeline(m_device, m_pipeline2, NULL);
                    m_pipeline2 = VK_NULL_HANDLE;
                }
            }

            if (m_commandPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(m_device, m_commandPool, NULL);
                m_commandPool = VK_NULL_HANDLE;
            }

            if (m_commandPool2 != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(m_device, m_commandPool2, NULL);
                m_commandPool2 = VK_NULL_HANDLE;
            }

            if (m_device != VK_NULL_HANDLE)
            {
                vkDestroyDevice(m_device, NULL);
                m_device = VK_NULL_HANDLE;
            }

            if (m_instance != VK_NULL_HANDLE)
            {
                vkDestroyInstance(m_instance, NULL);
                m_instance = VK_NULL_HANDLE;
            }
        }

        void RunOnGPU(bool nlmFilter, bool nonlinear, bool multiframe)
        {
            // Set members
            m_nlmFilter = nlmFilter;
            m_linear = !nonlinear;
            m_multiframe = multiframe;
            assert(m_nlmFilter || !multiframe);
            //

            const int deviceId{0};
            std::cout << "\tinit vulkan for device " << deviceId << "\n";

            m_instance = vk_utils::CreateInstance(enableValidationLayers, m_enabledLayers);
            if (enableValidationLayers)
            {
                vk_utils::InitDebugReportCallback(m_instance,
                        &debugReportCallbackFn, &m_debugReportCallback);
            }

            m_physicalDevice = vk_utils::FindPhysicalDevice(m_instance, true, deviceId);

            uint32_t queueFamilyIndex{vk_utils::GetComputeQueueFamilyIndex(m_physicalDevice)};
            m_device = vk_utils::CreateLogicalDevice(queueFamilyIndex, m_physicalDevice, m_enabledLayers);
            vkGetDeviceQueue(m_device, queueFamilyIndex, 0, &m_queue);

            //----------------------------------------------------------------------------------------------------------------------
            std::cout << "\tloading image data\n";
            //----------------------------------------------------------------------------------------------------------------------

            using image_t = std::vector<unsigned int>;
            const int framesToUse{(multiframe) ? 10 : 1};

            int w{}, h{};
            std::vector<image_t> imageData{};
            {
                std::string fileName{ "res/frame-0.bmp" };
                imageData.push_back(LoadBMP(fileName.c_str(), &w, &h));

                for (int ii{1}; ii < framesToUse; ++ii)
                {
                    int _w{}, _h{};
                    fileName[10] = '0' + ii;
                    imageData.push_back(LoadBMP(fileName.c_str(), &_w, &_h));

                    if (!imageData[ii].size())
                    {
                        std::cout << "\tcan't load texture " + fileName + "\n";
                        return;
                    }

                    assert(_w == w && _h == h && "Input images shapes dont match");
                }
            }

            size_t bufferSize{sizeof(Pixel) * w * h};
            // GLSL alignment 
            size_t bufferSizeNLM{(sizeof(Pixel) + 4 * sizeof(float)) * w * h};

            //----------------------------------------------------------------------------------------------------------------------
            std::cout << "\tcreating io buffers/images of our shaders\n";
            //----------------------------------------------------------------------------------------------------------------------

            // NOTE: INPUT BUFFER/IMAGE FOR SHADERS
            if (m_linear)
            {
                CreateTexelBuffer(m_device, m_physicalDevice, bufferSize, &m_bufferTexel, &m_bufferMemoryTexel);
                CreateTexelBufferView(m_device, bufferSize, m_bufferTexel, &m_texelBufferView);
                std::cout << "\t\tlinear buffer created\n";
            }
            else
            {
                // for image #0
                m_targetImage.create(m_device, m_physicalDevice, w, h);
                if (m_nlmFilter)
                {
                    // for image #k [0..framesToUse]
                    m_neighbourImage.create(m_device, m_physicalDevice, w, h);
                }
                std::cout << "\t\tnon-linear buffer created\n";
            }

            if (m_nlmFilter)
            {
                CreateNLMWeightBuffer(m_device, m_physicalDevice, bufferSizeNLM, &m_bufferNLM, &m_bufferMemoryNLM);
            }

            // NOTE: OUTPUT BUFFER FOR GPU (device local) [for result image]
            CreateWriteOnlyBuffer(m_device, m_physicalDevice, bufferSize, &m_bufferGPU, &m_bufferMemoryGPU);

            //----------------------------------------------------------------------------------------------------------------------
            std::cout << "\tcreating descriptor sets for created resourses\n";
            //----------------------------------------------------------------------------------------------------------------------

            if (m_nlmFilter)
            {
                // DS for recording weighted pixels for result image
                CreateDescriptorSetLayoutNLM(m_device, &m_descriptorSetLayout, m_linear, false);
                CreateDescriptorSetNLM(m_device, m_bufferNLM, bufferSizeNLM, &m_descriptorSetLayout,
                        m_targetImage, m_neighbourImage, &m_descriptorPool, &m_descriptorSet);

                // DS for building result image (by normalizing)
                CreateDescriptorSetLayoutNLM(m_device, &m_descriptorSetLayout2, m_linear, true);
                CreateDescriptorSetNLM2(m_device, m_bufferGPU, bufferSize, &m_descriptorSetLayout2,
                        m_bufferNLM, bufferSizeNLM, &m_descriptorPool2, &m_descriptorSet2);

                // we have two sepparate descriptor pools for two DS for our NLM calculating and image building shaders
            }
            else
            {
                CreateDescriptorSetLayoutBialteral(m_device, &m_descriptorSetLayout, m_linear);
                CreateDescriptorSetBialteral(m_device, m_bufferGPU, bufferSize, &m_descriptorSetLayout,
                        m_targetImage, m_bufferTexel, &m_texelBufferView,
                        &m_descriptorPool, &m_descriptorSet, m_linear);
            }

            //----------------------------------------------------------------------------------------------------------------------
            std::cout << "\tcompiling shaders\n";
            //----------------------------------------------------------------------------------------------------------------------

            if (m_nlmFilter)
            {
                CreateComputePipelines(m_device, m_descriptorSetLayout, &m_computeShaderModule, &m_pipeline, &m_pipelineLayout,
                        "shaders/nonlocal.spv" );
                CreateComputePipelines(m_device, m_descriptorSetLayout2, &m_computeShaderModule2, &m_pipeline2, &m_pipelineLayout2,
                        "shaders/nonlocal_generate.spv" );
            }
            else
            {
                CreateComputePipelines(m_device, m_descriptorSetLayout, &m_computeShaderModule, &m_pipeline, &m_pipelineLayout,
                        (m_linear) ? "shaders/bialteral_linear.spv" : "shaders/bialteral.spv");
            }

            //----------------------------------------------------------------------------------------------------------------------
            std::cout << "\tcreating command buffer and load image #0 data to texture\n";
            //----------------------------------------------------------------------------------------------------------------------

            CreateCommandBuffer(m_device, queueFamilyIndex, m_pipeline, m_pipelineLayout, &m_commandPool, &m_commandBuffer);

            if (!m_linear)
            {
                // we feed our textures this buffer's data
                CreateDynamicBuffer(m_device, m_physicalDevice, w * h * sizeof(int), &m_bufferDynamic, &m_bufferMemoryDynamic);
            }

            LoadImageDataToBuffer(m_device, m_physicalDevice, imageData[0], w, h, m_bufferMemoryTexel, m_bufferMemoryDynamic, m_linear);

            if (!m_linear)
            {
                // DYNAMIC BUFFER => TEXTURE (COPYING)
                vkResetCommandBuffer(m_commandBuffer, 0);
                RecordCommandsOfCopyImageDataToTexture(m_commandBuffer, w, h, m_bufferDynamic, m_targetImage.getpImage());
                std::cout << "\t\t feeding 1st texture our target image\n";
                RunCommandBuffer(m_commandBuffer, m_queue, m_device);
            }

            //----------------------------------------------------------------------------------------------------------------------
            std::cout << "\tperforming computations\n";
            //----------------------------------------------------------------------------------------------------------------------

            // BUFFER TO TAKE DATA FROM GPU
            CreateStagingBuffer(m_device, m_physicalDevice, bufferSize, &m_bufferStaging, &m_bufferMemoryStaging);

            if (m_nlmFilter)
            {
                for (int ii{0}; ii < framesToUse; ++ii)
                {
                    std::cout << "\t\t loading image #" << ii << " data\n";
                    LoadImageDataToBuffer(m_device, m_physicalDevice, imageData[ii], w, h, m_bufferMemoryTexel, m_bufferMemoryDynamic, false);

                    // DYNAMIC BUFFER => TEXTURE (COPYING)
                    vkResetCommandBuffer(m_commandBuffer, 0);
                    RecordCommandsOfCopyImageDataToTexture(m_commandBuffer, w, h, m_bufferDynamic, m_neighbourImage.getpImage());
                    std::cout << "\t\t feeding #" << ii << "texture our neighbour image\n";
                    RunCommandBuffer(m_commandBuffer, m_queue, m_device);

                    vkResetCommandBuffer(m_commandBuffer, 0);
                    RecordCommandsOfExecuteNLM(m_commandBuffer, m_pipeline, m_pipelineLayout, m_descriptorSet, w, h);
                    RunCommandBuffer(m_commandBuffer, m_queue, m_device);
                }

                CreateCommandBuffer(m_device, queueFamilyIndex, m_pipeline2, m_pipelineLayout2, &m_commandPool2, &m_commandBuffer2);
                vkResetCommandBuffer(m_commandBuffer2, 0);

                {
                    void *mappedMemory{};
                    vkMapMemory(m_device, m_bufferMemoryNLM, 0, sizeof(float) * 8 * w * h, 0, &mappedMemory);

                    NLM *nlmArr = (NLM*)mappedMemory;

                    for (int y{h / 4}; y < h * 3 / 4; y += 50)
                    {
                        for (int x{}; x < w; x += 50)
                        {
                            std::cout << "(" << x << "; " << y << ") => | "
                                << nlmArr[w * y + x].weightedColor.r << " "
                                << nlmArr[w * y + x].weightedColor.g << " "
                                << nlmArr[w * y + x].weightedColor.b << " | "
                                << nlmArr[w * y + x].norm.r << "\n";
                        }
                    }
                    vkUnmapMemory(m_device, m_bufferMemoryNLM);
                }

                RecordCommandsOfTransferNLM(m_commandBuffer2, m_pipeline2, m_pipelineLayout2, m_descriptorSet2,
                        bufferSize, m_bufferGPU, m_bufferStaging, w, h);
                RunCommandBuffer(m_commandBuffer2, m_queue, m_device);
            }
            else
            {
                RecordCommandsOfExecuteAndTransfer(m_commandBuffer, m_pipeline, m_pipelineLayout, m_descriptorSet,
                        bufferSize, m_bufferGPU, m_bufferStaging, w, h);
                RunCommandBuffer(m_commandBuffer, m_queue, m_device);
            }

            //----------------------------------------------------------------------------------------------------------------------
            std::cout << "\tgetting image back\n";
            //----------------------------------------------------------------------------------------------------------------------

            std::vector<uint32_t> resultData(w * h);
            GetImageFromGPU(m_device, m_bufferMemoryStaging, w, h, resultData.data());

            std::cout << "\t\tsaving image\n";
            SaveBMP("result.bmp", resultData.data(), w, h);
            resultData = std::vector<uint32_t>();

            //----------------------------------------------------------------------------------------------------------------------
            std::cout << "\tcleaning up\n";
            //----------------------------------------------------------------------------------------------------------------------
            Cleanup();
        }

        void RunOnCPU(int numThreads)
        {
            int w{}, h{};
            std::vector<unsigned int> imageData{LoadBMP("res/frame-0.bmp", &w, &h)};
            std::vector<Pixel>        inputPixels(w * h);
            std::vector<Pixel>        outputPixels(w * h);

            for (int i{}; i < w * h; ++i)
            {
                const uint32_t b = (imageData[i] & 0x00FF0000) >> 16;
                const uint32_t g = (imageData[i] & 0x0000FF00) >> 8;
                const uint32_t r = (imageData[i] & 0x000000FF);

                inputPixels[i].r = float(r)*(1.0f/255.0f);
                inputPixels[i].g = float(g)*(1.0f/255.0f);
                inputPixels[i].b = float(b)*(1.0f/255.0f);
                inputPixels[i].a = 0.0f;
            }


            std::cout << "\tdoing computations\n";

            const int windowSize{10};

            tqdm bar{};
            bar.set_theme_braille();

            for (int y = windowSize; y <= h - windowSize; ++y)
            {
                bar.progress(y, h - windowSize);
#pragma omp parallel for default(shared) num_threads(numThreads)
                for (int x = windowSize; x <= w - windowSize; ++x)
                {
                    Pixel texColor = inputPixels[y * w + x];

                    // controls the influence of distant pixels
                    const float spatialSigma = 10.0f;
                    // controls the influence of pixels with intesity value different form pixel intensity
                    const float colorSigma   = 0.2f;

                    float normWeight = 0.0f;
                    Pixel weightColor{};

                    for (int i = -windowSize; i <= windowSize; ++i)
                    {
                        for (int j = -windowSize; j <= windowSize; ++j)
                        {
                            float spatialDistance = sqrt((float)pow(i, 2) + pow(j, 2));
                            float spatialWeight   = exp(-0.5 * pow(spatialDistance / spatialSigma, 2));

                            Pixel curColor       = inputPixels[w * (i + y) + j + x];
                            float colorDistance = sqrt(pow(texColor.r - curColor.r, 2)
                                    + pow(texColor.g - curColor.g, 2)
                                    + pow(texColor.b - texColor.b, 2));
                            float colorWeight   = exp(-0.5 * pow(colorDistance / colorSigma, 2));

                            float resultWeight = spatialWeight * colorWeight;

                            weightColor.r += curColor.r * resultWeight;
                            weightColor.g += curColor.g * resultWeight;
                            weightColor.b += curColor.b * resultWeight;

                            normWeight     += resultWeight;
                        }
                    }

                    outputPixels[y * w + x] = Pixel{ weightColor.r / normWeight, weightColor.g / normWeight, weightColor.b /normWeight, 0.0f};
                }
            }

            bar.finish();
            std::cout << "\tsaving image\n";

            for (int i = 0; i < w * h; ++i)
            {
                const uint32_t r = ((uint32_t) (255.0f * (outputPixels[i].r)));
                const uint32_t g = ((uint32_t) (255.0f * (outputPixels[i].g)));
                const uint32_t b = ((uint32_t) (255.0f * (outputPixels[i].b)));
                imageData[i] = (r << 0) | (g << 8) | (b << 16);
            }

            SaveBMP("cpu_result.bmp", imageData.data(), w, h);
        }
};

int main(int argc, char **argv)
{
    try
    {
        Timer timer{};
        ComputeApplication app{};

        /*
        std::cout << "######\nRunning on GPU (nonlinear bialteral)\n######\n";
        app.RunOnGPU(
                false, // NLM filter (bialteral => false)
                true,  // store image in nonlinear texture
                false  // multiframe (not supported for bialteral => should be false)
                );
        std::cout << FOREGROUND_COLOR << BACKGROUND_COLOR
            << "Time taken (with copying) "
            << timer.elapsed()
            << " seconds\n\n"
            << CLEAR_COLOR;

        std::cout << "######\nRunning on GPU (linear bialteral)\n######\n";
        timer.reset();
        app.RunOnGPU(
                false, // NLM filter (bialteral => false)
                false, // store image in nonlinear texture
                false  // multiframe (not supported for bialteral => should be false)
                );
        std::cout << FOREGROUND_COLOR << BACKGROUND_COLOR
            << "Time taken (with copying) "
            << timer.elapsed()
            << " seconds\n\n"
            << CLEAR_COLOR;

*/
        std::cout << "######\nRunning on GPU (nonlocal)\n######\n";
        timer.reset();
        app.RunOnGPU(
                true,  // NLM filter (bialteral => false)
                true,  // store image in nonlinear texture
                false  // multiframe (not supported for bialteral => should be false)
                );
        std::cout << FOREGROUND_COLOR << BACKGROUND_COLOR
            << "Time taken (with copying) "
            << timer.elapsed()
            << " seconds\n\n"
            << CLEAR_COLOR;

        /* 
           std::cout << "######\nRunning on GPU (multiframe nonlocal)\n######\n";
           timer.reset();
           app.RunOnGPU(
           true,  // NLM filter (bialteral => false)
           true,  // store image in nonlinear texture
           true   // multiframe (not supported for bialteral => should be false)
           );
           std::cout << FOREGROUND_COLOR << BACKGROUND_COLOR
           << "Time taken (with copying) "
           << timer.elapsed()
           << " seconds\n\n"
           << CLEAR_COLOR;

           std::cout << "######\nRunning on CPU (1 thread bialteral)\n######\n";
           timer.reset();
           app.RunOnCPU(1);
           std::cout << FOREGROUND_COLOR << BACKGROUND_COLOR
           << "Time taken: "
           << timer.elapsed()
           << " seconds\n\n"
           << CLEAR_COLOR;


           std::cout << "######\nRunning on CPU (8 threads bialteral)\n######\n";
           timer.reset();
           app.RunOnCPU(8);
           std::cout << FOREGROUND_COLOR << BACKGROUND_COLOR
           << "Time taken: "
           << timer.elapsed()
           << " seconds\n\n"
           << CLEAR_COLOR;
           */
    }
    catch (const std::runtime_error& e)
    {
        printf("%s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

