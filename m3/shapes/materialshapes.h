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
 *
 * Every shape is normalised exactly as Compose does (RoundedPolygon.normalized(),
 * i.e. fitted into the 0..1 unit square), built once and cached. Draw one with
 * m3shapes::toPath(MaterialShapes::cookie9Sided(), rect) from shapepath.h.
 */
#ifndef M3SHAPES_MATERIALSHAPES_H
#define M3SHAPES_MATERIALSHAPES_H

#include "roundedpolygon.h"

#include <vector>

namespace MaterialShapes {
    // A circle shape.
    const m3shapes::RoundedPolygon& circle();
    // A rounded square shape.
    const m3shapes::RoundedPolygon& square();
    // A slanted square shape.
    const m3shapes::RoundedPolygon& slanted();
    // An arch shape.
    const m3shapes::RoundedPolygon& arch();
    // A fan shape.
    const m3shapes::RoundedPolygon& fan();
    // An arrow shape.
    const m3shapes::RoundedPolygon& arrow();
    // A semi-circle shape.
    const m3shapes::RoundedPolygon& semiCircle();
    // An oval shape.
    const m3shapes::RoundedPolygon& oval();
    // A pill shape.
    const m3shapes::RoundedPolygon& pill();
    // A rounded triangle shape.
    const m3shapes::RoundedPolygon& triangle();
    // A diamond shape.
    const m3shapes::RoundedPolygon& diamond();
    // A clam-shell shape.
    const m3shapes::RoundedPolygon& clamShell();
    // A pentagon shape.
    const m3shapes::RoundedPolygon& pentagon();
    // A gem shape.
    const m3shapes::RoundedPolygon& gem();
    // A sunny shape.
    const m3shapes::RoundedPolygon& sunny();
    // A very-sunny shape.
    const m3shapes::RoundedPolygon& verySunny();
    // A 4-sided cookie shape.
    const m3shapes::RoundedPolygon& cookie4Sided();
    // A 6-sided cookie shape.
    const m3shapes::RoundedPolygon& cookie6Sided();
    // A 7-sided cookie shape.
    const m3shapes::RoundedPolygon& cookie7Sided();
    // A 9-sided cookie shape.
    const m3shapes::RoundedPolygon& cookie9Sided();
    // A 12-sided cookie shape.
    const m3shapes::RoundedPolygon& cookie12Sided();
    // A ghost-ish shape.
    const m3shapes::RoundedPolygon& ghostish();
    // A 4-leaf clover shape.
    const m3shapes::RoundedPolygon& clover4Leaf();
    // An 8-leaf clover shape.
    const m3shapes::RoundedPolygon& clover8Leaf();
    // A burst shape.
    const m3shapes::RoundedPolygon& burst();
    // A soft-burst shape.
    const m3shapes::RoundedPolygon& softBurst();
    // A boom shape.
    const m3shapes::RoundedPolygon& boom();
    // A soft-boom shape.
    const m3shapes::RoundedPolygon& softBoom();
    // A flower shape.
    const m3shapes::RoundedPolygon& flower();
    // A puffy shape.
    const m3shapes::RoundedPolygon& puffy();
    // A puffy-diamond shape.
    const m3shapes::RoundedPolygon& puffyDiamond();
    // A pixel-circle shape.
    const m3shapes::RoundedPolygon& pixelCircle();
    // A pixel-triangle shape.
    const m3shapes::RoundedPolygon& pixelTriangle();
    // A bun shape.
    const m3shapes::RoundedPolygon& bun();
    // A heart shape.
    const m3shapes::RoundedPolygon& heart();

    struct NamedShape {
            const char* name; // Compose name, e.g. "Cookie9Sided"
            const m3shapes::RoundedPolygon& (*shape)();
    };

    // Every shape above, in the order MaterialShapes.kt declares them.
    const std::vector<NamedShape>& all();
} // namespace MaterialShapes

#endif // M3SHAPES_MATERIALSHAPES_H
