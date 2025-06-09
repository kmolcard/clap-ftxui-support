#include "embedded-terminal.h"

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <memory>
#include <unordered_map>

// Custom NSView for rendering terminal content using simple text rendering
@interface FTXUITerminalView : NSView
@property(nonatomic, strong) NSString *terminalContent;
@property(nonatomic, strong) NSFont *terminalFont;
@property(nonatomic, assign) NSSize characterSize;
@property(nonatomic, assign) NSInteger updateCount;
- (void)updateContent:(NSString *)content;
@end

@implementation FTXUITerminalView

- (instancetype)initWithFrame:(NSRect)frame
{
    if (self = [super initWithFrame:frame])
    {
        // Initialize font
        self.terminalFont = [NSFont fontWithName:@"Monaco" size:12.0];
        if (!self.terminalFont)
        {
            self.terminalFont = [NSFont systemFontOfSize:12.0];
        }

        // Calculate character size
        NSString *testChar = @"M";
        NSDictionary *attributes = @{NSFontAttributeName : self.terminalFont};
        NSSize charSize = [testChar sizeWithAttributes:attributes];
        self.characterSize = charSize;

        self.updateCount = 0;
    }
    return self;
}

- (void)updateContent:(NSString *)content
{
    self.updateCount++;
    BOOL contentChanged = ![self.terminalContent isEqualToString:content];

    NSLog(@"FTXUITerminalView updateContent #%ld called, content changed: %@",
          (long)self.updateCount, contentChanged ? @"YES" : @"NO");

    if (contentChanged || !self.terminalContent)
    {
        NSLog(@"Content: %@", content);
        self.terminalContent = content;
        [self setNeedsDisplay:YES];

        // Force an immediate display update instead of waiting for the next run loop
        [self displayIfNeeded];
    }
    else
    {
        NSLog(@"Content unchanged, skipping display update");
    }
}

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];

    NSLog(@"drawRect called, bounds: %@, terminalContent: %@", NSStringFromRect(self.bounds),
          self.terminalContent);

    // For debugging, always show some text even if terminalContent is nil
    NSString *textToRender = self.terminalContent;
    if (!textToRender || [textToRender length] == 0)
    {
        NSLog(@"No terminal content, showing test text");
        textToRender = @"TEST: FTXUI Plugin GUI\nIf you can see this, text rendering works!";
    }
    else
    {
        // Strip ANSI escape sequences for now (like [1m, [22m)
        NSError *error = nil;
        NSRegularExpression *ansiRegex =
            [NSRegularExpression regularExpressionWithPattern:@"\\[[0-9;]*m"
                                                      options:0
                                                        error:&error];
        if (!error)
        {
            textToRender =
                [ansiRegex stringByReplacingMatchesInString:textToRender
                                                    options:0
                                                      range:NSMakeRange(0, [textToRender length])
                                               withTemplate:@""];
        }
    }

    // Clear the background first
    [[NSColor darkGrayColor] setFill];
    NSRectFill(self.bounds);

    // For now, use simple text rendering as fallback
    // In a full implementation, this would render the terminal content using
    // Metal
    NSMutableAttributedString *attributedString =
        [[NSMutableAttributedString alloc] initWithString:textToRender];

    [attributedString addAttribute:NSFontAttributeName
                             value:self.terminalFont
                             range:NSMakeRange(0, textToRender.length)];
    [attributedString addAttribute:NSForegroundColorAttributeName
                             value:[NSColor whiteColor]
                             range:NSMakeRange(0, textToRender.length)];

    NSRect textRect = NSMakeRect(5, 5, self.bounds.size.width - 10, self.bounds.size.height - 10);

    NSLog(@"Drawing text in rect: %@", NSStringFromRect(textRect));
    [attributedString drawInRect:textRect];
}

@end

// Platform-specific storage for macOS windows
static std::unordered_map<void *, FTXUITerminalView *> g_platform_views;

namespace ftxui_clap_support
{

bool embedded_terminal::platform_initialize()
{
    return true; // macOS-specific initialization if needed
}

void embedded_terminal::platform_shutdown() { g_platform_views.clear(); }

bool embedded_terminal::platform_create_window(editor_window &window, void *parent_handle, int x,
                                               int y, int width, int height)
{
    @autoreleasepool
    {
        NSView *parentView = (__bridge NSView *)parent_handle;
        if (!parentView)
        {
            NSLog(@"Error: parent_handle is null in platform_create_window");
            return false;
        }

        NSRect frame = NSMakeRect(x, y, width, height);
        FTXUITerminalView *terminalView = [[FTXUITerminalView alloc] initWithFrame:frame];

        // Ensure the view is visible and has a background color for debugging
        [terminalView setHidden:NO];
        [terminalView setWantsLayer:YES];
        terminalView.layer.backgroundColor =
            [[NSColor grayColor] CGColor]; // Gray background for debugging

        NSLog(@"Creating terminal view with frame: %@", NSStringFromRect(frame));
        NSLog(@"Parent view bounds: %@", NSStringFromRect(parentView.bounds));

        [parentView addSubview:terminalView];

        window.platform_handle = (__bridge void *)terminalView;
        g_platform_views[window.platform_handle] = terminalView;

        NSLog(@"Terminal view created and added to parent view");
        return true;
    }
}

void embedded_terminal::platform_update_window(editor_window &window)
{
    @autoreleasepool
    {
        auto it = g_platform_views.find(window.platform_handle);
        if (it != g_platform_views.end())
        {
            FTXUITerminalView *view = it->second;
            NSString *content = [NSString stringWithUTF8String:window.content.c_str()];

            // Debug logging
            NSLog(@"Updating window content: %@ (length: %lu)", content, [content length]);

            [view updateContent:content];
            // updateContent now calls displayIfNeeded internally, so we don't need to call
            // setNeedsDisplay again
        }
    }
}

void embedded_terminal::platform_resize_window(editor_window &window, int width, int height)
{
    @autoreleasepool
    {
        auto it = g_platform_views.find(window.platform_handle);
        if (it != g_platform_views.end())
        {
            FTXUITerminalView *view = it->second;
            NSRect newFrame = view.frame;
            newFrame.size.width = width;
            newFrame.size.height = height;
            [view setFrame:newFrame];
        }
    }
}

void embedded_terminal::platform_show_window(editor_window &window, bool visible)
{
    @autoreleasepool
    {
        auto it = g_platform_views.find(window.platform_handle);
        if (it != g_platform_views.end())
        {
            FTXUITerminalView *view = it->second;
            [view setHidden:!visible];
        }
    }
}

void embedded_terminal::platform_destroy_window(editor_window &window)
{
    @autoreleasepool
    {
        auto it = g_platform_views.find(window.platform_handle);
        if (it != g_platform_views.end())
        {
            FTXUITerminalView *view = it->second;
            [view removeFromSuperview];
            g_platform_views.erase(it);
        }
        window.platform_handle = nullptr;
    }
}

} // namespace ftxui_clap_support

#endif // __APPLE__
