# Terminal Backspace / Line-Editing Fix — Design Document

## 1. Overview

Two intermittent bugs are reported in the integrated Terminal plugin
(`Application/Terminal`):

1. **Backspace sometimes does not work** — a character deletion at the shell
   prompt has no visible effect, or erases the wrong cell.
2. **Broken line editing** — while editing a command line, stray text is
   displayed *past the end of the line* (e.g. leftover fragments such as
   `2K`, `0m`, or the tail of the old line surviving after a redraw).

Both symptoms are intermittent because they only trigger when the output
stream contains a malformed/interrupted escape sequence, or when the edited
line contains characters whose display width the emulator computes
differently from ConPTY (Japanese ambiguous-width characters, non-BMP
characters).

This document records the root-cause analysis and the fix design. It is the
implementation guide for a follow-up change; no code is modified by this
document itself.

---

## 2. Background

### Data flow

```
child process (cmd / powershell / claude ...)
  └─► ConPTY (conhost re-renders into a clean VT stream, UTF-8)
        └─► ConPtySession reader thread (4096-byte ReadFile chunks)
              └─► TerminalView::OnTerminalOutput()          TerminalView.cpp:602
                    - joins split multibyte sequences (pendingPartial_)
                    - decodes to UTF-16
                    └─► TerminalEmulator::process(ws)       TerminalEmulator.cpp:129
                          - per-character state machine
                          - Ground / Escape / Csi / CsiParam / Osc / DcsEntry /
                            CharsetG0 / CharsetG1
                          └─► TerminalBuffer                 TerminalBuffer.cpp
                                - cell grid, cursor, scroll region,
                                  deferred wrap (pendingWrap_)
```

Keyboard input goes the other way: `TerminalView::EncodeKey()`
(TerminalView.cpp:1758) translates `WM_KEYDOWN` into VT sequences and writes
them to the PTY. Backspace is sent as `0x7F` (DEL), which ConPTY correctly
maps to `VK_BACK` — **the input side is not the cause of the main bug**, but
has two small defects listed in §3.6.

### The VT parser rules being violated

The reference parser (DEC STD-070 / vt100.net "A parser for DEC's ANSI-
compatible video terminals", implemented by xterm and Windows Terminal)
mandates, for the *escape-sequence-in-progress* states:

- **ESC (0x1B) received inside a CSI/OSC/DCS sequence** aborts the current
  sequence and begins a new escape sequence.
- **C0 controls other than ESC/CAN/SUB** (BS, CR, LF, TAB, BEL …) received
  inside a CSI sequence are **executed immediately** and the sequence
  continues to accumulate afterwards.
- **CAN (0x18) and SUB (0x1A)** abort the sequence and return to Ground.
- Escape intermediates (`ESC #`, `ESC %`, `ESC SP`) consume exactly one
  following final byte.

`TerminalEmulator::process()` violates all four rules; the consequences map
directly onto the reported symptoms.

---

## 3. Root-cause findings (ranked)

### 3.1 F1 — ESC inside CSI is consumed as a parameter byte  ★ primary

**Location:** `TerminalEmulator.cpp:186-197` (Csi / CsiParam states)

```cpp
if (state_ == State::Csi || state_ == State::CsiParam) {
    if (ch >= 0x40 && ch <= 0x7e) {   // final byte
        handleCsi(csiParams_, ch);
        ...
    } else {
        csiParams_ += ch;             // ← ESC (0x1B) lands here!
        state_ = State::CsiParam;
    }
    continue;
}
```

Any byte outside `0x40–0x7E` — including ESC itself — is appended to
`csiParams_`. If a CSI sequence is ever truncated (child killed mid-write,
application bug, interleaved writer), the stream recovers *two sequences
late* and prints garbage:

```
input:   ESC [ 1     ESC [ 2 K        (first CSI truncated)
parse:   ESC[  → Csi
         '1'   → param
         ESC   → appended to params (should ABORT)
         '['   → 0x5B is in 0x40–0x7E → treated as FINAL byte of old CSI
                 → handleCsi("1\x1b", '[') → unsupported, dropped
         '2'   → Ground → printed as literal text "2"
         'K'   → Ground → printed as literal text "K"
```

Net effect: the erase (`ESC[2K`) is **lost** (line not cleared → old text
survives past the new end of line) *and* the characters `2K` are **printed
at the cursor** — exactly the reported "strings displayed following after
the end of line".

### 3.2 F2 — C0 controls inside CSI/OSC/DCS are swallowed

**Location:** same accumulation branches (`TerminalEmulator.cpp:138-197`)

A `\b` (BS), `\r`, `\n`, or `\t` arriving between `ESC[` and the final byte
is appended to `csiParams_` and later silently discarded by the character
filter at the top of `handleCsi()` (`TerminalEmulator.cpp:309-313`). Per
spec these controls must be *executed* (cursor moves) while the sequence
continues. A swallowed `\b` is a directly lost backspace. CAN/SUB likewise
do not abort the sequence — they are absorbed as parameter noise.

The OSC state (`:138`) additionally absorbs *all* C0 controls into the OSC
text, so a lost terminator (BEL/ST) makes the terminal appear to eat all
subsequent output until the next ESC arrives.

### 3.3 F3 — Escape intermediates leak their final byte as printed text

**Location:** `TerminalEmulator.cpp:269-303` (`handleEscape`)

`handleEscape()` dispatches on a single character. Sequences of the form
`ESC <intermediate> <final>` — e.g. `ESC # 8` (DECALN), `ESC % G`
(select UTF-8), `ESC SP F` — hit the `default:` branch on the intermediate
and return to Ground; the *final byte* is then printed as a literal
character (`8`, `G`, `F`) at the cursor.

### 3.4 F4 — characterWidth() diverges from ConPTY  ★ co-primary

**Location:** `TerminalBuffer.cpp:650-702`

ConPTY computes repaint coordinates using conhost's notion of character
width. Whenever `TerminalBuffer::characterWidth()` disagrees, the local
cursor drifts from where conhost believes it is, so a line-edit repaint
lands shifted: the tail of the old line survives past the true end of line,
and a backspace repaint erases the wrong cells. Two divergences:

1. **All UTF-16 surrogates are width 2** (`0xd800–0xdfff` in the wide
   table): every non-BMP character — including narrow ones such as
   mathematical alphanumerics (𝐀), gothic letters, many symbols — occupies
   two cells locally while conhost treats most of them as width 1.
2. **East-Asian *ambiguous* characters are width 1** (■ ● ★ ①, Greek,
   Cyrillic, some box/arrow symbols < U+2E80): on a Japanese system
   (CP932), conhost treats these as fullwidth (2 cells). Editing a line
   containing them desynchronizes every subsequent column on the row.

This finding explains why the bug appears "sometimes": only lines
containing such characters break.

### 3.5 Parser robustness / correctness (minor)

| # | Issue | Location |
|---|-------|----------|
| M1 | DECSACE dead code: the param filter strips `*`, so `params.find(L'*')` at line 362 never matches | `TerminalEmulator.cpp:309-313`, `:362` |
| M2 | No length cap on `oscText_`, `dcsBuffer_`, `csiParams_` — a lost terminator swallows output forever and grows memory unboundedly | `TerminalEmulator.cpp:138-197` |
| M3 | Colon-form SGR sub-parameters (`38:2::r:g:b`) misparsed — `stoi` reads only `38` | `TerminalEmulator.cpp:648-751` |
| M4 | Combining character (width 0) arriving while `pendingWrap_` is set attaches to column N−2 instead of the just-printed char at N−1 | `TerminalBuffer.cpp:159-165` |

### 3.6 F5 — backspace skips wide-continuation cells  ★ kanji over-deletion

*(Added after the §4.1/§4.2 implementation landed in commit `61206ad`.)*

**Symptom:** Backspacing over **kanji** (fullwidth chars) deletes 1 character
on the first press but 2 characters on the second press (3 total). ASCII is
unaffected.

**Location:** `TerminalBuffer.cpp:250-256` (`TerminalBuffer::backspace()`)

```cpp
void TerminalBuffer::backspace() {
    pendingWrap_ = false;
    if (cursorColumn_ <= 0) return;
    --cursorColumn_;
    if (cursorColumn_ > 0 && screen_[cursorRow_][cursorColumn_].wideContinuation)
        --cursorColumn_;                    // ← BUG: extra decrement
}
```

Per the VT spec, BS moves the cursor **exactly one column** left. ConPTY /
conhost already accounts for fullwidth characters occupying two cells and
emits `\b\b` itself to cross one. The extra "skip the continuation cell"
decrement makes the local cursor drift one column further left than conhost's
model each time a `\b` lands on a continuation cell. The subsequent
erase/repaint then lands one full character too far left, eating the
neighbouring kanji. No continuation cells exist for ASCII text, which is why
only fullwidth characters are affected.

**Fix:** BS must move exactly one column (stopping at the left margin when
DECLRMM margins are active):

```cpp
void TerminalBuffer::backspace() {
    pendingWrap_ = false;
    const int leftStop = (leftRightMarginEnabled_ && cursorColumn_ > leftMargin_)
                         ? leftMargin_ : 0;
    if (cursorColumn_ > leftStop) --cursorColumn_;
}
```

(`moveCursorRelative`/CUB have no such skip and are already 1-column-exact —
verified; no other movement op needs changing.)

**Tests** (`Application/Terminal/tests/test_backspace_wide.cpp`, new):

| Case | Input | Expected |
|------|-------|----------|
| Buffer-level wide BS | put `あい` (cursor col 4), `backspace()` ×2 | cursor at col **2** (bug: col 1) |
| Conhost erase pattern | `process(L"あい")` then `process(L"\b\b  \b\b")` | cols 0-1 keep `あ`, cols 2-3 blank, cursor col 2 |
| ASCII guard | `abc` + `\b \b` | exactly one char erased, cursor col 2 |

### 3.7 Input-side defects (minor)

| # | Issue | Location |
|---|-------|----------|
| I1 | Ctrl+Space: `return "\x00";` constructs an **empty** `std::string` from a `const char*` — nothing is sent. Must use `std::string(1, '\0')` | `TerminalView.cpp:1788` |
| I2 | Ctrl+Backspace sends the same `\x7f` as plain Backspace; conventional terminals send `\x08` for Ctrl+Backspace (word delete in many shells) | `TerminalView.cpp:1843` |

---

## 4. Fix design

### 4.1 Parser state machine hardening (F1, F2, F3, M2)

All changes are confined to `TerminalEmulator::process()` /
`handleEscape()` in `Application/Terminal/src/TerminalEmulator.cpp`.

**a) ESC aborts a pending CSI.** In the `Csi`/`CsiParam` branch, before the
final-byte check:

```cpp
if (ch == L'\x1b') {          // ESC aborts the sequence, starts a new one
    csiParams_.clear();
    state_ = State::Escape;
    continue;
}
```

(The OSC/DCS branches already treat ESC as a terminator; keep that
behaviour — for string sequences, ESC doubles as the start of ST.)

**b) Execute embedded C0 controls in CSI; CAN/SUB abort.** Still in the
`Csi`/`CsiParam` branch:

```cpp
if (ch == L'\x18' || ch == L'\x1a') {           // CAN / SUB: abort
    csiParams_.clear();
    state_ = State::Ground;
    continue;
}
if (ch < 0x20) {                                 // other C0: execute, continue
    switch (ch) {
    case L'\r': buffer_->carriageReturn(); break;
    case L'\n': case L'\v': case L'\f': buffer_->lineFeed(); break;
    case L'\b': buffer_->backspace(); break;
    case L'\t': buffer_->tab(); break;
    case L'\a': MessageBeep(-1); break;
    default: break;                              // ignore the rest
    }
    continue;                                    // stay in CsiParam
}
```

Factor the Ground-state C0 switch (`process()`, lines 228-239) into a small
private helper (`executeC0(wchar_t)`) so Ground and CsiParam share one
implementation.

Add CAN/SUB abort to the OSC and DCS accumulation branches as well
(discard the accumulated string, return to Ground without dispatching).

**c) Consume escape intermediates.** Add a new state
`State::EscapeIntermediate` (stores the intermediate char). In
`handleEscape()`:

```cpp
case L'#': case L'%': case L' ':
    escIntermediate_ = ch;
    state_ = State::EscapeIntermediate;
    break;
```

In `process()`, the new state consumes exactly one byte. Implement
`ESC # 8` (DECALN: fill screen with `E`, home cursor — trivial via
`buffer_->fillRect(0,0,rows-1,cols-1,L'E',TerminalCell())` +
`moveCursorTo(0,0)`); treat all other combinations as a recognized no-op
(swallow the final byte instead of printing it).

**d) Buffer length caps.** Cap `csiParams_` at 256 chars and
`oscText_`/`dcsBuffer_` at 4 MB (Sixel images can legitimately be large).
On overflow, discard the sequence and return to Ground so the terminal
self-heals instead of freezing.

### 4.2 Width-table alignment (F4)

In `TerminalBuffer.cpp:650-702` (`characterWidth`):

1. Remove `0xd800–0xdfff` from the wide table. Instead, in
   `characterWidth(const std::wstring&)`, combine surrogate pairs into a
   code point and classify supplementary planes properly: wide for
   `0x1F300–0x1F64F`, `0x1F900–0x1FAFF`, `0x20000–0x3FFFD` (CJK Ext B+),
   `0x1F000-0x1F0FF` optional; width 1 otherwise.
2. Add an *ambiguous-width* classification (`■ ● ★ ①` U+2460–24FF,
   U+25A0–25FF block elements/geometric shapes, U+2600–26FF (non-emoji
   presentation), Greek U+0370–03FF, Cyrillic U+0400–04FF, U+2010–2027,
   etc. — follow Unicode `EastAsianWidth=A`). Return 2 for these **when the
   session codepage is 932** (already available: `TerminalView::codePage_`
   — plumb it into the buffer via a new
   `TerminalBuffer::setAmbiguousWide(bool)` set from `OnCreate`), else 1.
3. Keep the existing CJK/fullwidth ranges (they are correct for kana/kanji).

This must be validated *against conhost behaviour*, not just the Unicode
tables — see the test plan.

### 4.3 Input-side fixes (I1, I2)

In `TerminalView::EncodeKey()`:

- `case VK_SPACE: return std::string(1, '\0');`
- `case VK_BACK: return altPfx + (ctrl ? "\x08" : "\x7f");`

### 4.4 Not in scope of the fix (documented follow-ups)

- M3 colon-form SGR sub-parameters (cosmetic — colors only).
- M4 combining-char-at-margin (needs `pendingWrap_` awareness in
  `putText`'s width-0 path).
- M1 DECSACE: add `*` to the param filter keep-list when rectangle-op
  support is next touched.

---

## 5. Test plan

### Unit tests — `Application/Terminal/tests/`

The existing harness (`main.cpp`, `test_cursor_move.cpp`,
`test_cursor_save.cpp`, `test_buffer_boundary.cpp`) instantiates
`TerminalEmulator` + `TerminalBuffer` directly and feeds wide strings to
`process()`. Add `test_esc_recovery.cpp` with cases:

| Case | Input | Expected |
|------|-------|----------|
| Truncated CSI recovery | `"AB" ESC[1 ESC[2K` | line cleared from col 2; **no literal `2K` cells** |
| BS inside CSI executed | `"AB" ESC[ \b "3" m` | cursor moved back by the BS; SGR 3 applied; no dropped BS |
| CAN aborts CSI | `ESC[1;3 CAN "X"` | `X` printed as text, no cursor move |
| DECALN | `ESC#8` | screen filled with `E`, cursor home, **no literal `8`** |
| ESC%G swallowed | `ESC%G "hi"` | only `hi` printed |
| OSC cap self-heal | `ESC]0;` + >cap junk + `"ok"` | terminal recovers, `ok` printed |
| Chunk split mid-CSI | `ESC[3` / (separate `process` call) `D` | cursor left 3 — regression guard for existing behaviour |
| Non-BMP width | `𝐀` (U+1D400) | occupies **1** cell |
| Ambiguous width CP932 | `■` with ambiguous-wide on | occupies 2 cells; off → 1 cell |

### Manual verification

1. Build `ecode.exe` + Terminal plugin (`bin/Release/plugins/`).
2. In cmd.exe: type a long command mixing ASCII + Japanese (`dir これはテスト■①`),
   press Backspace repeatedly through the Japanese section — no leftover
   fragments past the end of line, cursor erases the correct cells.
3. In PowerShell (PSReadLine): recall a long history entry, edit in the
   middle, verify redraw correctness.
4. `type` a file containing emoji and box-drawing output (e.g. run
   `claude`) — no stray `2K`/`0m` fragments.

---

## 6. Affected files (for the follow-up implementation)

| File | Change |
|------|--------|
| `Application/Terminal/src/TerminalEmulator.cpp` | §4.1 a–d |
| `Application/Terminal/include/TerminalEmulator.h` | new state `EscapeIntermediate`, `escIntermediate_`, caps |
| `Application/Terminal/src/TerminalBuffer.cpp` / `include/TerminalBuffer.h` | §4.2 width table + `setAmbiguousWide` |
| `Application/Terminal/src/TerminalView.cpp` | §4.3 key encoding; plumb codepage → `setAmbiguousWide` |
| `Application/Terminal/tests/test_esc_recovery.cpp` (new) | §5 unit tests |
