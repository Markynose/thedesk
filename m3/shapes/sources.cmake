# Material 3 Expressive shapes, shape morphing and the loading indicator,
# ported from androidx graphics-shapes and Compose Material 3 (Apache 2.0;
# see shapes/README.md). Paths are relative to the m3 directory.
set(M3_SHAPES_SOURCES
    shapes/point.h
    shapes/utils.h shapes/utils.cpp
    shapes/cornerrounding.h
    shapes/cubic.h shapes/cubic.cpp
    shapes/feature.h shapes/feature.cpp
    shapes/featuredetector.h shapes/featuredetector.cpp
    shapes/floatmapping.h shapes/floatmapping.cpp
    shapes/featuremapping.h shapes/featuremapping.cpp
    shapes/polygonmeasure.h shapes/polygonmeasure.cpp
    shapes/roundedpolygon.h shapes/roundedpolygon.cpp
    shapes/shapes.cpp
    shapes/morph.h shapes/morph.cpp
    shapes/shapepath.h shapes/shapepath.cpp
    shapes/materialshapes.h shapes/materialshapes.cpp
    shapes/m3loadingindicator.h shapes/m3loadingindicator.cpp
)
