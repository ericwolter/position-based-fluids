include_directories(
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/lib/glm
    ${CMAKE_SOURCE_DIR}/lib/glfw/include
    ${CMAKE_SOURCE_DIR}/lib/soil/src/
    ${CMAKE_SOURCE_DIR}/lib/AntTweakBar/include
    ${GLEW_INCLUDE_DIRS}
	${OPENCL_INCLUDE_DIRS}
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS} -O3 -pedantic -Wall -Wextra -Werror -Wfatal-errors -D_MACOSX -ObjC++")
endif()

set(GLOBAL PROPERTY USE_FOLDERS ON)

add_definitions(-DTW_EXPORTS)

set(SOURCE
    main.cpp
    Runner.cpp
    Simulation.cpp
    Resources.cpp
    ParamUtils.cpp
    OCLPerfMon.cpp
    UIManager.cpp    
    ZPR.cpp
    OGL_Utils.cpp
    OCL_Logger.cpp
    OGL_RenderStageInspector.cpp
)

set(HEADER
    hesp.hpp
    Particle.hpp
    Runner.hpp
    Simulation.hpp
    Resources.hpp
    Parameters.hpp  
    ParamUtils.hpp
    OCLPerfMon.h
    UIManager.h
    ZPR.h
    OGL_Utils.h
    OCL_Logger.h
    OGL_RenderStageInspector.h
    Precomp_OpenGL.h
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(visual)
add_subdirectory(ocl)

set(KERNELS
    "${PBF_SOURCE_DIR}/src/hesp.hpp"
    "${PBF_SOURCE_DIR}/src/parameters.hpp"
    "${PBF_SOURCE_DIR}/src/kernels/utilities.cl"
    "${PBF_SOURCE_DIR}/src/kernels/apply_viscosity.cl"
    "${PBF_SOURCE_DIR}/src/kernels/apply_vorticity.cl"
    "${PBF_SOURCE_DIR}/src/kernels/build_friends_list.cl"
    "${PBF_SOURCE_DIR}/src/kernels/compute_delta.cl"
    "${PBF_SOURCE_DIR}/src/kernels/compute_scaling.cl"
    "${PBF_SOURCE_DIR}/src/kernels/pack_data.cl"
    "${PBF_SOURCE_DIR}/src/kernels/predict_positions.cl"
    "${PBF_SOURCE_DIR}/src/kernels/radixsort.cl"
    "${PBF_SOURCE_DIR}/src/kernels/reset_grid.cl"
    "${PBF_SOURCE_DIR}/src/kernels/update_cells.cl"
    "${PBF_SOURCE_DIR}/src/kernels/update_positions.cl"
    "${PBF_SOURCE_DIR}/src/kernels/update_predicted.cl"
    "${PBF_SOURCE_DIR}/src/kernels/update_velocities.cl"
    "${PBF_SOURCE_DIR}/src/kernels/logging.cl"
)

SET(SHADERS
    "${PBF_SOURCE_DIR}/src/shaders/particles.vs"
    "${PBF_SOURCE_DIR}/src/shaders/particles_color.fs"
    "${PBF_SOURCE_DIR}/src/shaders/standard.vs"
    "${PBF_SOURCE_DIR}/src/shaders/standard_color.fs"
    "${PBF_SOURCE_DIR}/src/shaders/standard_copy.fs"
    "${PBF_SOURCE_DIR}/src/shaders/fluid_depth_smoothing.fs"
    "${PBF_SOURCE_DIR}/src/shaders/fluid_final_render.fs"
)

SET(SCENARIOS
    "${PBF_SOURCE_DIR}/assets/scenarios/dam_coarse.par"
)

SET(TEXTURES
    "${PBF_SOURCE_DIR}/assets/textures/wall.tga"
)

SET(ANT_TWEAK_BAR_SRC
    "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/LoadOGL.cpp"
    "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/LoadOGLCore.cpp"
    "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/TwOpenGL.cpp"
    "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/TwOpenGLCore.cpp"
    "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/TwFonts.cpp"
    "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/TwMgr.cpp"
    "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/TwBar.cpp"
    "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/TwColors.cpp"
    "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/TwEventGLFW.c"
)

if (WIN32)
    SET(ANT_TWEAK_BAR_SRC_WIN32
        "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/TwDirect3D9.cpp"
        "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/TwDirect3D10.cpp"
        "${CMAKE_SOURCE_DIR}/lib/AntTweakBar/src/TwDirect3D11.cpp"
    )
endif()
    
SOURCE_GROUP("_Resources" FILES ${TEXTURES})
SOURCE_GROUP("_Shaders" FILES ${SHADERS})
SOURCE_GROUP("_Kernels" FILES ${KERNELS})
SOURCE_GROUP("_AntTweakBar" FILES ${ANT_TWEAK_BAR_SRC} ${ANT_TWEAK_BAR_SRC_WIN32})

# also headers are not necessary for cmake, 
# include them here so they appear in IDE like Xcode
add_executable(pbf ${SOURCE} ${HEADER} ${KERNELS} ${SHADERS} ${SCENARIOS} ${TEXTURES} ${ANT_TWEAK_BAR_SRC} ${ANT_TWEAK_BAR_SRC_WIN32})

if (APPLE)
    target_link_libraries(pbf
        glfw
        soil
        ${OPENGL_LIBRARY}
        ${OPENCL_LIBRARY}
        ${COREFOUNDATION_LIBRARY}
        ${COCOA_LIB}
        ${IOKIT_LIB}
        ${COREVIDEO_LIB}
        ${ANTTWEAKBAR_LIBRARY}
        ${GLEW_LIBRARIES}
    )
elseif(MSVC)
    target_link_libraries(pbf
        glfw
        soil
        glew32s
        ${OPENGL_LIBRARY}
        ${OPENCL_LIBRARY}
        ${ANTTWEAKBAR_LIBRARY}
    )
else()
    target_link_libraries(pbf
        glfw
        soil
        ${OPENGL_LIBRARY}
        ${OPENCL_LIBRARY}
        ${ANTTWEAKBAR_LIBRARY}
    )
endif()

set_target_properties( pbf PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

add_custom_target(copy ALL
    COMMENT "Copying support files")

foreach(KERNEL ${KERNELS})
    get_filename_component(FILENAME ${KERNEL} NAME)
    set(SRC "${KERNEL}")
    set(DST "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/kernels/${FILENAME}")

    add_custom_command(
        TARGET copy
        COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
        )
endforeach(KERNEL)

foreach(SHADER ${SHADERS})
    get_filename_component(FILENAME ${SHADER} NAME)
    set(SRC "${SHADER}")
    set(DST "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/shaders/${FILENAME}")

    add_custom_command(
        TARGET copy
        COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
        )
endforeach(SHADER)

foreach(SCENARIO ${SCENARIOS})
    get_filename_component(FILENAME ${SCENARIO} NAME)
    set(SRC "${SCENARIO}")
    set(DST "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/scenarios/${FILENAME}")

    add_custom_command(
        TARGET copy
        COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
        )
endforeach(SCENARIO)

foreach(TEXTURE ${TEXTURES})
    get_filename_component(FILENAME ${TEXTURE} NAME)
    set(SRC "${TEXTURE}")
    set(DST "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/textures/${FILENAME}")

    add_custom_command(
        TARGET copy
        COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
        )
endforeach(TEXTURE)

