file(GLOB_RECURSE BLOCH_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/bloch/*.cpp
)

add_library(bloch_lib ${BLOCH_SOURCES})

target_include_directories(bloch_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(bloch
    main.cpp
)

target_link_libraries(bloch
    bloch_lib
)