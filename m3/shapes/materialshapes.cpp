/*
 * Copyright 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Ported to C++/Qt for theDesk; modified.
 * Source: androidx compose/material3/material3/src/commonMain/kotlin/
 *           androidx/compose/material3/MaterialShapes.kt
 * The rotateZ/scale/map helpers reproduce androidx.compose.ui.graphics.Matrix
 * (compose/ui/ui-graphics/.../Matrix.kt, Copyright 2020 The Android Open Source
 * Project) so transformed shapes get the same float results as on Android.
 */
#include "materialshapes.h"

#include <cmath>

using m3shapes::CornerRounding;
using m3shapes::Point;
using m3shapes::RoundedPolygon;
using m3shapes::TransformResult;
namespace kmath = m3shapes::kmath;

namespace {

    // androidx.compose.ui.graphics.Matrix, restricted to the 2D parts these shapes use
    struct ComposeMatrix {
            // [row, column]
            float m00 = 1, m01 = 0, m10 = 0, m11 = 1, m30 = 0, m31 = 0;

            void rotateZ(float degrees) {
                double r = degrees * (3.14159265358979323846 / 180.0);
                float s = static_cast<float>(std::sin(r));
                float c = static_cast<float>(std::cos(r));

                float a00 = m00;
                float a10 = m10;
                float v00 = c * a00 + s * a10;
                float v10 = -s * a00 + c * a10;

                float a01 = m01;
                float a11 = m11;
                float v01 = c * a01 + s * a11;
                float v11 = -s * a01 + c * a11;

                m00 = v00;
                m01 = v01;
                m10 = v10;
                m11 = v11;
            }

            void scale(float x, float y) {
                m00 *= x;
                m01 *= x;
                m10 *= y;
                m11 *= y;
            }

            TransformResult map(float x, float y) const {
                // z = v03 * x + v13 * y + v33 with v03 = v13 = 0, v33 = 1
                const float pZ = 1.f;
                return TransformResult(pZ * (m00 * x + m10 * y + m30), pZ * (m01 * x + m11 * y + m31));
            }
    };

    // ShapeUtil.kt: RoundedPolygon.transformed(matrix: Matrix)
    RoundedPolygon transformed(const RoundedPolygon& polygon, const ComposeMatrix& matrix) {
        return polygon.transformed([matrix](float x, float y) { return matrix.map(x, y); });
    }

    ComposeMatrix rotationMatrix(float degrees) {
        ComposeMatrix m;
        m.rotateZ(degrees);
        return m;
    }

    // Cache various roundings for use below
    constexpr CornerRounding cornerRound15 = CornerRounding(.15f);
    constexpr CornerRounding cornerRound20 = CornerRounding(.2f);
    constexpr CornerRounding cornerRound30 = CornerRounding(.3f);
    constexpr CornerRounding cornerRound50 = CornerRounding(.5f);
    constexpr CornerRounding cornerRound100 = CornerRounding(1.f);

    const ComposeMatrix rotateNeg45 = rotationMatrix(-45.f);
    const ComposeMatrix rotateNeg90 = rotationMatrix(-90.f);
    const ComposeMatrix rotateNeg135 = rotationMatrix(-135.f);

    struct PointNRound {
            Point o;
            CornerRounding r = m3shapes::Unrounded;
    };

    float toRadians(float degrees) {
        return degrees / 360.f * 2 * m3shapes::FloatPi;
    }

    float angleDegrees(const Point& p) {
        return kmath::atan2(p.y, p.x) * 180.f / m3shapes::FloatPi;
    }

    Point rotateDegrees(const Point& p, float angle, const Point& center) {
        float a = toRadians(angle);
        Point off = p - center;
        return Point(off.x * kmath::cos(a) - off.y * kmath::sin(a), off.x * kmath::sin(a) + off.y * kmath::cos(a)) + center;
    }

    std::vector<PointNRound> doRepeat(const std::vector<PointNRound>& points, int reps, const Point& center, bool mirroring) {
        std::vector<PointNRound> result;
        if (mirroring) {
            std::vector<float> angles;
            std::vector<float> distances;
            for (const auto& it : points) {
                angles.push_back(angleDegrees(it.o - center));
                distances.push_back((it.o - center).getDistance());
            }
            const int actualReps = reps * 2;
            const float sectionAngle = 360.f / actualReps;
            const int lastIndex = static_cast<int>(points.size()) - 1;
            for (int it = 0; it < actualReps; it++) {
                for (int index = 0; index <= lastIndex; index++) {
                    int i = (it % 2 == 0) ? index : lastIndex - index;
                    if (i > 0 || it % 2 == 0) {
                        float a = toRadians(sectionAngle * it +
                                            ((it % 2 == 0) ? angles[i] : sectionAngle - angles[i] + 2 * angles[0]));
                        Point finalPoint = Point(kmath::cos(a), kmath::sin(a)) * distances[i] + center;
                        result.push_back({finalPoint, points[i].r});
                    }
                }
            }
        } else {
            const int np = static_cast<int>(points.size());
            for (int it = 0; it < np * reps; it++) {
                Point point = rotateDegrees(points[it % np].o, (it / np) * 360.f / reps, center);
                result.push_back({point, points[it % np].r});
            }
        }
        return result;
    }

    RoundedPolygon customPolygon(const std::vector<PointNRound>& pnr, int reps,
        Point center = Point(0.5f, 0.5f), bool mirroring = false) {
        auto actualPoints = doRepeat(pnr, reps, center, mirroring);
        std::vector<float> vertices;
        std::vector<CornerRounding> perVertexRounding;
        vertices.reserve(actualPoints.size() * 2);
        for (const auto& p : actualPoints) {
            vertices.push_back(p.o.x);
            vertices.push_back(p.o.y);
            perVertexRounding.push_back(p.r);
        }
        return RoundedPolygon::fromVertices(vertices, m3shapes::Unrounded, perVertexRounding, center.x, center.y);
    }

    // ---- The raw (un-normalised) shapes, exactly as MaterialShapes.kt builds them ----

    RoundedPolygon circle(int numVertices = 10) {
        return RoundedPolygon::circle(numVertices);
    }

    RoundedPolygon square() {
        return RoundedPolygon::rectangle(1.f, 1.f, cornerRound30);
    }

    RoundedPolygon slanted() {
        return customPolygon(
            {
                {Point(0.926f, 0.970f), CornerRounding(0.189f, 0.811f)},
                {Point(-0.021f, 0.967f), CornerRounding(0.187f, 0.057f)},
            },
            2);
    }

    RoundedPolygon arch() {
        return transformed(RoundedPolygon::fromNumVertices(4, 1.f, 0.f, 0.f, m3shapes::Unrounded,
                               {cornerRound100, cornerRound100, cornerRound20, cornerRound20}),
            rotateNeg135);
    }

    RoundedPolygon fan() {
        return customPolygon(
            {
                {Point(1.004f, 1.000f), CornerRounding(0.148f, 0.417f)},
                {Point(0.000f, 1.000f), CornerRounding(0.151f)},
                {Point(0.000f, -0.003f), CornerRounding(0.148f)},
                {Point(0.978f, 0.020f), CornerRounding(0.803f)},
            },
            1);
    }

    RoundedPolygon arrow() {
        return customPolygon(
            {
                {Point(0.500f, 0.892f), CornerRounding(0.313f)},
                {Point(-0.216f, 1.050f), CornerRounding(0.207f)},
                {Point(0.499f, -0.160f), CornerRounding(0.215f, 1.000f)},
                {Point(1.225f, 1.060f), CornerRounding(0.211f)},
            },
            1);
    }

    RoundedPolygon semiCircle() {
        return RoundedPolygon::rectangle(1.6f, 1.f, m3shapes::Unrounded,
            {cornerRound20, cornerRound20, cornerRound100, cornerRound100});
    }

    RoundedPolygon oval() {
        ComposeMatrix m;
        m.scale(1.f, 0.64f);
        return transformed(transformed(RoundedPolygon::circle(), m), rotateNeg45);
    }

    RoundedPolygon pill() {
        return customPolygon(
            {
                {Point(0.961f, 0.039f), CornerRounding(0.426f)},
                {Point(1.001f, 0.428f)},
                {Point(1.000f, 0.609f), CornerRounding(1.000f)},
            },
            2, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon triangle() {
        return transformed(RoundedPolygon::fromNumVertices(3, 1.f, 0.f, 0.f, cornerRound20), rotateNeg90);
    }

    RoundedPolygon diamond() {
        return customPolygon(
            {
                {Point(0.500f, 1.096f), CornerRounding(0.151f, 0.524f)},
                {Point(0.040f, 0.500f), CornerRounding(0.159f)},
            },
            2);
    }

    RoundedPolygon clamShell() {
        return customPolygon(
            {
                {Point(0.171f, 0.841f), CornerRounding(0.159f)},
                {Point(-0.020f, 0.500f), CornerRounding(0.140f)},
                {Point(0.170f, 0.159f), CornerRounding(0.159f)},
            },
            2);
    }

    RoundedPolygon pentagon() {
        return customPolygon(
            {
                {Point(0.500f, -0.009f), CornerRounding(0.172f)},
                {Point(1.030f, 0.365f), CornerRounding(0.164f)},
                {Point(0.828f, 0.970f), CornerRounding(0.169f)},
            },
            1, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon gem() {
        return customPolygon(
            {
                {Point(0.499f, 1.023f), CornerRounding(0.241f, 0.778f)},
                {Point(-0.005f, 0.792f), CornerRounding(0.208f)},
                {Point(0.073f, 0.258f), CornerRounding(0.228f)},
                {Point(0.433f, -0.000f), CornerRounding(0.491f)},
            },
            1, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon sunny() {
        return RoundedPolygon::star(8, 1.f, .8f, cornerRound15);
    }

    RoundedPolygon verySunny() {
        return customPolygon(
            {
                {Point(0.500f, 1.080f), CornerRounding(0.085f)},
                {Point(0.358f, 0.843f), CornerRounding(0.085f)},
            },
            8);
    }

    RoundedPolygon cookie4() {
        return customPolygon(
            {
                {Point(1.237f, 1.236f), CornerRounding(0.258f)},
                {Point(0.500f, 0.918f), CornerRounding(0.233f)},
            },
            4);
    }

    RoundedPolygon cookie6() {
        // 6-point cookie
        return customPolygon(
            {
                {Point(0.723f, 0.884f), CornerRounding(0.394f)},
                {Point(0.500f, 1.099f), CornerRounding(0.398f)},
            },
            6);
    }

    RoundedPolygon cookie7() {
        // 7-point cookie
        return transformed(RoundedPolygon::star(7, 1.f, .75f, cornerRound50), rotateNeg90);
    }

    RoundedPolygon cookie9() {
        return transformed(RoundedPolygon::star(9, 1.f, .8f, cornerRound50), rotateNeg90);
    }

    RoundedPolygon cookie12() {
        return transformed(RoundedPolygon::star(12, 1.f, .8f, cornerRound50), rotateNeg90);
    }

    RoundedPolygon ghostish() {
        return customPolygon(
            {
                {Point(0.500f, 0.f), CornerRounding(1.000f)},
                {Point(1.f, 0.f), CornerRounding(1.000f)},
                {Point(1.f, 1.140f), CornerRounding(0.254f, 0.106f)},
                {Point(0.575f, 0.906f), CornerRounding(0.253f)},
            },
            1, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon clover4() {
        return customPolygon(
            {
                {Point(0.500f, 0.074f)},
                {Point(0.725f, -0.099f), CornerRounding(0.476f)},
            },
            4, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon clover8() {
        return customPolygon(
            {
                {Point(0.500f, 0.036f)},
                {Point(0.758f, -0.101f), CornerRounding(0.209f)},
            },
            8);
    }

    RoundedPolygon burst() {
        return customPolygon(
            {
                {Point(0.500f, -0.006f), CornerRounding(0.006f)},
                {Point(0.592f, 0.158f), CornerRounding(0.006f)},
            },
            12);
    }

    RoundedPolygon softBurst() {
        return customPolygon(
            {
                {Point(0.193f, 0.277f), CornerRounding(0.053f)},
                {Point(0.176f, 0.055f), CornerRounding(0.053f)},
            },
            10);
    }

    RoundedPolygon boom() {
        return customPolygon(
            {
                {Point(0.457f, 0.296f), CornerRounding(0.007f)},
                {Point(0.500f, -0.051f), CornerRounding(0.007f)},
            },
            15);
    }

    RoundedPolygon softBoom() {
        return customPolygon(
            {
                {Point(0.733f, 0.454f)},
                {Point(0.839f, 0.437f), CornerRounding(0.532f)},
                {Point(0.949f, 0.449f), CornerRounding(0.439f, 1.000f)},
                {Point(0.998f, 0.478f), CornerRounding(0.174f)},
            },
            16, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon flower() {
        return customPolygon(
            {
                {Point(0.370f, 0.187f)},
                {Point(0.416f, 0.049f), CornerRounding(0.381f)},
                {Point(0.479f, 0.001f), CornerRounding(0.095f)},
            },
            8, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon puffy() {
        ComposeMatrix m;
        m.scale(1.f, 0.742f);
        return transformed(customPolygon(
                               {
                                   {Point(0.500f, 0.053f)},
                                   {Point(0.545f, -0.040f), CornerRounding(0.405f)},
                                   {Point(0.670f, -0.035f), CornerRounding(0.426f)},
                                   {Point(0.717f, 0.066f), CornerRounding(0.574f)},
                                   {Point(0.722f, 0.128f)},
                                   {Point(0.777f, 0.002f), CornerRounding(0.360f)},
                                   {Point(0.914f, 0.149f), CornerRounding(0.660f)},
                                   {Point(0.926f, 0.289f), CornerRounding(0.660f)},
                                   {Point(0.881f, 0.346f)},
                                   {Point(0.940f, 0.344f), CornerRounding(0.126f)},
                                   {Point(1.003f, 0.437f), CornerRounding(0.255f)},
                               },
                               2, Point(0.5f, 0.5f), true),
            m);
    }

    RoundedPolygon puffyDiamond() {
        return customPolygon(
            {
                {Point(0.870f, 0.130f), CornerRounding(0.146f)},
                {Point(0.818f, 0.357f)},
                {Point(1.000f, 0.332f), CornerRounding(0.853f)},
            },
            4, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon pixelCircle() {
        return customPolygon(
            {
                {Point(0.500f, 0.000f)},
                {Point(0.704f, 0.000f)},
                {Point(0.704f, 0.065f)},
                {Point(0.843f, 0.065f)},
                {Point(0.843f, 0.148f)},
                {Point(0.926f, 0.148f)},
                {Point(0.926f, 0.296f)},
                {Point(1.000f, 0.296f)},
            },
            2, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon pixelTriangle() {
        return customPolygon(
            {
                {Point(0.110f, 0.500f)},
                {Point(0.113f, 0.000f)},
                {Point(0.287f, 0.000f)},
                {Point(0.287f, 0.087f)},
                {Point(0.421f, 0.087f)},
                {Point(0.421f, 0.170f)},
                {Point(0.560f, 0.170f)},
                {Point(0.560f, 0.265f)},
                {Point(0.674f, 0.265f)},
                {Point(0.675f, 0.344f)},
                {Point(0.789f, 0.344f)},
                {Point(0.789f, 0.439f)},
                {Point(0.888f, 0.439f)},
            },
            1, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon bun() {
        return customPolygon(
            {
                {Point(0.796f, 0.500f)},
                {Point(0.853f, 0.518f), CornerRounding(1.f)},
                {Point(0.992f, 0.631f), CornerRounding(1.f)},
                {Point(0.968f, 1.000f), CornerRounding(1.f)},
            },
            2, Point(0.5f, 0.5f), true);
    }

    RoundedPolygon heart() {
        return customPolygon(
            {
                {Point(0.500f, 0.268f), CornerRounding(0.016f)},
                {Point(0.792f, -0.066f), CornerRounding(0.958f)},
                {Point(1.064f, 0.276f), CornerRounding(1.000f)},
                {Point(0.501f, 0.946f), CornerRounding(0.129f)},
            },
            1, Point(0.5f, 0.5f), true);
    }

} // namespace

// Public, normalised and cached, as the MaterialShapes companion properties
#define M3_MATERIAL_SHAPE(publicName, builder)                       \
    const RoundedPolygon& MaterialShapes::publicName() {             \
        static const RoundedPolygon shape = builder().normalized(); \
        return shape;                                               \
    }

M3_MATERIAL_SHAPE(circle, ::circle)
M3_MATERIAL_SHAPE(square, ::square)
M3_MATERIAL_SHAPE(slanted, ::slanted)
M3_MATERIAL_SHAPE(arch, ::arch)
M3_MATERIAL_SHAPE(fan, ::fan)
M3_MATERIAL_SHAPE(arrow, ::arrow)
M3_MATERIAL_SHAPE(semiCircle, ::semiCircle)
M3_MATERIAL_SHAPE(oval, ::oval)
M3_MATERIAL_SHAPE(pill, ::pill)
M3_MATERIAL_SHAPE(triangle, ::triangle)
M3_MATERIAL_SHAPE(diamond, ::diamond)
M3_MATERIAL_SHAPE(clamShell, ::clamShell)
M3_MATERIAL_SHAPE(pentagon, ::pentagon)
M3_MATERIAL_SHAPE(gem, ::gem)
M3_MATERIAL_SHAPE(sunny, ::sunny)
M3_MATERIAL_SHAPE(verySunny, ::verySunny)
M3_MATERIAL_SHAPE(cookie4Sided, ::cookie4)
M3_MATERIAL_SHAPE(cookie6Sided, ::cookie6)
M3_MATERIAL_SHAPE(cookie7Sided, ::cookie7)
M3_MATERIAL_SHAPE(cookie9Sided, ::cookie9)
M3_MATERIAL_SHAPE(cookie12Sided, ::cookie12)
M3_MATERIAL_SHAPE(ghostish, ::ghostish)
M3_MATERIAL_SHAPE(clover4Leaf, ::clover4)
M3_MATERIAL_SHAPE(clover8Leaf, ::clover8)
M3_MATERIAL_SHAPE(burst, ::burst)
M3_MATERIAL_SHAPE(softBurst, ::softBurst)
M3_MATERIAL_SHAPE(boom, ::boom)
M3_MATERIAL_SHAPE(softBoom, ::softBoom)
M3_MATERIAL_SHAPE(flower, ::flower)
M3_MATERIAL_SHAPE(puffy, ::puffy)
M3_MATERIAL_SHAPE(puffyDiamond, ::puffyDiamond)
M3_MATERIAL_SHAPE(pixelCircle, ::pixelCircle)
M3_MATERIAL_SHAPE(pixelTriangle, ::pixelTriangle)
M3_MATERIAL_SHAPE(bun, ::bun)
M3_MATERIAL_SHAPE(heart, ::heart)

#undef M3_MATERIAL_SHAPE

const std::vector<MaterialShapes::NamedShape>& MaterialShapes::all() {
    static const std::vector<NamedShape> shapes = {
        {"Circle", &circle},
        {"Square", &square},
        {"Slanted", &slanted},
        {"Arch", &arch},
        {"Fan", &fan},
        {"Arrow", &arrow},
        {"SemiCircle", &semiCircle},
        {"Oval", &oval},
        {"Pill", &pill},
        {"Triangle", &triangle},
        {"Diamond", &diamond},
        {"ClamShell", &clamShell},
        {"Pentagon", &pentagon},
        {"Gem", &gem},
        {"Sunny", &sunny},
        {"VerySunny", &verySunny},
        {"Cookie4Sided", &cookie4Sided},
        {"Cookie6Sided", &cookie6Sided},
        {"Cookie7Sided", &cookie7Sided},
        {"Cookie9Sided", &cookie9Sided},
        {"Cookie12Sided", &cookie12Sided},
        {"Ghostish", &ghostish},
        {"Clover4Leaf", &clover4Leaf},
        {"Clover8Leaf", &clover8Leaf},
        {"Burst", &burst},
        {"SoftBurst", &softBurst},
        {"Boom", &boom},
        {"SoftBoom", &softBoom},
        {"Flower", &flower},
        {"Puffy", &puffy},
        {"PuffyDiamond", &puffyDiamond},
        {"PixelCircle", &pixelCircle},
        {"PixelTriangle", &pixelTriangle},
        {"Bun", &bun},
        {"Heart", &heart},
    };
    return shapes;
}
