#pragma once

#include "TerminalBuffer.h"
#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// TerminalEmulator
//
// Parses raw VT/ANSI byte streams from a ConPTY and mutates a TerminalBuffer.
//
// Feature sources:
//   termdock-main  — CSI/OSC/SGR skeleton, cursor/scroll/erase ops
//   terminalpp     — application cursor mode (?1), italic (SGR 3),
//                    blink (SGR 5), strikethrough (SGR 9), REP (CSI b),
//                    SGR mouse encoding (?1006), OSC 8 hyperlinks,
//                    OSC 52 clipboard, bold-is-bright, decor color (SGR 58)
// ---------------------------------------------------------------------------
class TerminalEmulator {
public:
    TerminalEmulator() = default;

    // Attach a buffer.  Call before process().
    void reset(TerminalBuffer* buffer);

    // Feed decoded UTF-16 output from the PTY.
    void process(const std::wstring& text);

    // --- callbacks (replace Qt signals) ---

    // Terminal → PTY response (device reports, hyperlink queries, etc.)
    void setResponseCallback(std::function<void(const std::wstring&)> cb) {
        onResponse_ = std::move(cb);
    }
    // Window / tab title changed (OSC 0/2)
    void setTitleCallback(std::function<void(const std::wstring&)> cb) {
        onTitle_ = std::move(cb);
    }
    // Clipboard write request (OSC 52, base64-decoded text)
    void setClipboardCallback(std::function<void(const std::wstring&)> cb) {
        onClipboard_ = std::move(cb);
    }
    // Hyperlink open (OSC 8 URL, left-click)
    void setHyperlinkOpenCallback(std::function<void(const std::wstring&)> cb) {
        onHyperlinkOpen_ = std::move(cb);
    }
    // Whether bold colours should use the bright palette variants (terminalpp)
    void setBoldIsBright(bool v) { boldIsBright_ = v; }

    // --- OSC 1337 callbacks ---

    // Raw decoded image data for inline display
    using ImageDataCallback = std::function<void(
        const std::vector<uint8_t>& data,
        int width, int height,
        bool preserveAspectRatio,
        const std::wstring& name)>;
    void setImageDataCallback(ImageDataCallback cb) {
        onImageData_ = std::move(cb);
    }

    // File download request (inline=0)
    using FileDownloadCallback = std::function<void(
        const std::vector<uint8_t>& data,
        const std::wstring& name)>;
    void setFileDownloadCallback(FileDownloadCallback cb) {
        onFileDownload_ = std::move(cb);
    }

    // --- Sixel DCS callback ---
    using SixelDataCallback = std::function<void(const std::vector<uint8_t>& data)>;
    void setSixelDataCallback(SixelDataCallback cb) {
        onSixelData_ = std::move(cb);
    }

private:
    // --- parsing state ---
    enum class State {
        Ground,
        Escape,       // received ESC
        Csi,          // ESC [
        CsiParam,
        Osc,          // ESC ]
        DcsEntry,     // ESC P  (accumulated for sixel)
        CharsetG0,    // ESC (
        CharsetG1,    // ESC )
    };

    void handleEscape(wchar_t ch);
    void handleCsi(const std::wstring& params, wchar_t finalByte);
    void handleOsc(const std::wstring& text);
    void handlePrivateMode(const std::wstring& params, bool enabled);
    void handlePrivateModeSaveRestore(const std::wstring& params, bool save);
    void handleSgr(const std::vector<std::wstring>& parts);
    TermColor parseSgrExtendedColor(const std::vector<std::wstring>& parts, size_t& i);
    void handleCursorStyle(int value);
    void sendDeviceStatusReport(int value);
    void sendWindowReport(int value);
    void handleKeyModifierOptions(const std::wstring& params);
    void queryKeyModifierOptions(const std::wstring& params);
    void handleKittyKeyboardProtocol(const std::wstring& params);
    void queryKittyKeyboardProtocol();
    wchar_t mapLineDrawingChar(wchar_t ch) const;

    // OSC 1337 handlers
    void handleOsc1337(const std::wstring& params);
    void handleOsc1337File(const std::wstring& s);
    void handleOsc1337MultipartStart(const std::wstring& opts);
    void handleOsc1337FilePart(const std::wstring& b64chunk);
    void handleOsc1337FileEnd();

    // helper: split wstring by delimiter
    static std::vector<std::wstring> splitParams(const std::wstring& s, wchar_t delim = L';');
    static int paramInt(const std::vector<std::wstring>& p, size_t i, int def = 0);

    void emitResponse(const std::wstring& s) { if (onResponse_) onResponse_(s); }

    // DCS (Sixel) handler
    void handleDcs(const std::string& data);

    // --- state ---
    TerminalBuffer* buffer_ = nullptr;
    State state_            = State::Ground;
    std::wstring csiParams_;
    std::wstring oscText_;
    wchar_t      csiFinalByte_ = 0;

    // current SGR attributes (applied to each new cell)
    TerminalCell currentAttrs_;

    // boldIsBright: bold + 30-37 fg → use bright palette (terminalpp)
    bool boldIsBright_     = true;
    bool isBold_           = false;     // tracks bold separately for boldIsBright
    bool isInverseMode_    = false;     // tracks inverse separately to swap correctly

    // line drawing charset G0
    bool lineDrawingG0_ = false;

    // active hyperlink URL (OSC 8)
    std::wstring activeHyperlinkUrl_;

    // DCS accumulation buffer
    std::string dcsBuffer_;

    // callbacks
    std::function<void(const std::wstring&)> onResponse_;
    std::function<void(const std::wstring&)> onTitle_;
    std::function<void(const std::wstring&)> onClipboard_;
    std::function<void(const std::wstring&)> onHyperlinkOpen_;
    ImageDataCallback    onImageData_;
    FileDownloadCallback onFileDownload_;
    SixelDataCallback    onSixelData_;

    // --- OSC 1337 multipart state ---
    bool         multipartActive_ = false;
    std::wstring multipartOptions_;   // options from MultipartFile=
    std::string  multipartBuffer_;    // accumulated base64 data
};
