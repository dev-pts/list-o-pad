import sys
from bdflib import reader

def format_code(code):
    return ' '.join('0x{:02X},'.format(a) for a in code)

with open(sys.argv[1], "rb") as font_file:
    font = reader.read_bdf(font_file)

print('''#define ICON_W 32
#define ICON_H 32

static uint8_t icon[] = {''')

for i in range(256):
    g = font.get(i)
    if g is None:
        bitmap = [0] * 32 * 4
    else:
        bitmap = []
        for i in g.data:
            for j in range(int((32 + 7) / 8)):
                bitmap.append(i & 0xff)
                i >>= 8
        for i in range(len(bitmap), 32 * 4):
            bitmap.append(0)
    print('\t' + format_code(bitmap[::-1]))

print('};')
