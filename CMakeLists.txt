cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

##############################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
##############################################################################################################

project(fty-utils
    VERSION 1.0.0
)

##############################################################################################################

etn_target(interface ${PROJECT_NAME}
    PUBLIC
        fty/command-line.h
        fty/convert.h
        fty/split.h
        fty/traits.h
        fty/expected.h
        fty/event.h
        fty/thread-pool.h
)


##############################################################################################################

if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
