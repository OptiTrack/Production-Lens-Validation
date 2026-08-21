# focus_core: the platform-independent focus scoring math.
#
# Deliberately free of Qt, OpenCV and Camera SDK dependencies so that it (and
# its unit tests) build on every platform the project targets.

add_library(focus_core STATIC
    FocusScore.cpp
)

target_compile_features(focus_core PUBLIC cxx_std_17)

# Headers are included as "core/FocusScore.h", so publish the parent directory.
target_include_directories(focus_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

set_target_properties(focus_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
