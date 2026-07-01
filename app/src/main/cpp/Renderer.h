#ifndef ANDROIDGLINVESTIGATIONS_RENDERER_H
#define ANDROIDGLINVESTIGATIONS_RENDERER_H

#include <vulkan/vulkan.h>
#include <chrono>
#include <memory>
#include <vector>

#include "Model.h"     // Vertex / Index / Vector3 / Vector2 types
#include "Shader.h"
#include "TextureAsset.h"

struct android_app;

/*!
 * An axis-aligned rectangle in the renderer's orthographic projection space
 * (y-up, x-right). Used both for drawing the on-screen buttons and for
 * hit-testing touch events against them.
 */
struct ButtonRect {
    float centerX = 0.f;
    float centerY = 0.f;
    float halfWidth = 0.f;
    float halfHeight = 0.f;
};

/*!
 * A Vulkan 1.3 renderer. Replaces the previous OpenGL ES implementation while
 * preserving the same feature set: a textured robot that rotates 90 degrees per
 * button press (with smooth easing), and a "proliferate" mode that tiles the
 * screen with 2^n copies separated by dividing lines. Shaders are loaded from
 * pre-compiled SPIR-V bytecode.
 */
class Renderer {
public:
    /*!
     * @param pApp the android_app this Renderer belongs to, needed to create the
     *             Vulkan surface from the ANativeWindow.
     */
    inline explicit Renderer(android_app *pApp) : app_(pApp) {
        initRenderer();
    }

    virtual ~Renderer();

    /*!
     * Handles input from the android_app. Clears the input queue.
     */
    void handleInput();

    /*!
     * Renders and presents a single frame.
     */
    void render();

private:
    static constexpr int kMaxFramesInFlight = 2;

    void initRenderer();
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSurface();
    void createSwapChain();
    void createImageViews();
    void createCommandPool();
    void createCommandBuffers();
    void createDescriptorSetLayout();
    void createPipeline();
    void createDescriptorPool();
    void createDescriptorSets();
    void createSampler();
    void createTextureResources();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createSyncObjects();
    void recreateSwapChain();
    void cleanupSwapChain();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawScene(VkCommandBuffer commandBuffer);
    void drawRobotGrid(VkCommandBuffer commandBuffer);
    void drawGridLines(VkCommandBuffer commandBuffer);
    void drawButton(VkCommandBuffer commandBuffer, const ButtonRect &rect,
                    float r, float g, float b, float a);

    /// Pushes the per-draw model matrix (offset 0) and color tint (offset 64).
    void pushModelColor(VkCommandBuffer commandBuffer, const float *model, const float *color);

    void updateProjection();
    void updateUniformBuffer(uint32_t currentFrame);

    void proliferate();
    void computeGrid(int &cols, int &rows) const;
    void updateButtonRects();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &available);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &available);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    android_app *app_;

    // Core Vulkan state.
    VkInstance instance_;
    VkPhysicalDevice physicalDevice_;
    VkDevice device_;
    VkQueue graphicsQueue_;
    uint32_t graphicsFamily_ = 0;
    VkSurfaceKHR surface_;

    // Swapchain.
    VkSwapchainKHR swapchain_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{0, 0};

    // Commands / sync.
    VkCommandPool commandPool_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    uint32_t currentFrame_ = 0;
    bool framebufferResized_ = false;

    // Pipeline / descriptors.
    VkDescriptorSetLayout descriptorSetLayout_;
    VkPipelineLayout pipelineLayout_;
    VkPipeline pipeline_;
    VkDescriptorPool descriptorPool_;
    // One set per frame-in-flight, per texture (robot vs white).
    std::vector<VkDescriptorSet> robotDescriptorSets_;
    std::vector<VkDescriptorSet> whiteDescriptorSets_;
    VkSampler textureSampler_;

    // Shaders (SPIR-V modules).
    std::unique_ptr<Shader> shader_;

    // Textures.
    std::shared_ptr<TextureAsset> robotTexture_;
    std::shared_ptr<TextureAsset> whiteTexture_;

    // Geometry buffers (a single quad shared by the robot, buttons and dividers).
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    uint32_t indexCount_ = 0;

    // Projection UBOs (one per frame-in-flight), persistently mapped.
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBuffersMemory_;
    std::vector<void *> uniformBuffersMapped_;
    bool shaderNeedsNewProjectionMatrix_ = true;
    float projectionMatrix_[16]{};

    // ---- Feature state (preserved from the GL version) ---------------------
    // Accumulated (displayed) rotation and the instant target the user requests.
    float robotRotationDegrees_ = 0.f;
    float targetRotationDegrees_ = 0.f;
    std::chrono::steady_clock::time_point lastFrameTime_;

    // Proliferation level: 2^level copies of the robot fill the screen.
    int proliferationLevel_ = 0;

    // On-screen button rectangles, in projection space.
    ButtonRect leftButton_;
    ButtonRect rightButton_;
};

#endif //ANDROIDGLINVESTIGATIONS_RENDERER_H