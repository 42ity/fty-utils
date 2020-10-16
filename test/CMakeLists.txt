include(CTest)

etn_test(${PROJECT_NAME}-test
    SOURCES
        main.cpp
        split.cpp
        convert.cpp
        expected.cpp
        events.cpp
        process.cpp
        translate.cpp
    USES
        ${PROJECT_NAME}
        Catch2::Catch2
        pthread
)
