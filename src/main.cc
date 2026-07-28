#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// ── helpers ──────────────────────────────────────────────────────────

static const char* bool_str(VkBool32 b) { return b ? "yes" : "no"; }

static const char* device_type_str(VkPhysicalDeviceType t)
{
    switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
    default:                                     return "Other";
    }
}

static const char* vendor_str(uint32_t id)
{
    switch (id) {
    case 0x1002: return "AMD";
    case 0x1010: return "ImgTec";
    case 0x10DE: return "NVIDIA";
    case 0x13B5: return "Arm";
    case 0x14E4: return "Broadcom";
    case 0x1AE0: return "Google";
    case 0x8086: return "Intel";
    case 0x8087: return "Intel (GPU)";
    case 0x5143: return "Qualcomm";
    case 0x106B: return "Apple";
    default:     return "Unknown";
    }
}

static std::string driver_version_str(uint32_t vid, uint32_t ver)
{
    // NVIDIA: major.minor.patch.extra
    if (vid == 0x10DE) {
        uint32_t major = (ver >> 22) & 0x3FF;
        uint32_t minor = (ver >> 14) & 0xFF;
        uint32_t patch = (ver >> 6) & 0xFF;
        uint32_t extra = ver & 0x3F;
        std::ostringstream os;
        os << major << '.' << minor << '.' << patch << '.' << extra;
        return os.str();
    }
    // Vulkan default (KHR scheme): major.minor.patch
    uint32_t major = ver >> 22;
    uint32_t minor = (ver >> 12) & 0x3FF;
    uint32_t patch = ver & 0xFFF;
    std::ostringstream os;
    os << major << '.' << minor << '.' << patch;
    return os.str();
}

// ── extension categorisation ─────────────────────────────────────────
//
// Extensions promoted into core Vulkan 1.1, 1.2, 1.3 — grouped by the
// version they were folded into so we can show them in context.

struct ExtEntry {
    std::string_view name;
    uint32_t         promotedVer;   // 0 = non-core, 100 = WSI
};

// clang-format off
static const ExtEntry s_extTable[] = {
    // Vulkan 1.1
    { "VK_KHR_bind_memory2",                 111 },
    { "VK_KHR_dedicated_allocation",         111 },
    { "VK_KHR_descriptor_update_template",   111 },
    { "VK_KHR_device_group",                 111 },
    { "VK_KHR_external_fence",               111 },
    { "VK_KHR_external_memory",              111 },
    { "VK_KHR_external_semaphore",           111 },
    { "VK_KHR_get_memory_requirements2",     111 },
    { "VK_KHR_maintenance1",                 111 },
    { "VK_KHR_maintenance2",                 111 },
    { "VK_KHR_maintenance3",                 111 },
    { "VK_KHR_multiview",                    111 },
    { "VK_KHR_relaxed_block_layout",         111 },
    { "VK_KHR_sampler_ycbcr_conversion",     111 },
    { "VK_KHR_shader_draw_parameters",       111 },
    { "VK_KHR_storage_buffer_storage_class", 111 },
    { "VK_KHR_variable_pointers",            111 },
    // Vulkan 1.2
    { "VK_KHR_8bit_storage",                 112 },
    { "VK_KHR_16bit_storage",                112 },
    { "VK_KHR_buffer_device_address",        112 },
    { "VK_KHR_create_renderpass2",           112 },
    { "VK_KHR_depth_stencil_resolve",        112 },
    { "VK_KHR_draw_indirect_count",          112 },
    { "VK_KHR_driver_properties",            112 },
    { "VK_KHR_image_format_list",            112 },
    { "VK_KHR_imageless_framebuffer",        112 },
    { "VK_KHR_sampler_mirror_clamp_to_edge", 112 },
    { "VK_KHR_separate_depth_stencil_layouts",   112 },
    { "VK_KHR_shader_atomic_int64",          112 },
    { "VK_KHR_shader_float16_int8",          112 },
    { "VK_KHR_shader_float_controls",        112 },
    { "VK_KHR_shader_subgroup_extended_types",   112 },
    { "VK_KHR_spirv_1_4",                   112 },
    { "VK_KHR_subgroup_extended_types",      112 },
    { "VK_KHR_timeline_semaphore",           112 },
    { "VK_KHR_uniform_buffer_standard_layout",   112 },
    { "VK_KHR_vulkan_memory_model",           112 },
    { "VK_EXT_host_query_reset",             112 },
    { "VK_EXT_sampler_filter_minmax",        112 },
    { "VK_EXT_scalar_block_layout",          112 },
    { "VK_EXT_separate_stencil_usage",       112 },
    { "VK_EXT_descriptor_indexing",          112 },
    { "VK_EXT_shader_viewport_index_layer",   112 },
    // Vulkan 1.3
    { "VK_KHR_copy_commands2",               113 },
    { "VK_KHR_dynamic_rendering",            113 },
    { "VK_KHR_format_feature_flags2",        113 },
    { "VK_KHR_global_priority",              113 },
    { "VK_KHR_maintenance4",                 113 },
    { "VK_KHR_shader_integer_dot_product",   113 },
    { "VK_KHR_shader_non_semantic_info",     113 },
    { "VK_KHR_shader_terminate_invocation",  113 },
    { "VK_KHR_synchronization2",             113 },
    { "VK_KHR_zero_initialize_workgroup_memory", 113 },
    { "VK_EXT_4444_formats",                 113 },
    { "VK_EXT_extended_dynamic_state",       113 },
    { "VK_EXT_extended_dynamic_state2",      113 },
    { "VK_EXT_image_robustness",             113 },
    { "VK_EXT_inline_uniform_block",          113 },
    { "VK_EXT_pipeline_creation_cache_control", 113 },
    { "VK_EXT_pipeline_creation_feedback",   113 },
    { "VK_EXT_private_data",                 113 },
    { "VK_EXT_shader_demote_to_helper_invocation", 113 },
    { "VK_EXT_subgroup_size_control",        113 },
    { "VK_EXT_texel_buffer_alignment",       113 },
    { "VK_EXT_texture_compression_astc_hdr", 113 },
    { "VK_EXT_tooling_info",                 113 },
    { "VK_EXT_ycbcr_2plane_444_formats",     113 },
    // WSI / surface (special group)
    { "VK_KHR_surface",                      100 },
    { "VK_KHR_swapchain",                    100 },
    { "VK_KHR_display",                      100 },
    { "VK_KHR_display_swapchain",            100 },
    { "VK_KHR_shared_presentable_image",     100 },
    { "VK_KHR_win32_surface",                100 },
    { "VK_KHR_xlib_surface",                 100 },
    { "VK_KHR_xcb_surface",                  100 },
    { "VK_KHR_wayland_surface",              100 },
    { "VK_EXT_swapchain_maintenance1",       100 },
    { "VK_EXT_full_screen_exclusive",        100 },
    { "VK_EXT_headless_surface",             100 },
    { "VK_EXT_metal_surface",                100 },
    { "VK_FUCHSIA_imagepipe_surface",         100 },
    { "VK_GGP_stream_descriptor_surface",    100 },
    { "VK_MVK_macos_surface",                100 },
    { "VK_MVK_ios_surface",                  100 },
    { "VK_NN_vi_surface",                    100 },
    { "VK_QNX_screen_surface",               100 },
    { "VK_KHR_surface_protected_capabilities",100 },
    { "VK_EXT_surface_maintenance1",         100 },
};
// clang-format on

static uint32_t ext_category(std::string_view name)
{
    for (auto& e : s_extTable)
        if (e.name == name) return e.promotedVer;
    return 0; // other / vendor
}

// ── main ─────────────────────────────────────────────────────────────

int main()
{
    // ── 1. Instance ─────────────────────────────────────────────

    // Enumerate instance-level extensions
    uint32_t instExtCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, nullptr);
    std::vector<VkExtensionProperties> instExts(instExtCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, instExts.data());

    std::cout << "====== Vulkan Device Query ======\n\n";
    std::cout << "Instance-layer extensions (" << instExtCount << "):\n";
    for (auto& e : instExts)
        std::cout << "  " << e.extensionName << " (spec v" << e.specVersion << ")\n";
    std::cout << '\n';

    VkApplicationInfo appInfo{};
    appInfo.sType           = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Device Query";
    appInfo.apiVersion      = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        std::cerr << "vkCreateInstance failed (" << res << ")\n";
        return -1;
    }

    // ── 2. Enumerate physical devices ───────────────────────────

    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
    if (devCount == 0) {
        std::cout << "No Vulkan-capable devices found.\n";
        vkDestroyInstance(instance, nullptr);
        return 0;
    }

    std::vector<VkPhysicalDevice> devices(devCount);
    vkEnumeratePhysicalDevices(instance, &devCount, devices.data());
    std::cout << "Physical devices found: " << devCount << "\n\n";

    // ── 3. Per-device query ─────────────────────────────────────

    for (uint32_t di = 0; di < devCount; ++di) {
        VkPhysicalDevice pd = devices[di];

        // ── 3a. Properties (name, type, IDs, limits, sparse) ────
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);

        std::cout << "─── Device " << di << " ──────────────────────────────────\n";
        std::cout << "  Name            : " << props.deviceName << '\n';
        std::cout << "  Type            : " << device_type_str(props.deviceType) << '\n';
        std::cout << "  Vendor          : " << vendor_str(props.vendorID)
                  << " (0x" << std::hex << props.vendorID << std::dec << ")\n";
        std::cout << "  Device ID       : 0x" << std::hex << props.deviceID << std::dec << '\n';
        std::cout << "  Driver version  : " << driver_version_str(props.vendorID, props.driverVersion)
                  << "  (raw: 0x" << std::hex << props.driverVersion << std::dec << ")\n";
        std::cout << "  API version     : " << VK_VERSION_MAJOR(props.apiVersion) << '.'
                  << VK_VERSION_MINOR(props.apiVersion) << '.'
                  << VK_VERSION_PATCH(props.apiVersion) << '\n';
        std::cout << '\n';

        const auto& lim = props.limits;
        std::cout << "  Limits:\n";
        std::cout << "    maxImageDimension1D/2D/3D     : " << lim.maxImageDimension1D
                  << " / " << lim.maxImageDimension2D
                  << " / " << lim.maxImageDimension3D << '\n';
        std::cout << "    maxImageDimensionCube         : " << lim.maxImageDimensionCube << '\n';
        std::cout << "    maxVertexInputAttributes      : " << lim.maxVertexInputAttributes << '\n';
        std::cout << "    maxVertexInputBindings        : " << lim.maxVertexInputBindings << '\n';
        std::cout << "    maxViewports                  : " << lim.maxViewports << '\n';
        std::cout << "    maxViewportDimensions         : " << lim.maxViewportDimensions[0]
                  << 'x' << lim.maxViewportDimensions[1] << '\n';
        std::cout << "    maxFramebufferWidth/Height    : " << lim.maxFramebufferWidth
                  << " / " << lim.maxFramebufferHeight << '\n';
        std::cout << "    maxFramebufferLayers          : " << lim.maxFramebufferLayers << '\n';
        std::cout << "    maxColorAttachments           : " << lim.maxColorAttachments << '\n';
        std::cout << "    maxPushConstantsSize          : " << lim.maxPushConstantsSize << " bytes\n";
        std::cout << "    maxMemoryAllocationCount      : " << lim.maxMemoryAllocationCount << '\n';
        std::cout << "    maxSamplerAllocationCount     : " << lim.maxSamplerAllocationCount << '\n';
        std::cout << "    maxSamplerAnisotropy          : " << lim.maxSamplerAnisotropy << '\n';
        std::cout << "    maxTextureLodBias             : " << lim.maxSamplerLodBias << '\n';
        std::cout << "    minMemoryMapAlignment         : " << lim.minMemoryMapAlignment << " bytes\n";
        std::cout << "    minTexelBufferOffsetAlignment : " << lim.minTexelBufferOffsetAlignment << '\n';
        std::cout << "    minStorageBufferOffsetAlignment: " << lim.minStorageBufferOffsetAlignment << '\n';
        std::cout << "    minUniformBufferOffsetAlignment: " << lim.minUniformBufferOffsetAlignment << '\n';
        std::cout << "    maxComputeWorkGroupCount      : "
                  << lim.maxComputeWorkGroupCount[0] << 'x'
                  << lim.maxComputeWorkGroupCount[1] << 'x'
                  << lim.maxComputeWorkGroupCount[2] << '\n';
        std::cout << "    maxComputeWorkGroupInvocations: " << lim.maxComputeWorkGroupInvocations << '\n';
        std::cout << "    maxComputeWorkGroupSize       : "
                  << lim.maxComputeWorkGroupSize[0] << 'x'
                  << lim.maxComputeWorkGroupSize[1] << 'x'
                  << lim.maxComputeWorkGroupSize[2] << '\n';
        std::cout << "    maxComputeSharedMemorySize    : " << lim.maxComputeSharedMemorySize << " bytes\n";
        std::cout << "    bufferImageGranularity        : " << lim.bufferImageGranularity << '\n';
        std::cout << "    maxTessellationGenerationLevel: " << lim.maxTessellationGenerationLevel << '\n';
        std::cout << "    maxTessellationPatchSize      : " << lim.maxTessellationPatchSize << '\n';
        std::cout << '\n';

        // Sparse resource properties
        auto& sp = props.sparseProperties;
        std::cout << "  Sparse resource support:\n";
        std::cout << "    residencyStandard2DBlockShape          : "
                  << bool_str(sp.residencyStandard2DBlockShape) << '\n';
        std::cout << "    residencyStandard2DMultisampleBlockShape: "
                  << bool_str(sp.residencyStandard2DMultisampleBlockShape) << '\n';
        std::cout << "    residencyStandard3DBlockShape          : "
                  << bool_str(sp.residencyStandard3DBlockShape) << '\n';
        std::cout << "    residencyAlignedMipSize                : "
                  << bool_str(sp.residencyAlignedMipSize) << '\n';
        std::cout << "    residencyNonResidentStrict             : "
                  << bool_str(sp.residencyNonResidentStrict) << '\n';
        std::cout << '\n';

        // ── 3b. Features ────────────────────────────────────────
        VkPhysicalDeviceFeatures features{};
        vkGetPhysicalDeviceFeatures(pd, &features);

        std::cout << "  Features:\n";
#define SHOW_FEAT(f) std::cout << "    " #f " : " << bool_str(features.f) << '\n'
        SHOW_FEAT(robustBufferAccess);
        SHOW_FEAT(fullDrawIndexUint32);
        SHOW_FEAT(imageCubeArray);
        SHOW_FEAT(independentBlend);
        SHOW_FEAT(geometryShader);
        SHOW_FEAT(tessellationShader);
        SHOW_FEAT(sampleRateShading);
        SHOW_FEAT(dualSrcBlend);
        SHOW_FEAT(logicOp);
        SHOW_FEAT(multiDrawIndirect);
        SHOW_FEAT(drawIndirectFirstInstance);
        SHOW_FEAT(depthClamp);
        SHOW_FEAT(depthBiasClamp);
        SHOW_FEAT(fillModeNonSolid);
        SHOW_FEAT(depthBounds);
        SHOW_FEAT(wideLines);
        SHOW_FEAT(largePoints);
        SHOW_FEAT(alphaToOne);
        SHOW_FEAT(multiViewport);
        SHOW_FEAT(samplerAnisotropy);
        SHOW_FEAT(textureCompressionETC2);
        SHOW_FEAT(textureCompressionASTC_LDR);
        SHOW_FEAT(textureCompressionBC);
        SHOW_FEAT(occlusionQueryPrecise);
        SHOW_FEAT(pipelineStatisticsQuery);
        SHOW_FEAT(vertexPipelineStoresAndAtomics);
        SHOW_FEAT(fragmentStoresAndAtomics);
        SHOW_FEAT(shaderTessellationAndGeometryPointSize);
        SHOW_FEAT(shaderImageGatherExtended);
        SHOW_FEAT(shaderStorageImageExtendedFormats);
        SHOW_FEAT(shaderStorageImageMultisample);
        SHOW_FEAT(shaderStorageImageReadWithoutFormat);
        SHOW_FEAT(shaderStorageImageWriteWithoutFormat);
        SHOW_FEAT(shaderUniformBufferArrayDynamicIndexing);
        SHOW_FEAT(shaderSampledImageArrayDynamicIndexing);
        SHOW_FEAT(shaderStorageBufferArrayDynamicIndexing);
        SHOW_FEAT(shaderStorageImageArrayDynamicIndexing);
        SHOW_FEAT(shaderClipDistance);
        SHOW_FEAT(shaderCullDistance);
        SHOW_FEAT(shaderFloat64);
        SHOW_FEAT(shaderInt64);
        SHOW_FEAT(shaderInt16);
        SHOW_FEAT(shaderResourceResidency);
        SHOW_FEAT(shaderResourceMinLod);
        SHOW_FEAT(sparseBinding);
        SHOW_FEAT(sparseResidencyBuffer);
        SHOW_FEAT(sparseResidencyImage2D);
        SHOW_FEAT(sparseResidencyImage3D);
        SHOW_FEAT(sparseResidency2Samples);
        SHOW_FEAT(sparseResidency4Samples);
        SHOW_FEAT(sparseResidency8Samples);
        SHOW_FEAT(sparseResidency16Samples);
        SHOW_FEAT(sparseResidencyAliased);
        SHOW_FEAT(variableMultisampleRate);
        SHOW_FEAT(inheritedQueries);
#undef SHOW_FEAT
        std::cout << '\n';

        // ── 3c. Queue families ───────────────────────────────────
        uint32_t qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfProps(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, qfProps.data());

        std::cout << "  Queue families (" << qfCount << "):\n";
        for (uint32_t q = 0; q < qfCount; ++q) {
            auto& qf    = qfProps[q];
            auto& gran  = qf.minImageTransferGranularity;

            std::ostringstream types;
            if (qf.queueFlags & VK_QUEUE_GRAPHICS_BIT)      types << "graphics ";
            if (qf.queueFlags & VK_QUEUE_COMPUTE_BIT)       types << "compute ";
            if (qf.queueFlags & VK_QUEUE_TRANSFER_BIT)      types << "transfer ";
            if (qf.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) types << "sparse ";
            if (qf.queueFlags & VK_QUEUE_PROTECTED_BIT)     types << "protected ";
            if (qf.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) types << "video-decode ";
            if (qf.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) types << "video-encode ";
            if (qf.queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV) types << "optical-flow ";

            std::cout << "    [" << q << "] " << qf.queueCount << " queues  "
                      << types.str() << "\n"
                      << "          timestampValidBits: " << (int)qf.timestampValidBits
                      << "  minImageTransferGranularity: "
                      << gran.width << 'x' << gran.height << 'x' << gran.depth << '\n';
        }
        std::cout << '\n';

        // ── 3d. Memory ───────────────────────────────────────────
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(pd, &memProps);

        std::cout << "  Memory heaps (" << memProps.memoryHeapCount << "):\n";
        for (uint32_t h = 0; h < memProps.memoryHeapCount; ++h) {
            auto& heap = memProps.memoryHeaps[h];
            std::cout << "    heap[" << h << "]: " << (heap.size >> 20) << " MiB ("
                      << (heap.size >> 30) << " GiB)"
                      << ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? " device-local" : "")
                      << ((heap.flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) ? " multi-instance" : "")
                      << '\n';
        }
        std::cout << "  Memory types (" << memProps.memoryTypeCount << "):\n";
        for (uint32_t t = 0; t < memProps.memoryTypeCount; ++t) {
            auto& mt = memProps.memoryTypes[t];
            std::ostringstream flags;
            if (mt.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)     flags << "DEVICE_LOCAL ";
            if (mt.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)     flags << "HOST_VISIBLE ";
            if (mt.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)    flags << "HOST_COHERENT ";
            if (mt.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)      flags << "HOST_CACHED ";
            if (mt.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) flags << "LAZILY_ALLOCATED ";
            if (mt.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT)        flags << "PROTECTED ";
            if (mt.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) flags << "DEVICE_COHERENT_AMD ";
            if (mt.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD) flags << "DEVICE_UNCACHED_AMD ";
            if (mt.propertyFlags & VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV)  flags << "RDMA_CAPABLE_NV ";

            std::cout << "    type[" << t << "]: heap=" << mt.heapIndex
                      << "  " << flags.str() << '\n';
        }
        std::cout << '\n';

        // ── 3e. Device extensions ────────────────────────────────
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data());

        std::sort(exts.begin(), exts.end(),
                  [](auto& a, auto& b) { return strcmp(a.extensionName, b.extensionName) < 0; });

        std::cout << "  Device extensions (" << extCount << " total):\n";

        // Group: core-promoted (1.1 → 1.2 → 1.3), WSI, then vendor/other
        std::vector<const VkExtensionProperties*> core11, core12, core13, wsi, other;
        for (auto& e : exts) {
            auto cat = ext_category(e.extensionName);
            if (cat == 111)      core11.push_back(&e);
            else if (cat == 112) core12.push_back(&e);
            else if (cat == 113) core13.push_back(&e);
            else if (cat == 100) wsi.push_back(&e);
            else                 other.push_back(&e);
        }

        auto print_group = [](const char* label, auto& vec) {
            if (vec.empty()) return;
            std::cout << "    --- " << label << " ---\n";
            for (auto* e : vec)
                std::cout << "      " << e->extensionName << " (v" << e->specVersion << ")\n";
        };

        print_group("Vulkan 1.1 promoted", core11);
        print_group("Vulkan 1.2 promoted", core12);
        print_group("Vulkan 1.3 promoted", core13);
        print_group("WSI / surface",       wsi);
        print_group("Other / vendor",      other);
        std::cout << '\n';
    }

    // ── 4. Cleanup ──────────────────────────────────────────────
    vkDestroyInstance(instance, nullptr);
    std::cout << "====== Done ======\n";
    return 0;
}

