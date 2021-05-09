project(SimpleExample)

# Set Sources
set(SOURCES
        src/main.cpp
        )

# Create the executable
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${ForestIM_SOURCE_DIR}/external/asio/include
        )