//
// ftxui-clap-editor.h
// CLAP FTXUI Support Library
//

#ifndef CLAP_FTXUI_SUPPORT_FTXUI_CLAP_EDITOR_H
#define CLAP_FTXUI_SUPPORT_FTXUI_CLAP_EDITOR_H

#include "clap/ext/gui.h"
#include "clap/ext/timer-support.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <memory>

/// @brief Base class for FTXUI-based CLAP plugin editors
///
/// This class provides the interface between CLAP plugin GUIs and the FTXUI
/// library. Plugin developers should inherit from this class and implement
/// onCreateComponent() to define their UI layout using FTXUI components.
///
/// Unlike ImGui's immediate mode approach, FTXUI uses a retained component
/// model where the UI is built once and then updated through data binding.
struct ftxui_clap_editor {
  virtual ~ftxui_clap_editor() = default;

  /// @brief Called when the GUI is created by the host
  /// Override this to perform any initialization needed for your UI
  virtual void onGuiCreate() {}

  /// @brief Called when the GUI is destroyed by the host
  /// Override this to perform any cleanup needed for your UI
  virtual void onGuiDestroy() {}

  /// @brief Create and return the main FTXUI component for your plugin
  /// This is the core method that defines your plugin's UI structure.
  /// @return A Component that represents the entire plugin interface
  /// @note This will be called once during GUI creation. The returned component
  ///       should handle all UI interactions through FTXUI's event system.
  virtual ftxui::Component onCreateComponent() = 0;

  /// @brief Handle custom events not processed by FTXUI components
  /// Override this to handle special events or key combinations
  /// @param event The FTXUI event to handle
  /// @return true if the event was handled, false to pass it to FTXUI
  /// components
  virtual bool onEvent(const ftxui::Event &event) { return false; }

  /// @brief Called periodically to allow parameter updates from the audio
  /// thread Override this to poll parameter changes and update your UI
  /// components
  virtual void onParameterUpdate() {}

  /// @brief Get preferred terminal dimensions for this editor
  /// Override this to specify the ideal size for your plugin's terminal UI
  /// @param cols Reference to store preferred column count
  /// @param rows Reference to store preferred row count
  virtual void getPreferredSize(int &cols, int &rows) const {
    cols = 80; // Standard terminal width
    rows = 24; // Standard terminal height
  }

  /// @brief Check if the editor can be resized
  /// @return true if the UI can adapt to different terminal sizes
  virtual bool canResize() const { return true; }

  /// @brief Adjust the requested size to fit the UI constraints
  /// Override this to enforce specific size requirements or aspect ratios
  /// @param cols Reference to column count, may be modified
  /// @param rows Reference to row count, may be modified
  /// @return true if the size was adjusted, false if the requested size is
  /// acceptable
  virtual bool adjustSize(int &cols, int &rows) const {
    // Enforce minimum size
    if (cols < 40)
      cols = 40;
    if (rows < 10)
      rows = 10;

    // Enforce maximum size to prevent excessive resource usage
    if (cols > 120)
      cols = 120;
    if (rows > 40)
      rows = 40;

    return true;
  }

  /// @brief Platform-specific context pointer
  /// This is managed internally by the clap-ftxui-support library
  /// and should not be accessed directly by plugin code
  void *ctx{nullptr};
};

/// @brief Configuration options for the FTXUI terminal renderer
struct ftxui_clap_terminal_options {
  /// Terminal size constraints
  int min_cols = 40;
  int min_rows = 10;
  int max_cols = 120;
  int max_rows = 40;

  /// Character aspect ratio for pixel-to-character conversion
  /// Typical monospace fonts have width/height ratio around 0.5-0.6
  float char_aspect_ratio = 0.55f;

  /// Rendering options
  bool enable_mouse = true;
  bool enable_colors = true;
  bool enable_unicode = true;

  /// Performance tuning
  int target_fps = 30;
  bool use_dirty_tracking = true;

  /// Font preferences (may be ignored if not supported by host)
  const char *preferred_font_family = "monospace";
  int preferred_font_size = 12;
};

// Core API functions for CLAP integration
// These mirror the clap-imgui-support API but work with FTXUI

/// @brief Create and initialize the FTXUI-based GUI
/// @param editor Pointer to the plugin's ftxui_clap_editor instance
/// @param timer Host timer support interface (optional, may be nullptr)
/// @param options Configuration options for the terminal renderer (optional)
/// @return true if the GUI was successfully created
bool ftxui_clap_guiCreateWith(
    ftxui_clap_editor *editor, const clap_host_timer_support_t *timer,
    const ftxui_clap_terminal_options *options = nullptr);

/// @brief Destroy the FTXUI-based GUI and free all resources
/// @param editor Pointer to the plugin's ftxui_clap_editor instance
/// @param timer Host timer support interface (should match create call)
void ftxui_clap_guiDestroyWith(ftxui_clap_editor *editor,
                               const clap_host_timer_support_t *timer);

/// @brief Set the parent window for the embedded terminal UI
/// @param editor Pointer to the plugin's ftxui_clap_editor instance
/// @param window CLAP window structure containing platform-specific parent
/// window
/// @return true if the parent was successfully set
bool ftxui_clap_guiSetParentWith(ftxui_clap_editor *editor,
                                 const clap_window *window);

/// @brief Set the size of the GUI in pixels (will be converted to terminal
/// dimensions)
/// @param editor Pointer to the plugin's ftxui_clap_editor instance
/// @param width Width in pixels
/// @param height Height in pixels
/// @return true if the size was successfully set
bool ftxui_clap_guiSetSizeWith(ftxui_clap_editor *editor, int width,
                               int height);

/// @brief Show the GUI (make it visible)
/// @param editor Pointer to the plugin's ftxui_clap_editor instance
/// @return true if the GUI was successfully shown
bool ftxui_clap_guiShowWith(ftxui_clap_editor *editor);

/// @brief Hide the GUI (make it invisible but don't destroy it)
/// @param editor Pointer to the plugin's ftxui_clap_editor instance
/// @return true if the GUI was successfully hidden
bool ftxui_clap_guiHideWith(ftxui_clap_editor *editor);

/// @brief Get the current size of the GUI in pixels
/// @param editor Pointer to the plugin's ftxui_clap_editor instance
/// @param width Reference to store the current width in pixels
/// @param height Reference to store the current height in pixels
/// @return true if the size was successfully retrieved
bool ftxui_clap_guiGetSizeWith(ftxui_clap_editor *editor, int &width,
                               int &height);

#endif // CLAP_FTXUI_SUPPORT_FTXUI_CLAP_EDITOR_H
