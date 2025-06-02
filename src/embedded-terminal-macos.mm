#include "embedded-terminal.h"

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <memory>
#include <unordered_map>

// Custom NSView for rendering terminal content using Metal
@interface FTXUITerminalView : MTKView
@property(nonatomic, strong) NSString *terminalContent;
@property(nonatomic, strong) NSFont *terminalFont;
@property(nonatomic, assign) NSSize characterSize;
- (void)updateContent:(NSString *)content;
- (void)setupMetal;
@end

@implementation FTXUITerminalView {
  id<MTLRenderPipelineState> _pipelineState;
  id<MTLBuffer> _vertexBuffer;
  id<MTLBuffer> _uniformBuffer;
  id<MTLTexture> _fontTexture;
  CTFontRef _font;
  CGColorSpaceRef _colorSpace;
}

- (instancetype)initWithFrame:(NSRect)frame device:(id<MTLDevice>)device {
  if (self = [super initWithFrame:frame device:device]) {
    self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    // Initialize font
    self.terminalFont = [NSFont fontWithName:@"Monaco" size:12.0];
    if (!self.terminalFont) {
      self.terminalFont = [NSFont systemFontOfSize:12.0];
    }

    // Calculate character size
    NSString *testChar = @"M";
    NSDictionary *attributes = @{NSFontAttributeName : self.terminalFont};
    NSSize charSize = [testChar sizeWithAttributes:attributes];
    self.characterSize = charSize;

    [self setupMetal];
  }
  return self;
}

- (void)setupMetal {
  // Create Metal pipeline for text rendering
  NSError *error = nil;

  // Simplified vertex shader for text quads
  NSString *vertexShaderSource = @R"(
        #include <metal_stdlib>
        using namespace metal;
        
        struct VertexIn {
            float2 position [[attribute(0)]];
            float2 texCoord [[attribute(1)]];
        };
        
        struct VertexOut {
            float4 position [[position]];
            float2 texCoord;
        };
        
        vertex VertexOut vertex_main(VertexIn in [[stage_in]]) {
            VertexOut out;
            out.position = float4(in.position, 0.0, 1.0);
            out.texCoord = in.texCoord;
            return out;
        }
    )";

  // Fragment shader for text rendering
  NSString *fragmentShaderSource = @R"(
        #include <metal_stdlib>
        using namespace metal;
        
        fragment float4 fragment_main(float2 texCoord [[stage_in]],
                                     texture2d<float> fontTexture [[texture(0)]],
                                     sampler fontSampler [[sampler(0)]]) {
            float alpha = fontTexture.sample(fontSampler, texCoord).r;
            return float4(1.0, 1.0, 1.0, alpha); // White text
        }
    )";

  id<MTLLibrary> library = [self.device
      newLibraryWithSource:[vertexShaderSource
                               stringByAppendingString:fragmentShaderSource]
                   options:nil
                     error:&error];

  if (error) {
    NSLog(@"Metal library compilation error: %@", error.localizedDescription);
    return;
  }

  MTLRenderPipelineDescriptor *pipelineDescriptor =
      [[MTLRenderPipelineDescriptor alloc] init];
  pipelineDescriptor.vertexFunction =
      [library newFunctionWithName:@"vertex_main"];
  pipelineDescriptor.fragmentFunction =
      [library newFunctionWithName:@"fragment_main"];
  pipelineDescriptor.colorAttachments[0].pixelFormat = self.colorPixelFormat;

  _pipelineState =
      [self.device newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                  error:&error];
  if (error) {
    NSLog(@"Pipeline state creation error: %@", error.localizedDescription);
  }
}

- (void)updateContent:(NSString *)content {
  self.terminalContent = content;
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
  [super drawRect:dirtyRect];

  if (!self.terminalContent) {
    return;
  }

  // For now, use simple text rendering as fallback
  // In a full implementation, this would render the terminal content using
  // Metal
  NSMutableAttributedString *attributedString =
      [[NSMutableAttributedString alloc] initWithString:self.terminalContent];

  [attributedString addAttribute:NSFontAttributeName
                           value:self.terminalFont
                           range:NSMakeRange(0, self.terminalContent.length)];
  [attributedString addAttribute:NSForegroundColorAttributeName
                           value:[NSColor whiteColor]
                           range:NSMakeRange(0, self.terminalContent.length)];

  NSRect textRect = NSMakeRect(5, 5, self.bounds.size.width - 10,
                               self.bounds.size.height - 10);
  [attributedString drawInRect:textRect];
}

@end

// Platform-specific storage for macOS windows
static std::unordered_map<void *, FTXUITerminalView *> g_platform_views;

namespace ftxui_clap_support {

bool embedded_terminal::platform_initialize() {
  return true; // macOS-specific initialization if needed
}

void embedded_terminal::platform_shutdown() { g_platform_views.clear(); }

bool embedded_terminal::platform_create_window(editor_window &window,
                                               void *parent_handle, int x,
                                               int y, int width, int height) {
  @autoreleasepool {
    NSView *parentView = (__bridge NSView *)parent_handle;
    if (!parentView) {
      return false;
    }

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
      return false;
    }

    NSRect frame = NSMakeRect(x, y, width, height);
    FTXUITerminalView *terminalView =
        [[FTXUITerminalView alloc] initWithFrame:frame device:device];

    [parentView addSubview:terminalView];

    window.platform_handle = (__bridge void *)terminalView;
    g_platform_views[window.platform_handle] = terminalView;

    return true;
  }
}

void embedded_terminal::platform_update_window(editor_window &window) {
  @autoreleasepool {
    auto it = g_platform_views.find(window.platform_handle);
    if (it != g_platform_views.end()) {
      FTXUITerminalView *view = it->second;
      NSString *content =
          [NSString stringWithUTF8String:window.content.c_str()];
      [view updateContent:content];
    }
  }
}

void embedded_terminal::platform_resize_window(editor_window &window, int width,
                                               int height) {
  @autoreleasepool {
    auto it = g_platform_views.find(window.platform_handle);
    if (it != g_platform_views.end()) {
      FTXUITerminalView *view = it->second;
      NSRect newFrame = view.frame;
      newFrame.size.width = width;
      newFrame.size.height = height;
      [view setFrame:newFrame];
    }
  }
}

void embedded_terminal::platform_show_window(editor_window &window,
                                             bool visible) {
  @autoreleasepool {
    auto it = g_platform_views.find(window.platform_handle);
    if (it != g_platform_views.end()) {
      FTXUITerminalView *view = it->second;
      [view setHidden:!visible];
    }
  }
}

void embedded_terminal::platform_destroy_window(editor_window &window) {
  @autoreleasepool {
    auto it = g_platform_views.find(window.platform_handle);
    if (it != g_platform_views.end()) {
      FTXUITerminalView *view = it->second;
      [view removeFromSuperview];
      g_platform_views.erase(it);
    }
    window.platform_handle = nullptr;
  }
}

} // namespace ftxui_clap_support

#endif // __APPLE__
