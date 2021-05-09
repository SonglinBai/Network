project(NetCommon)

add_library(${PROJECT_NAME} INTERFACE)
add_library(bsl::NetCommon ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
        INTERFACE
        ${ForestIM_SOURCE_DIR}/external/asio/include
        ${PROJECT_SOURCE_DIR}/include
        )