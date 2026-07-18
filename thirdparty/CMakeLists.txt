cmake_minimum_required(VERSION 3.28)

include(FetchContent)

# 1. nlohmann_json (Header-only JSON library)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

# 2. NeuralAmpModelerCore
FetchContent_Declare(
    nam_core
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG v0.5.4
)

# Populate nam_core without calling add_subdirectory, to prevent compiling its tools
FetchContent_GetProperties(nam_core)
if(NOT nam_core_POPULATED)
    FetchContent_Populate(nam_core)
endif()

# Define our own NamCore library target by compiling NAM files directly
file(GLOB NAM_SOURCES_TOP "${nam_core_SOURCE_DIR}/NAM/*.cpp" "${nam_core_SOURCE_DIR}/NAM/*.c")
file(GLOB NAM_SOURCES_SUB "${nam_core_SOURCE_DIR}/NAM/*/*.cpp")
set(NAM_SOURCES ${NAM_SOURCES_TOP} ${NAM_SOURCES_SUB})

add_library(NamCore STATIC ${NAM_SOURCES})
target_link_libraries(NamCore PUBLIC nlohmann_json::nlohmann_json)

# Include paths for NAM:
target_include_directories(NamCore PUBLIC
    ${nam_core_SOURCE_DIR}
    ${nam_core_SOURCE_DIR}/Dependencies/nlohmann
    ${nam_core_SOURCE_DIR}/Dependencies/eigen
)

# 3. JUCE 8 (Audio framework)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.1
)
set(JUCE_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(JUCE_BUILD_EXTRAS OFF CACHE INTERNAL "")
set(JUCE_BUILD_HELPER_TOOLS OFF CACHE INTERNAL "")
set(JUCE_COPY_PLUGIN_AFTER_BUILD OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(juce)
