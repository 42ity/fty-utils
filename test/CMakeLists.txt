etn_target(exe ${PROJECT_NAME}-test
    SOURCES
        main.cpp
        split.cpp
        convert.cpp
        expected.cpp
        events.cpp
    USES
        ${PROJECT_NAME}
        catch2
        pthread
)

find_package(Catch2 PATHS ${CMAKE_BINARY_DIR}/deps-runtime)
include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME}-test)
