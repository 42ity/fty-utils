etn_target(exe ${PROJECT_NAME}-test
    SOURCES
        main.cpp
        split.cpp
        convert.cpp
        expected.cpp
        events.cpp
    USES
        ${PROJECT_NAME}
        Catch2::Catch2
        pthread
)

ParseAndAddCatchTests(${PROJECT_NAME}-test)
