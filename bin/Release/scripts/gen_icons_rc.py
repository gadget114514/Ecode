import os, sys

ico_dir = sys.argv[1]
out_rc = sys.argv[2]
out_h = sys.argv[3]
base_id = int(sys.argv[4]) if len(sys.argv) > 4 else 11000

files = sorted(os.listdir(ico_dir))
ico_files = [f for f in files if f.lower().endswith('.ico')]

rc_path = os.path.dirname(out_rc)
if rc_path and not os.path.exists(rc_path):
    os.makedirs(rc_path)

with open(out_rc, 'w', encoding='utf-8') as rc:
    for i, f in enumerate(ico_files):
        abs_path = os.path.abspath(os.path.join(ico_dir, f)).replace('\\', '/')
        rc.write(f'{base_id + i} ICON "{abs_path}"\n')

with open(out_h, 'w', encoding='utf-8') as h:
    h.write('#pragma once\n\n')
    h.write(f'#define IDI_ICON_COUNT {len(ico_files)}\n')
    h.write(f'#define IDI_ICON_BASE {base_id}\n\n')
    for i, f in enumerate(ico_files):
        name = os.path.splitext(f)[0]
        h.write(f'#define IDI_ICON_{i} {base_id + i}\n')
    h.write('\n')
    h.write('static const wchar_t* g_iconNames[IDI_ICON_COUNT] = {\n')
    for i, f in enumerate(ico_files):
        name = os.path.splitext(f)[0]
        escaped = name.replace('\\', '\\\\').replace('"', '\\"')
        comma = ',' if i < len(ico_files) - 1 else ''
        h.write(f'    L"{escaped}"{comma}\n')
    h.write('};\n')

print(f'Generated {len(ico_files)} icon entries')
