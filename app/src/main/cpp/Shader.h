#ifndef ANDROIDGLINVESTIGATIONS_SHADER_H
#define ANDROIDGLINVESTIGATIONS_SHADER_H

#include <vulkan/vulkan.h>
#include <memory>

/*!
 * Holds the SPIR-V shader modules for the single graphics pipeline used by the
 * renderer. The SPIR-V bytecode is embedded at build time (see ShaderSPIRV.h,
 * generated from shaders/shader.vert|frag by glslc), so the app no longer ships
 * GLSL source strings.
 */
class Shader {
public:
    /*!
     * Creates the vertex and fragment shader modules from the embedded SPIR-V.
     * @param device the logical Vulkan device that will own the modules.
     * @return a Shader on success, or nullptr on failure.
     */
    static std::unique_ptr<Shader> create(VkDevice device);

    ~Shader();

    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;

    constexpr VkShaderModule getVertexModule() const { return vertexModule_; }
    constexpr VkShaderModule getFragmentModule() const { return fragmentModule_; }

private:
    Shader(VkDevice device, VkShaderModule vertexModule, VkShaderModule fragmentModule);

    static VkShaderModule createShaderModule(VkDevice device, const uint8_t *code, uint32_t size);

    VkDevice device_;
    VkShaderModule vertexModule_;
    VkShaderModule fragmentModule_;
};

#endif //ANDROIDGLINVESTIGATIONS_SHADER_H