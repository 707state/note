#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/native_window.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "AndroidOut.h"
#include "Shader.h"
#include "Utility.h"
#include "TextureAsset.h"
#include "VulkanUtil.h"

namespace {
// Half the height of the orthographic projection. Renderable area height = 4 (y in [-2, 2]).
constexpr float kProjectionHalfHeight = 2.f;
constexpr float kProjectionNearPlane = -1.f;
constexpr float kProjectionFarPlane = 1.f;

// Rotation easing: bigger = snappier. ~10 settles a 90-degree turn in ~0.3s.
constexpr float kRotationSmoothingSpeed = 10.f;
// Clamp dt so a long pause doesn't cause a giant rotation jump.
constexpr float kRotationMaxFrameDelta = 0.05f;

// Highest proliferation level before wrapping back to a single instance (2^8 = 256).
constexpr int kMaxProliferationLevel = 8;

// Cornflower-blue clear color, matching the original GL sample.
constexpr std::array<float, 4> kClearColor = {100.f / 255.f, 149.f / 255.f, 237.f / 255.f, 1.f};

// Push constant layout: mat4 uModel (offset 0, 64 bytes) + vec4 uColor (offset 64, 16 bytes).
constexpr VkDeviceSize kPushModelOffset = 0;
constexpr VkDeviceSize kPushModelSize = 64;
constexpr VkDeviceSize kPushColorOffset = 64;
constexpr VkDeviceSize kPushColorSize = 16;
constexpr VkDeviceSize kPushTotalSize = kPushModelSize + kPushColorSize; // 80 bytes (<= 128 min)

// Projection UBO size (a single mat4).
constexpr VkDeviceSize kUniformBufferSize = 64;

const std::vector<Vertex> kQuadVertices = {
        Vertex(Vector3{1, 1, 0}, Vector2{0, 0}),   // 0
        Vertex(Vector3{-1, 1, 0}, Vector2{1, 0}),  // 1
        Vertex(Vector3{-1, -1, 0}, Vector2{1, 1}), // 2
        Vertex(Vector3{1, -1, 0}, Vector2{0, 1})   // 3
};
const std::vector<Index> kQuadIndices = {0, 1, 2, 0, 2, 3};
} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

Renderer::~Renderer() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);

        for (auto sem: imageAvailableSemaphores_) vkDestroySemaphore(device_, sem, nullptr);
        for (auto sem: renderFinishedSemaphores_) vkDestroySemaphore(device_, sem, nullptr);
        for (auto fence: inFlightFences_) vkDestroyFence(device_, fence, nullptr);

        for (auto buf: uniformBuffers_) vkDestroyBuffer(device_, buf, nullptr);
        for (auto mem: uniformBuffersMemory_) vkFreeMemory(device_, mem, nullptr);

        if (indexBuffer_) vkDestroyBuffer(device_, indexBuffer_, nullptr);
        if (indexBufferMemory_) vkFreeMemory(device_, indexBufferMemory_, nullptr);
        if (vertexBuffer_) vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        if (vertexBufferMemory_) vkFreeMemory(device_, vertexBufferMemory_, nullptr);

        if (textureSampler_) vkDestroySampler(device_, textureSampler_, nullptr);
        robotTexture_.reset();
        whiteTexture_.reset();
        shader_.reset();

        if (descriptorPool_) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
        if (pipelineLayout_) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        if (descriptorSetLayout_) vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);

        if (commandPool_) vkDestroyCommandPool(device_, commandPool_, nullptr);

        cleanupSwapChain();

        if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyDevice(device_, nullptr);
    }
    if (instance_) vkDestroyInstance(instance_, nullptr);
}

void Renderer::initRenderer() {
    lastFrameTime_ = std::chrono::steady_clock::now();

    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createCommandPool();
    createTextureResources();
    createDescriptorSetLayout();
    createSampler();
    createPipeline();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createVertexBuffer();
    createIndexBuffer();
    createCommandBuffers();
    createSyncObjects();

    updateProjection();
    for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
        updateUniformBuffer(i);
    }

    aout << "Vulkan renderer initialized (api 1.3, dynamic rendering)" << std::endl;
}

// ---------------------------------------------------------------------------
// Instance / physical device / logical device / surface
// ---------------------------------------------------------------------------

void Renderer::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MyApplication";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    // Request the highest practical API version: Vulkan 1.3 enables core
    // dynamic rendering and synchronization2.
    appInfo.apiVersion = VK_API_VERSION_1_3;

    const std::vector<const char *> instanceExtensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Vulkan instance");
    }
}

void Renderer::createSurface() {
    VkAndroidSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = app_->window;

    if (vkCreateAndroidSurfaceKHR(instance_, &createInfo, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Android surface");
    }
}

void Renderer::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("no Vulkan-capable GPU found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (const auto &device: devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        // Find a graphics queue family that also supports presentation to our surface.
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        uint32_t family = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport) {
                family = i;
                break;
            }
        }
        if (family == UINT32_MAX) continue;

        // Require the swapchain extension.
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, exts.data());
        bool hasSwapchain = false;
        for (const auto &e: exts) {
            if (strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
                break;
            }
        }
        if (!hasSwapchain) continue;

        // Prefer a 1.3-capable device that supports dynamic rendering.
        if (props.apiVersion < VK_API_VERSION_1_3) continue;

        VkPhysicalDeviceVulkan13Features v13{};
        v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        VkPhysicalDeviceFeatures2 feats2{};
        feats2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        feats2.pNext = &v13;
        vkGetPhysicalDeviceFeatures2(device, &feats2);
        if (!v13.dynamicRendering) continue;

        physicalDevice_ = device;
        graphicsFamily_ = family;
        aout << "Selected GPU: " << props.deviceName << " (api 0x" << std::hex << props.apiVersion
             << std::dec << ")" << std::endl;
        return;
    }

    throw std::runtime_error(
            "no GPU with Vulkan 1.3 + dynamic rendering + swapchain support found");
}

void Renderer::createLogicalDevice() {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsFamily_;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Enable the Vulkan 1.3 core dynamic-rendering feature (no render pass / framebuffer objects).
    VkPhysicalDeviceVulkan13Features v13Features{};
    v13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    v13Features.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &v13Features;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device");
    }

    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
}

// ---------------------------------------------------------------------------
// Swapchain
// ---------------------------------------------------------------------------

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR> &available) {
    // Prefer BGRA8 UNORM (the most common Android format), fall back to the first.
    for (const auto &f: available) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return available[0];
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &available) {
    // FIFO is always supported on Android and is the lowest-latency guaranteed mode.
    for (const auto &m: available) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    int32_t width = ANativeWindow_getWidth(app_->window);
    int32_t height = ANativeWindow_getHeight(app_->window);
    VkExtent2D extent;
    extent.width = std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
    extent.height = std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);
    return extent;
}

void Renderer::createSwapChain() {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());

    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &modeCount, modes.data());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(modes);
    VkExtent2D extent = chooseSwapExtent(capabilities);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swapchain");
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
}

void Renderer::createImageViews() {
    swapchainImageViews_.resize(swapchainImages_.size());
    for (size_t i = 0; i < swapchainImages_.size(); i++) {
        swapchainImageViews_[i] = vkutil::createImageView(device_, swapchainImages_[i],
                                                          swapchainImageFormat_,
                                                          VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void Renderer::cleanupSwapChain() {
    for (auto view: swapchainImageViews_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchainImageViews_.clear();
    if (swapchain_) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void Renderer::recreateSwapChain() {
    // Handle minimization: if the window has no size, wait until it does.
    int32_t width = ANativeWindow_getWidth(app_->window);
    int32_t height = ANativeWindow_getHeight(app_->window);
    if (width == 0 || height == 0) {
        return;
    }

    vkDeviceWaitIdle(device_);

    cleanupSwapChain();
    createSwapChain();
    createImageViews();

    // The projection depends on the swapchain extent; recompute and refresh UBOs.
    shaderNeedsNewProjectionMatrix_ = true;
    updateProjection();
    for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
        updateUniformBuffer(i);
    }
}

// ---------------------------------------------------------------------------
// Command pool / buffers
// ---------------------------------------------------------------------------

void Renderer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily_;

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool");
    }
}

void Renderer::createCommandBuffers() {
    commandBuffers_.resize(kMaxFramesInFlight);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

    if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers");
    }
}

// ---------------------------------------------------------------------------
// Descriptor set layout / pipeline
// ---------------------------------------------------------------------------

void Renderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboBinding, samplerBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
}

void Renderer::createPipeline() {
    // SPIR-V shader modules (compiled from GLSL at build time, embedded as bytes).
    shader_ = Shader::create(device_);
    assert(shader_);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = shader_->getVertexModule();
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = shader_->getFragmentModule();
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};

    // Vertex input: location 0 = vec3 position, location 1 = vec2 uv.
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attrDescs{};
    attrDescs[0].location = 0;
    attrDescs[0].binding = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset = offsetof(Vertex, position);
    attrDescs[1].location = 1;
    attrDescs[1].binding = 0;
    attrDescs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[1].offset = offsetof(Vertex, uv);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1; // viewport/scissor are dynamic

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    // Blend matches the original GL setup: glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA).
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                   VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = static_cast<uint32_t>(kPushTotalSize);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout");
    }

    // Dynamic rendering: declare the color attachment format so the pipeline knows
    // what it renders into, without a render pass / framebuffer object.
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &swapchainImageFormat_;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &pipeline_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline");
    }
}

// ---------------------------------------------------------------------------
// Descriptor pool / sets / sampler / textures
// ---------------------------------------------------------------------------

void Renderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(kMaxFramesInFlight * 2);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(kMaxFramesInFlight * 2);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = static_cast<uint32_t>(kMaxFramesInFlight * 2);
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
}

void Renderer::createDescriptorSets() {
    robotDescriptorSets_.resize(kMaxFramesInFlight);
    whiteDescriptorSets_.resize(kMaxFramesInFlight);

    // Allocate all sets (they share the same layout).
    std::vector<VkDescriptorSetLayout> layouts(kMaxFramesInFlight * 2, descriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> sets(layouts.size());
    if (vkAllocateDescriptorSets(device_, &allocInfo, sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets");
    }
    for (int i = 0; i < kMaxFramesInFlight; i++) {
        robotDescriptorSets_[i] = sets[i * 2];
        whiteDescriptorSets_[i] = sets[i * 2 + 1];
    }

    // Write each set: binding 0 = this frame's projection UBO, binding 1 = texture.
    for (int i = 0; i < kMaxFramesInFlight; i++) {
        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = uniformBuffers_[i];
        uboInfo.offset = 0;
        uboInfo.range = kUniformBufferSize;

        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet = robotDescriptorSets_[i];
        uboWrite.dstBinding = 0;
        uboWrite.dstArrayElement = 0;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &uboInfo;

        VkDescriptorImageInfo robotImgInfo{};
        robotImgInfo.sampler = textureSampler_;
        robotImgInfo.imageView = robotTexture_->getImageView();
        robotImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet robotSamplerWrite{};
        robotSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        robotSamplerWrite.dstSet = robotDescriptorSets_[i];
        robotSamplerWrite.dstBinding = 1;
        robotSamplerWrite.dstArrayElement = 0;
        robotSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        robotSamplerWrite.descriptorCount = 1;
        robotSamplerWrite.pImageInfo = &robotImgInfo;

        VkWriteDescriptorSet whiteUboWrite = uboWrite;
        whiteUboWrite.dstSet = whiteDescriptorSets_[i];

        VkDescriptorImageInfo whiteImgInfo{};
        whiteImgInfo.sampler = textureSampler_;
        whiteImgInfo.imageView = whiteTexture_->getImageView();
        whiteImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet whiteSamplerWrite{};
        whiteSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        whiteSamplerWrite.dstSet = whiteDescriptorSets_[i];
        whiteSamplerWrite.dstBinding = 1;
        whiteSamplerWrite.dstArrayElement = 0;
        whiteSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        whiteSamplerWrite.descriptorCount = 1;
        whiteSamplerWrite.pImageInfo = &whiteImgInfo;

        VkWriteDescriptorSet writes[] = {uboWrite, robotSamplerWrite, whiteUboWrite,
                                         whiteSamplerWrite};
        vkUpdateDescriptorSets(device_, 4, writes, 0, nullptr);
    }
}

void Renderer::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device_, &samplerInfo, nullptr, &textureSampler_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler");
    }
}

void Renderer::createTextureResources() {
    TextureAsset::VulkanContext ctx{physicalDevice_, device_, graphicsQueue_, commandPool_};

    auto assetManager = app_->activity->assetManager;
    robotTexture_ = TextureAsset::loadAsset(assetManager, "android_robot.png", ctx);
    // A 1x1 white texture is sampled by the tinted button / divider-line quads; the
    // actual color comes from the push constant.
    whiteTexture_ = TextureAsset::createSolidColor(255, 255, 255, 255, ctx);
}

// ---------------------------------------------------------------------------
// Geometry + uniform buffers
// ---------------------------------------------------------------------------

void Renderer::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(kQuadVertices[0]) * kQuadVertices.size();
    vkutil::createBuffer(device_, physicalDevice_, bufferSize,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         vertexBuffer_, vertexBufferMemory_);

    void *data;
    vkMapMemory(device_, vertexBufferMemory_, 0, bufferSize, 0, &data);
    memcpy(data, kQuadVertices.data(), (size_t) bufferSize);
    vkUnmapMemory(device_, vertexBufferMemory_);
}

void Renderer::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(kQuadIndices[0]) * kQuadIndices.size();
    indexCount_ = static_cast<uint32_t>(kQuadIndices.size());
    vkutil::createBuffer(device_, physicalDevice_, bufferSize,
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         indexBuffer_, indexBufferMemory_);

    void *data;
    vkMapMemory(device_, indexBufferMemory_, 0, bufferSize, 0, &data);
    memcpy(data, kQuadIndices.data(), (size_t) bufferSize);
    vkUnmapMemory(device_, indexBufferMemory_);
}

void Renderer::createUniformBuffers() {
    uniformBuffers_.resize(kMaxFramesInFlight);
    uniformBuffersMemory_.resize(kMaxFramesInFlight);
    uniformBuffersMapped_.resize(kMaxFramesInFlight);

    for (int i = 0; i < kMaxFramesInFlight; i++) {
        vkutil::createBuffer(device_, physicalDevice_, kUniformBufferSize,
                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             uniformBuffers_[i], uniformBuffersMemory_[i]);
        vkMapMemory(device_, uniformBuffersMemory_[i], 0, kUniformBufferSize, 0,
                    &uniformBuffersMapped_[i]);
    }
}

void Renderer::createSyncObjects() {
    imageAvailableSemaphores_.resize(kMaxFramesInFlight);
    renderFinishedSemaphores_.resize(kMaxFramesInFlight);
    inFlightFences_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < kMaxFramesInFlight; i++) {
        if (vkCreateSemaphore(device_, &semInfo, nullptr, &imageAvailableSemaphores_[i]) !=
                    VK_SUCCESS ||
            vkCreateSemaphore(device_, &semInfo, nullptr, &renderFinishedSemaphores_[i]) !=
                    VK_SUCCESS ||
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects");
        }
    }
}

// ---------------------------------------------------------------------------
// Projection UBO updates
// ---------------------------------------------------------------------------

void Renderer::updateProjection() {
    if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        return;
    }
    Utility::buildOrthographicMatrix(projectionMatrix_, kProjectionHalfHeight,
                                     float(swapchainExtent_.width) / float(swapchainExtent_.height),
                                     kProjectionNearPlane, kProjectionFarPlane);
    shaderNeedsNewProjectionMatrix_ = false;
}

void Renderer::updateUniformBuffer(uint32_t currentFrame) {
    memcpy(uniformBuffersMapped_[currentFrame], projectionMatrix_, kUniformBufferSize);
}

// ---------------------------------------------------------------------------
// Frame rendering
// ---------------------------------------------------------------------------

void Renderer::render() {
    // Advance the smooth rotation tween toward the target angle.
    {
        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;
        dt = std::min(dt, kRotationMaxFrameDelta);
        const float alpha = 1.f - expf(-kRotationSmoothingSpeed * dt);
        robotRotationDegrees_ += (targetRotationDegrees_ - robotRotationDegrees_) * alpha;
    }

    // If the window changed size, recreate the swapchain before rendering.
    if (app_->window) {
        int32_t w = ANativeWindow_getWidth(app_->window);
        int32_t h = ANativeWindow_getHeight(app_->window);
        if (w != int32_t(swapchainExtent_.width) || h != int32_t(swapchainExtent_.height)) {
            recreateSwapChain();
        }
    }
    if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        return;
    }

    if (shaderNeedsNewProjectionMatrix_) {
        updateProjection();
        for (uint32_t i = 0; i < kMaxFramesInFlight; i++) updateUniformBuffer(i);
    }

    // Keep the button rectangles in sync with the current render area.
    updateButtonRects();

    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                            imageAvailableSemaphores_[currentFrame_],
                                            VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swapchain image");
    }

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
    recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_];
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_];

    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(graphicsQueue_, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer");
    }

// Dynamic rendering does not manage the swapchain image layout, so transition it
// UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL before rendering. (The acquire semaphore
// guards the read at the COLOR_ATTACHMENT_OUTPUT stage.)
VkImageMemoryBarrier toColorBarrier{};
toColorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
toColorBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
toColorBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
toColorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
toColorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
toColorBarrier.image = swapchainImages_[imageIndex];
toColorBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
toColorBarrier.srcAccessMask = 0;
toColorBarrier.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                     &toColorBarrier);

// Clear to cornflower blue, draw, then leave the attachment in COLOR_ATTACHMENT_OPTIMAL.
VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = swapchainImageViews_[imageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color.float32[0] = kClearColor[0];
    colorAttachment.clearValue.color.float32[1] = kClearColor[1];
    colorAttachment.clearValue.color.float32[2] = kClearColor[2];
    colorAttachment.clearValue.color.float32[3] = kClearColor[3];

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, swapchainExtent_};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    // Negative viewport height flips the Y axis so the existing y-up projection
    // matrix and rotation logic match the original GL rendering exactly.
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = float(swapchainExtent_.height);
    viewport.width = float(swapchainExtent_.width);
    viewport.height = -float(swapchainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, swapchainExtent_};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    drawScene(commandBuffer);

vkCmdEndRendering(commandBuffer);

// Transition the swapchain image COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR so the
// presentation engine can display it.
VkImageMemoryBarrier toPresentBarrier{};
toPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
toPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
toPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
toPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
toPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
toPresentBarrier.image = swapchainImages_[imageIndex];
toPresentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
toPresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
toPresentBarrier.dstAccessMask = 0;
vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                     &toPresentBarrier);

if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer");
    }
}

void Renderer::drawScene(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, &offset);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);

    // Robots use the robot texture descriptor set.
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &robotDescriptorSets_[currentFrame_], 0, nullptr);
    drawRobotGrid(commandBuffer);

    // Buttons / dividers use the 1x1 white texture descriptor set.
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &whiteDescriptorSets_[currentFrame_], 0, nullptr);
    drawGridLines(commandBuffer);
    // Left button: warm orange.
    drawButton(commandBuffer, leftButton_, 1.f, 0.55f, 0.f, 0.85f);
    // Right button: teal/blue.
    drawButton(commandBuffer, rightButton_, 0.f, 0.6f, 1.f, 0.85f);
}

void Renderer::pushModelColor(VkCommandBuffer commandBuffer, const float *model,
                              const float *color) {
    vkCmdPushConstants(commandBuffer, pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       static_cast<uint32_t>(kPushModelOffset),
                       static_cast<uint32_t>(kPushModelSize), model);
    vkCmdPushConstants(commandBuffer, pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       static_cast<uint32_t>(kPushColorOffset),
                       static_cast<uint32_t>(kPushColorSize), color);
}

void Renderer::drawRobotGrid(VkCommandBuffer commandBuffer) {
    if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        return;
    }

    const float halfHeight = kProjectionHalfHeight;
    const float aspect = float(swapchainExtent_.width) / float(swapchainExtent_.height);
    const float halfWidth = halfHeight * aspect;

    int cols, rows;
    computeGrid(cols, rows);

    const float cellW = (2.f * halfWidth) / float(cols);
    const float cellH = (2.f * halfHeight) / float(rows);
    const float s = std::min(cellW, cellH) * 0.5f;

    float rot[16];
    float scale[16];
    float rotScale[16];
    Utility::buildRotationZMatrix(rot, robotRotationDegrees_);
    Utility::buildScaleMatrix(scale, s, s, 1.f);
    Utility::multiplyMatrix(rotScale, rot, scale);

    // Magenta tint preserved from the previous GL configuration.
    const float color[4] = {1.f, 0.5f, 1.f, 1.f};

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            const float centerX = -halfWidth + (float(c) + 0.5f) * cellW;
            const float centerY = -halfHeight + (float(r) + 0.5f) * cellH;

            float translate[16];
            float model[16];
            Utility::buildTranslationMatrix(translate, centerX, centerY, 0.f);
            Utility::multiplyMatrix(model, translate, rotScale);

            pushModelColor(commandBuffer, model, color);
            vkCmdDrawIndexed(commandBuffer, indexCount_, 1, 0, 0, 0);
        }
    }
}

void Renderer::drawGridLines(VkCommandBuffer commandBuffer) {
    if (proliferationLevel_ <= 0 || swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        return;
    }
    int cols, rows;
    computeGrid(cols, rows);
    if (cols <= 1 && rows <= 1) return;

    const float halfHeight = kProjectionHalfHeight;
    const float aspect = float(swapchainExtent_.width) / float(swapchainExtent_.height);
    const float halfWidth = halfHeight * aspect;
    const float cellW = (2.f * halfWidth) / float(cols);
    const float cellH = (2.f * halfHeight) / float(rows);
    const float lineHalf = std::min(cellW, cellH) * 0.03f;

    const float white[4] = {1.f, 1.f, 1.f, 1.f};
    for (int c = 1; c < cols; c++) {
        const float x = -halfWidth + float(c) * cellW;
        drawButton(commandBuffer, {x, 0.f, lineHalf, halfHeight}, white[0], white[1], white[2],
                   white[3]);
    }
    for (int r = 1; r < rows; r++) {
        const float y = -halfHeight + float(r) * cellH;
        drawButton(commandBuffer, {0.f, y, halfWidth, lineHalf}, white[0], white[1], white[2],
                   white[3]);
    }
}

void Renderer::drawButton(VkCommandBuffer commandBuffer, const ButtonRect &rect, float r, float g,
                          float b, float a) {
    float scale[16];
    float translate[16];
    float model[16];
    Utility::buildScaleMatrix(scale, rect.halfWidth, rect.halfHeight, 1.f);
    Utility::buildTranslationMatrix(translate, rect.centerX, rect.centerY, 0.f);
    Utility::multiplyMatrix(model, translate, scale);

    const float color[4] = {r, g, b, a};
    pushModelColor(commandBuffer, model, color);
    vkCmdDrawIndexed(commandBuffer, indexCount_, 1, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// Proliferation + button layout (logic preserved from the GL version)
// ---------------------------------------------------------------------------

void Renderer::proliferate() {
    proliferationLevel_++;
    if (proliferationLevel_ > kMaxProliferationLevel) {
        proliferationLevel_ = 0;
    }
    aout << "Proliferation level = " << proliferationLevel_
         << " (" << (1 << proliferationLevel_) << " copies)" << std::endl;
}

void Renderer::computeGrid(int &cols, int &rows) const {
    const int instanceCount = 1 << proliferationLevel_;
    const float aspect = (swapchainExtent_.height > 0)
                                 ? float(swapchainExtent_.width) / float(swapchainExtent_.height)
                                 : 1.f;

    int bestCols = 1;
    int bestRows = instanceCount;
    float bestError = std::fabs(float(bestCols) / float(bestRows) - aspect);
    for (int a = 0; a <= proliferationLevel_; a++) {
        int cc = 1 << a;
        int rr = instanceCount >> a;
        float error = std::fabs(float(cc) / float(rr) - aspect);
        if (error < bestError) {
            bestError = error;
            bestCols = cc;
            bestRows = rr;
        }
    }
    cols = bestCols;
    rows = bestRows;
}

void Renderer::updateButtonRects() {
    if (swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        return;
    }
    const float halfHeight = kProjectionHalfHeight;
    const float aspect = float(swapchainExtent_.width) / float(swapchainExtent_.height);
    const float halfWidth = halfHeight * aspect;

    const float buttonHalf = std::min(0.4f, halfWidth * 0.28f);
    const float margin = std::max(0.15f, halfWidth * 0.08f);
    const float offset = halfWidth - buttonHalf - margin;
    const float centerY = -halfHeight + buttonHalf + margin;

    leftButton_ = {-offset, centerY, buttonHalf, buttonHalf};
    rightButton_ = {offset, centerY, buttonHalf, buttonHalf};
}

// ---------------------------------------------------------------------------
// Input handling (logic preserved from the GL version)
// ---------------------------------------------------------------------------

void Renderer::handleInput() {
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) {
        return;
    }

    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto action = motionEvent.action;

        auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        aout << "Pointer(s): ";

        auto &pointer = motionEvent.pointers[pointerIndex];
        auto x = GameActivityPointerAxes_getX(&pointer);
        auto y = GameActivityPointerAxes_getY(&pointer);

        switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                aout << "(" << pointer.id << ", " << x << ", " << y << ") "
                     << "Pointer Down";

                if (swapchainExtent_.width > 0 && swapchainExtent_.height > 0) {
                    const float halfHeight = kProjectionHalfHeight;
                    const float halfWidth =
                            halfHeight * (float(swapchainExtent_.width) /
                                          float(swapchainExtent_.height));
                    const float glX =
                            (2.f * x / float(swapchainExtent_.width) - 1.f) * halfWidth;
                    const float glY =
                            (1.f - 2.f * y / float(swapchainExtent_.height)) * halfHeight;

                    auto inRect = [](float px, float py, const ButtonRect &r) {
                        return px >= r.centerX - r.halfWidth
                               && px <= r.centerX + r.halfWidth
                               && py >= r.centerY - r.halfHeight
                               && py <= r.centerY + r.halfHeight;
                    };

                    if (inRect(glX, glY, leftButton_)) {
                        targetRotationDegrees_ += 90.f;
                        aout << " [LEFT button] target = " << targetRotationDegrees_;
                    } else if (inRect(glX, glY, rightButton_)) {
                        targetRotationDegrees_ -= 90.f;
                        aout << " [RIGHT button] target = " << targetRotationDegrees_;
                    } else {
                        proliferate();
                    }
                }
                break;

            case AMOTION_EVENT_ACTION_CANCEL:
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                aout << "(" << pointer.id << ", " << x << ", " << y << ") "
                     << "Pointer Up";
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                for (auto index = 0; index < motionEvent.pointerCount; index++) {
                    pointer = motionEvent.pointers[index];
                    x = GameActivityPointerAxes_getX(&pointer);
                    y = GameActivityPointerAxes_getY(&pointer);
                    aout << "(" << pointer.id << ", " << x << ", " << y << ")";
                    if (index != (motionEvent.pointerCount - 1)) aout << ",";
                    aout << " ";
                }
                aout << "Pointer Move";
                break;
            default:
                aout << "Unknown MotionEvent Action: " << action;
        }
        aout << std::endl;
    }
    android_app_clear_motion_events(inputBuffer);

    for (auto i = 0; i < inputBuffer->keyEventsCount; i++) {
        auto &keyEvent = inputBuffer->keyEvents[i];
        aout << "Key: " << keyEvent.keyCode << " ";
        switch (keyEvent.action) {
            case AKEY_EVENT_ACTION_DOWN:
                aout << "Key Down";
                break;
            case AKEY_EVENT_ACTION_UP:
                aout << "Key Up";
                break;
            case AKEY_EVENT_ACTION_MULTIPLE:
                aout << "Multiple Key Actions";
                break;
            default:
                aout << "Unknown KeyEvent Action: " << keyEvent.action;
        }
        aout << std::endl;
    }
    android_app_clear_key_events(inputBuffer);
}