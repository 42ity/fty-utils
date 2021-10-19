cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

project(fty-utils
    VERSION 1.0.0
)

##############################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
##############################################################################################################


##############################################################################################################

etn_target(interface ${PROJECT_NAME}
    PUBLIC
        fty/command-line.h
        fty/convert.h
        fty/string-utils.h
        fty/traits.h
        fty/expected.h
        fty/event.h
        fty/thread-pool.h
        fty/flags.h
        fty/process.h
        fty/translate.h
        fty/timer.h
    USES_PUBLIC
        fmt::fmt
)

##############################################################################################################
etn_test_target(${PROJECT_NAME}
    SOURCES
        test/main.cpp
        test/split.cpp
        test/convert.cpp
        test/expected.cpp
        test/events.cpp
        test/process.cpp
        test/translate.cpp
        test/timer.cpp
        test/thread-pool.cpp
        test/command-line.cpp
    USES
        pthread
)
