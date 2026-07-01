#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <android/imagedecoder.h>

#include "AndroidOut.h"
#include "Shader.h"
#include "Utility.h"
#include "TextureAsset.h"

//! executes glGetString and outputs the result to logcat
#define PRINT_GL_STRING(s) {aout << #s": "<< glGetString(s) << std::endl;}

/*!
 * @brief if glGetString returns a space separated list of elements, prints each one on a new line
 *
 * This works by creating an istringstream of the input c-style string. Then that is used to create
 * a vector -- each element of the vector is a new element in the input string. Finally a foreach
 * loop consumes this and outputs it to logcat using @a aout
 */
#define PRINT_GL_STRING_AS_LIST(s) { \
std::istringstream extensionStream((const char *) glGetString(s));\
std::vector<std::string> extensionList(\
        std::istream_iterator<std::string>{extensionStream},\
        std::istream_iterator<std::string>());\
aout << #s":\n";\
for (auto& extension: extensionList) {\
    aout << extension << "\n";\
}\
aout << std::endl;\
}

//! Color for cornflower blue. Can be sent directly to glClearColor
#define CORNFLOWER_BLUE 100 / 255.f, 149 / 255.f, 237 / 255.f, 1

// Vertex shader, you'd typically load this from assets
static const char *vertex = R"vertex(#version 300 es
in vec3 inPosition;
in vec2 inUV;

out vec2 fragUV;

uniform mat4 uProjection;
uniform mat4 uModel;

void main() {
    fragUV = inUV;
    gl_Position = uProjection * uModel * vec4(inPosition, 1.0);
}
)vertex";

// Fragment shader, you'd typically load this from assets
static const char *fragment = R"fragment(#version 300 es
precision mediump float;

in vec2 fragUV;

uniform sampler2D uTexture;
uniform vec4 uColor;

out vec4 outColor;

void main() {
    outColor = texture(uTexture, fragUV) * uColor;
}
)fragment";

/*!
 * Half the height of the projection matrix. This gives you a renderable area of height 4 ranging
 * from -2 to 2
 */
static constexpr float kProjectionHalfHeight = 2.f;

/*!
 * The near plane distance for the projection matrix. Since this is an orthographic projection
 * matrix, it's convenient to have negative values for sorting (and avoiding z-fighting at 0).
 */
static constexpr float kProjectionNearPlane = -1.f;

/*!
 * The far plane distance for the projection matrix. Since this is an orthographic porjection
 * matrix, it's convenient to have the far plane equidistant from 0 as the near plane.
 */
static constexpr float kProjectionFarPlane = 1.f;

/*!
 * How quickly the displayed rotation eases toward the target angle, in 1/seconds. Larger values
 * make the rotation snappier; smaller values make it lazier. ~10 settles a 90-degree turn in
 * roughly a third of a second with a smooth ease-out feel.
 */
static constexpr float kRotationSmoothingSpeed = 10.f;

/*!
 * Maximum per-frame delta time (in seconds) used by the rotation easing. Prevents a huge jump
 * when the app resumes after being backgrounded (where the steady clock can report a large gap).
 */
static constexpr float kRotationMaxFrameDelta = 0.05f;

Renderer::~Renderer() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void Renderer::render() {
    // Advance the smooth rotation tween toward the target angle. This runs every frame so the
    // robot eases into its new orientation instead of snapping when a button is tapped.
    {
        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;
        // Clamp dt so a long pause (e.g. app backgrounded) doesn't cause a giant jump.
        dt = std::min(dt, kRotationMaxFrameDelta);

        // Frame-rate-independent exponential ease-out: the remaining gap is closed by
        // (1 - e^(-speed*dt)) each frame. This is smooth and naturally slows down near the target.
        const float alpha = 1.f - expf(-kRotationSmoothingSpeed * dt);
        robotRotationDegrees_ += (targetRotationDegrees_ - robotRotationDegrees_) * alpha;
    }

    // Check to see if the surface has changed size. This is _necessary_ to do every frame when
    // using immersive mode as you'll get no other notification that your renderable area has
    // changed.
    updateRenderArea();

    // When the renderable area changes, the projection matrix has to also be updated. This is true
    // even if you change from the sample orthographic projection matrix as your aspect ratio has
    // likely changed.
    if (shaderNeedsNewProjectionMatrix_) {
        // a placeholder projection matrix allocated on the stack. Column-major memory layout
        float projectionMatrix[16] = {0};

        // build an orthographic projection matrix for 2d rendering
        Utility::buildOrthographicMatrix(
                projectionMatrix,
                kProjectionHalfHeight,
                float(width_) / height_,
                kProjectionNearPlane,
                kProjectionFarPlane);

        // send the matrix to the shader
        // Note: the shader must be active for this to work. Since we only have one shader for this
        // demo, we can assume that it's active.
        shader_->setProjectionMatrix(projectionMatrix);

        // make sure the matrix isn't generated every frame
        shaderNeedsNewProjectionMatrix_ = false;
    }

    // clear the color buffer
    glClear(GL_COLOR_BUFFER_BIT);

    // Keep the on-screen button rectangles in sync with the current render area.
    updateButtonRects();

    // Render all the models. There's no depth testing in this sample so they're accepted in the
    // order provided. But the sample EGL setup requests a 24 bit depth buffer so you could
    // configure it at the end of initRenderer
    if (!models_.empty()) {
        // The robot itself is rotated about its center by the (stackable) accumulated angle.
        float modelMatrix[16];
        Utility::buildRotationZMatrix(modelMatrix, robotRotationDegrees_);
        shader_->setModelMatrix(modelMatrix);

        // No tint for the textured robot.
        shader_->setColor(1.f, 1.f, 1.f, 1.f);

        for (const auto &model: models_) {
            shader_->drawModel(model);
        }
    }

    // Draw the on-screen buttons on top of the robot. Left = rotate counter-clockwise (left),
    // right = rotate clockwise (right). Distinct colors make them easy to tell apart.
    // Left button: warm orange.
    drawButton(leftButton_, 1.f, 0.55f, 0.f, 0.85f);
    // Right button: teal/blue.
    drawButton(rightButton_, 0.f, 0.6f, 1.f, 0.85f);

    // Present the rendered image. This is an implicit glFlush.
    auto swapResult = eglSwapBuffers(display_, surface_);
    assert(swapResult == EGL_TRUE);
}

void Renderer::initRenderer() {
    // Choose your render attributes
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    // The default display is probably what you want on Android
    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    // figure out how many configs there are
    EGLint numConfigs;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);

    // get the list of configurations
    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);

    // Find a config we like.
    // Could likely just grab the first if we don't care about anything else in the config.
    // Otherwise hook in your own heuristic
    auto config = *std::find_if(
            supportedConfigs.get(),
            supportedConfigs.get() + numConfigs,
            [&display](const EGLConfig &config) {
                EGLint red, green, blue, depth;
                if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
                    && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green)
                    && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue)
                    && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth)) {

                    aout << "Found config with " << red << ", " << green << ", " << blue << ", "
                         << depth << std::endl;
                    return red == 8 && green == 8 && blue == 8 && depth == 24;
                }
                return false;
            });

    aout << "Found " << numConfigs << " configs" << std::endl;
    aout << "Chose " << config << std::endl;

    // create the proper window surface
    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);

    // Create a GLES 3 context
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);

    // get some window metrics
    auto madeCurrent = eglMakeCurrent(display, surface, surface, context);
    assert(madeCurrent);

    display_ = display;
    surface_ = surface;
    context_ = context;

    // make width and height invalid so it gets updated the first frame in @a updateRenderArea()
    width_ = -1;
    height_ = -1;

    PRINT_GL_STRING(GL_VENDOR);
    PRINT_GL_STRING(GL_RENDERER);
    PRINT_GL_STRING(GL_VERSION);
    PRINT_GL_STRING_AS_LIST(GL_EXTENSIONS);

    shader_ = std::unique_ptr<Shader>(
            Shader::loadShader(vertex, fragment, "inPosition", "inUV", "uProjection", "uModel",
                               "uColor"));
    assert(shader_);

    // Note: there's only one shader in this demo, so I'll activate it here. For a more complex game
    // you'll want to track the active shader and activate/deactivate it as necessary
    shader_->activate();

    // setup any other gl related global states
    glClearColor(CORNFLOWER_BLUE);

    // enable alpha globally for now, you probably don't want to do this in a game
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // get some demo models into memory
    createModels();
}

void Renderer::updateRenderArea() {
    EGLint width;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);

    EGLint height;
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);

        // make sure that we lazily recreate the projection matrix before we render
        shaderNeedsNewProjectionMatrix_ = true;
    }
}

/**
 * @brief Create any demo models we want for this demo.
 */
void Renderer::createModels() {
    /*
     * This is a square:
     * 0 --- 1
     * | \   |
     * |  \  |
     * |   \ |
     * 3 --- 2
     */
    std::vector<Vertex> vertices = {
            Vertex(Vector3{1, 1, 0}, Vector2{0, 0}), // 0
            Vertex(Vector3{-1, 1, 0}, Vector2{1, 0}), // 1
            Vertex(Vector3{-1, -1, 0}, Vector2{1, 1}), // 2
            Vertex(Vector3{1, -1, 0}, Vector2{0, 1}) // 3
    };
    std::vector<Index> indices = {
            0, 1, 2, 0, 2, 3
    };

    // loads an image and assigns it to the square.
    //
    // Note: there is no texture management in this sample, so if you reuse an image be careful not
    // to load it repeatedly. Since you get a shared_ptr you can safely reuse it in many models.
    auto assetManager = app_->activity->assetManager;
    auto spAndroidRobotTexture = TextureAsset::loadAsset(assetManager, "android_robot.png");

    // Create a model and put it in the back of the render list.
    models_.emplace_back(vertices, indices, spAndroidRobotTexture);

    // A 1x1 white texture used for drawing solid-color button quads. The actual color is provided
    // per-draw via the uColor uniform.
    whiteTexture_ = TextureAsset::createSolidColor(255, 255, 255, 255);

    // Both on-screen buttons share the same 2x2 quad geometry; they are positioned and sized
    // through the model matrix in drawButton().
    buttonModel_ = std::make_unique<Model>(vertices, indices, whiteTexture_);
}

void Renderer::updateButtonRects() {
    // Until the surface has a valid size there's nothing meaningful to compute.
    if (width_ <= 0 || height_ <= 0) {
        return;
    }

    const float halfHeight = kProjectionHalfHeight;
    const float aspect = float(width_) / float(height_);
    const float halfWidth = halfHeight * aspect;

    // Size the buttons so they remain comfortably tappable but shrink on narrow (portrait)
    // screens; this guarantees the left and right buttons never overlap regardless of aspect.
    const float buttonHalf = std::min(0.4f, halfWidth * 0.28f);
    const float margin = std::max(0.15f, halfWidth * 0.08f);

    // Anchor the buttons to the bottom-left and bottom-right corners.
    const float offset = halfWidth - buttonHalf - margin;
    const float centerY = -halfHeight + buttonHalf + margin;

    leftButton_ = {-offset, centerY, buttonHalf, buttonHalf};
    rightButton_ = {offset, centerY, buttonHalf, buttonHalf};
}

void Renderer::drawButton(const ButtonRect &rect, float r, float g, float b, float a) {
    if (!buttonModel_) {
        return;
    }

    // The shared button quad spans [-1,1] in both axes (a 2x2 square). Scale it to the desired
    // size (2 * half) then translate it to the button center: model = translate * scale.
    float scale[16];
    float translate[16];
    float model[16];
    Utility::buildScaleMatrix(scale, rect.halfWidth, rect.halfHeight, 1.f);
    Utility::buildTranslationMatrix(translate, rect.centerX, rect.centerY, 0.f);
    Utility::multiplyMatrix(model, translate, scale);

    shader_->setModelMatrix(model);
    shader_->setColor(r, g, b, a);
    shader_->drawModel(*buttonModel_);
}

void Renderer::handleInput() {
    // handle all queued inputs
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) {
        // no inputs yet.
        return;
    }

    // handle motion events (motionEventsCounts can be 0).
    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto action = motionEvent.action;

        // Find the pointer index, mask and bitshift to turn it into a readable value.
        auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        aout << "Pointer(s): ";

        // get the x and y position of this event if it is not ACTION_MOVE.
        auto &pointer = motionEvent.pointers[pointerIndex];
        auto x = GameActivityPointerAxes_getX(&pointer);
        auto y = GameActivityPointerAxes_getY(&pointer);

        // determine the action type and process the event accordingly.
        switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                aout << "(" << pointer.id << ", " << x << ", " << y << ") "
                     << "Pointer Down";

                // Hit-test the on-screen buttons. A tap inside a button rotates the robot by
                // 90 degrees in the corresponding direction. The angle accumulates across taps
                // so repeated presses keep adding up (e.g. four left taps == full 360).
                if (width_ > 0 && height_ > 0) {
                    const float halfHeight = kProjectionHalfHeight;
                    const float halfWidth = halfHeight * (float(width_) / float(height_));
                    const float glX = (2.f * x / float(width_) - 1.f) * halfWidth;
                    // Pixel y grows downward, GL y grows upward.
                    const float glY = (1.f - 2.f * y / float(height_)) * halfHeight;

                    auto inRect = [](float px, float py, const ButtonRect &r) {
                        return px >= r.centerX - r.halfWidth
                               && px <= r.centerX + r.halfWidth
                               && py >= r.centerY - r.halfHeight
                               && py <= r.centerY + r.halfHeight;
                    };

                    if (inRect(glX, glY, leftButton_)) {
                        // Counter-clockwise in GL (y-up) == "turn left". We only move the target
                        // here; robotRotationDegrees_ eases toward it smoothly every frame.
                        targetRotationDegrees_ += 90.f;
                        aout << " [LEFT button] target = " << targetRotationDegrees_;
                    } else if (inRect(glX, glY, rightButton_)) {
                        // Clockwise == "turn right".
                        targetRotationDegrees_ -= 90.f;
                        aout << " [RIGHT button] target = " << targetRotationDegrees_;
                    }
                }
                break;

            case AMOTION_EVENT_ACTION_CANCEL:
                // treat the CANCEL as an UP event: doing nothing in the app, except
                // removing the pointer from the cache if pointers are locally saved.
                // code pass through on purpose.
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                aout << "(" << pointer.id << ", " << x << ", " << y << ") "
                     << "Pointer Up";
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                // There is no pointer index for ACTION_MOVE, only a snapshot of
                // all active pointers; app needs to cache previous active pointers
                // to figure out which ones are actually moved.
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
    // clear the motion input count in this buffer for main thread to re-use.
    android_app_clear_motion_events(inputBuffer);

    // handle input key events.
    for (auto i = 0; i < inputBuffer->keyEventsCount; i++) {
        auto &keyEvent = inputBuffer->keyEvents[i];
        aout << "Key: " << keyEvent.keyCode <<" ";
        switch (keyEvent.action) {
            case AKEY_EVENT_ACTION_DOWN:
                aout << "Key Down";
                break;
            case AKEY_EVENT_ACTION_UP:
                aout << "Key Up";
                break;
            case AKEY_EVENT_ACTION_MULTIPLE:
                // Deprecated since Android API level 29.
                aout << "Multiple Key Actions";
                break;
            default:
                aout << "Unknown KeyEvent Action: " << keyEvent.action;
        }
        aout << std::endl;
    }
    // clear the key input count too.
    android_app_clear_key_events(inputBuffer);
}