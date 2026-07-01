#ifndef ANDROIDGLINVESTIGATIONS_TEXTUREASSET_H
#define ANDROIDGLINVESTIGATIONS_TEXTUREASSET_H

#include <memory>
#include <android/asset_manager.h>
#include <GLES3/gl3.h>
#include <string>
#include <vector>

class TextureAsset {
public:
    /*!
     * Loads a texture asset from the assets/ directory
     * @param assetManager Asset manager to use
     * @param assetPath The path to the asset
     * @return a shared pointer to a texture asset, resources will be reclaimed when it's cleaned up
     */
    static std::shared_ptr<TextureAsset>
    loadAsset(AAssetManager *assetManager, const std::string &assetPath);

    /*!
     * Creates a 1x1 RGBA texture filled with a single solid color. Useful for drawing tinted
     * primitives (e.g. on-screen buttons) when combined with a color uniform in the shader.
     * @param r red channel 0-255
     * @param g green channel 0-255
     * @param b blue channel 0-255
     * @param a alpha channel 0-255
     * @return a shared pointer to a texture asset, resources will be reclaimed when it's cleaned up
     */
    static std::shared_ptr<TextureAsset>
    createSolidColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    ~TextureAsset();

    /*!
     * @return the texture id for use with OpenGL
     */
    constexpr GLuint getTextureID() const { return textureID_; }

private:
    inline TextureAsset(GLuint textureId) : textureID_(textureId) {}

    GLuint textureID_;
};

#endif //ANDROIDGLINVESTIGATIONS_TEXTUREASSET_H