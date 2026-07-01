#include "Shader.h"
#include "AndroidOut.h"
#include "ShaderSPIRV.h" // generated: kVertSpv / kFragSpv

VkShaderModule Shader::createShaderModule(VkDevice device, const uint8_t *code, uint32_t size) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code);

    VkShaderModule module;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        aout << "failed to create shader module" << std::endl;
        return VK_NULL_HANDLE;
    }
    return module;
}

std::unique_ptr<Shader> Shader::create(VkDevice device) {
    VkShaderModule vert = createShaderModule(device, kVertSpv, kVertSpvSize);
    VkShaderModule frag = createShaderModule(device, kFragSpv, kFragSpvSize);
    if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
        if (vert) vkDestroyShaderModule(device, vert, nullptr);
        if (frag) vkDestroyShaderModule(device, frag, nullptr);
        return nullptr;
    }
    return std::unique_ptr<Shader>(new Shader(device, vert, frag));
}

Shader::Shader(VkDevice device, VkShaderModule vertexModule, VkShaderModule fragmentModule)
        : device_(device), vertexModule_(vertexModule), fragmentModule_(fragmentModule) {}

Shader::~Shader() {
    if (vertexModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, vertexModule_, nullptr);
    }
    if (fragmentModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, fragmentModule_, nullptr);
    }
}
