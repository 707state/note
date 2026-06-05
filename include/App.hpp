#pragma once

#include "Metal/MTLDevice.hpp"
#include "QuartzCore/CAMetalLayer.hpp"
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>


struct GLFWwindow;
class App {
public:
    App();
    ~App();

    void run();

private:
    bool initWindow();
    bool initMetal();
    bool initPipeline();
    void mainLoop();
    void render();
    void cleanup();

    GLFWwindow* m_window = nullptr;
    MTL::Device* m_device = nullptr;
    MTL::CommandQueue* m_commandQueue = nullptr;
    CA::MetalLayer* m_metalLayer = nullptr;
    MTL::RenderPipelineState* m_pipelineState = nullptr;

    static constexpr int kWindowWidth = 800;
    static constexpr int kWindowHeight = 600;
};
