project(SimpleServer)

set(SOURCES
        src/SimpleServer.cpp
        )

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
        bsl::NetCommon
        )