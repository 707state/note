#include "App.hpp"
#include <cstdlib>

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

void App::run() {
    if (!initWindow()) {
        return;
    }
    if (!initMetal()) {
        return;
    }
    if (!initPipeline()) {
        return;
    }
    mainLoop();
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

bool App::initPipeline() {
    // Load the compiled Metal library (default.metallib) from the build directory
    NS::Error* error = nullptr;

    // Get the path to the metallib next to the executable
    NS::String* libPath = NS::String::string("default.metallib", NS::UTF8StringEncoding);
    MTL::Library* library = m_device->newLibrary(libPath, &error);

    if (!library) {
        fprintf(stderr, "Failed to load Metal library: %s\n",
                error ? error->localizedDescription()->utf8String() : "unknown error");
        return false;
    }

    // Get shader functions
    MTL::Function* vertexFn = library->newFunction(
        NS::String::string("vertex_main", NS::UTF8StringEncoding));
    MTL::Function* fragmentFn = library->newFunction(
        NS::String::string("fragment_main", NS::UTF8StringEncoding));

    if (!vertexFn || !fragmentFn) {
        fprintf(stderr, "Failed to find shader functions\n");
        library->release();
        return false;
    }

    // Create render pipeline descriptor
    MTL::RenderPipelineDescriptor* pipelineDesc =
        MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFn);
    pipelineDesc->setFragmentFunction(fragmentFn);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(
        MTL::PixelFormatBGRA8Unorm);

    // Create pipeline state
    m_pipelineState = m_device->newRenderPipelineState(pipelineDesc, &error);
    if (!m_pipelineState) {
        fprintf(stderr, "Failed to create pipeline state: %s\n",
                error ? error->localizedDescription()->utf8String() : "unknown error");
        pipelineDesc->release();
        vertexFn->release();
        fragmentFn->release();
        library->release();
        return false;
    }

    printf("Render pipeline created successfully!\n");

    // Cleanup temporary objects
    pipelineDesc->release();
    vertexFn->release();
    fragmentFn->release();
    library->release();

    return true;
}

void App::mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        render();
    }
}

void App::render() {
    @autoreleasepool {
        CA::MetalDrawable* drawable = m_metalLayer->nextDrawable();
        if (!drawable) {
            return;
        }

        MTL::RenderPassDescriptor* renderPassDesc = MTL::RenderPassDescriptor::alloc()->init();
        MTL::RenderPassColorAttachmentDescriptor* colorAttachment =
            renderPassDesc->colorAttachments()->object(0);
        colorAttachment->setTexture(drawable->texture());
        colorAttachment->setLoadAction(MTL::LoadActionClear);
        colorAttachment->setClearColor(MTL::ClearColor(0.1, 0.2, 0.3, 1.0));
        colorAttachment->setStoreAction(MTL::StoreActionStore);

        MTL::CommandBuffer* commandBuffer = m_commandQueue->commandBuffer();
        MTL::RenderCommandEncoder* encoder =
            commandBuffer->renderCommandEncoder(renderPassDesc);

        // Draw the triangle using our pipeline
        encoder->setRenderPipelineState(m_pipelineState);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

        encoder->endEncoding();

        commandBuffer->presentDrawable(drawable);
        commandBuffer->commit();

        renderPassDesc->release();
    }
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
