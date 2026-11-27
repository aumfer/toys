#pragma once
#include <GLFW/glfw3.h>
#ifndef __EMSCRIPTEN__
#include <glad/gles2.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <HandmadeMath/HandmadeMath.h>
#include <float.h>
#include <memory.h>
#include <stb_image.h>
#include <stb_truetype.h>
#include <utarray.h>
#include <uthash.h>

#define _TEXT(...) #__VA_ARGS__ " "

#define RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | \
     ((uint32_t)(b) << 16) | \
     ((uint32_t)(g) <<  8) | \
     ((uint32_t)(r)        ))

union rgba8_t {
    operator uint32_t() const {return c;}
    rgba8_t(uint32_t _c) {c=_c;}
    uint32_t c;
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };
};

inline void random_color4(GLubyte *c) {
    // https://stackoverflow.com/questions/8476841/how-to-random-generate-fully-saturated-colors-only
    c[0] = rand() / (RAND_MAX / UCHAR_MAX);
    c[1] = rand() / (RAND_MAX / UCHAR_MAX);
    c[2] = rand() / (RAND_MAX / UCHAR_MAX);
    c[3] = 255;
    int max, min;
    if (c[0] > c[1]) {
        max = c[0] > c[2] ? 0 : 2;
        min = c[1] < c[2] ? 1 : 2;
    } else {
        max = c[1] > c[2] ? 1 : 2;
        int notmax = 1 + max % 2;
        min = c[0] < c[notmax] ? 0 : notmax;
    }
    c[max] = UCHAR_MAX;
    c[min] = 0;
}
inline void random_color(uint32_t *rgba) {
    return random_color4((GLubyte*)rgba);
}

inline bool HMM_Decompose(HMM_Mat4 ModelMatrix, HMM_Vec3 *Scale, HMM_Quat *Orientation, HMM_Vec3 *Translation, HMM_Vec3 *Skew, HMM_Vec4 *Perspective) {
    HMM_Mat4 LocalMatrix = ModelMatrix;

    if (fabs(LocalMatrix.Elements[3][3]) < 1e-6f)
        return false;

    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            LocalMatrix.Elements[i][j] /= LocalMatrix.Elements[3][3];

    HMM_Mat4 PerspectiveMatrix = LocalMatrix;
    for (int i = 0; i < 3; ++i)
        PerspectiveMatrix.Elements[i][3] = 0.0f;
    PerspectiveMatrix.Elements[3][3] = 1.0f;

    //if (fabs(HMM_Determinant(PerspectiveMatrix)) < 1e-6f)
    //    return false;

    if (fabs(LocalMatrix.Elements[0][3]) > 1e-6f ||
        fabs(LocalMatrix.Elements[1][3]) > 1e-6f ||
        fabs(LocalMatrix.Elements[2][3]) > 1e-6f)
    {
        HMM_Vec4 rhs = {
            LocalMatrix.Elements[0][3],
            LocalMatrix.Elements[1][3],
            LocalMatrix.Elements[2][3],
            LocalMatrix.Elements[3][3]
        };

        HMM_Mat4 invPersp = HMM_Transpose(HMM_InvGeneralM4(PerspectiveMatrix));
        *Perspective = HMM_MulM4V4(invPersp, rhs);

        LocalMatrix.Elements[0][3] = LocalMatrix.Elements[1][3] = LocalMatrix.Elements[2][3] = 0.0f;
        LocalMatrix.Elements[3][3] = 1.0f;
    }
    else
    {
        *Perspective = { 0, 0, 0, 1 };
    }

    *Translation = LocalMatrix[3].XYZ;
    LocalMatrix[3] = { 0, 0, 0, LocalMatrix[3].W };

    HMM_Vec3 Row[3];
    for (int i = 0; i < 3; ++i)
        Row[i] = { LocalMatrix.Elements[i][0], LocalMatrix.Elements[i][1], LocalMatrix.Elements[i][2] };

    Scale->X = HMM_Len(Row[0]);
    Row[0] = HMM_NormV3(Row[0]);

    Skew->Z = HMM_Dot(Row[0], Row[1]);
    Row[1] = HMM_SubV3(Row[1], HMM_MulV3F(Row[0], Skew->Z));

    Scale->Y = HMM_Len(Row[1]);
    Row[1] = HMM_NormV3(Row[1]);
    Skew->Z /= Scale->Y;

    Skew->Y = HMM_Dot(Row[0], Row[2]);
    Row[2] = HMM_SubV3(Row[2], HMM_MulV3F(Row[0], Skew->Y));
    Skew->X = HMM_Dot(Row[1], Row[2]);
    Row[2] = HMM_SubV3(Row[2], HMM_MulV3F(Row[1], Skew->X));

    Scale->Z = HMM_Len(Row[2]);
    Row[2] = HMM_NormV3(Row[2]);
    Skew->Y /= Scale->Z;
    Skew->X /= Scale->Z;

    HMM_Vec3 pdum3 = HMM_Cross(Row[1], Row[2]);
    if (HMM_Dot(Row[0], pdum3) < 0.0f) {
        for (int i = 0; i < 3; ++i) {
            Scale->Elements[i] *= -1.0f;
            Row[i] = HMM_MulV3F(Row[i], -1.0f);
        }
    }

    float trace = Row[0].X + Row[1].Y + Row[2].Z;
    if (trace > 0.0f) {
        float root = sqrtf(trace + 1.0f);
        Orientation->W = 0.5f * root;
        root = 0.5f / root;
        Orientation->X = root * (Row[1].Z - Row[2].Y);
        Orientation->Y = root * (Row[2].X - Row[0].Z);
        Orientation->Z = root * (Row[0].Y - Row[1].X);
    }
    else
    {
        int i = 0;
        if (Row[1].Y > Row[0].X) i = 1;
        if (Row[2].Z > Row[i].Elements[i]) i = 2;
        int j = (i + 1) % 3;
        int k = (j + 1) % 3;

        float root = sqrtf(Row[i].Elements[i] - Row[j].Elements[j] - Row[k].Elements[k] + 1.0f);
        float *quat = (float *)&(*Orientation);
        quat[i] = 0.5f * root;
        root = 0.5f / root;
        quat[j] = root * (Row[i].Elements[j] + Row[j].Elements[i]);
        quat[k] = root * (Row[i].Elements[k] + Row[k].Elements[i]);
        Orientation->W = root * (Row[j].Elements[k] - Row[k].Elements[j]);
    }

    return true;
}
