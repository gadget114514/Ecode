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
#include <cstdio>
#include <unordered_map>

static void DbgLog(const char* msg) {
    if (FILE* f = fopen("D:\\ws\\Ecode\\sixel_debug.log", "a")) {
        fputs(msg, f); fputc('\n', f); fclose(f);
    }
}

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

    // Reset multipart state
    multipartActive_  = false;
    multipartOptions_.clear();
    multipartBuffer_.clear();

    // Reset DCS buffer
    dcsBuffer_.clear();
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
        // DCS accumulation (Sixel data)
        // ----------------------------------------------------------------
        if (state_ == State::DcsEntry) {
            if (ch == L'\x1b') {
                // ESC always terminates DCS (handles cross-chunk ESC\ splits too)
                handleDcs(dcsBuffer_);
                dcsBuffer_.clear();
                if (i + 1 < text.size() && text[i+1] == L'\\') {
                    ++i; // consume the ST backslash
                    state_ = State::Ground;
                } else {
                    state_ = State::Escape; // let next char be processed as ESC sequence
                }
            } else if (ch == L'\a') {
                handleDcs(dcsBuffer_);
                dcsBuffer_.clear();
                state_ = State::Ground;
            } else if (ch >= 0x20 && ch <= 0x7e) {
                dcsBuffer_.push_back((char)ch);
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
    case L'[':
        state_ = State::Csi; csiParams_.clear(); break;
    case L']':
        state_ = State::Osc; oscText_.clear();   break;
    case L'P':
        state_ = State::DcsEntry; dcsBuffer_.clear();
        DbgLog("[emu] ESC P -> DcsEntry");
        break;
    case L'(':
        state_ = State::CharsetG0;               break;
    case L')':
        state_ = State::CharsetG1;               break;
    case L'7':
        buffer_->saveCursor();                   break;
    case L'8':
        buffer_->restoreCursor();                break;
    case L'D':
        buffer_->lineFeed();                     break;
    case L'E':
        buffer_->carriageReturn(); buffer_->lineFeed(); break;
    case L'M':
        buffer_->reverseIndex();                 break;
    case L'c': // RIS – full reset
        buffer_->clearScreen();
        buffer_->resetScrollRegion();
        reset(buffer_);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// CSI dispatch
// ---------------------------------------------------------------------------
void TerminalEmulator::handleCsi(const std::wstring& raw, wchar_t fin) {
    // strip leading parameter bytes that aren't digits/semicolons
    std::wstring params;
    for (wchar_t c : raw)
        if ((c >= L'0' && c <= L'9') || c == L';' || c == L':' || c == L'?' || c == L'>' || c == L'<' || c == L'!' || c == L'$')
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
            handlePrivateModeSaveRestore(rest, fin == L's');
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
        if (prefix == L'?' && fin == L'J') {
            // DECSED: Selective Erase in Display — treat as ED (DECSCA not implemented)
            buffer_->clearScreenMode(paramInt(splitParams(rest), 0, 0), currentAttrs_);
            return;
        }
        if (prefix == L'?' && fin == L'K') {
            // DECSEL: Selective Erase in Line — treat as EL (DECSCA not implemented)
            buffer_->clearLine(paramInt(splitParams(rest), 0, 0), currentAttrs_);
            return;
        }
        // fall through for unrecognised prefixes — strip prefix so numeric params parse correctly
        params = rest;
    }

    auto parts = splitParams(params);
    auto P = [&](size_t idx, int def) { return paramInt(parts, idx, def); };

    // --- DECSACE: CSI Ps *x (before $ check — no $ intermediate) ---
    if (fin == L'x' && params.find(L'*') != std::wstring::npos) {
        std::wstring clean;
        for (wchar_t c : params)
            if (c != L'*' && c != L' ') clean += c;
        int ps = 2;
        try { ps = std::stoi(clean); } catch(...) { ps = 2; }
        buffer_->setAttributeChangeExtent(ps);
        return;
    }

    // --- rectangle operations ($ intermediate byte) ---
    if (params.find(L'$') != std::wstring::npos) {
        std::wstring clean;
        for (wchar_t c : params)
            if (c != L'$') clean += c;
        auto rparts = splitParams(clean);
        auto RP = [&](size_t idx, int def) { return paramInt(rparts, idx, def); };

        if (fin == L'v') {
            // DECCRA: CSI Pts;Pls;Pbs;Prs;Pps;Ptd;Pld;Ppd $v
            buffer_->copyRect(RP(0,1)-1, RP(1,1)-1, RP(2,1)-1, RP(3,1)-1,
                              RP(5,1)-1, RP(6,1)-1);
            return;
        }
        if (fin == L'z') {
            // DECERA: CSI Pt;Pl;Pb;Pr $z
            buffer_->eraseRect(RP(0,1)-1, RP(1,1)-1, RP(2,1)-1, RP(3,1)-1, currentAttrs_);
            return;
        }
        if (fin == L'x') {
            // DECFRA: CSI Pch;Pt;Pl;Pb;Pr $x
            buffer_->fillRect(RP(1,1)-1, RP(2,1)-1, RP(3,1)-1, RP(4,1)-1,
                              (wchar_t)RP(0, L' '), currentAttrs_);
            return;
        }
        if (fin == L'{') {
            // DECSERA: CSI Pt;Pl;Pb;Pr ${
            buffer_->eraseRect(RP(0,1)-1, RP(1,1)-1, RP(2,1)-1, RP(3,1)-1, currentAttrs_);
            return;
        }
        if (fin == L'r') {
            // DECCARA: CSI Pt;Pl;Pb;Pr;Ps1..Psn $r
            int pt = RP(0,1)-1, pl = RP(1,1)-1, pb = RP(2,1)-1, pr = RP(3,1)-1;
            std::vector<int> attrs;
            for (size_t i = 4; i < rparts.size(); i++)
                attrs.push_back(RP(i, 0));
            buffer_->changeRectAttr(pt, pl, pb, pr, attrs);
            return;
        }
        if (fin == L't') {
            // DECRARA: CSI Pt;Pl;Pb;Pr;Ps1..Psn $t
            buffer_->reverseRectAttr(RP(0,1)-1, RP(1,1)-1, RP(2,1)-1, RP(3,1)-1);
            return;
        }
        return;
    }

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
    case L'J': buffer_->clearScreenMode(P(0,0), currentAttrs_); break;
    case L'K': buffer_->clearLine(P(0,0), currentAttrs_);        break;
    case L'X': buffer_->eraseCharacters(std::max(1, P(0,1)), currentAttrs_); break;

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

    // --- cursor style (DECSCUSR: CSI Ps SP q) ---
    case L'q':
        // Intermediate byte SP (0x20) is stripped from `params` during filtering,
        // so check the original `raw` string for the space instead.
        if (raw.find(L' ') != std::wstring::npos)
            handleCursorStyle(P(0,0));
        break;

    default:
        break;
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
        case 1001: /* highlight mouse — not supported */
            break;
        case 1002: buffer_->setMouseTrackingMode(enabled ? 1002 : 0); break;
        case 1003: buffer_->setMouseTrackingMode(enabled ? 1003 : 0); break;
        case 1004: buffer_->setFocusEventReportingEnabled(enabled);   break;
        case 1005: /* UTF-8 mouse encoding — not supported */
            break;
        case 1006: buffer_->setSgrMouseEnabled(enabled);              break; // SGR mouse (terminalpp)
        case 47:
        case 1047:
        case 1049: buffer_->useAlternateScreen(enabled);              break;
        case 1048:
            if (enabled) buffer_->saveCursor();
            else         buffer_->restoreCursor();
            break;
        case 2004: buffer_->setBracketedPasteEnabled(enabled);        break;
        // Windows Terminal extensions – not implemented
        case 2026:
            break;
        case 2027:
            break;
        case 2031:
            break;
        case 9001:
            break;
    default:
        break;
    }
}
}

void TerminalEmulator::handlePrivateModeSaveRestore(const std::wstring& params, bool save) {
    auto modes = splitParams(params);
    for (auto& m : modes) {
        int value = 0;
        try { value = std::stoi(m); } catch(...) { continue; }
        if (save) {
            buffer_->savePrivateMode(value);
        } else {
            buffer_->restorePrivateMode(value);
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
        case 7:  // inverse — flag only, renderer swaps fg/bg at draw time
            isInverseMode_ = true;
            currentAttrs_.inverse = true;
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
            isInverseMode_ = false;
            currentAttrs_.inverse = false;
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
    case 2: { // window/tab title
        if (onTitle_) onTitle_(arg1);
        break;
    }

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

    case 1337: // iTerm2 proprietary escape codes
        handleOsc1337(arg1);
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// OSC 1337 — iTerm2 inline images / file transfer
// ---------------------------------------------------------------------------
void TerminalEmulator::handleOsc1337(const std::wstring& params) {
    // Dispatch by keyword (first segment before '=' or ':')
    // "File=...", "MultipartFile=...", "FilePart=...", "FileEnd"
    // "SetUserVar=...", "SetBadgeFormat=...", "SetMark", etc. (ignored for now)

    if (params.find(L"File=") == 0) {
        handleOsc1337File(params.substr(5));  // strip "File="
    } else if (params.find(L"MultipartFile=") == 0) {
        handleOsc1337MultipartStart(params.substr(14));
    } else if (params.find(L"FilePart=") == 0) {
        handleOsc1337FilePart(params.substr(9));
    } else if (params.find(L"FileEnd") == 0) {
        handleOsc1337FileEnd();
    }
    // Other OSC 1337 sub-commands (SetUserVar, SetBadgeFormat, etc.) are ignored
}

// Parse key=value;key=value format into a map
static std::unordered_map<std::wstring, std::wstring> parseKeyValuePairs(const std::wstring& s) {
    std::unordered_map<std::wstring, std::wstring> map;
    size_t start = 0;
    while (start < s.size()) {
        size_t eq = s.find(L'=', start);
        if (eq == std::wstring::npos) break;
        std::wstring key = s.substr(start, eq - start);
        size_t semicolon = s.find(L';', eq + 1);
        std::wstring value;
        if (semicolon == std::wstring::npos) {
            value = s.substr(eq + 1);
            start = s.size();
        } else {
            value = s.substr(eq + 1, semicolon - eq - 1);
            start = semicolon + 1;
        }
        map[key] = value;
    }
    return map;
}

// Parse width/height values: N, Npx, N%, auto
static void parseDimension(const std::wstring& s, int cellSize, int viewportSize,
                           int& outPixels, bool& outAuto)
{
    outPixels = 0;
    outAuto = true; // default to auto

    if (s.empty() || s == L"auto") return;

    outAuto = false;

    if (s.size() >= 3 && s.substr(s.size() - 2) == L"px") {
        // Npx
        outPixels = std::stoi(s.substr(0, s.size() - 2));
    } else if (s.back() == L'%') {
        // N%
        std::wstring numStr = s.substr(0, s.size() - 1);
        int pct = std::stoi(numStr);
        outPixels = viewportSize * pct / 100;
    } else {
        // N (character cells)
        int cells = std::stoi(s);
        outPixels = cells * cellSize;
    }
    if (outPixels < 1) outPixels = 1;
}

void TerminalEmulator::handleOsc1337File(const std::wstring& s) {
    // Format: [key=value;...]:base64data
    // Find the ':' separator that separates options from data
    size_t colon = s.find(L':');
    if (colon == std::wstring::npos) return;

    std::wstring optStr = s.substr(0, colon);
    std::wstring b64data = s.substr(colon + 1);

    auto opts = parseKeyValuePairs(optStr);

    // Determine inline flag
    bool inlineFlag = true; // default to inline for safety
    auto itInline = opts.find(L"inline");
    if (itInline != opts.end() && itInline->second == L"0") {
        inlineFlag = false;
    }

    // Decode name (base64)
    std::wstring name = L"Unnamed file";
    auto itName = opts.find(L"name");
    if (itName != opts.end()) {
        std::string decodedName = base64Decode(itName->second);
        if (!decodedName.empty()) {
            int needed = MultiByteToWideChar(CP_UTF8, 0, decodedName.c_str(), -1, nullptr, 0);
            if (needed > 0) {
                std::wstring ws(needed, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, decodedName.c_str(), -1, ws.data(), needed);
                if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
                name = ws;
            }
        }
    }

    // Preserve aspect ratio
    bool preserveAspectRatio = true;
    auto itPar = opts.find(L"preserveAspectRatio");
    if (itPar != opts.end() && itPar->second == L"0") {
        preserveAspectRatio = false;
    }

    if (inlineFlag) {
        // Parse width/height
        int widthPx = 0, heightPx = 0;
        bool widthAuto = true, heightAuto = true;

        // We need cell dimensions from the buffer for proper sizing
        if (buffer_) {
            int cellW = buffer_->cellWidth();
            int cellH = buffer_->cellHeight();
            int viewW = cellW * buffer_->columns();
            int viewH = cellH * buffer_->rows();

            auto itW = opts.find(L"width");
            if (itW != opts.end())
                parseDimension(itW->second, cellW, viewW, widthPx, widthAuto);

            auto itH = opts.find(L"height");
            if (itH != opts.end())
                parseDimension(itH->second, cellH, viewH, heightPx, heightAuto);
        }

        // Decode base64 data
        std::string rawData = base64Decode(b64data);

        if (!rawData.empty()) {
            std::vector<uint8_t> data(rawData.begin(), rawData.end());

            if (onImageData_) {
                onImageData_(data, widthPx, heightPx, preserveAspectRatio, name);
            }
        }
    } else {
        // File download
        std::string rawData = base64Decode(b64data);
        if (!rawData.empty() && onFileDownload_) {
            std::vector<uint8_t> data(rawData.begin(), rawData.end());
            onFileDownload_(data, name);
        }
    }
}

void TerminalEmulator::handleOsc1337MultipartStart(const std::wstring& opts) {
    multipartActive_  = true;
    multipartOptions_ = opts;
    multipartBuffer_.clear();
}

void TerminalEmulator::handleOsc1337FilePart(const std::wstring& b64chunk) {
    if (!multipartActive_) return;
    // Accumulate base64 text (decode at the end)
    multipartBuffer_ += base64Decode(b64chunk);
}

void TerminalEmulator::handleOsc1337FileEnd() {
    if (!multipartActive_) return;
    multipartActive_ = false;

    if (multipartBuffer_.empty()) {
        multipartOptions_.clear();
        return;
    }

    // Re-parse options (may differ from original File= options)
    auto opts = parseKeyValuePairs(multipartOptions_);

    bool inlineFlag = true;
    auto itInline = opts.find(L"inline");
    if (itInline != opts.end() && itInline->second == L"0") {
        inlineFlag = false;
    }

    std::wstring name = L"Unnamed file";
    auto itName = opts.find(L"name");
    if (itName != opts.end()) {
        std::string decodedName = base64Decode(itName->second);
        if (!decodedName.empty()) {
            int needed = MultiByteToWideChar(CP_UTF8, 0, decodedName.c_str(), -1, nullptr, 0);
            if (needed > 0) {
                std::wstring ws(needed, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, decodedName.c_str(), -1, ws.data(), needed);
                if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
                name = ws;
            }
        }
    }

    bool preserveAspectRatio = true;
    auto itPar = opts.find(L"preserveAspectRatio");
    if (itPar != opts.end() && itPar->second == L"0") {
        preserveAspectRatio = false;
    }

    if (inlineFlag) {
        int widthPx = 0, heightPx = 0;
        bool widthAuto = true, heightAuto = true;

        if (buffer_) {
            int cellW = buffer_->cellWidth();
            int cellH = buffer_->cellHeight();
            int viewW = cellW * buffer_->columns();
            int viewH = cellH * buffer_->rows();

            auto itW = opts.find(L"width");
            if (itW != opts.end())
                parseDimension(itW->second, cellW, viewW, widthPx, widthAuto);

            auto itH = opts.find(L"height");
            if (itH != opts.end())
                parseDimension(itH->second, cellH, viewH, heightPx, heightAuto);
        }

        std::vector<uint8_t> data(multipartBuffer_.begin(), multipartBuffer_.end());
        if (!data.empty() && onImageData_) {
            onImageData_(data, widthPx, heightPx, preserveAspectRatio, name);
        }
    } else {
        std::vector<uint8_t> data(multipartBuffer_.begin(), multipartBuffer_.end());
        if (!data.empty() && onFileDownload_) {
            onFileDownload_(data, name);
        }
    }

    multipartBuffer_.clear();
    multipartOptions_.clear();
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
        int r = buffer_->cursorRow();
        if (buffer_->originMode()) {
            r -= buffer_->scrollTop();
        }
        swprintf(buf, 64, L"\x1b[%d;%dR",
                 r + 1, buffer_->cursorColumnRaw() + 1);
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

// ---------------------------------------------------------------------------
// DCS — Sixel data handler
// ---------------------------------------------------------------------------
void TerminalEmulator::handleDcs(const std::string& data) {
    char dbg[64];
    snprintf(dbg, sizeof(dbg), "[emu] handleDcs: %zu bytes", data.size());
    DbgLog(dbg);
    if (data.empty()) return;

    // Pass all accumulated data (including 'q' introducer) to the decoder.
    std::vector<uint8_t> sixelData(data.begin(), data.end());
    if (onSixelData_) {
        onSixelData_(sixelData);
    } else {
        DbgLog("[emu] handleDcs: onSixelData_ not set!");
    }
}
