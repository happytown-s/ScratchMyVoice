cmake_minimum_required(VERSION 3.22)
project(ScratchMyVoice VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE path - can be overridden with -DJUCE_PATH=...
if(NOT DEFINED JUCE_PATH)
    # Default: ~/JUCE (use $ENV{HOME} since ~ doesn't expand in CMake)
    set(JUCE_PATH "$ENV{HOME}/JUCE")
endif()

# Verify JUCE exists
if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_PATH}. Please specify -DJUCE_PATH=/path/to/JUCE")
endif()

# Add JUCE
add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")

# Define JUCE GUI Application
juce_add_gui_app(ScratchMyVoice
    PRODUCT_NAME "ScratchMyVoice"
    BUNDLE_ID "com.yourcompany.scratchmyvoice"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "マイクを使用します。"
)

# Source files
target_sources(ScratchMyVoice PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/AudioEngine.cpp
    Source/TurntableComponent.cpp
    Source/CrossfaderComponent.cpp
    Source/WaveformComponent.cpp
    Source/SampleListComponent.cpp
    Source/SampleSlotComponent.cpp
)

# Include directories
target_include_directories(ScratchMyVoice PRIVATE
    Source
    JuceLibraryCode
)

# Compile definitions
target_compile_definitions(ScratchMyVoice PRIVATE
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

# Link JUCE modules
target_link_libraries(ScratchMyVoice PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
