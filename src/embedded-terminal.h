#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ftxui_clap_support {

/**
 * Platform-specific embedded terminal emulator
 * This class manages the display of terminal-based UI content within a graphics
 * context
 */
class embedded_terminal {
public:
  embedded_terminal();
  ~embedded_terminal();

  // Initialize the terminal system
  bool initialize();

  // Shutdown and cleanup
  void shutdown();

  // Update content for a specific editor
  void update_content(const std::string &editor_id, const std::string &content);

  // Remove content for an editor
  void remove_editor(const std::string &editor_id);

  // Platform-specific window creation
  bool create_window(const std::string &editor_id, void *parent_handle, int x,
                     int y, int width, int height);

  // Update window size
  void resize_window(const std::string &editor_id, int width, int height);

  // Show/hide window
  void show_window(const std::string &editor_id, bool visible);

private:
  struct editor_window {
    std::string content;
    void *platform_handle = nullptr;
    int width = 0;
    int height = 0;
    bool visible = false;
  };

  std::unordered_map<std::string, std::unique_ptr<editor_window>> editors_;
  std::mutex editors_mutex_;

  // Platform-specific initialization
  bool platform_initialize();
  void platform_shutdown();

  // Platform-specific window management
  bool platform_create_window(editor_window &window, void *parent_handle, int x,
                              int y, int width, int height);
  void platform_update_window(editor_window &window);
  void platform_resize_window(editor_window &window, int width, int height);
  void platform_show_window(editor_window &window, bool visible);
  void platform_destroy_window(editor_window &window);
};

} // namespace ftxui_clap_support
