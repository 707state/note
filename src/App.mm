#include "App.hpp"
#include "App.h"
#include "Foundation/NSString.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "Metal/MTLResource.hpp"
#include "QuartzCore/CAMetalDrawable.hpp"
#include "simd/simd.h"
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
    simd::float3 triangleVertices[]={
        {
            -.5f,
            -.5f,
            0.f
        },
        {
            .5f,
            -.5f,
            0.f
        },
        {
            0.f,
            .5f,
            0.f
        }
    };
    triangleVertexBuffer=m_device->newBuffer(&triangleVertices,sizeof(triangleVertices),MTL::ResourceStorageModeShared);

}
void App::createDefaultLibrary(){
    metalDefaultLibrary=m_device->newDefaultLibrary();
    if(!metalDefaultLibrary){
        std::cerr<<"Failed to load default library"<<std::endl;
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
    MTL::Function* vertexShader=metalDefaultLibrary->newFunction(NS::String::string("vertexShader",NS::ASCIIStringEncoding));
    assert(vertexShader);
    MTL::Function *fragmentShader=metalDefaultLibrary->newFunction(NS::String::string("fragmentShader",NS::ASCIIStringEncoding));
    assert(fragmentShader);
    MTL::RenderPipelineDescriptor* renderPipelineDescriptor=MTL::RenderPipelineDescriptor::alloc()->init();
    renderPipelineDescriptor->setLabel(NS::String::string("RenderPipeline",NS::ASCIIStringEncoding));
    renderPipelineDescriptor->setVertexFunction(vertexShader);
    renderPipelineDescriptor->setFragmentFunction(fragmentShader);
    assert(renderPipelineDescriptor);
    MTL::PixelFormat pixelFormat=(MTL::PixelFormat)m_metalLayer->pixelFormat();
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
            metalDrawable=(__bridge CA::MetalDrawable*)[m_metalLayer nextDrawable];
            render();
        }
        glfwPollEvents();
    }
}
void App::render(){
    
}
void App::cleanup() {
    if (m_pipelineState) {
        m_pipelineState->release();
        m_pipelineState = nullptr;
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
