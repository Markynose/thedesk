# m3/shapes: Material 3 Expressive shapes for theDesk

A C++20/Qt 6 port of Android's shape morphing library (`androidx.graphics.shapes`),
the Compose Material 3 `MaterialShapes` catalogue and the Material 3 Expressive
`LoadingIndicator`. The algorithms, constants and shape definitions are the
upstream ones; see the notes at the end for the few deliberate differences.

Upstream: <https://github.com/androidx/androidx>, branch `androidx-main`, commit
`a3b1954381485275e7baaf50da777a5a05115f63`.

Licence: Apache License 2.0 (upstream). Each ported file keeps the Android Open
Source Project copyright notice and is marked "Ported to C++/Qt for theDesk; modified."

## Files

Paths are relative to the androidx repository root.

| theDesk file | Upstream source |
|---|---|
| `point.h` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/Point.kt` |
| `utils.h`, `utils.cpp` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/Utils.kt` |
| `cornerrounding.h` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/CornerRounding.kt` |
| `cubic.h`, `cubic.cpp` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/Cubic.kt` |
| `feature.h`, `feature.cpp` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/Features.kt` |
| `featuredetector.h`, `featuredetector.cpp` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/FeatureDetector.kt`, `.../PolygonValidation.kt` |
| `floatmapping.h`, `floatmapping.cpp` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/FloatMapping.kt` |
| `featuremapping.h`, `featuremapping.cpp` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/FeatureMapping.kt` |
| `polygonmeasure.h`, `polygonmeasure.cpp` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/PolygonMeasure.kt` |
| `roundedpolygon.h`, `roundedpolygon.cpp` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/RoundedPolygon.kt` |
| `shapes.cpp` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/Shapes.kt` |
| `morph.h`, `morph.cpp` | `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/Morph.kt` |
| `shapepath.h`, `shapepath.cpp` | `compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/internal/ShapeUtil.kt`, `toShape()` in `.../material3/MaterialShapes.kt` |
| `materialshapes.h`, `materialshapes.cpp` | `compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/MaterialShapes.kt`; `rotateZ`/`scale`/`map` from `compose/ui/ui-graphics/src/commonMain/kotlin/androidx/compose/ui/graphics/Matrix.kt` |
| `m3loadingindicator.h`, `m3loadingindicator.cpp` | `compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/LoadingIndicator.kt`, `.../material3/tokens/LoadingIndicatorTokens.kt`; spring maths from `compose/animation/animation-core/src/commonMain/kotlin/androidx/compose/animation/core/SpringSimulation.kt`, `SpringEstimation.kt`, `FloatAnimationSpec.kt` |

Not ported: `FeatureSerializer.kt`, `SvgPathParser.kt`, `Format.kt` (serialisation
and SVG import, which nothing here needs), and the Android-only/Compose-only
adapters (`android.graphics.Path` and Compose `Path`/`Shape` conversions, which
are replaced by `shapepath.h`).

`sources.cmake` sets `M3_SHAPES_SOURCES` for the `thedesk-m3` library.

## Port notes

- Kotlin's overloaded `RoundedPolygon(...)` constructors are
  `RoundedPolygon::fromNumVertices`, `fromVertices` and `fromFeatures`; the
  `RoundedPolygon.Companion` shapes are static members (`circle`, `rectangle`,
  `star`, `pill`, `pillStar`). A null `perVertexRounding` is an empty vector and
  a null `innerRounding` is a null pointer.
- `Feature` is one value type with an Edge/Corner tag, not a class hierarchy.
- Float maths goes through double and back as Kotlin/JVM does, `Float.MIN_VALUE`
  keeps its Kotlin meaning (smallest positive float), and sorts are stable like
  `sortedBy`, so results should match Android to the float.
- `require(...)` failures throw `std::invalid_argument`/`std::logic_error`.
- Paths are `QPainterPath`s of cubic Béziers. As in Compose, a path is centred by its
  control point bounds (Skia's `Path.getBounds()`).
- The indeterminate loading indicator's coroutines (a spring every 650 ms plus a
  linear 4666 ms rotation) are evaluated as a pure function of elapsed time.
  Compose clips the indicator to its container. Here the clip is applied only when
  the indicator actually extends past the container (by path intersection, because
  QPainter clip paths are not antialiased). With the default shapes that never
  happens.
