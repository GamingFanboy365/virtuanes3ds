#!/usr/bin/env python3
#
# Checks the Famicom 3D System support (stereoscopic 3D) in Azahar.
#
# Generates an NROM test ROM that works like a game for the Famicom 3D
# System: every vblank it flips OUT1 ($4016 bit 1), which tells the shutter
# glasses which eye the next frame is for (0 = left, 1 = right), and draws
# that frame in the eye's colour: red for the left eye, blue for the right.
# While NES A is held it stops flipping OUT1, like a game whose 3D mode was
# turned off.
#
# Azahar runs with its side-by-side 3D mode, where the left half of the top
# screen shows the left eye and the right half the right eye, first with
# the 3D slider up and then with it down:
#
#   slider up:   left half red, right half blue
#   A held:      back to 2D, both halves the same colour
#   slider down: only the left eye's picture (red), never the right's,
#                so that the picture doesn't flicker
#
#   tools/build.sh && tools/azahar/stereotest.py
#
import os
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from smoketest import read_png  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, '.azahar', 'stereotest')

RED, BLUE = 0x16, 0x12          # NES palette colours for the left and right eye


def assemble():
    base = 0xC000
    reset = bytearray([
        0x78,                   # sei
        0xD8,                   # cld
        0xA2, 0xFF,             # ldx #$ff
        0x9A,                   # txs
        0xA9, 0x00,             # lda #0
        0x85, 0x00,             # sta $00     ; field: 0 = left, 2 = right (OUT1 = bit 1)
        0x8D, 0x00, 0x20,       # sta $2000
        0x8D, 0x01, 0x20,       # sta $2001
        0x2C, 0x02, 0x20,       # bit $2002   ; wait for two vblanks
        0x10, 0xFB,             # bpl -5
        0x2C, 0x02, 0x20,       # bit $2002
        0x10, 0xFB,             # bpl -5
        0xA9, 0x80,             # lda #$80    ; nmi on
        0x8D, 0x00, 0x20,       # sta $2000
        0xA9, 0x0A,             # lda #$0a    ; background on
        0x8D, 0x01, 0x20,       # sta $2001
        0x4C, 0x00, 0x00,       # jmp *
    ])
    reset[-2:] = struct.pack('<H', base + len(reset) - 3)

    nmi = bytes([
        0x48,                   # pha
        0xA5, 0x00,             # lda $00     ; strobe the controller, keeping OUT1
        0x09, 0x01,             # ora #1
        0x8D, 0x16, 0x40,       # sta $4016
        0x29, 0x02,             # and #2
        0x8D, 0x16, 0x40,       # sta $4016
        0xAD, 0x16, 0x40,       # lda $4016   ; A button
        0x29, 0x01,             # and #1
        0xD0, 0x09,             # bne +9      ; A held: 3D off, keep the field
        0xA5, 0x00,             # lda $00     ; next field is for the other eye
        0x49, 0x02,             # eor #2
        0x85, 0x00,             # sta $00
        0x8D, 0x16, 0x40,       # sta $4016   ; OUT1: which eye the next frame is for
        0xA0, RED,              # ldy #RED    ; the next frame's colour
        0xA5, 0x00,             # lda $00
        0xF0, 0x02,             # beq +2
        0xA0, BLUE,             # ldy #BLUE
        0xA9, 0x3F,             # lda #$3f    ; backdrop colour at $3f00
        0x8D, 0x06, 0x20,       # sta $2006
        0xA9, 0x00,             # lda #0
        0x8D, 0x06, 0x20,       # sta $2006
        0x8C, 0x07, 0x20,       # sty $2007
        0xA9, 0x00,             # lda #0      ; reset scroll
        0x8D, 0x05, 0x20,       # sta $2005
        0x8D, 0x05, 0x20,       # sta $2005
        0xA9, 0x80,             # lda #$80
        0x8D, 0x00, 0x20,       # sta $2000
        0x68,                   # pla
        0x40,                   # rti
    ])
    nmi_offset = 0x100

    prg = bytearray(0x4000)
    prg[0:len(reset)] = reset
    prg[nmi_offset:nmi_offset + len(nmi)] = nmi
    prg[0x3FFA:] = struct.pack('<HHH', base + nmi_offset, base, base)
    header = b'NES\x1a' + bytes([1, 1, 0, 0]) + bytes(8)   # NROM, 16K PRG, 8K CHR
    return header + bytes(prg) + bytes(0x2000)


def colour(rgb):
    r, g, b = rgb
    if r > 120 and b < 100:
        return 'red'
    if b > 120 and r < 100:
        return 'blue'
    return 'rgb%s' % (rgb,)


def halves(out, name):
    """The colour in the middle of the left and right halves of the top
    screen. In side-by-side mode each half shows one eye, squeezed to half
    the width."""
    rows = read_png(os.path.join(out, name + '.png'))
    row = rows[120]
    return colour(tuple(row[100 * 3:100 * 3 + 3])), colour(tuple(row[300 * 3:300 * 3 + 3]))


def run(rom, name, slider, steps):
    out = os.path.join(OUT, name)
    os.makedirs(out, exist_ok=True)
    config = os.path.join(out, 'renderer.ini')
    with open(config, 'w') as f:
        # render_3d 1 = side by side; factor_3d is the 3D slider (0-100).
        f.write('\n[Layout]\nrender_3d=1\nrender_3d\\default=false\n'
                'factor_3d=%d\nfactor_3d\\default=false\n' % slider)
    subprocess.run([
        os.path.join(ROOT, 'tools', 'azahar', 'run.sh'),
        '-r', rom, '-c', config, '-o', out, '-t', '180', '--',
        'wait:10', 'key:a', 'wait:8'] + steps, check=True)
    return out


def main():
    with tempfile.TemporaryDirectory() as tmp:
        rom = os.path.join(tmp, 'stereotest.nes')
        with open(rom, 'wb') as f:
            f.write(assemble())

        up = run(rom, 'slider_up', 100, [
            'shot:stereo1', 'wait:0.3', 'shot:stereo2', 'wait:0.3', 'shot:stereo3',
            'down:a', 'wait:3', 'shot:mono', 'up:a'])
        down = run(rom, 'slider_down', 0, [
            'shot:left1', 'wait:0.3', 'shot:left2', 'wait:0.3', 'shot:left3'])

    results = []

    def expect(out, name, want, label):
        got = halves(out, name)
        ok = want(got)
        results.append(ok)
        print('%-28s left half %-5s right half %-5s %s' % (label, got[0], got[1], 'PASS' if ok else 'FAIL'))

    for n in (1, 2, 3):
        expect(up, 'stereo%d' % n, lambda g: g == ('red', 'blue'), 'slider up #%d (want red|blue)' % n)
    expect(up, 'mono', lambda g: g[0] == g[1] and g[0] in ('red', 'blue'), 'A held: 2D (want same)')
    for n in (1, 2, 3):
        expect(down, 'left%d' % n, lambda g: g == ('red', 'red'), 'slider down #%d (want red)' % n)

    print('%s - screenshots in %s' % ('PASS' if all(results) else 'FAIL', OUT))
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
