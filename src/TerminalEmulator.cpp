#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../include/TerminalEmulator.h"
#include <algorithm>
#include <cassert>
#include <cwchar>

// ---------------------------------------------------------------------------
// 16-colour ANSI palette  (Windows Terminal / xterm standard)
// ---------------------------------------------------------------------------
static const TermColor kAnsiPalette[16] = {
    // Normal (0-7)
    TermColor::fromRgb( 12,  12,  12),  // 0  Black
    TermColor::fromRgb(197,  15,  31),  // 1  Red
    TermColor::fromRgb( 19, 161,  14),  // 2  Green
    TermColor::fromRgb(193, 156,   0),  // 3  Yellow
    TermColor::fromRgb(  0,  55, 218),  // 4  Blue
    TermColor::fromRgb(136,  23, 152),  // 5  Magenta
    TermColor::fromRgb( 58, 150, 221),  // 6  Cyan
    TermColor::fromRgb(204, 204, 204),  // 7  White
    // Bright (8-15)
    TermColor::fromRgb(118, 118, 118),  // 8  Bright Black
    TermColor::fromRgb(231,  72,  86),  // 9  Bright Red
    TermColor::fromRgb( 22, 198,  12),  // 10 Bright Green
    TermColor::fromRgb(249, 241, 165),  // 11 Bright Yellow
    TermColor::fromRgb( 59, 120, 255),  // 12 Bright Blue
    TermColor::fromRgb(180,   0, 158),  // 13 Bright Magenta
    TermColor::fromRgb( 97, 214, 214),  // 14 Bright Cyan
    TermColor::fromRgb(242, 242, 242),  // 15 Bright White
};

static TermColor ansiColor(int index) {
    if (index < 0 || index > 15) index = 0;
    return kAnsiPalette[index];
}

// xterm 256-colour palette
static TermColor color256(int index) {
    if (index < 0)   index = 0;
    if (index > 255) index = 255;
    if (index < 16)  return kAnsiPalette[index];
    if (index >= 232) {
        int v = 8 + (index - 232) * 10;
        return TermColor::fromRgb((uint8_t)v, (uint8_t)v, (uint8_t)v);
    }
    int c = index - 16;
    auto comp = [](int v){ return v == 0 ? 0 : 55 + v * 40; };
    return TermColor::fromRgb(
        (uint8_t)comp(c / 36),
        (uint8_t)comp((c / 6) % 6),
        (uint8_t)comp(c % 6)
    );
}

// Base64 decode (for OSC 52 clipboard)
static std::string base64Decode(const std::wstring& in) {
    static const char* table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, bits = -8;
    for (wchar_t wc : in) {
        if (wc > 127) continue;
        const char* p = strchr(table, (char)wc);
        if (!p) continue;
        val = (val << 6) + (int)(p - table);
        bits += 6;
        if (bits >= 0) {
            out.push_back((char)((val >> bits) & 0xff));
            bits -= 8;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
std::vector<std::wstring> TerminalEmulator::splitParams(const std::wstring& s, wchar_t delim) {
    std::vector<std::wstring> parts;
    std::wstring cur;
    for (wchar_t c : s) {
        if (c == delim) { parts.push_back(cur); cur.clear(); }
        else            cur.push_back(c);
    }
    parts.push_back(cur);
    return parts;
}

int TerminalEmulator::paramInt(const std::vector<std::wstring>& p, size_t i, int def) {
    if (i >= p.size() || p[i].empty()) return def;
    try { return std::stoi(p[i]); } catch(...) { return def; }
}

// ---------------------------------------------------------------------------
// reset / attach buffer
// ---------------------------------------------------------------------------
void TerminalEmulator::reset(TerminalBuffer* buffer) {
    buffer_           = buffer;
    state_            = State::Ground;
    csiParams_.clear();
    oscText_.clear();
    currentAttrs_     = TerminalCell();
    isBold_           = false;
    isInverseMode_    = false;
    lineDrawingG0_    = false;
    activeHyperlinkUrl_.clear();
}

// ---------------------------------------------------------------------------
// main input processing
// ---------------------------------------------------------------------------
void TerminalEmulator::process(const std::wstring& text) {
    if (!buffer_) return;

    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];

        // ----------------------------------------------------------------
        // OSC accumulation
        // ----------------------------------------------------------------
        if (state_ == State::Osc) {
            if (ch == L'\a') {
                handleOsc(oscText_);
                oscText_.clear();
                state_ = State::Ground;
            } else if (ch == L'\x1b' && i + 1 < text.size() && text[i+1] == L'\\') {
                handleOsc(oscText_);
                oscText_.clear();
                state_ = State::Ground;
                ++i;
            } else {
                oscText_ += ch;
            }
            continue;
        }

        // ----------------------------------------------------------------
        // DCS accumulation (ignored, consumed until ST)
        // ----------------------------------------------------------------
        if (state_ == State::DcsEntry) {
            if (ch == L'\x1b' && i + 1 < text.size() && text[i+1] == L'\\') {
                state_ = State::Ground; ++i;
            } else if (ch == L'\a') {
                state_ = State::Ground;
            }
            continue;
        }

        // ----------------------------------------------------------------
        // CSI parameter accumulation
        // ----------------------------------------------------------------
        if (state_ == State::Csi || state_ == State::CsiParam) {
            if (ch >= 0x40 && ch <= 0x7e) {
                // final byte
                handleCsi(csiParams_, ch);
                csiParams_.clear();
                state_ = State::Ground;
            } else {
                csiParams_ += ch;
                state_ = State::CsiParam;
            }
            continue;
        }

        // ----------------------------------------------------------------
        // ESC dispatch
        // ----------------------------------------------------------------
        if (state_ == State::Escape) {
            state_ = State::Ground;
            handleEscape(ch);
            continue;
        }

        // ----------------------------------------------------------------
        // Charset designations G0/G1
        // ----------------------------------------------------------------
        if (state_ == State::CharsetG0) {
            lineDrawingG0_ = (ch == L'0');
            state_ = State::Ground;
            continue;
        }
        if (state_ == State::CharsetG1) {
            // G1 charset – not tracked
            state_ = State::Ground;
            continue;
        }

        // ----------------------------------------------------------------
        // Ground state
        // ----------------------------------------------------------------
        if (ch == L'\x1b') { state_ = State::Escape; continue; }

        // C0 controls
        switch (ch) {
        case L'\r': buffer_->carriageReturn(); continue;
        case L'\n':
        case L'\v':
        case L'\f': buffer_->lineFeed();       continue;
        case L'\b': buffer_->backspace();       continue;
        case L'\t': buffer_->tab();             continue;
        case L'\a': /* bell – ignore */         continue;
        case L'\x0f': lineDrawingG0_ = false;   continue; // SI
        case L'\x0e': lineDrawingG0_ = true;    continue; // SO
        default:    break;
        }

        // Printable BMP character
        if (ch >= 0x20 && ch != 0x7f) {
            // handle surrogate pair (UTF-16 wide chars)
            std::wstring cluster;
            cluster += ch;
            if (ch >= 0xd800 && ch <= 0xdbff && i + 1 < text.size()) {
                wchar_t next = text[i+1];
                if (next >= 0xdc00 && next <= 0xdfff) {
                    cluster += next;
                    ++i;
                }
            }

            // apply line-drawing substitution
            if (lineDrawingG0_ && cluster.size() == 1)
                cluster[0] = mapLineDrawingChar(cluster[0]);

            // stamp active hyperlink URL onto attributes
            currentAttrs_.hyperlinkUrl = activeHyperlinkUrl_;

            buffer_->putText(cluster, currentAttrs_);
        }
    }
}

// ---------------------------------------------------------------------------
// ESC X handling
// ---------------------------------------------------------------------------
void TerminalEmulator::handleEscape(wchar_t ch) {
    switch (ch) {
    case L'[': state_ = State::Csi; csiParams_.clear(); break;
    case L']': state_ = State::Osc; oscText_.clear();   break;
    case L'P': state_ = State::DcsEntry;                break;
    case L'(': state_ = State::CharsetG0;               break;
    case L')': state_ = State::CharsetG1;               break;
    case L'7': buffer_->saveCursor();                   break;
    case L'8': buffer_->restoreCursor();                break;
    case L'D': buffer_->lineFeed();                     break;
    case L'E': buffer_->carriageReturn(); buffer_->lineFeed(); break;
    case L'M': buffer_->reverseIndex();                 break;
    case L'c': // RIS – full reset
        buffer_->clearScreen();
        buffer_->resetScrollRegion();
        reset(buffer_);
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// CSI dispatch
// ---------------------------------------------------------------------------
void TerminalEmulator::handleCsi(const std::wstring& raw, wchar_t fin) {
    // strip leading parameter bytes that aren't digits/semicolons
    std::wstring params;
    for (wchar_t c : raw)
        if ((c >= L'0' && c <= L'9') || c == L';' || c == L':' || c == L'?' || c == L'>' || c == L'<' || c == L'!')
            params += c;

    // --- private / extended first-byte prefixes ---
    if (!params.empty() && (params[0] == L'?' || params[0] == L'>')) {
        wchar_t prefix = params[0];
        std::wstring rest = params.substr(1);
        if (prefix == L'?' && (fin == L'h' || fin == L'l')) {
            handlePrivateMode(rest, fin == L'h');
            return;
        }
        if (prefix == L'?' && (fin == L's' || fin == L'r')) {
            // save/restore private modes — ignored
            return;
        }
        if (prefix == L'>' && fin == L'c') {
            emitResponse(L"\x1b[>0;10;1c"); // secondary DA — pretend VT100
            return;
        }
        if (prefix == L'>' && fin == L'm') {
            handleKeyModifierOptions(rest);
            return;
        }
        if (prefix == L'?' && fin == L'm') {
            queryKeyModifierOptions(rest);
            return;
        }
        if (prefix == L'?' && fin == L'u') {
            queryKittyKeyboardProtocol();
            return;
        }
        // fall through for unrecognised prefixes
    }

    auto parts = splitParams(params);
    auto P = [&](size_t idx, int def) { return paramInt(parts, idx, def); };

    switch (fin) {
    // --- cursor movement ---
    case L'A': buffer_->moveCursorRelative(-std::max(1, P(0,1)), 0);  break;
    case L'B': buffer_->moveCursorRelative( std::max(1, P(0,1)), 0);  break;
    case L'C': buffer_->moveCursorRelative(0,  std::max(1, P(0,1))); break;
    case L'D': buffer_->moveCursorRelative(0, -std::max(1, P(0,1))); break;
    case L'E': buffer_->moveCursorNextLine(std::max(1, P(0,1)));     break;
    case L'F': buffer_->moveCursorPreviousLine(std::max(1, P(0,1)));  break;
    case L'G': buffer_->moveCursorColumn(P(0,1) - 1);                 break;
    case L'H': case L'f':
        buffer_->moveCursorTo(P(0,1) - 1, P(1,1) - 1); break;
    case L'd': buffer_->moveCursorRow(P(0,1) - 1);                    break;
    case L'`': buffer_->moveCursorColumn(P(0,1) - 1);                 break;
    case L'a': buffer_->moveCursorRelative(0, std::max(1, P(0,1)));   break;

    // --- erase ---
    case L'J': buffer_->clearScreenMode(P(0,0)); break;
    case L'K': buffer_->clearLine(P(0,0));        break;
    case L'X': buffer_->eraseCharacters(std::max(1, P(0,1))); break;

    // --- insert / delete ---
    case L'@': buffer_->insertCharacters(std::max(1, P(0,1))); break;
    case L'P': buffer_->deleteCharacters(std::max(1, P(0,1))); break;
    case L'L': buffer_->insertLines(std::max(1, P(0,1)));      break;
    case L'M': buffer_->deleteLines(std::max(1, P(0,1)));      break;

    // --- scroll ---
    case L'S': buffer_->scrollUpLines(std::max(1, P(0,1)));   break;
    case L'T': buffer_->scrollDownLines(std::max(1, P(0,1))); break;

    // --- repeat previous char (REP) — terminalpp ---
    case L'b': buffer_->repeatPreviousChar(std::max(1, P(0,1))); break;

    // --- scroll region ---
    case L'r': buffer_->setScrollRegion(P(0,1) - 1, P(1, buffer_->rows()) - 1); break;

    // --- cursor save/restore ---
    case L's': buffer_->saveCursor();    break;
    case L'u':
        if (!params.empty() && params[0] == L'?') { queryKittyKeyboardProtocol(); break; }
        if (params.empty()) { buffer_->restoreCursor(); break; }
        handleKittyKeyboardProtocol(params); break;

    // --- SGR ---
    case L'm': {
        auto sgr = splitParams(params);
        handleSgr(sgr);
        break;
    }

    // --- device attributes ---
    case L'c':
        if (P(0,0) == 0)
            emitResponse(L"\x1b[?1;2c"); // primary DA — VT100 with AVO
        break;

    // --- DSR ---
    case L'n': sendDeviceStatusReport(P(0,0)); break;

    // --- window operations ---
    case L't': sendWindowReport(P(0,0)); break;

    // --- cursor style (DECSCUSR) ---
    case L'q':
        if (!params.empty() && (params.back() == L' ' || params.front() == L' '))
            handleCursorStyle(P(0,0));
        break;

    default: break;
    }
}

// ---------------------------------------------------------------------------
// private mode (?h / ?l)
// ---------------------------------------------------------------------------
void TerminalEmulator::handlePrivateMode(const std::wstring& params, bool enabled) {
    auto modes = splitParams(params);
    for (auto& m : modes) {
        int value = 0;
        try { value = std::stoi(m); } catch(...) { continue; }
        switch (value) {
        case 1:    buffer_->setApplicationCursorMode(enabled);       break; // DECCKM (terminalpp)
        case 6:    buffer_->setOriginMode(enabled);                   break;
        case 7:    buffer_->setAutoWrapEnabled(enabled);              break;
        case 12:   buffer_->setCursorBlink(enabled);                  break;
        case 25:   buffer_->setCursorVisible(enabled);                break;
        case 1000: buffer_->setMouseTrackingMode(enabled ? 1000 : 0); break;
        case 1001: /* highlight mouse — not supported */              break;
        case 1002: buffer_->setMouseTrackingMode(enabled ? 1002 : 0); break;
        case 1003: buffer_->setMouseTrackingMode(enabled ? 1003 : 0); break;
        case 1004: buffer_->setFocusEventReportingEnabled(enabled);   break;
        case 1005: /* UTF-8 mouse encoding — not supported */         break;
        case 1006: buffer_->setSgrMouseEnabled(enabled);              break; // SGR mouse (terminalpp)
        case 47:
        case 1047:
        case 1049: buffer_->useAlternateScreen(enabled);              break;
        case 2004: buffer_->setBracketedPasteEnabled(enabled);        break;
        default:   break;
        }
    }
}

// ---------------------------------------------------------------------------
// SGR (Select Graphic Rendition)
// Combines termdock + terminalpp coverage
// ---------------------------------------------------------------------------
void TerminalEmulator::handleSgr(const std::vector<std::wstring>& parts) {
    const auto& p = parts.empty() ? std::vector<std::wstring>{L"0"} : parts;
    for (size_t i = 0; i < p.size(); ++i) {
        int v = 0;
        try { v = p[i].empty() ? 0 : std::stoi(p[i]); } catch(...) {}
        switch (v) {
        case 0:  // reset
            currentAttrs_  = TerminalCell();
            isBold_        = false;
            isInverseMode_ = false;
            break;
        case 1:  // bold
            isBold_             = true;
            currentAttrs_.bold  = true;
            break;
        case 2:  // dim / faint
            currentAttrs_.dim   = true;
            break;
        case 3:  // italic (terminalpp)
            currentAttrs_.italic = true;
            break;
        case 4:  // underline
            currentAttrs_.underline = true;
            break;
        case 5:  // blink (terminalpp)
            currentAttrs_.blink = true;
            break;
        case 7:  // inverse
            if (!isInverseMode_) {
                isInverseMode_ = true;
                currentAttrs_.inverse = true;
                std::swap(currentAttrs_.foreground, currentAttrs_.background);
            }
            break;
        case 9:  // strikethrough (terminalpp)
            currentAttrs_.strikethrough = true;
            break;
        case 21: // bold off
            isBold_             = false;
            currentAttrs_.bold  = false;
            break;
        case 22: // normal intensity
            isBold_             = false;
            currentAttrs_.bold  = false;
            currentAttrs_.dim   = false;
            currentAttrs_.italic= false;
            break;
        case 23: // italic off (terminalpp)
            currentAttrs_.italic = false;
            break;
        case 24: // underline off
            currentAttrs_.underline = false;
            break;
        case 25: // blink off (terminalpp)
            currentAttrs_.blink = false;
            break;
        case 27: // inverse off
            if (isInverseMode_) {
                isInverseMode_ = false;
                currentAttrs_.inverse = false;
                std::swap(currentAttrs_.foreground, currentAttrs_.background);
            }
            break;
        case 29: // strikethrough off (terminalpp)
            currentAttrs_.strikethrough = false;
            break;
        case 38: { // extended foreground
            TermColor c = parseSgrExtendedColor(p, i);
            currentAttrs_.foreground = c;
            currentAttrs_.decorColor = c;
            break;
        }
        case 39: // default foreground
            currentAttrs_.foreground = DefaultFg();
            currentAttrs_.decorColor = DefaultDecor();
            break;
        case 48: { // extended background
            TermColor c = parseSgrExtendedColor(p, i);
            currentAttrs_.background = c;
            break;
        }
        case 49: // default background
            currentAttrs_.background = DefaultBg();
            break;
        case 58: { // underline/decoration colour (terminalpp / kitty)
            TermColor c = parseSgrExtendedColor(p, i);
            currentAttrs_.decorColor = c;
            break;
        }
        case 59: // reset decoration colour
            currentAttrs_.decorColor = DefaultDecor();
            break;
        default:
            if (v >= 30 && v <= 37) {
                int idx = v - 30;
                if (boldIsBright_ && isBold_) idx += 8;
                currentAttrs_.foreground = ansiColor(idx);
                currentAttrs_.decorColor = currentAttrs_.foreground;
            } else if (v >= 40 && v <= 47) {
                currentAttrs_.background = ansiColor(v - 40);
            } else if (v >= 90 && v <= 97) {
                currentAttrs_.foreground = ansiColor(v - 82); // 90-97 → 8-15
                currentAttrs_.decorColor = currentAttrs_.foreground;
            } else if (v >= 100 && v <= 107) {
                currentAttrs_.background = ansiColor(v - 92);
            }
            break;
        }
    }
}

// Extended colour parsing (38;5;n or 38;2;r;g;b)
TermColor TerminalEmulator::parseSgrExtendedColor(const std::vector<std::wstring>& parts, size_t& i) {
    ++i;
    if (i >= parts.size()) return DefaultFg();
    int mode = 0;
    try { mode = std::stoi(parts[i]); } catch(...) { return DefaultFg(); }
    if (mode == 5) { // 256-colour
        ++i;
        if (i >= parts.size()) return DefaultFg();
        int idx = 0;
        try { idx = std::stoi(parts[i]); } catch(...) {}
        return color256(idx);
    }
    if (mode == 2) { // truecolor
        if (i + 3 >= parts.size()) { i += 3; return DefaultFg(); }
        int r = 0, g = 0, b = 0;
        try { r = std::stoi(parts[i+1]); } catch(...) {}
        try { g = std::stoi(parts[i+2]); } catch(...) {}
        try { b = std::stoi(parts[i+3]); } catch(...) {}
        i += 3;
        return TermColor::fromRgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
    }
    return DefaultFg();
}

// ---------------------------------------------------------------------------
// OSC (Operating System Command)
// ---------------------------------------------------------------------------
void TerminalEmulator::handleOsc(const std::wstring& text) {
    const size_t sep = text.find(L';');
    if (sep == std::wstring::npos) return;

    int num = 0;
    try { num = std::stoi(text.substr(0, sep)); } catch(...) { return; }
    std::wstring arg1 = text.substr(sep + 1);

    switch (num) {
    case 0:
    case 2: // window/tab title
        if (onTitle_) onTitle_(arg1);
        break;

    case 8: { // OSC 8 hyperlink (terminalpp)
        // format: params ; url  (params ignored)
        const size_t sep2 = arg1.find(L';');
        if (sep2 == std::wstring::npos) break;
        std::wstring url = arg1.substr(sep2 + 1);
        if (url.empty())
            activeHyperlinkUrl_.clear();   // close hyperlink
        else
            activeHyperlinkUrl_ = url;     // open hyperlink
        break;
    }

    case 52: { // OSC 52 clipboard write (terminalpp)
        const size_t sep2 = arg1.find(L';');
        if (sep2 == std::wstring::npos) break;
        std::wstring b64 = arg1.substr(sep2 + 1);
        std::string decoded = base64Decode(b64);
        if (onClipboard_ && !decoded.empty()) {
            // convert UTF-8 to wstring
            int needed = MultiByteToWideChar(CP_UTF8, 0, decoded.c_str(), -1, nullptr, 0);
            std::wstring ws(needed, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, decoded.c_str(), -1, ws.data(), needed);
            if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
            onClipboard_(ws);
        }
        break;
    }

    case 112: // reset cursor colour
        buffer_->setCursorBlink(true);
        break;

    default: break;
    }
}

// ---------------------------------------------------------------------------
// Cursor style (DECSCUSR — CSI Ps SP q)
// ---------------------------------------------------------------------------
void TerminalEmulator::handleCursorStyle(int value) {
    switch (value) {
    case 0: case 1: buffer_->setCursorShape(TerminalBuffer::CursorShape::Block);     buffer_->setCursorBlink(true);  break;
    case 2:         buffer_->setCursorShape(TerminalBuffer::CursorShape::Block);     buffer_->setCursorBlink(false); break;
    case 3:         buffer_->setCursorShape(TerminalBuffer::CursorShape::Underline); buffer_->setCursorBlink(true);  break;
    case 4:         buffer_->setCursorShape(TerminalBuffer::CursorShape::Underline); buffer_->setCursorBlink(false); break;
    case 5:         buffer_->setCursorShape(TerminalBuffer::CursorShape::Bar);       buffer_->setCursorBlink(true);  break;
    case 6:         buffer_->setCursorShape(TerminalBuffer::CursorShape::Bar);       buffer_->setCursorBlink(false); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Device Status Reports
// ---------------------------------------------------------------------------
void TerminalEmulator::sendDeviceStatusReport(int value) {
    if (!buffer_) return;
    if (value == 5) {
        emitResponse(L"\x1b[0n");
    } else if (value == 6) {
        wchar_t buf[64];
        swprintf(buf, 64, L"\x1b[%d;%dR",
                 buffer_->cursorRow() + 1, buffer_->cursorColumnRaw() + 1);
        emitResponse(buf);
    }
}

void TerminalEmulator::sendWindowReport(int value) {
    if (!buffer_) return;
    wchar_t buf[64];
    switch (value) {
    case 14:
        swprintf(buf, 64, L"\x1b[4;%d;%dt",
                 buffer_->cellHeight() * buffer_->rows(),
                 buffer_->cellWidth()  * buffer_->columns());
        emitResponse(buf); break;
    case 16:
        swprintf(buf, 64, L"\x1b[6;%d;%dt", buffer_->cellHeight(), buffer_->cellWidth());
        emitResponse(buf); break;
    case 18:
        swprintf(buf, 64, L"\x1b[8;%d;%dt", buffer_->rows(), buffer_->columns());
        emitResponse(buf); break;
    case 19:
        swprintf(buf, 64, L"\x1b[9;%d;%dt",
                 buffer_->cellHeight() * buffer_->rows(),
                 buffer_->cellWidth()  * buffer_->columns());
        emitResponse(buf); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// modifyOtherKeys / Kitty keyboard
// ---------------------------------------------------------------------------
void TerminalEmulator::handleKeyModifierOptions(const std::wstring& params) {
    auto parts = splitParams(params);
    int resource = paramInt(parts, 0, 0);
    int value    = paramInt(parts, 1, 0);
    if (resource == 4) buffer_->setModifyOtherKeysMode(value);
}

void TerminalEmulator::queryKeyModifierOptions(const std::wstring& params) {
    auto parts = splitParams(params);
    int resource = paramInt(parts, 0, 0);
    if (resource == 4) {
        wchar_t buf[32];
        swprintf(buf, 32, L"\x1b[>4;%dm", buffer_->modifyOtherKeysMode());
        emitResponse(buf);
    }
}

void TerminalEmulator::handleKittyKeyboardProtocol(const std::wstring& params) {
    auto parts = splitParams(params);
    // strip leading '?' if present
    std::wstring first = parts.empty() ? L"" : parts[0];
    if (!first.empty() && first[0] == L'?') first = first.substr(1);
    int flags = 0;
    try { flags = std::stoi(first); } catch(...) {}
    buffer_->setKittyKeyboardFlags(flags);
}

void TerminalEmulator::queryKittyKeyboardProtocol() {
    emitResponse(L"\x1b[>1;0u");
}

// ---------------------------------------------------------------------------
// Line-drawing character set (ESC ( 0)  — termdock + terminalpp
// ---------------------------------------------------------------------------
wchar_t TerminalEmulator::mapLineDrawingChar(wchar_t ch) const {
    switch (ch) {
    case L'j': return L'┘'; // BOX DRAWINGS LIGHT UP AND LEFT
    case L'k': return L'┐'; // BOX DRAWINGS LIGHT DOWN AND LEFT
    case L'l': return L'┌'; // BOX DRAWINGS LIGHT DOWN AND RIGHT
    case L'm': return L'└'; // BOX DRAWINGS LIGHT UP AND RIGHT
    case L'n': return L'┼'; // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
    case L'q': return L'─'; // BOX DRAWINGS LIGHT HORIZONTAL
    case L't': return L'├'; // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
    case L'u': return L'┤'; // BOX DRAWINGS LIGHT VERTICAL AND LEFT
    case L'v': return L'┴'; // BOX DRAWINGS LIGHT UP AND HORIZONTAL
    case L'w': return L'┬'; // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
    case L'x': return L'│'; // BOX DRAWINGS LIGHT VERTICAL
    case L'a': return L'▒'; // MEDIUM SHADE (checkerboard)
    case L'`': return L'◆'; // BLACK DIAMOND
    case L'f': return L'°'; // DEGREE SIGN
    case L'g': return L'±'; // PLUS-MINUS SIGN
    case L'~': return L'·'; // MIDDLE DOT
    default:   return ch;
    }
}
