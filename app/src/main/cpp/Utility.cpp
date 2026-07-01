#include "Utility.h"

#include <cmath>

float *
Utility::buildOrthographicMatrix(float *outMatrix, float halfHeight, float aspect, float near,
                                 float far) {
    float halfWidth = halfHeight * aspect;

    // column 1
    outMatrix[0] = 1.f / halfWidth;
    outMatrix[1] = 0.f;
    outMatrix[2] = 0.f;
    outMatrix[3] = 0.f;

    // column 2
    outMatrix[4] = 0.f;
    outMatrix[5] = 1.f / halfHeight;
    outMatrix[6] = 0.f;
    outMatrix[7] = 0.f;

    // column 3
    outMatrix[8] = 0.f;
    outMatrix[9] = 0.f;
    outMatrix[10] = -2.f / (far - near);
    outMatrix[11] = -(far + near) / (far - near);

    // column 4
    outMatrix[12] = 0.f;
    outMatrix[13] = 0.f;
    outMatrix[14] = 0.f;
    outMatrix[15] = 1.f;

    return outMatrix;
}

float *Utility::buildIdentityMatrix(float *outMatrix) {
    // column 1
    outMatrix[0] = 1.f;
    outMatrix[1] = 0.f;
    outMatrix[2] = 0.f;
    outMatrix[3] = 0.f;

    // column 2
    outMatrix[4] = 0.f;
    outMatrix[5] = 1.f;
    outMatrix[6] = 0.f;
    outMatrix[7] = 0.f;

    // column 3
    outMatrix[8] = 0.f;
    outMatrix[9] = 0.f;
    outMatrix[10] = 1.f;
    outMatrix[11] = 0.f;

    // column 4
    outMatrix[12] = 0.f;
    outMatrix[13] = 0.f;
    outMatrix[14] = 0.f;
    outMatrix[15] = 1.f;

    return outMatrix;
}

float *Utility::buildRotationZMatrix(float *outMatrix, float degrees) {
    // Standard 2D rotation. In column-major layout column 0 = (c, s, 0, 0),
    // column 1 = (-s, c, 0, 0). Positive degrees rotate counter-clockwise in GL
    // (y-up) space.
    float radians = degrees * (3.14159265358979323846f / 180.f);
    float c = cosf(radians);
    float s = sinf(radians);

    // column 1
    outMatrix[0] = c;
    outMatrix[1] = s;
    outMatrix[2] = 0.f;
    outMatrix[3] = 0.f;

    // column 2
    outMatrix[4] = -s;
    outMatrix[5] = c;
    outMatrix[6] = 0.f;
    outMatrix[7] = 0.f;

    // column 3
    outMatrix[8] = 0.f;
    outMatrix[9] = 0.f;
    outMatrix[10] = 1.f;
    outMatrix[11] = 0.f;

    // column 4
    outMatrix[12] = 0.f;
    outMatrix[13] = 0.f;
    outMatrix[14] = 0.f;
    outMatrix[15] = 1.f;

    return outMatrix;
}

float *Utility::buildTranslationMatrix(float *outMatrix, float x, float y, float z) {
    buildIdentityMatrix(outMatrix);

    // column 4 holds the translation (column-major)
    outMatrix[12] = x;
    outMatrix[13] = y;
    outMatrix[14] = z;

    return outMatrix;
}

float *Utility::buildScaleMatrix(float *outMatrix, float sx, float sy, float sz) {
    buildIdentityMatrix(outMatrix);

    outMatrix[0] = sx;   // column 1, row 0
    outMatrix[5] = sy;   // column 2, row 1
    outMatrix[10] = sz;  // column 3, row 2

    return outMatrix;
}

float *Utility::multiplyMatrix(float *outMatrix, const float *a, const float *b) {
    // Compute out = a * b for two 4x4 column-major matrices.
    // Element at (row r, col c) is stored at index c*4 + r.
    float result[16];
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            float sum = 0.f;
            for (int k = 0; k < 4; k++) {
                sum += a[k * 4 + r] * b[c * 4 + k];
            }
            result[c * 4 + r] = sum;
        }
    }
    for (int i = 0; i < 16; i++) {
        outMatrix[i] = result[i];
    }
    return outMatrix;
}
