#pragma once
#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// TermColor — lightweight RGB color with a "default" flag
// ---------------------------------------------------------------------------
struct TermColor {
    uint8_t r = 0, g = 0, b = 0;
    bool isDefault = true;

    static TermColor fromRgb(uint8_t r, uint8_t g, uint8_t b) noexcept {
        TermColor c; c.r = r; c.g = g; c.b = b; c.isDefault = false; return c;
    }
    bool operator==(const TermColor& o) const noexcept {
        return isDefault == o.isDefault && r == o.r && g == o.g && b == o.b;
    }
    bool operator!=(const TermColor& o) const noexcept { return !(*this == o); }
};

// Default terminal palette (Windows Terminal / xterm colors)
// isDefault stays true so DrawCell can skip rendering the background for blank cells.
inline TermColor DefaultFg()    noexcept { TermColor c; c.r=204; c.g=204; c.b=204; return c; }
inline TermColor DefaultBg()    noexcept { TermColor c; c.r=12;  c.g=12;  c.b=12;  return c; }
inline TermColor DefaultDecor() noexcept { TermColor c; c.r=204; c.g=204; c.b=204; return c; }

// ---------------------------------------------------------------------------
// TerminalCell — one character cell in the terminal grid
//
// Attributes sourced from:
//   - termdock-main  : bold, dim, underline, inverse, wide, wideContinuation
//   - terminalpp     : italic, blink, strikethrough, decorColor, hyperlink
// ---------------------------------------------------------------------------
struct TerminalCell {
    // Character content
    wchar_t       ch   = L' ';   // BMP codepoint (primary render character)
    std::wstring  text = L" ";   // full grapheme cluster (surrogates / combining marks)

    // Colors
    TermColor foreground  = DefaultFg();
    TermColor background  = DefaultBg();
    TermColor decorColor  = DefaultDecor(); // underline / strikethrough color (SGR 58)

    // Text attributes (SGR)
    bool bold           = false;
    bool dim            = false;   // SGR 2
    bool italic         = false;   // SGR 3  (terminalpp)
    bool underline      = false;   // SGR 4
    bool blink          = false;   // SGR 5  (terminalpp)
    bool inverse        = false;   // SGR 7
    bool strikethrough  = false;   // SGR 9  (terminalpp)

    // Layout
    bool wide             = false;  // cell occupies 2 columns (CJK, emoji)
    bool wideContinuation = false;  // right half placeholder of a wide cell

    // OSC 8 hyperlink URL (empty = none) — from terminalpp
    std::wstring hyperlinkUrl;

    // Soft-wrap flag — set when line was wrapped by autowrap (not a hard newline)
    // Used for correct copy/paste behaviour (terminalpp)
    bool softWrapped = false;
};
