cmake_minimum_required(VERSION 3.28)
project(guitarfx VERSION 1.0.0 LANGUAGES CXX C)

# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compile NAM and project using float samples
add_compile_definitions(NAM_SAMPLE_FLOAT)

# Option for sanitizers
option(GUITARFX_ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(GUITARFX_ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)

# Common Sanitizer Flags
set(SANITIZER_FLAGS "")
if(GUITARFX_ENABLE_ASAN)
    list(APPEND SANITIZER_FLAGS "-fsanitize=address")
endif()
if(GUITARFX_ENABLE_UBSAN)
    list(APPEND SANITIZER_FLAGS "-fsanitize=undefined" "-fno-sanitize-recover=all")
endif()

# Strict warnings for our code
set(GUITARFX_WARNINGS
    "-Wall"
    "-Wextra"
    "-Wpedantic"
    "-Wshadow"
    "-Wnon-virtual-dtor"
    "-Wunused"
    "-Woverloaded-virtual"
    "-Wconversion"
    "-Wsign-conversion"
    "-Wnull-dereference"
    "-Wformat=2"
)

# Fetch Dependencies
add_subdirectory(thirdparty)

# ---------------------------------------------------------------------------
# Target: guitarfx-core (DSP Engine)
# ---------------------------------------------------------------------------
add_library(guitarfx-core STATIC
    src/core/AsyncAssetLoader.cpp
    src/core/CabinetProcessor.cpp
    src/core/DSPEngine.cpp
    src/core/EQProcessor.cpp
    src/core/InputGainProcessor.cpp
    src/core/NAMProcessor.cpp
    src/core/NoiseGateProcessor.cpp
    src/core/OutputProcessor.cpp
    src/core/PresetManager.cpp
)

# Public headers live in include/guitarfx and are consumed as <guitarfx/...>
target_include_directories(guitarfx-core PUBLIC include)
target_link_libraries(guitarfx-core PUBLIC
    NamCore
    nlohmann_json::nlohmann_json
)
target_compile_options(guitarfx-core PRIVATE ${GUITARFX_WARNINGS} ${SANITIZER_FLAGS})
target_link_options(guitarfx-core PUBLIC ${SANITIZER_FLAGS})

# ---------------------------------------------------------------------------
# Target: guitarfx-app (JUCE GUI standalone application)
# ---------------------------------------------------------------------------
# JUCE 8 CMake API creates the executable
juce_add_gui_app(guitarfx-app
    PRODUCT_NAME "GuitarFX"
    VERSION 1.0.0
    BUNDLE_ID "com.guitarfx.app"
)

# App-private headers live next to their sources in src/app
target_sources(guitarfx-app PRIVATE
    src/app/AudioEngine.cpp
    src/app/Main.cpp
    src/app/MainComponent.cpp
)
target_include_directories(guitarfx-app PRIVATE src/app)

# Configure JUCE 8 modules we use
target_link_libraries(guitarfx-app PRIVATE
    guitarfx-core
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_core
)

# Disable web browser module to prevent unnecessary dependencies
target_compile_definitions(guitarfx-app PRIVATE
    JUCE_USE_LOLA_WEBVIEW=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ALSA=1
    JUCE_JACK=1
)

target_compile_options(guitarfx-app PRIVATE ${GUITARFX_WARNINGS} ${SANITIZER_FLAGS})
target_link_options(guitarfx-app PRIVATE ${SANITIZER_FLAGS})

# Enable JACK/ALSA backend support explicitly in JUCE options if needed

# ---------------------------------------------------------------------------
# Target: guitarfx-tests (Unit and Offline tests)
# ---------------------------------------------------------------------------
add_executable(guitarfx-tests tests/main.cpp)
target_link_libraries(guitarfx-tests PRIVATE guitarfx-core)
target_compile_options(guitarfx-tests PRIVATE ${GUITARFX_WARNINGS} ${SANITIZER_FLAGS})
target_link_options(guitarfx-tests PRIVATE ${SANITIZER_FLAGS})

# ---------------------------------------------------------------------------
# Target: guitarfx-benchmark (Headless performance benchmark)
# ---------------------------------------------------------------------------
add_executable(guitarfx-benchmark benchmarks/main.cpp)
target_link_libraries(guitarfx-benchmark PRIVATE guitarfx-core)
target_compile_options(guitarfx-benchmark PRIVATE ${GUITARFX_WARNINGS} ${SANITIZER_FLAGS})
target_link_options(guitarfx-benchmark PRIVATE ${SANITIZER_FLAGS})
