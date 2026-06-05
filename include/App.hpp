#pragma once

#include "Metal/MTLCommandQueue.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "QuartzCore/CAMetalDrawable.hpp"
#include "QuartzCore/CAMetalLayer.hpp"
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

struct GLFWwindow;
class App {
public:
  App();
  ~App();
    void init();
  void run();

private:
  bool initWindow();
  bool initMetal();
  void mainLoop();
  void createTriangle();
  void createDefaultLibrary();
  void createCommandQueue();
  void createRenderPipeline();
  void sendRenderCommand();
  void encodeRenderCommand(MTL::RenderCommandEncoder *renderEncoder);
  void render();
  void cleanup();

  GLFWwindow *m_window = nullptr;
  MTL::Device *m_device = nullptr;
  MTL::CommandQueue *m_commandQueue = nullptr;
  CA::MetalLayer *m_metalLayer = nullptr;
  CA::MetalDrawable *metalDrawable;

  MTL::RenderPipelineState *m_pipelineState = nullptr;
  MTL::Library *metalDefaultLibrary;
  MTL::CommandQueue *metalCommandQueue;
  MTL::CommandBuffer *metalCommandBuffer;
  MTL::RenderPipelineState *metalRenderPSO;
  MTL::Buffer *triangleVertexBuffer;

  static constexpr int kWindowWidth = 800;
  static constexpr int kWindowHeight = 600;
};
