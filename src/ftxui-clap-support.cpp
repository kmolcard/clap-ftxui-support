#include "embedded-terminal.h"
#include "ftxui-clap-support/ftxui-clap-editor.h"
#include <algorithm>
#include <atomic>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace ftxui_clap_support {

// Context structure for each editor instance
struct FTXUIContext {
  ftxui_clap_editor *editor;
  ftxui::Component component;
  std::unique_ptr<embedded_terminal> terminal;
  int cols = 80;
  int rows = 24;
  bool visible = false;

  FTXUIContext(ftxui_clap_editor *ed) : editor(ed) {}
};

// Global state for managing editors and the embedded terminal
static std::unique_ptr<embedded_terminal> g_terminal;
static std::mutex g_editors_mutex;
static std::vector<ftxui_clap_editor *> g_active_editors;
static std::thread g_render_thread;
static std::atomic<bool> g_should_stop{false};

// Thread-safe parameter update queue
struct parameter_update {
  uint32_t param_id;
  double value;
  ftxui_clap_editor *editor;
};

static std::queue<parameter_update> g_parameter_queue;
static std::mutex g_parameter_mutex;

// Main rendering loop for the embedded terminal
static void render_loop() {
  while (!g_should_stop) {
    // Process parameter updates
    {
      std::lock_guard<std::mutex> lock(g_parameter_mutex);
      while (!g_parameter_queue.empty()) {
        auto update = g_parameter_queue.front();
        g_parameter_queue.pop();

        if (update.editor) {
          update.editor->onParameterUpdate();
        }
      }
    }

    // Update all active editors
    std::vector<ftxui_clap_editor *> active_editors;
    {
      std::lock_guard<std::mutex> lock(g_editors_mutex);
      active_editors = g_active_editors;
    }

    // Render each editor and update terminal
    for (auto editor : active_editors) {
      if (!editor || !editor->ctx)
        continue;

      auto ctx = static_cast<FTXUIContext *>(editor->ctx);
      if (ctx->visible && ctx->component) {
        auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(ctx->cols),
                                            ftxui::Dimension::Fixed(ctx->rows));
        ftxui::Render(screen, ctx->component->Render());

        // Convert screen to string and send to terminal
        std::string output = screen.ToString();
        if (g_terminal) {
          std::string editor_id =
              std::to_string(reinterpret_cast<uintptr_t>(editor));
          g_terminal->update_content(editor_id, output);
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
  }
}

bool initialize() {
  if (g_terminal) {
    return true; // Already initialized
  }

  try {
    g_terminal = std::make_unique<embedded_terminal>();
    if (!g_terminal->initialize()) {
      g_terminal.reset();
      return false;
    }

    g_should_stop = false;
    g_render_thread = std::thread(render_loop);

    return true;
  } catch (const std::exception &) {
    return false;
  }
}

void shutdown() {
  g_should_stop = true;

  if (g_render_thread.joinable()) {
    g_render_thread.join();
  }

  {
    std::lock_guard<std::mutex> lock(g_editors_mutex);
    g_active_editors.clear();
  }

  {
    std::lock_guard<std::mutex> lock(g_parameter_mutex);
    while (!g_parameter_queue.empty()) {
      g_parameter_queue.pop();
    }
  }

  g_terminal.reset();
}

void register_editor(ftxui_clap_editor *editor) {
  std::lock_guard<std::mutex> lock(g_editors_mutex);
  g_active_editors.push_back(editor);
}

void unregister_editor(ftxui_clap_editor *editor) {
  std::lock_guard<std::mutex> lock(g_editors_mutex);
  g_active_editors.erase(
      std::remove(g_active_editors.begin(), g_active_editors.end(), editor),
      g_active_editors.end());
}

void queue_parameter_update(uint32_t param_id, double value,
                            ftxui_clap_editor *editor) {
  std::lock_guard<std::mutex> lock(g_parameter_mutex);
  g_parameter_queue.push({param_id, value, editor});
}

} // namespace ftxui_clap_support

// C API implementation for CLAP integration
bool ftxui_clap_guiCreateWith(ftxui_clap_editor *editor,
                              const clap_host_timer_support_t *timer,
                              const ftxui_clap_terminal_options *options) {

  if (!editor)
    return false;

  // Initialize the library if needed
  if (!ftxui_clap_support::initialize()) {
    return false;
  }

  // Create context for this editor
  auto ctx = std::make_unique<ftxui_clap_support::FTXUIContext>(editor);
  editor->ctx = ctx.release();

  // Register editor
  ftxui_clap_support::register_editor(editor);

  // Call editor's lifecycle callback
  editor->onGuiCreate();

  // Create the main component
  auto context = static_cast<ftxui_clap_support::FTXUIContext *>(editor->ctx);
  context->component = editor->onCreateComponent();

  return true;
}

void ftxui_clap_guiDestroyWith(ftxui_clap_editor *editor,
                               const clap_host_timer_support_t *timer) {
  if (!editor || !editor->ctx)
    return;

  // Call editor's lifecycle callback
  editor->onGuiDestroy();

  // Clean up terminal window if it exists
  auto ctx = static_cast<ftxui_clap_support::FTXUIContext *>(editor->ctx);
  if (ctx->terminal) {
    std::string editor_id = std::to_string(reinterpret_cast<uintptr_t>(editor));
    ctx->terminal->remove_editor(editor_id);
  }

  // Unregister editor
  ftxui_clap_support::unregister_editor(editor);

  // Clean up context
  delete ctx;
  editor->ctx = nullptr;
}

bool ftxui_clap_guiSetParentWith(ftxui_clap_editor *editor,
                                 const clap_window *window) {
  if (!editor || !editor->ctx || !window)
    return false;

  auto ctx = static_cast<ftxui_clap_support::FTXUIContext *>(editor->ctx);

  // Initialize platform-specific terminal rendering
  if (!ctx->terminal) {
    ctx->terminal = std::make_unique<ftxui_clap_support::embedded_terminal>();
    if (!ctx->terminal->initialize()) {
      return false;
    }
  }

  // Create terminal window with parent window
  std::string editor_id = std::to_string(reinterpret_cast<uintptr_t>(editor));
  void *parent_handle = nullptr;

#ifdef __APPLE__
  parent_handle = window->cocoa;
#elif defined(_WIN32)
  parent_handle = window->win32;
#elif defined(__linux__)
  parent_handle = window->x11;
#endif

  if (parent_handle) {
    return ctx->terminal->create_window(editor_id, parent_handle, 0, 0,
                                        ctx->cols * 8, ctx->rows * 16);
  }

  return false;
}

bool ftxui_clap_guiSetSizeWith(ftxui_clap_editor *editor, int width,
                               int height) {
  if (!editor || !editor->ctx)
    return false;

  auto ctx = static_cast<ftxui_clap_support::FTXUIContext *>(editor->ctx);

  // Convert pixel dimensions to character dimensions
  // Assuming average character size of 8x16 pixels
  int cols = width / 8;
  int rows = height / 16;

  // Apply constraints
  cols = std::max(40, std::min(120, cols));
  rows = std::max(10, std::min(40, rows));

  // Allow editor to adjust size
  if (!editor->adjustSize(cols, rows)) {
    editor->getPreferredSize(cols, rows);
  }

  ctx->cols = cols;
  ctx->rows = rows;

  return true;
}

bool ftxui_clap_guiShowWith(ftxui_clap_editor *editor) {
  if (!editor || !editor->ctx)
    return false;

  auto ctx = static_cast<ftxui_clap_support::FTXUIContext *>(editor->ctx);
  ctx->visible = true;

  return true;
}

bool ftxui_clap_guiHideWith(ftxui_clap_editor *editor) {
  if (!editor || !editor->ctx)
    return false;

  auto ctx = static_cast<ftxui_clap_support::FTXUIContext *>(editor->ctx);
  ctx->visible = false;

  return true;
}

bool ftxui_clap_guiGetSizeWith(ftxui_clap_editor *editor, int &width,
                               int &height) {
  if (!editor || !editor->ctx)
    return false;

  auto ctx = static_cast<ftxui_clap_support::FTXUIContext *>(editor->ctx);

  // Convert character dimensions back to pixels
  width = ctx->cols * 8;
  height = ctx->rows * 16;

  return true;
} // namespace ftxui_clap_support
