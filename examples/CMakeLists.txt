# Examples CMakeLists.txt

# Example FTXUI editor
add_library(example-ftxui-editor STATIC
    example-ftxui-editor.cpp
)

target_include_directories(example-ftxui-editor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ../include
)

target_link_libraries(example-ftxui-editor
    PUBLIC
        ftxui-clap-support
        ftxui::screen
        ftxui::dom
        ftxui::component
)

# Make sure the example can be used by other projects
target_include_directories(example-ftxui-editor
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
