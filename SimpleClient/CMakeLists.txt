project(SimpleClient)

set(SOURCES
        src/SimpleClient.cpp
        )

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
        bsl::NetCommon
        )