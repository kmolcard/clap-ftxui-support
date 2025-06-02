#include "embedded-terminal.h"

#ifdef __linux__

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <fontconfig/fontconfig.h>
#include <memory>
#include <sstream>
#include <unordered_map>

namespace ftxui_clap_support {

// Linux-specific terminal renderer using X11 and Xft
class LinuxTerminalRenderer {
public:
  LinuxTerminalRenderer(Display *display, Window window);
  ~LinuxTerminalRenderer();

  bool initialize();
  void render(const std::string &content);
  void resize(int width, int height);

private:
  Display *display_;
  Window window_;
  XftDraw *xft_draw_;
  XftFont *font_;
  XftColor text_color_;
  XftColor background_color_;
  GC gc_;

  int char_width_ = 8;
  int char_height_ = 16;
  int width_ = 0;
  int height_ = 0;

  void parse_terminal_content(const std::string &content,
                              std::vector<std::string> &lines);
};

LinuxTerminalRenderer::LinuxTerminalRenderer(Display *display, Window window)
    : display_(display), window_(window), xft_draw_(nullptr), font_(nullptr),
      gc_(0) {}

LinuxTerminalRenderer::~LinuxTerminalRenderer() {
  if (xft_draw_) {
    XftDrawDestroy(xft_draw_);
  }
  if (font_) {
    XftFontClose(display_, font_);
  }
  if (gc_) {
    XFreeGC(display_, gc_);
  }
}

bool LinuxTerminalRenderer::initialize() {
  // Get window attributes
  XWindowAttributes attrs;
  if (XGetWindowAttributes(display_, window_, &attrs) == 0) {
    return false;
  }

  width_ = attrs.width;
  height_ = attrs.height;

  // Create graphics context
  gc_ = XCreateGC(display_, window_, 0, nullptr);
  if (!gc_) {
    return false;
  }

  // Set background to black
  XSetForeground(display_, gc_, BlackPixel(display_, DefaultScreen(display_)));

  // Create Xft draw context
  xft_draw_ = XftDrawCreate(display_, window_,
                            DefaultVisual(display_, DefaultScreen(display_)),
                            DefaultColormap(display_, DefaultScreen(display_)));
  if (!xft_draw_) {
    return false;
  }

  // Load monospace font
  font_ = XftFontOpenName(display_, DefaultScreen(display_), "monospace-12");
  if (!font_) {
    // Fallback fonts
    font_ = XftFontOpenName(display_, DefaultScreen(display_), "fixed-12");
    if (!font_) {
      font_ = XftFontOpenName(display_, DefaultScreen(display_), "fixed");
    }
  }
  if (!font_) {
    return false;
  }

  // Calculate character dimensions
  XGlyphInfo glyph_info;
  XftTextExtentsUtf8(display_, font_, (const FcChar8 *)"M", 1, &glyph_info);
  char_width_ = glyph_info.xOff;
  char_height_ = font_->height;

  // Set up colors
  Colormap colormap = DefaultColormap(display_, DefaultScreen(display_));
  if (!XftColorAllocName(display_,
                         DefaultVisual(display_, DefaultScreen(display_)),
                         colormap, "white", &text_color_)) {
    // Fallback to default colors
    XftColorAllocValue(
        display_, DefaultVisual(display_, DefaultScreen(display_)), colormap,
        &(XRenderColor){0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}, &text_color_);
  }

  if (!XftColorAllocName(display_,
                         DefaultVisual(display_, DefaultScreen(display_)),
                         colormap, "black", &background_color_)) {
    XftColorAllocValue(
        display_, DefaultVisual(display_, DefaultScreen(display_)), colormap,
        &(XRenderColor){0x0000, 0x0000, 0x0000, 0xFFFF}, &background_color_);
  }

  return true;
}

void LinuxTerminalRenderer::parse_terminal_content(
    const std::string &content, std::vector<std::string> &lines) {
  std::istringstream stream(content);
  std::string line;
  lines.clear();

  while (std::getline(stream, line)) {
    lines.push_back(line);
  }
}

void LinuxTerminalRenderer::render(const std::string &content) {
  if (!xft_draw_ || !font_) {
    return;
  }

  // Clear window with black background
  XFillRectangle(display_, window_, gc_, 0, 0, width_, height_);

  // Parse content into lines
  std::vector<std::string> lines;
  parse_terminal_content(content, lines);

  // Render each line
  int y_offset = char_height_;
  for (const auto &line : lines) {
    if (y_offset > height_) {
      break; // Don't render beyond window bounds
    }

    if (!line.empty()) {
      XftDrawStringUtf8(xft_draw_, &text_color_, font_, 5,
                        y_offset, // 5px left margin
                        (const FcChar8 *)line.c_str(), line.length());
    }
    y_offset += char_height_;
  }

  // Flush to ensure rendering
  XFlush(display_);
}

void LinuxTerminalRenderer::resize(int width, int height) {
  width_ = width;
  height_ = height;
}

// Event handling for X11 windows
static int x11_error_handler(Display *display, XErrorEvent *error) {
  // Log error but don't crash
  char error_text[256];
  XGetErrorText(display, error->error_code, error_text, sizeof(error_text));
  // In a real implementation, you might want to log this
  return 0;
}

// Platform-specific storage for Linux
static std::unordered_map<void *, std::unique_ptr<LinuxTerminalRenderer>>
    g_renderers;
static Display *g_display = nullptr;

bool embedded_terminal::platform_initialize() {
  if (!g_display) {
    g_display = XOpenDisplay(nullptr);
    if (!g_display) {
      return false;
    }

    // Set up error handler
    XSetErrorHandler(x11_error_handler);
  }
  return true;
}

void embedded_terminal::platform_shutdown() {
  g_renderers.clear();
  if (g_display) {
    XCloseDisplay(g_display);
    g_display = nullptr;
  }
}

bool embedded_terminal::platform_create_window(editor_window &window,
                                               void *parent_handle, int x,
                                               int y, int width, int height) {
  if (!g_display) {
    return false;
  }

  Window parent_window = reinterpret_cast<Window>(parent_handle);
  if (!parent_window) {
    parent_window = DefaultRootWindow(g_display);
  }

  // Create child window
  Window child_window =
      XCreateSimpleWindow(g_display, parent_window, x, y, width, height, 0,
                          BlackPixel(g_display, DefaultScreen(g_display)),
                          BlackPixel(g_display, DefaultScreen(g_display)));

  if (!child_window) {
    return false;
  }

  // Select events
  XSelectInput(g_display, child_window, ExposureMask | StructureNotifyMask);

  // Map window
  XMapWindow(g_display, child_window);

  // Create renderer
  auto renderer =
      std::make_unique<LinuxTerminalRenderer>(g_display, child_window);
  if (!renderer->initialize()) {
    XDestroyWindow(g_display, child_window);
    return false;
  }

  window.platform_handle = reinterpret_cast<void *>(child_window);
  g_renderers[window.platform_handle] = std::move(renderer);

  XFlush(g_display);
  return true;
}

void embedded_terminal::platform_update_window(editor_window &window) {
  auto it = g_renderers.find(window.platform_handle);
  if (it != g_renderers.end()) {
    it->second->render(window.content);
  }
}

void embedded_terminal::platform_resize_window(editor_window &window, int width,
                                               int height) {
  Window x_window = reinterpret_cast<Window>(window.platform_handle);
  if (x_window && g_display) {
    XResizeWindow(g_display, x_window, width, height);

    auto it = g_renderers.find(window.platform_handle);
    if (it != g_renderers.end()) {
      it->second->resize(width, height);
    }

    XFlush(g_display);
  }
}

void embedded_terminal::platform_show_window(editor_window &window,
                                             bool visible) {
  Window x_window = reinterpret_cast<Window>(window.platform_handle);
  if (x_window && g_display) {
    if (visible) {
      XMapWindow(g_display, x_window);
    } else {
      XUnmapWindow(g_display, x_window);
    }
    XFlush(g_display);
  }
}

void embedded_terminal::platform_destroy_window(editor_window &window) {
  Window x_window = reinterpret_cast<Window>(window.platform_handle);
  if (x_window && g_display) {
    g_renderers.erase(window.platform_handle);
    XDestroyWindow(g_display, x_window);
    XFlush(g_display);
    window.platform_handle = nullptr;
  }
}

} // namespace ftxui_clap_support

#endif // __linux__
