#!/usr/bin/env python3
"""
parse_conpty_dump.py
--------------------
Parse a ConPTY binary dump file and list all VT/ANSI escape sequences.

Usage:
    python parse_conpty_dump.py <path_to_dump.bin>
    python parse_conpty_dump.py <path_to_dump.bin> > output.txt

Output: one token per line
    [offset]  TYPE  detail

Token types:
    PRINT   - printable text (UTF-8 decoded)
    ESC     - raw ESC (0x1B) not followed by a recognised sequence
    CSI     - ESC [ ... final  (Control Sequence Introducer)
    OSC     - ESC ] ... ST/BEL (Operating System Command)
    DCS     - ESC P ... ST     (Device Control String)
    SS3     - ESC O x          (Single Shift 3 — application cursor keys)
    ESC2    - ESC <char>       (other 2-char ESC sequences)
    C0      - ASCII control character (NUL/BEL/BS/HT/LF/CR/SO/SI...)
"""

import sys
import os


# ---------------------------------------------------------------------------
# Known CSI sequence descriptions
# ---------------------------------------------------------------------------
CSI_NAMES = {
    'A': 'CUU  Cursor Up',
    'B': 'CUD  Cursor Down',
    'C': 'CUF  Cursor Forward',
    'D': 'CUB  Cursor Back',
    'E': 'CNL  Cursor Next Line',
    'F': 'CPL  Cursor Previous Line',
    'G': 'CHA  Cursor Horizontal Absolute',
    'H': 'CUP  Cursor Position',
    'J': 'ED   Erase in Display',
    'K': 'EL   Erase in Line',
    'L': 'IL   Insert Lines',
    'M': 'DL   Delete Lines',
    'P': 'DCH  Delete Characters',
    'S': 'SU   Scroll Up',
    'T': 'SD   Scroll Down',
    'X': 'ECH  Erase Characters',
    'Z': 'CBT  Cursor Backward Tab',
    '@': 'ICH  Insert Characters',
    'b': 'REP  Repeat Previous Char',
    'd': 'VPA  Line Position Absolute',
    'f': 'HVP  Horizontal Vertical Position',
    'h': 'SM   Set Mode',
    'l': 'RM   Reset Mode',
    'm': 'SGR  Select Graphic Rendition',
    'n': 'DSR  Device Status Report',
    'r': 'DECSTBM Set Scroll Region',
    's': 'SCP  Save Cursor Position',
    'u': 'RCP  Restore Cursor Position',
    '`': 'HPA  Horizontal Position Absolute',
}

# CSI ? ... h/l  (DEC private modes)
DEC_PRIVATE = {
    '1':    '?1   DECCKM Application Cursor Keys',
    '6':    '?6   DECOM  Origin Mode',
    '7':    '?7   DECAWM Auto-Wrap Mode',
    '12':   '?12  ATT160 Cursor Blink',
    '25':   '?25  DECTCEM Cursor Visible',
    '47':   '?47  Alternate Screen (old)',
    '1000': '?1000 Mouse: Normal',
    '1002': '?1002 Mouse: Button',
    '1003': '?1003 Mouse: All Motion',
    '1004': '?1004 Focus Event Reporting',
    '1006': '?1006 SGR Mouse Encoding',
    '1049': '?1049 Alternate Screen + Save Cursor',
    '2004': '?2004 Bracketed Paste Mode',
    '9001': '?9001 Win32 Input Mode',
}

C0_NAMES = {
    0x00: 'NUL', 0x07: 'BEL', 0x08: 'BS ', 0x09: 'HT ',
    0x0A: 'LF ', 0x0B: 'VT ', 0x0C: 'FF ', 0x0D: 'CR ',
    0x0E: 'SO ', 0x0F: 'SI ', 0x1A: 'SUB', 0x7F: 'DEL',
}

ESC2_NAMES = {
    'M': 'RI   Reverse Index',
    '7': 'DECSC Save Cursor',
    '8': 'DECRC Restore Cursor',
    '=': 'DECKPAM Application Keypad',
    '>': 'DECKPNM Normal Keypad',
    'c': 'RIS  Full Reset',
}


# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------
def parse(data: bytes):
    """Yield (offset, type_str, detail_str) tuples."""
    i = 0
    n = len(data)

    def peek(j, count=1):
        return data[j:j + count]

    while i < n:
        b = data[i]
        offset = i

        # ---- C0 control (exclude ESC, handled separately) ----
        if b < 0x20 and b != 0x1B:
            name = C0_NAMES.get(b, f'0x{b:02X}')
            yield offset, 'C0  ', name
            i += 1
            continue

        # ---- ESC sequences ----
        if b == 0x1B:
            if i + 1 >= n:
                yield offset, 'ESC ', '(bare, end of data)'
                i += 1
                continue

            next_b = data[i + 1]

            # CSI  ESC [
            if next_b == ord('['):
                # Collect params + intermediate + final
                j = i + 2
                seq_bytes = bytearray()
                while j < n and data[j] < 0x40:
                    seq_bytes.append(data[j])
                    j += 1
                final = chr(data[j]) if j < n else '?'
                j += 1

                seq_str = seq_bytes.decode('ascii', errors='replace')

                # Decode ?h/?l DEC private
                if seq_str.startswith('?'):
                    params = seq_str[1:]
                    action = 'set' if final == 'h' else ('reset' if final == 'l' else final)
                    names = []
                    for p in params.split(';'):
                        names.append(DEC_PRIVATE.get(p, f'?{p}'))
                    detail = f'ESC[{seq_str}{final}  ({action}: {", ".join(names)})'
                else:
                    desc = CSI_NAMES.get(final, f'CSI {final}')
                    detail = f'ESC[{seq_str}{final}  {desc}'

                yield offset, 'CSI ', detail
                i = j
                continue

            # OSC  ESC ]
            if next_b == ord(']'):
                j = i + 2
                osc_bytes = bytearray()
                while j < n:
                    if data[j] == 0x07:  # BEL terminator
                        j += 1
                        break
                    if data[j] == 0x1B and j + 1 < n and data[j + 1] == ord('\\'):  # ST
                        j += 2
                        break
                    osc_bytes.append(data[j])
                    j += 1
                osc_str = osc_bytes.decode('utf-8', errors='replace')
                # Truncate long OSC for readability
                if len(osc_str) > 80:
                    osc_str = osc_str[:77] + '...'
                yield offset, 'OSC ', f'ESC]{osc_str}'
                i = j
                continue

            # DCS  ESC P
            if next_b == ord('P'):
                j = i + 2
                dcs_bytes = bytearray()
                while j < n:
                    if data[j] == 0x1B and j + 1 < n and data[j + 1] == ord('\\'):
                        j += 2
                        break
                    dcs_bytes.append(data[j])
                    j += 1
                dcs_str = dcs_bytes.decode('ascii', errors='replace')
                if len(dcs_str) > 60:
                    dcs_str = dcs_str[:57] + '...'
                yield offset, 'DCS ', f'ESCP{dcs_str}'
                i = j
                continue

            # SS3  ESC O (application cursor / function keys)
            if next_b == ord('O'):
                ch = chr(data[i + 2]) if i + 2 < n else '?'
                yield offset, 'SS3 ', f'ESC O {ch}'
                i += 3
                continue

            # Other 2-char ESC
            ch = chr(next_b)
            desc = ESC2_NAMES.get(ch, '')
            yield offset, 'ESC2', f'ESC {ch}  {desc}'
            i += 2
            continue

        # ---- DEL ----
        if b == 0x7F:
            yield offset, 'C0  ', 'DEL'
            i += 1
            continue

        # ---- Printable / UTF-8 text — collect a run ----
        j = i
        text_bytes = bytearray()
        while j < n and data[j] >= 0x20 and data[j] != 0x7F and data[j] != 0x1B:
            text_bytes.append(data[j])
            j += 1

        text = text_bytes.decode('utf-8', errors='replace')
        # Truncate long text runs for readability
        display = repr(text[:100]) + ('...' if len(text) > 100 else '')
        yield offset, 'PRINT', display
        i = j


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    if len(sys.argv) < 2:
        print('Usage: python parse_conpty_dump.py <dump.bin> [> output.txt]', file=sys.stderr)
        sys.exit(1)

    path = sys.argv[1]
    if not os.path.exists(path):
        print(f'File not found: {path}', file=sys.stderr)
        sys.exit(1)

    with open(path, 'rb') as f:
        data = f.read()

    print(f'# ConPTY dump: {path}  ({len(data)} bytes)\n')

    for offset, kind, detail in parse(data):
        print(f'  {offset:6d}  {kind}  {detail}')

    print(f'\n# End of dump ({len(data)} bytes)')


if __name__ == '__main__':
    main()
