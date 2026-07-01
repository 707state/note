#ifndef ANDROIDGLINVESTIGATIONS_VULKANUTIL_H
#define ANDROIDGLINVESTIGATIONS_VULKANUTIL_H

#include <vulkan/vulkan.h>
#include <cstdint>

/*!
 * Small collection of Vulkan helper functions used by the renderer and texture
 * loader: memory type selection, buffer/image creation, single-use command
 * buffers and image layout transitions. These wrap the verbose boilerplate that
 * Vulkan requires for resource setup.
 */
namespace vkutil {

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                            uint32_t typeFilter,
                            VkMemoryPropertyFlags properties);

    void createBuffer(VkDevice device,
                      VkPhysicalDevice physicalDevice,
                      VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer &buffer,
                      VkDeviceMemory &bufferMemory);

    void createImage(VkDevice device,
                     VkPhysicalDevice physicalDevice,
                     uint32_t width,
                     uint32_t height,
                     VkFormat format,
                     VkImageUsageFlags usage,
                     VkImageTiling tiling,
                     VkMemoryPropertyFlags properties,
                     VkImage &image,
                     VkDeviceMemory &memory);

    /// Begins a one-time-submit command buffer from the given pool.
    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);

    /// Submits the command buffer and blocks until it completes.
    void endSingleTimeCommands(VkDevice device,
                               VkQueue queue,
                               VkCommandPool pool,
                               VkCommandBuffer commandBuffer);

    /// Records and submits a layout transition for @a image (synchronous).
    void transitionImageLayout(VkDevice device,
                               VkQueue queue,
                               VkCommandPool pool,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout);

    /// Copies a buffer's contents into an image (synchronous).
    void copyBufferToImage(VkDevice device,
                           VkQueue queue,
                           VkCommandPool pool,
                           VkBuffer buffer,
                           VkImage image,
                           uint32_t width,
                           uint32_t height);

    VkImageView createImageView(VkDevice device,
                                VkImage image,
                                VkFormat format,
                                VkImageAspectFlags aspectFlags);

} // namespace vkutil

#endif //ANDROIDGLINVESTIGATIONS_VULKANUTIL_H

