cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

##############################################################################################################

project(fty-utils
    VERSION 1.0.0
)

include(fty-project/cmake/fty-project.cmake)

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
