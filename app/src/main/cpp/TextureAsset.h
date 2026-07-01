#ifndef ANDROIDGLINVESTIGATIONS_TEXTUREASSET_H
#define ANDROIDGLINVESTIGATIONS_TEXTUREASSET_H

#include <memory>
#include <android/asset_manager.h>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

/*!
 * A Vulkan texture: a VkImage backed by device memory plus a VkImageView. The
 * sampler is owned by the Renderer (a single shared sampler), so this class only
 * holds the image and its view. Resources are released on destruction.
 */
class TextureAsset {
public:
    /*!
     * Bundles the Vulkan handles TextureAsset needs to upload image data. The
     * renderer fills this in and passes it to the factory methods.
     */
    struct VulkanContext {
        VkPhysicalDevice physicalDevice;
        VkDevice device;
        VkQueue graphicsQueue;
        VkCommandPool commandPool;
    };

    /*!
     * Loads a texture from the assets/ directory, decoding it to RGBA and
     * uploading it to a VkImage (R8G8B8A8_UNORM, shader-read layout).
     */
    static std::shared_ptr<TextureAsset>
    loadAsset(AAssetManager *assetManager, const std::string &assetPath, const VulkanContext &ctx);

    /*!
     * Creates a 1x1 RGBA texture filled with a single solid color. Used to draw
     * tinted button / divider-line quads (color supplied via push constants).
     */
    static std::shared_ptr<TextureAsset>
    createSolidColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a, const VulkanContext &ctx);

    ~TextureAsset();

    TextureAsset(const TextureAsset &) = delete;
    TextureAsset &operator=(const TextureAsset &) = delete;

    constexpr VkImageView getImageView() const { return imageView_; }
    constexpr VkImage getImage() const { return image_; }

private:
    TextureAsset(VkDevice device, VkImage image, VkDeviceMemory memory, VkImageView imageView);

    // Uploads a block of RGBA8 pixels to a new VkImage and returns it wrapped in a
    // TextureAsset. Declared here (private) so it can invoke the private constructor.
    static std::shared_ptr<TextureAsset>
    uploadPixels(const VulkanContext &ctx, const uint8_t *pixels, uint32_t width, uint32_t height);

    VkDevice device_;
    VkImage image_;
    VkDeviceMemory memory_;
    VkImageView imageView_;
};

#endif //ANDROIDGLINVESTIGATIONS_TEXTUREASSET_H