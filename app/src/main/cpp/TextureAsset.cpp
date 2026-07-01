#include <android/imagedecoder.h>
#include "TextureAsset.h"
#include "AndroidOut.h"
#include "VulkanUtil.h"

#include <cassert>
#include <stdexcept>

std::shared_ptr<TextureAsset>
TextureAsset::uploadPixels(const TextureAsset::VulkanContext &ctx,
                          const uint8_t *pixels,
                          uint32_t width,
                          uint32_t height) {
    VkDeviceSize imageSize = VkDeviceSize(width) * height * 4;

    // Staging buffer in host-visible memory.
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    vkutil::createBuffer(ctx.device, ctx.physicalDevice, imageSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         stagingBuffer, stagingMemory);

    void *data;
    vkMapMemory(ctx.device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(ctx.device, stagingMemory);

    // The destination image lives in device-local memory for efficient GPU sampling.
    VkImage image;
    VkDeviceMemory imageMemory;
    vkutil::createImage(ctx.device, ctx.physicalDevice, width, height, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, imageMemory);

    vkutil::transitionImageLayout(ctx.device, ctx.graphicsQueue, ctx.commandPool, image,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkutil::copyBufferToImage(ctx.device, ctx.graphicsQueue, ctx.commandPool, stagingBuffer, image,
                              width, height);
    vkutil::transitionImageLayout(ctx.device, ctx.graphicsQueue, ctx.commandPool, image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(ctx.device, stagingBuffer, nullptr);
    vkFreeMemory(ctx.device, stagingMemory, nullptr);

    VkImageView view = vkutil::createImageView(ctx.device, image, VK_FORMAT_R8G8B8A8_UNORM,
                                               VK_IMAGE_ASPECT_COLOR_BIT);

    return std::shared_ptr<TextureAsset>(new TextureAsset(ctx.device, image, imageMemory, view));
}

std::shared_ptr<TextureAsset>
TextureAsset::loadAsset(AAssetManager *assetManager, const std::string &assetPath,
                        const VulkanContext &ctx) {
    auto pAsset = AAssetManager_open(assetManager, assetPath.c_str(), AASSET_MODE_BUFFER);
    if (!pAsset) {
        throw std::runtime_error("failed to open texture asset: " + assetPath);
    }

    AImageDecoder *pDecoder = nullptr;
    auto result = AImageDecoder_createFromAAsset(pAsset, &pDecoder);
    assert(result == ANDROID_IMAGE_DECODER_SUCCESS);

    AImageDecoder_setAndroidBitmapFormat(pDecoder, ANDROID_BITMAP_FORMAT_RGBA_8888);

    const AImageDecoderHeaderInfo *pHeader = AImageDecoder_getHeaderInfo(pDecoder);
    auto width = AImageDecoderHeaderInfo_getWidth(pHeader);
    auto height = AImageDecoderHeaderInfo_getHeight(pHeader);
    auto stride = AImageDecoder_getMinimumStride(pDecoder);

    std::vector<uint8_t> pixels(height * stride);
    auto decodeResult = AImageDecoder_decodeImage(pDecoder, pixels.data(), stride, pixels.size());
    assert(decodeResult == ANDROID_IMAGE_DECODER_SUCCESS);

    AImageDecoder_delete(pDecoder);
    AAsset_close(pAsset);

    // If the decoder's stride is larger than width*4, repack into a tight RGBA buffer because
    // copyBufferToImage expects tightly-packed rows.
    if (stride == width * 4) {
        return uploadPixels(ctx, pixels.data(), width, height);
    }

    std::vector<uint8_t> tight(width * height * 4);
    for (uint32_t r = 0; r < height; r++) {
        memcpy(tight.data() + r * width * 4, pixels.data() + r * stride, width * 4);
    }
    return uploadPixels(ctx, tight.data(), width, height);
}

std::shared_ptr<TextureAsset>
TextureAsset::createSolidColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a, const VulkanContext &ctx) {
    uint8_t pixel[4] = {r, g, b, a};
    return uploadPixels(ctx, pixel, 1, 1);
}

TextureAsset::TextureAsset(VkDevice device, VkImage image, VkDeviceMemory memory,
                           VkImageView imageView)
        : device_(device), image_(image), memory_(memory), imageView_(imageView) {}

TextureAsset::~TextureAsset() {
    if (imageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, imageView_, nullptr);
    }
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, image_, nullptr);
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
    }
}