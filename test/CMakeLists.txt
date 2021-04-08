include(CTest)

etn_test_target(${PROJECT_NAME}
    SOURCES
        main.cpp
        split.cpp
        convert.cpp
        expected.cpp
        events.cpp
        process.cpp
        translate.cpp
    USES
        pthread
)

