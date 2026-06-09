#include "App.hpp"
#include "Foundation/NSString.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPass.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "Metal/MTLResource.hpp"
#include "QuartzCore/CAMetalDrawable.hpp"
#include "simd/simd.h"

// 顶点结构体，内存布局必须与 shader 中的 VertexIn 完全一致
struct VertexData {
    simd::float3 position;
    simd::float4 color;
};
#include <cassert>
#include <cstdlib>
#include <iostream>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

App::App() = default;

App::~App() {
    cleanup();
}

void App::init() {
    if (!initWindow()) {
        return;
    }
    if (!initMetal()) {
        return;
    }
    createTriangle();
    createDefaultLibrary();
    createCommandQueue();
    createRenderPipeline();
}

void App::createTriangle(){
    // 每个顶点包含位置 + 颜色，对应 shader 里的 VertexIn
    VertexData triangleVertices[] = {
        { {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },  // 顶部  — 红
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },  // 左下  — 绿
        { {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },  // 右下  — 蓝
    };
    triangleVertexBuffer = m_device->newBuffer(
        triangleVertices,
        sizeof(triangleVertices),
        MTL::ResourceStorageModeShared
    );
}
void App::createDefaultLibrary(){
    // newDefaultLibrary() requires an App Bundle. For a plain CLI binary the
    // executable lives next to default.metallib inside the build directory, so
    // we load it by relative path.
    NS::Error* error = nullptr;
    NS::String* libPath = NS::String::string("default.metallib", NS::UTF8StringEncoding);
    metalDefaultLibrary = m_device->newLibrary(libPath, &error);
    if(!metalDefaultLibrary){
        std::cerr<<"Failed to load default library: "
                 <<(error ? error->localizedDescription()->utf8String() : "unknown")
                 <<std::endl;
        std::exit(-1);
    }
}

void App::createCommandQueue(){
    m_commandQueue=m_device->newCommandQueue();
}

bool App::initWindow() {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }

    // Tell GLFW not to create an OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_window = glfwCreateWindow(kWindowWidth, kWindowHeight,
                                "LearnMetal", nullptr, nullptr);
    if (!m_window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    return true;
}
void App::createRenderPipeline(){
    MTL::Function* vertexShader=metalDefaultLibrary->newFunction(NS::String::string("vertex_main",NS::ASCIIStringEncoding));
    assert(vertexShader);
    MTL::Function *fragmentShader=metalDefaultLibrary->newFunction(NS::String::string("fragment_main",NS::ASCIIStringEncoding));
    assert(fragmentShader);
    MTL::RenderPipelineDescriptor* renderPipelineDescriptor=MTL::RenderPipelineDescriptor::alloc()->init();
    renderPipelineDescriptor->setLabel(NS::String::string("RenderPipeline",NS::ASCIIStringEncoding));
    renderPipelineDescriptor->setVertexFunction(vertexShader);
    renderPipelineDescriptor->setFragmentFunction(fragmentShader);
    assert(renderPipelineDescriptor);
    MTL::PixelFormat pixelFormat = MTL::PixelFormatBGRA8Unorm;
    renderPipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(pixelFormat);
    NS::Error* error;
    metalRenderPSO=m_device->newRenderPipelineState(renderPipelineDescriptor,&error);
    renderPipelineDescriptor->release();
}

bool App::initMetal() {
    m_device = MTL::CreateSystemDefaultDevice();
    if (!m_device) {
        fprintf(stderr, "Failed to create Metal device\n");
        return false;
    }

    m_commandQueue = m_device->newCommandQueue();
    if (!m_commandQueue) {
        fprintf(stderr, "Failed to create command queue\n");
        return false;
    }

    // Get the native Cocoa window and set up CAMetalLayer
    NSWindow* nsWindow = glfwGetCocoaWindow(m_window);
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    metalLayer.device = (__bridge id<MTLDevice>)m_device;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalLayer.drawableSize = CGSizeMake(kWindowWidth, kWindowHeight);

    nsWindow.contentView.layer = metalLayer;
    nsWindow.contentView.wantsLayer = YES;

    m_metalLayer = (__bridge CA::MetalLayer*)metalLayer;

    printf("Metal device: %s\n", m_device->name()->utf8String());

    return true;
}

void App::run(){
    while(!glfwWindowShouldClose(m_window)){
        @autoreleasepool{
            metalDrawable = m_metalLayer->nextDrawable();
            render();
        }
        glfwPollEvents();
    }
}
void App::render(){
   sendRenderCommand(); 
}

void App::sendRenderCommand(){
    metalCommandBuffer=m_commandQueue->commandBuffer();
    MTL::RenderPassDescriptor* renderPassDescriptor=MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor *cd=renderPassDescriptor->colorAttachments()->object(0);
    cd->setTexture(metalDrawable->texture());
    cd->setLoadAction(MTL::LoadActionClear);
    cd->setClearColor(MTL::ClearColor(41.0f/255.0f, 42.0f/255.0f, 48.0f/255.0f, 1.0));
    cd->setStoreAction(MTL::StoreActionStore);
    MTL::RenderCommandEncoder* renderCommandEncoder = metalCommandBuffer->renderCommandEncoder(renderPassDescriptor);
    encodeRenderCommand(renderCommandEncoder);
    renderCommandEncoder->endEncoding();

    metalCommandBuffer->presentDrawable(metalDrawable);
    metalCommandBuffer->commit();
    metalCommandBuffer->waitUntilCompleted();
    renderPassDescriptor->release();
}
void App::encodeRenderCommand(MTL::RenderCommandEncoder *renderCommandEncoder){
    renderCommandEncoder->setRenderPipelineState(metalRenderPSO);
    renderCommandEncoder->setVertexBuffer(triangleVertexBuffer, 0, 0);
    MTL::PrimitiveType typeTriangle = MTL::PrimitiveTypeTriangle;
    NS::UInteger vertexStart = 0;
    NS::UInteger vertexCount = 3;
    renderCommandEncoder->drawPrimitives(typeTriangle, vertexStart, vertexCount);
}

void App::cleanup() {
    if (metalRenderPSO) {
        metalRenderPSO->release();
        metalRenderPSO = nullptr;
    }
    if (m_commandQueue) {
        m_commandQueue->release();
        m_commandQueue = nullptr;
    }
    if (m_device) {
        m_device->release();
        m_device = nullptr;
    }
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}
