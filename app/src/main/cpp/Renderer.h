#ifndef ANDROIDGLINVESTIGATIONS_RENDERER_H
#define ANDROIDGLINVESTIGATIONS_RENDERER_H

#include <EGL/egl.h>
#include <chrono>
#include <memory>

#include "Model.h"
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

class Renderer {
public:
    /*!
     * @param pApp the android_app this Renderer belongs to, needed to configure GL
     */
    inline Renderer(android_app *pApp) :
            app_(pApp),
            display_(EGL_NO_DISPLAY),
            surface_(EGL_NO_SURFACE),
            context_(EGL_NO_CONTEXT),
            width_(0),
            height_(0),
            shaderNeedsNewProjectionMatrix_(true),
            robotRotationDegrees_(0.f),
            targetRotationDegrees_(0.f),
            lastFrameTime_(std::chrono::steady_clock::now()) {
        initRenderer();
    }

    virtual ~Renderer();

    /*!
     * Handles input from the android_app.
     *
     * Note: this will clear the input queue
     */
    void handleInput();

    /*!
     * Renders all the models in the renderer
     */
    void render();

private:
    /*!
     * Performs necessary OpenGL initialization. Customize this if you want to change your EGL
     * context or application-wide settings.
     */
    void initRenderer();

    /*!
     * @brief we have to check every frame to see if the framebuffer has changed in size. If it has,
     * update the viewport accordingly
     */
    void updateRenderArea();

    /*!
     * Creates the models for this sample. You'd likely load a scene configuration from a file or
     * use some other setup logic in your full game.
     */
    void createModels();

    /*!
     * Recomputes the on-screen button rectangles based on the current render area. The buttons
     * are anchored to the bottom-left and bottom-right corners of the screen and scale down on
     * narrow (portrait) screens so they never overlap.
     */
    void updateButtonRects();

    /*!
     * Draws a single tinted button quad at the given rectangle.
     */
    void drawButton(const ButtonRect &rect, float r, float g, float b, float a);

    android_app *app_;
    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    EGLint width_;
    EGLint height_;

    bool shaderNeedsNewProjectionMatrix_;

    std::unique_ptr<Shader> shader_;
    std::vector<Model> models_;

    // A 1x1 white texture used to draw solid-color button quads (tinted via the uColor uniform).
    std::shared_ptr<TextureAsset> whiteTexture_;
    // A single reusable quad model used for both on-screen buttons.
    std::unique_ptr<Model> buttonModel_;

    // Accumulated rotation of the android robot in degrees. Each tap on the left/right button
    // adjusts this by +/-90, so the effect stacks across multiple taps.
    float robotRotationDegrees_;

    // The angle the user is currently asking for (updated instantly on tap). robotRotationDegrees_
    // eases toward this value every frame so the rotation animates smoothly instead of snapping.
    float targetRotationDegrees_;

    // Timestamp of the previous frame, used to compute a frame-rate-independent delta time for the
    // rotation easing.
    std::chrono::steady_clock::time_point lastFrameTime_;

    // Current on-screen button rectangles, in projection space. Recomputed whenever the
    // render area changes (see updateButtonRects).
    ButtonRect leftButton_;
    ButtonRect rightButton_;
};

#endif //ANDROIDGLINVESTIGATIONS_RENDERER_H