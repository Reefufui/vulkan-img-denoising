#include "vk_utils.h"
#include "texture.hpp"

#include <cassert>

void CustomVulkanTexture::create(VkDevice a_device, VkPhysicalDevice a_physDevice, const int a_width, const int a_height, bool a_isHDR)
{
    m_device = a_device;
    m_used = true;

    VkImageCreateInfo imgCreateInfo{};
    imgCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCreateInfo.pNext         = nullptr;
    imgCreateInfo.flags         = 0;
    imgCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
    imgCreateInfo.format        = (a_isHDR) ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
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
        imageViewInfo.format     = (a_isHDR) ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
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

void CustomVulkanTexture::release()
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

