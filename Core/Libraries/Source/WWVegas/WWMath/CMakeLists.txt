set(WWMATH_SRC
    aabox.cpp
    aabox.h
    aabtreecull.cpp
    aabtreecull.h
    aaplane.h
    cardinalspline.cpp
    cardinalspline.h
    castres.h
    catmullromspline.cpp
    catmullromspline.h
    colmath.cpp
    colmath.h
    colmathaabox.cpp
    colmathaabox.h
    colmathaabtri.cpp
    colmathfrustum.cpp
    colmathfrustum.h
    colmathinlines.h
    colmathline.cpp
    colmathline.h
    colmathobbobb.cpp
    colmathobbox.cpp
    colmathobbtri.cpp
    colmathplane.cpp
    colmathplane.h
    colmathsphere.cpp
    cullsys.cpp
    cullsys.h
    culltype.h
    curve.cpp
    curve.h
    euler.cpp
    euler.h
    frustum.cpp
    frustum.h
    gridcull.cpp
    gridcull.h
    hermitespline.cpp
    hermitespline.h
    lineseg.cpp
    lineseg.h
    lookuptable.cpp
    lookuptable.h
    matrix3.cpp
    matrix3.h
    matrix3d.cpp
    matrix3d.h
    matrix4.cpp
    matrix4.h
    #normalcone.h
    obbox.cpp
    obbox.h
    ode.cpp
    ode.h
    plane.h
    pot.cpp
    pot.h
    quat.cpp
    quat.h
    rect.h
    sphere.h
    tcbspline.cpp
    tcbspline.h
    tri.cpp
    tri.h
    v3_rnd.cpp
    v3_rnd.h
    vector2.h
    vector2i.h
    vector3.h
    Vector3i.h
    vector4.h
    vehiclecurve.cpp
    vehiclecurve.h
    vp.cpp
    vp.h
    wwmath.cpp
    wwmath.h
    wwmathids.h
)

add_library(core_wwmath STATIC)
set_target_properties(core_wwmath PROPERTIES OUTPUT_NAME wwmath)

target_sources(core_wwmath PRIVATE ${WWMATH_SRC})

target_link_libraries(core_wwmath PRIVATE
    core_wwcommon
    corei_always
    core_wwsaveload
)

if (SAGE_USE_DETERMINISTIC_MATH)
    target_link_libraries(core_wwmath PUBLIC
        gamemath
    )
endif()

# @todo Test its impact and see what to do with the legacy functions.
#add_compile_definitions(core_wwmath PUBLIC ALLOW_TEMPORARIES) # Enables legacy math with "temporaries"
