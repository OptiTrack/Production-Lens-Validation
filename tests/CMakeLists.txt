# Unit tests for the platform-independent parts of the project.
#
# No external test framework is fetched: the harness in test_support.h keeps CI
# hermetic (no network, no package manager) and buildable with just a C++17
# compiler on Windows and Linux alike.

add_executable(focus_core_tests
    test_support.cpp
    test_focus_score.cpp
)

target_link_libraries(focus_core_tests PRIVATE focus_core)
target_include_directories(focus_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(MSVC)
    target_compile_options(focus_core_tests PRIVATE /W4 /EHsc)
else()
    target_compile_options(focus_core_tests PRIVATE -Wall -Wextra)
endif()

# Register every TEST(...) in the sources below as its own CTest test, so a
# failure names the exact case instead of the whole binary. The runner exits
# with code 2 when a filter matches nothing, which catches names that drift.
set(TEST_SOURCES test_focus_score.cpp)

set(_discovered_tests 0)
foreach(_source IN LISTS TEST_SOURCES)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${_source}" _test_lines REGEX "^TEST\\(")
    foreach(_line IN LISTS _test_lines)
        string(REGEX REPLACE "^TEST\\(([A-Za-z0-9_]+)\\).*$" "\\1" _test_name "${_line}")
        add_test(NAME "focus_core.${_test_name}"
                 COMMAND focus_core_tests --filter "${_test_name}")
        set_tests_properties("focus_core.${_test_name}" PROPERTIES TIMEOUT 60)
        math(EXPR _discovered_tests "${_discovered_tests} + 1")
    endforeach()
endforeach()

if(_discovered_tests EQUAL 0)
    message(FATAL_ERROR "No TEST(...) cases were discovered in: ${TEST_SOURCES}")
endif()

message(STATUS "Registered ${_discovered_tests} unit tests with CTest")
