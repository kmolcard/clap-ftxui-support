# Test application CMakeLists.txt

# Test executable
add_executable(test-ftxui-clap
    test-ftxui-clap.cpp
)

target_include_directories(test-ftxui-clap
    PRIVATE
        ../include
        ../examples
)

target_link_libraries(test-ftxui-clap
    PRIVATE
        ftxui-clap-support
        example-ftxui-editor
        ftxui::screen
        ftxui::dom
        ftxui::component
)

# Add as a test
add_test(NAME ftxui-clap-basic-test COMMAND test-ftxui-clap)
