#ifndef ANDROIDGLINVESTIGATIONS_UTILITY_H
#define ANDROIDGLINVESTIGATIONS_UTILITY_H

class Utility {
public:
    /**
     * Generates an orthographic projection matrix given the half height, aspect ratio, near, and far
     * planes
     *
     * @param outMatrix the matrix to write into
     * @param halfHeight half of the height of the screen
     * @param aspect the width of the screen divided by the height
     * @param near the distance of the near plane
     * @param far the distance of the far plane
     * @return the generated matrix, this will be the same as @a outMatrix so you can chain calls
     *     together if needed
     */
    static float *buildOrthographicMatrix(
            float *outMatrix,
            float halfHeight,
            float aspect,
            float near,
            float far);

    static float *buildIdentityMatrix(float *outMatrix);

    /**
     * Builds a 4x4 rotation matrix about the Z axis (column-major, suitable for 2D rotation).
     *
     * @param outMatrix the matrix to write into
     * @param degrees rotation angle in degrees. Positive values rotate counter-clockwise.
     * @return the same pointer as @a outMatrix for chaining
     */
    static float *buildRotationZMatrix(float *outMatrix, float degrees);

    /**
     * Builds a 4x4 translation matrix (column-major).
     */
    static float *buildTranslationMatrix(float *outMatrix, float x, float y, float z);

    /**
     * Builds a 4x4 scale matrix (column-major).
     */
    static float *buildScaleMatrix(float *outMatrix, float sx, float sy, float sz);

    /**
     * Multiplies two 4x4 column-major matrices: out = a * b.
     * @a outMatrix may alias @a a or @a b.
     */
    static float *multiplyMatrix(float *outMatrix, const float *a, const float *b);
};

#endif //ANDROIDGLINVESTIGATIONS_UTILITY_H