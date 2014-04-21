include_directories(
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/lib/glm
    ${CMAKE_SOURCE_DIR}/lib/glfw/include
    ${CMAKE_SOURCE_DIR}/lib/soil/src/
    ${CMAKE_SOURCE_DIR}/lib/AntTweakBar/include
    ${GLEW_INCLUDE_DIRS}
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
    UIManager.cpp    
    ZPR.cpp
    OGL_Utils.cpp
    OGL_RenderStageInspector.cpp
)

set(HEADER
    Runner.hpp
    Simulation.hpp
    Resources.hpp
    Parameters.hpp  
    ParamUtils.hpp
    UIManager.h
    ZPR.h
    OGL_Utils.h
    OGL_RenderStageInspector.h
    Precomp_OpenGL.h
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(visual)

set(KERNELS_SRC_SHARE
    "${PBF_SOURCE_DIR}/src/parameters.hpp"
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
    
SOURCE_GROUP("_Kernels" FILES ${KERNELS})
SOURCE_GROUP("_AntTweakBar" FILES ${ANT_TWEAK_BAR_SRC} ${ANT_TWEAK_BAR_SRC_WIN32})

# also headers are not necessary for cmake, 
# include them here so they appear in IDE like Xcode
add_executable(pbf ${SOURCE} ${HEADER} ${KERNELS_SRC_SHARE} ${ANT_TWEAK_BAR_SRC} ${ANT_TWEAK_BAR_SRC_WIN32})

if (APPLE)
    target_link_libraries(pbf
        glfw
        soil
        ${OPENGL_LIBRARY}
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
        ${ANTTWEAKBAR_LIBRARY}
    )
else()
    target_link_libraries(pbf
        glfw
        soil
        ${OPENGL_LIBRARY}
        ${ANTTWEAKBAR_LIBRARY}
    )
endif()

set_target_properties( pbf PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

foreach(KERNELS_SRC_SHARE ${KERNELS_SRC_SHARE})
    get_filename_component(FILENAME ${KERNELS_SRC_SHARE} NAME)
    set(SRC "${KERNELS_SRC_SHARE}")
    set(DST "${PBF_SOURCE_DIR}/assets/kernels/${FILENAME}")

    add_custom_command(
        TARGET pbf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
        )
endforeach(KERNELS_SRC_SHARE)
