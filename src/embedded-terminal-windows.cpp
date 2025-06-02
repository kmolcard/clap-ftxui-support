#include "embedded-terminal.h"

#ifdef _WIN32

#include <d2d1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <memory>
#include <unordered_map>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace ftxui_clap_support {

// Windows-specific terminal renderer using Direct2D
class WindowsTerminalRenderer {
public:
  WindowsTerminalRenderer(HWND hwnd);
  ~WindowsTerminalRenderer();

  bool initialize();
  void render(const std::string &content);
  void resize(int width, int height);

private:
  HWND hwnd_;
  ComPtr<ID2D1Factory> d2d_factory_;
  ComPtr<ID2D1HwndRenderTarget> render_target_;
  ComPtr<IDWriteFactory> dwrite_factory_;
  ComPtr<IDWriteTextFormat> text_format_;
  ComPtr<ID2D1SolidColorBrush> text_brush_;
  ComPtr<ID2D1SolidColorBrush> background_brush_;

  float char_width_ = 8.0f;
  float char_height_ = 16.0f;
};

WindowsTerminalRenderer::WindowsTerminalRenderer(HWND hwnd) : hwnd_(hwnd) {}

WindowsTerminalRenderer::~WindowsTerminalRenderer() {}

bool WindowsTerminalRenderer::initialize() {
  HRESULT hr;

  // Create D2D factory
  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory_);
  if (FAILED(hr))
    return false;

  // Create DWrite factory
  hr = DWriteCreateFactory(
      DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
      reinterpret_cast<IUnknown **>(dwrite_factory_.GetAddressOf()));
  if (FAILED(hr))
    return false;

  // Create text format
  hr = dwrite_factory_->CreateTextFormat(
      L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &text_format_);
  if (FAILED(hr)) {
    // Fallback to Courier New
    hr = dwrite_factory_->CreateTextFormat(
        L"Courier New", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us",
        &text_format_);
  }
  if (FAILED(hr))
    return false;

  // Get client rect for render target
  RECT rect;
  GetClientRect(hwnd_, &rect);

  // Create render target
  D2D1_SIZE_U size =
      D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top);
  hr = d2d_factory_->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(hwnd_, size), &render_target_);
  if (FAILED(hr))
    return false;

  // Create brushes
  hr = render_target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                             &text_brush_);
  if (FAILED(hr))
    return false;

  hr = render_target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                                             &background_brush_);
  if (FAILED(hr))
    return false;

  // Calculate character dimensions
  ComPtr<IDWriteTextLayout> layout;
  hr = dwrite_factory_->CreateTextLayout(L"M", 1, text_format_.Get(), 100, 100,
                                         &layout);
  if (SUCCEEDED(hr)) {
    DWRITE_TEXT_METRICS metrics;
    layout->GetMetrics(&metrics);
    char_width_ = metrics.width;
    char_height_ = metrics.height;
  }

  return true;
}

void WindowsTerminalRenderer::render(const std::string &content) {
  if (!render_target_)
    return;

  render_target_->BeginDraw();

  // Clear background
  render_target_->Clear(D2D1::ColorF(D2D1::ColorF::Black));

  // Convert content to wide string
  int wide_size =
      MultiByteToWideChar(CP_UTF8, 0, content.c_str(), -1, nullptr, 0);
  if (wide_size > 0) {
    std::vector<wchar_t> wide_content(wide_size);
    MultiByteToWideChar(CP_UTF8, 0, content.c_str(), -1, wide_content.data(),
                        wide_size);

    // Create text layout
    ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = dwrite_factory_->CreateTextLayout(
        wide_content.data(), wide_size - 1, text_format_.Get(), 1000, 1000,
        &layout);

    if (SUCCEEDED(hr)) {
      // Draw text
      render_target_->DrawTextLayout(D2D1::Point2F(5.0f, 5.0f), layout.Get(),
                                     text_brush_.Get());
    }
  }

  render_target_->EndDraw();
}

void WindowsTerminalRenderer::resize(int width, int height) {
  if (render_target_) {
    D2D1_SIZE_U size = D2D1::SizeU(width, height);
    render_target_->Resize(size);
  }
}

// Window procedure for terminal windows
LRESULT CALLBACK TerminalWindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  WindowsTerminalRenderer *renderer =
      reinterpret_cast<WindowsTerminalRenderer *>(
          GetWindowLongPtr(hwnd, GWLP_USERDATA));

  switch (msg) {
  case WM_PAINT: {
    if (renderer) {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      // Rendering is handled by the renderer
      EndPaint(hwnd, &ps);
    }
    return 0;
  }
  case WM_SIZE: {
    if (renderer) {
      int width = LOWORD(lParam);
      int height = HIWORD(lParam);
      renderer->resize(width, height);
    }
    return 0;
  }
  case WM_DESTROY:
    return 0;
  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
}

// Platform-specific storage for Windows
static std::unordered_map<void *, std::unique_ptr<WindowsTerminalRenderer>>
    g_renderers;
static bool g_class_registered = false;

bool embedded_terminal::platform_initialize() {
  if (!g_class_registered) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = TerminalWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"FTXUITerminalWindow";

    if (!RegisterClassEx(&wc)) {
      return false;
    }
    g_class_registered = true;
  }
  return true;
}

void embedded_terminal::platform_shutdown() {
  g_renderers.clear();
  if (g_class_registered) {
    UnregisterClass(L"FTXUITerminalWindow", GetModuleHandle(nullptr));
    g_class_registered = false;
  }
}

bool embedded_terminal::platform_create_window(editor_window &window,
                                               void *parent_handle, int x,
                                               int y, int width, int height) {
  HWND parent_hwnd = static_cast<HWND>(parent_handle);
  if (!parent_hwnd) {
    return false;
  }

  HWND child_hwnd = CreateWindowEx(
      0, L"FTXUITerminalWindow", L"FTXUI Terminal", WS_CHILD | WS_VISIBLE, x, y,
      width, height, parent_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

  if (!child_hwnd) {
    return false;
  }

  auto renderer = std::make_unique<WindowsTerminalRenderer>(child_hwnd);
  if (!renderer->initialize()) {
    DestroyWindow(child_hwnd);
    return false;
  }

  SetWindowLongPtr(child_hwnd, GWLP_USERDATA,
                   reinterpret_cast<LONG_PTR>(renderer.get()));

  window.platform_handle = child_hwnd;
  g_renderers[window.platform_handle] = std::move(renderer);

  return true;
}

void embedded_terminal::platform_update_window(editor_window &window) {
  auto it = g_renderers.find(window.platform_handle);
  if (it != g_renderers.end()) {
    it->second->render(window.content);
    InvalidateRect(static_cast<HWND>(window.platform_handle), nullptr, FALSE);
  }
}

void embedded_terminal::platform_resize_window(editor_window &window, int width,
                                               int height) {
  HWND hwnd = static_cast<HWND>(window.platform_handle);
  if (hwnd) {
    SetWindowPos(hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);

    auto it = g_renderers.find(window.platform_handle);
    if (it != g_renderers.end()) {
      it->second->resize(width, height);
    }
  }
}

void embedded_terminal::platform_show_window(editor_window &window,
                                             bool visible) {
  HWND hwnd = static_cast<HWND>(window.platform_handle);
  if (hwnd) {
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
  }
}

void embedded_terminal::platform_destroy_window(editor_window &window) {
  HWND hwnd = static_cast<HWND>(window.platform_handle);
  if (hwnd) {
    g_renderers.erase(window.platform_handle);
    DestroyWindow(hwnd);
    window.platform_handle = nullptr;
  }
}

} // namespace ftxui_clap_support

#endif // _WIN32
