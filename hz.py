import sys

table = {}

with open(sys.argv[1], "rb") as f:
    while True:
        l = f.readline()
        if not l:
            break
        l = l.strip().decode()

        if l.startswith('FONTBOUNDINGBOX'):
            _, w, h, x, y = l.strip().split(' ')
            print(f'''static struct BitmapFont icon = {{
\t.size = {{ .w = {w}, .h = {h} }},
\t.bitmap = (struct BitmapData[]) {{''')
            continue

        if l.startswith('ENCODING'):
            _, code = l.strip().split(' ')
            code = int(code)
            continue

        if l.startswith('BBX'):
            _, w, h, x, y = l.strip().split(' ')
            table[code] = {}
            table[code]['bb'] = [ w, h, x, y ]
            table[code]['bitmap'] = []
            continue

        if l.startswith('BITMAP'):
            while True:
                l = f.readline()
                if not l:
                    break
                l = l.strip().decode()
                if l.startswith('ENDCHAR'):
                    break
                table[code]['bitmap'].append(', '.join('0x' + l[i:i + 2] for i in range(0, len(l), 2)))
            continue

for i in table:
    t = table[i]
    bb = t['bb']
    bitmap = t['bitmap']
    print(f'\t\t{{ .bb = {{ .w = {bb[0]}, .h = {bb[1]} }}, .data = (uint8_t[]) {{ {", ".join(bitmap)} }} }},')

print('\t}')
print('};')
