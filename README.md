# clap-ftxui-support

A library providing CLAP plugin GUI support using FTXUI (a terminal-based UI library) instead of traditional graphics frameworks.

## Overview

`clap-ftxui-support` enables CLAP audio plugins to use terminal-based user interfaces that can be embedded within graphical DAW hosts. This unique approach leverages FTXUI's powerful text-based UI components and renders them within traditional plugin windows using platform-specific graphics APIs.

## Features

- **Cross-platform support**: macOS (Metal), Windows (Direct2D), Linux (X11/Xft)
- **Component-based UI architecture**: Following FTXUI's component model
- **Thread-safe parameter updates**: Safe communication between audio and UI threads
- **Embedded terminal rendering**: Terminal UI embedded in graphical DAW windows
- **Modern C++ design**: Uses C++17 features and RAII principles
- **Minimal dependencies**: Only requires FTXUI and platform graphics libraries

## Architecture

The library consists of several key components:

### Core Components

- **`ftxui_clap_editor`**: Base class for FTXUI-based CLAP editors
- **`embedded_terminal`**: Platform-specific terminal rendering system
- **Global render loop**: Manages all active editors and updates

### Platform Integration

- **macOS**: Uses Metal/MetalKit for hardware-accelerated text rendering
- **Windows**: Uses Direct2D/DirectWrite for high-quality text display
- **Linux**: Uses X11/Xft for efficient text rendering with font support

## Quick Start

### 1. Add to Your Project

```cmake
add_subdirectory(clap-ftxui-support)
target_link_libraries(your_plugin PRIVATE clap-ftxui-support)
```

### 2. Create Your Editor

```cpp
#include "ftxui-clap-support/ftxui-clap-editor.h"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

class my_plugin_editor : public ftxui_clap_support::ftxui_clap_editor {
public:
    ftxui::Component get_component() override {
        return ftxui::Renderer([this]() {
            return ftxui::vbox({
                ftxui::text("My Plugin") | ftxui::bold,
                ftxui::separator(),
                ftxui::text("Parameter: " + std::to_string(parameter_value_)),
                // Add your UI components here
            }) | ftxui::border;
        });
    }
    
protected:
    void on_parameter_changed_main_thread(uint32_t param_id, double value) override {
        if (param_id == 0) {
            parameter_value_ = value;
        }
    }
    
private:
    double parameter_value_ = 0.0;
};
```

### 3. Initialize and Use

```cpp
// Initialize the library
ftxui_clap_support::initialize();

// Create and open your editor
auto editor = std::make_shared<my_plugin_editor>();
editor->open(plugin, host);
editor->set_size(80, 25);
editor->show();

// Handle parameter changes from audio thread
editor->on_parameter_changed_audio_thread(param_id, value);

// Cleanup
editor->close();
ftxui_clap_support::shutdown();
```

## Building

### Prerequisites

- C++17 compatible compiler
- CMake 3.15 or later
- FTXUI library
- Platform-specific dependencies:
  - **macOS**: Xcode with Metal framework
  - **Windows**: Visual Studio with Windows SDK
  - **Linux**: X11 development libraries, fontconfig, Xft

### Build Steps

```bash
# Clone with submodules (if FTXUI is included as submodule)
git clone --recursive https://github.com/your-repo/clap-ftxui-support.git
cd clap-ftxui-support

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Optional: Build tests and examples
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build . --config Release
```

### CMake Options

- `BUILD_TESTS=ON/OFF`: Build test applications (default: OFF)
- `BUILD_EXAMPLES=ON/OFF`: Build example plugins (default: OFF)
- `ENABLE_ASAN=ON/OFF`: Enable AddressSanitizer for debugging (default: OFF)

## API Reference

### ftxui_clap_editor

Base class for all FTXUI-based CLAP plugin editors.

#### Core Methods

```cpp
// Lifecycle
bool open(const clap_plugin* plugin, const clap_host* host);
void close();
void show();
void hide();

// Configuration
void set_size(uint32_t width, uint32_t height);
void set_scale(double scale);

// Thread-safe parameter updates
void on_parameter_changed_audio_thread(uint32_t param_id, double value);
```

#### Virtual Methods to Override

```cpp
// UI component creation
virtual ftxui::Component get_component() = 0;

// Lifecycle callbacks
virtual bool on_open();
virtual void on_close();
virtual void on_show();
virtual void on_hide();
virtual void on_size_changed(uint32_t width, uint32_t height);
virtual void on_scale_changed(double scale);

// Parameter updates (called on main thread)
virtual void on_parameter_changed_main_thread(uint32_t param_id, double value);
```

### Global Functions

```cpp
namespace ftxui_clap_support {
    // Library lifecycle
    bool initialize();
    void shutdown();
    
    // Editor registration (called automatically)
    void register_editor(std::shared_ptr<ftxui_clap_editor> editor);
    
    // Thread-safe parameter updates (called automatically)
    void queue_parameter_update(uint32_t param_id, double value, 
                               std::weak_ptr<ftxui_clap_editor> editor);
}
```

## Examples

The library includes a comprehensive example plugin demonstrating:

- Parameter display and manipulation
- Keyboard navigation
- Real-time parameter updates
- Component-based UI design
- Thread-safe communication

See `examples/example-ftxui-editor.h` and `examples/example-ftxui-editor.cpp` for details.

## Technical Details

### Rendering Pipeline

1. **Component Creation**: Each editor creates FTXUI components
2. **Screen Rendering**: Components are rendered to offscreen FTXUI screens
3. **String Conversion**: Screens are converted to text using `Screen::ToString()`
4. **Platform Rendering**: Text is rendered using platform-specific graphics APIs
5. **Display Update**: Rendered content is displayed in the plugin window

### Thread Safety

- **Audio Thread**: Calls `on_parameter_changed_audio_thread()` 
- **Parameter Queue**: Thread-safe queue buffers parameter updates
- **Main Thread**: Processes queued updates and renders UI
- **Render Loop**: Runs at ~60 FPS in dedicated thread

### Memory Management

- **RAII Design**: Automatic resource cleanup
- **Shared Pointers**: Safe editor lifetime management
- **Weak References**: Prevents circular dependencies
- **Platform Resources**: Automatically managed per platform

## Platform-Specific Notes

### macOS
- Uses Metal for hardware-accelerated rendering
- Requires macOS 10.13+ for Metal support
- Font rendering via CoreText
- Automatic high-DPI support

### Windows
- Uses Direct2D/DirectWrite for text rendering
- Requires Windows 7+ with Platform Update
- Hardware acceleration when available
- ClearType font rendering

### Linux
- Uses X11 with Xft for font rendering
- Fontconfig for font management
- Works with most X11-based desktop environments
- Supports both bitmap and vector fonts

## Troubleshooting

### Common Issues

1. **Library fails to initialize**
   - Check platform-specific dependencies are installed
   - Verify graphics drivers are up to date
   - Ensure permissions for graphics API access

2. **Text rendering issues**
   - Install monospace fonts (Monaco, Consolas, etc.)
   - Check font configuration on Linux systems
   - Verify graphics API support

3. **Performance problems**
   - Reduce update frequency for complex UIs
   - Use simpler FTXUI components
   - Check for memory leaks in custom components

### Debug Mode

Build with debug flags to enable additional logging:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Ensure all platforms build successfully
6. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- **FTXUI**: Excellent terminal UI framework
- **CLAP**: Modern audio plugin standard
- **clap-imgui-support**: Inspiration for architecture design

## Future Enhancements

- **Mouse input support**: Full mouse interaction within terminal UI
- **Color theme support**: Customizable color schemes
- **High-DPI scaling**: Better support for high-resolution displays
- **Accessibility features**: Screen reader compatibility
- **Performance optimizations**: GPU-accelerated text rendering
- **Additional platforms**: Support for embedded systems
