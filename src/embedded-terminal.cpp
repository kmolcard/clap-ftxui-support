#include "embedded-terminal.h"
#include <algorithm>
#include <sstream>

namespace ftxui_clap_support {

embedded_terminal::embedded_terminal() {}

embedded_terminal::~embedded_terminal() { shutdown(); }

bool embedded_terminal::initialize() { return platform_initialize(); }

void embedded_terminal::shutdown() {
  std::lock_guard<std::mutex> lock(editors_mutex_);

  for (auto &[id, window] : editors_) {
    platform_destroy_window(*window);
  }
  editors_.clear();

  platform_shutdown();
}

void embedded_terminal::update_content(const std::string &editor_id,
                                       const std::string &content) {
  std::lock_guard<std::mutex> lock(editors_mutex_);

  auto it = editors_.find(editor_id);
  if (it != editors_.end()) {
    it->second->content = content;
    platform_update_window(*it->second);
  }
}

void embedded_terminal::remove_editor(const std::string &editor_id) {
  std::lock_guard<std::mutex> lock(editors_mutex_);

  auto it = editors_.find(editor_id);
  if (it != editors_.end()) {
    platform_destroy_window(*it->second);
    editors_.erase(it);
  }
}

bool embedded_terminal::create_window(const std::string &editor_id,
                                      void *parent_handle, int x, int y,
                                      int width, int height) {
  std::lock_guard<std::mutex> lock(editors_mutex_);

  auto window = std::make_unique<editor_window>();
  window->width = width;
  window->height = height;

  if (!platform_create_window(*window, parent_handle, x, y, width, height)) {
    return false;
  }

  editors_[editor_id] = std::move(window);
  return true;
}

void embedded_terminal::resize_window(const std::string &editor_id, int width,
                                      int height) {
  std::lock_guard<std::mutex> lock(editors_mutex_);

  auto it = editors_.find(editor_id);
  if (it != editors_.end()) {
    it->second->width = width;
    it->second->height = height;
    platform_resize_window(*it->second, width, height);
  }
}

void embedded_terminal::show_window(const std::string &editor_id,
                                    bool visible) {
  std::lock_guard<std::mutex> lock(editors_mutex_);

  auto it = editors_.find(editor_id);
  if (it != editors_.end()) {
    it->second->visible = visible;
    platform_show_window(*it->second, visible);
  }
}

// Platform-specific implementations will be in separate files:
// embedded-terminal-macos.mm, embedded-terminal-windows.cpp,
// embedded-terminal-linux.cpp

#if defined(__APPLE__)
// macOS implementation will be in embedded-terminal-macos.mm
#elif defined(_WIN32)
// Windows implementation will be in embedded-terminal-windows.cpp
#elif defined(__linux__)
// Linux implementation will be in embedded-terminal-linux.cpp
#else
// Fallback implementation for unsupported platforms
bool embedded_terminal::platform_initialize() { return false; }
void embedded_terminal::platform_shutdown() {}
bool embedded_terminal::platform_create_window(editor_window &, void *, int,
                                               int, int, int) {
  return false;
}
void embedded_terminal::platform_update_window(editor_window &) {}
void embedded_terminal::platform_resize_window(editor_window &, int, int) {}
void embedded_terminal::platform_show_window(editor_window &, bool) {}
void embedded_terminal::platform_destroy_window(editor_window &) {}
#endif

} // namespace ftxui_clap_support
