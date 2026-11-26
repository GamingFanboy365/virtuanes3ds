#!/usr/bin/env python3
#
# Checks the automatic FDS disk swap ("Swap disk sides automatically") in
# Azahar, with a made-up disk and BIOS (no Nintendo code): the BIOS is the
# test program. Only its header check's addresses, $E445 and $E6E3, are
# those of the real BIOS, as the emulator looks for them.
#
# The disk image has 4 sides, either of one game or of two joined images with
# headers ("cat zenpen.fds kouhen.fds"), each with its own game code. With the
# setting on, which is the default, the program checks that:
#
#   - polling the drive before the first read (like the real BIOS's boot
#     screen) doesn't eject the disk
#   - polling it after that ejects the disk and puts one in again, and the
#     header check for disk 2 side A then finds it
#   - a header check for another side finds it without a prompt
#   - a header check that matches any side keeps the one that is in
#   - reading the header outside the header check doesn't swap
#   - code that polls all the time gets one swap only, and a prompt's polls
#     still work after that
#   - reading the disk while it is out during a swap finds it
#
# The screen is blue while it waits for a swap, green when every check
# passes and red when one fails. With the setting off it must stay blue.
#
#   tools/build.sh && tools/azahar/fdsswaptest.py
#
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from smoketest import pixel  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, '.azahar', 'fdsswaptest')
CFG = os.path.join(ROOT, '.azahar', 'headless', '.local', 'share',
                   'azahar-emu', 'sdmc', 'virtuanes_3ds.cfg')

BLUE, GREEN, RED = 0x12, 0x2A, 0x16


class Asm:
    """Just enough of a 6502 assembler: bytes, labels and fixups."""
    def __init__(self, base, size):
        self.base, self.mem, self.pc = base, bytearray(size), base
        self.labels, self.fixups = {}, []

    def org(self, addr):
        assert addr >= self.pc, 'overlap at $%04X' % addr
        self.pc = addr

    def label(self, name):
        self.labels[name] = self.pc

    def b(self, *data):
        for d in data:
            self.mem[self.pc - self.base] = d
            self.pc += 1

    def abs(self, op, name):
        self.b(op)
        self.fixups.append((self.pc, name, 'abs'))
        self.b(0, 0)

    def rel(self, op, name):
        self.b(op)
        self.fixups.append((self.pc, name, 'rel'))
        self.b(0)

    def pointer(self, name):        # ($00) = name
        for op, which, zp in ((0xA9, 'lo', 0), (0xA9, 'hi', 1)):
            self.b(op)
            self.fixups.append((self.pc, name, which))
            self.b(0)
            self.b(0x85, zp)

    def finish(self):
        for at, name, kind in self.fixups:
            target, i = self.labels[name], at - self.base
            if kind == 'abs':
                self.mem[i:i + 2] = target.to_bytes(2, 'little')
            elif kind == 'lo':
                self.mem[i] = target & 0xFF
            elif kind == 'hi':
                self.mem[i] = target >> 8
            else:
                offset = target - (at + 1)
                assert -128 <= offset <= 127, name
                self.mem[i] = offset & 0xFF
        return bytes(self.mem)


def header(side, joined):
    """The 10 bytes the header check compares: maker, game name and type,
    version, side, disk, disk type, unknown. Two-part games sold separately
    have the same disk number but different names."""
    if joined:
        return bytes([0x99]) + b'TS%d ' % (side // 2 + 1) + bytes([0, side & 1, 0, 0, 0])
    return bytes([0x99]) + b'TST ' + bytes([0, side & 1, side >> 1, 0, 0])


def disk(joined):
    sides = []
    for side in range(4):
        block = b'\x01*NINTENDO-HVC*' + header(side, joined)
        sides.append(block + bytes(65500 - len(block)))

    def image(sides):
        return b'FDS\x1a' + bytes([len(sides)]) + bytes(11) + b''.join(sides)
    return image(sides[:2]) + image(sides[2:]) if joined else image(sides)


def bios(joined):
    a = Asm(0xE000, 0x2000)
    JSR, JMP, BEQ, BNE, BPL = 0x20, 0x4C, 0xF0, 0xD0, 0x10

    def fail_if_ne():
        a.b(BEQ, 3)
        a.abs(JMP, 'failed')

    def prompt(want, n):                            # wait for out and in again
        a.label('out%d' % n)
        a.abs(JSR, 'poll')
        a.rel(BEQ, 'out%d' % n)
        a.label('in%d' % n)
        a.abs(JSR, 'poll')
        a.rel(BNE, 'in%d' % n)
        a.pointer(want)
        a.abs(JSR, 'check')
        fail_if_ne()

    a.label('reset')
    a.b(0x78, 0xD8, 0xA2, 0xFF, 0x9A)               # sei; cld; stack
    a.b(0xA9, 0x00, 0x8D, 0x00, 0x20, 0x8D, 0x01, 0x20)
    a.b(0xA9, 0x26, 0x8D, 0x25, 0x40)               # drive idle
    for _ in range(3):
        a.abs(JSR, 'vblank')
    a.b(0xA9, BLUE)
    a.abs(JSR, 'colour')

    # Wait for the disk that is in at power-on; it must stay in for 100
    # frames of polling.
    a.label('boot')
    a.abs(JSR, 'poll')
    a.rel(BNE, 'boot')
    a.b(0xA9, 100, 0x85, 0x10)                      # $10 = 100
    a.label('boot2')
    a.abs(JSR, 'poll')
    fail_if_ne()
    a.b(0xC6, 0x10)                                 # dec $10
    a.rel(BNE, 'boot2')

    a.pointer('want0')                              # side 0 is in
    a.abs(JSR, 'check')
    fail_if_ne()

    prompt('want2', 1)                              # "SET DISK 2 SIDE A"

    a.pointer('want3')                              # no prompt
    a.abs(JSR, 'check')
    fail_if_ne()

    a.pointer('any')                                # keeps side 3
    a.abs(JSR, 'check')
    fail_if_ne()

    a.pointer('want0')                              # not the header check
    a.abs(JSR, 'readheader')
    a.b(0xA2, 0x06)                                 # ldx #6
    a.label('skip')
    a.b(0xAD, 0x31, 0x40, 0xCA)                     # lda $4031; dex
    a.rel(BNE, 'skip')
    a.b(0xAD, 0x31, 0x40, 0xC9, 0x01)               # side: 1
    fail_if_ne()
    a.b(0xAD, 0x31, 0x40, 0xC9, 0 if joined else 1)  # disk
    fail_if_ne()
    a.b(0xA9, 0x26, 0x8D, 0x25, 0x40)               # drive idle

    # 506 frames of polling in the background: out once and in again.
    a.b(0xA9, 0x00, 0x85, 0x11, 0x85, 0x12)         # changes, last status
    a.b(0xA9, 250, 0x85, 0x10, 0xA9, 0x02, 0x85, 0x13)
    a.label('bg')
    a.abs(JSR, 'bgpoll')
    a.b(0xC5, 0x12, 0xF0, 0x04, 0x85, 0x12, 0xE6, 0x11)
    a.b(0xC6, 0x10)
    a.rel(BNE, 'bg')
    a.b(0xC6, 0x13)
    a.rel(BNE, 'bg')
    a.b(0xA5, 0x11, 0xC9, 0x02)
    fail_if_ne()

    prompt('want2', 2)                              # a prompt still works

    # No polls for a while, then read the disk as soon as it goes out.
    a.b(0xA9, 80, 0x85, 0x10)
    a.label('quiet')
    a.abs(JSR, 'vblank')
    a.b(0xC6, 0x10)
    a.rel(BNE, 'quiet')
    a.label('bgout')
    a.abs(JSR, 'bgpoll')
    a.rel(BEQ, 'bgout')
    a.pointer('want0')
    a.abs(JSR, 'check')
    fail_if_ne()

    a.b(0xA9, GREEN)
    a.abs(JMP, 'end')
    a.label('failed')
    a.b(0xA9, RED)
    a.label('end')
    a.abs(JSR, 'colour')
    a.b(0xA9, 0x26, 0x8D, 0x25, 0x40)
    a.label('stop')
    a.abs(JMP, 'stop')

    a.label('vblank')
    a.b(0x2C, 0x02, 0x20)                           # bit $2002
    a.label('vb')
    a.b(0x2C, 0x02, 0x20)
    a.rel(BPL, 'vb')
    a.b(0x60)

    a.label('poll')                                 # once a frame; Z = disk in
    a.abs(JSR, 'vblank')
    a.b(0xAD, 0x32, 0x40, 0x29, 0x01, 0x60)

    a.label('bgpoll')                               # the same, elsewhere
    a.abs(JSR, 'vblank')
    a.b(0xAD, 0x32, 0x40, 0x29, 0x01, 0x60)

    a.label('colour')                               # backdrop = A
    a.b(0x48)
    a.abs(JSR, 'vblank')
    a.b(0xA9, 0x3F, 0x8D, 0x06, 0x20, 0xA9, 0x00, 0x8D, 0x06, 0x20)
    a.b(0x68, 0x8D, 0x07, 0x20)
    a.b(0xA9, 0x00, 0x8D, 0x06, 0x20, 0x8D, 0x06, 0x20)
    a.b(0x8D, 0x05, 0x20, 0x8D, 0x05, 0x20)
    a.b(0xA9, 0x0A, 0x8D, 0x01, 0x20, 0x60)

    a.label('check')                                # header check; Z = ok
    a.abs(JSR, 'CheckDiskHeader')
    a.b(0x48, 0xA9, 0x26, 0x8D, 0x25, 0x40, 0x68, 0x60)

    for name, data in (('want0', header(0, joined)), ('want2', header(2, joined)),
                       ('want3', header(3, joined)), ('any', b'\xFF' * 10)):
        a.label(name)
        a.b(*data)

    a.org(0xE445)
    a.label('CheckDiskHeader')
    a.abs(JSR, 'readheader')
    a.b(0xA0, 0x00)                                 # ldy #0
    a.label('cmp')
    a.b(0xAD, 0x31, 0x40, 0xD1, 0x00)               # lda $4031; cmp ($00),y
    a.rel(BEQ, 'same')
    a.b(0xB1, 0x00, 0xC9, 0xFF)                     # lda ($00),y; cmp #$ff
    a.rel(BNE, 'differs')
    a.label('same')
    a.b(0xC8, 0xC0, 0x0A)                           # iny; cpy #10
    a.rel(BNE, 'cmp')
    a.b(0xA9, 0x00, 0x60)
    a.label('differs')
    a.b(0xA9, 0x01, 0x60)

    a.org(0xE6E3)
    a.label('readheader')                           # up to the 10 bytes
    for value in (0x27, 0x25, 0x65):                # reset, run, start a block
        a.b(0xA9, value, 0x8D, 0x25, 0x40)
    a.b(0xA2, 0x0F)                                 # ldx #15
    a.label('rh')
    a.b(0xAD, 0x31, 0x40, 0xCA)
    a.rel(BNE, 'rh')
    a.b(0x60)

    a.org(0xFFF0)
    a.label('rti')
    a.b(0x40)
    a.org(0xFFFA)
    for name in ('rti', 'reset', 'rti'):
        a.fixups.append((a.pc, name, 'abs'))
        a.b(0, 0)
    return a.finish()


def colour(rgb):
    r, g, b = rgb
    if b > 150 and r < 100:
        return 'blue'
    if g > 120 and r < 120 and b < 120:
        return 'green'
    if r > 120 and g < 100 and b < 100:
        return 'red'
    return 'rgb%s' % (rgb,)


def run(tmp, name, joined=False, cfg=None):
    out = os.path.join(OUT, name)
    os.makedirs(out, exist_ok=True)
    rom, bios_rom = os.path.join(tmp, 'swaptest.fds'), os.path.join(tmp, 'disksys.rom')
    with open(rom, 'wb') as f:
        f.write(disk(joined))
    with open(bios_rom, 'wb') as f:
        f.write(bios(joined))
    command = [
        os.path.join(ROOT, 'tools', 'azahar', 'run.sh'), '-r', rom,
        '-s', bios_rom + ':3ds/virtuanes_3ds/bios/disksys.rom',
        '-o', out, '-t', '180']
    if cfg:
        # The settings remember the ROM, so it is already selected.
        command += ['-s', cfg + ':virtuanes_3ds.cfg', '--', 'wait:10']
    else:
        command += ['--', 'wait:10', 'key:down', 'wait:1']   # below "3ds"
    subprocess.run(command + ['key:a', 'wait:35', 'shot:result'], check=True)
    return colour(pixel(out, 'result', 200, 120))


def main():
    with tempfile.TemporaryDirectory() as tmp:
        # The default settings...
        results = [('on (the default)', run(tmp, 'on'), 'green')]

        # ...are saved when a ROM is loaded.
        text = open(CFG).read()
        line = 'FDSAutoSwap=1\n'
        assert line in text, 'not on by default:\n' + text
        for name, replacement, want in (
                ('off', 'FDSAutoSwap=0\n', 'blue'),
                # A file of a version without the setting, or with the old
                # name it had for a while, when it was off by default.
                ('older', '', 'green'),
                ('oldname', 'FDSAutoDiskSwap=0\n', 'green')):
            cfg = os.path.join(tmp, name + '.cfg')
            with open(cfg, 'w') as f:
                f.write(text.replace(line, replacement))
            results.append((name, run(tmp, name, cfg=cfg), want))

        results.append(('joined images', run(tmp, 'joined', joined=True), 'green'))

    ok = True
    for name, got, want in results:
        ok = ok and got == want
        print('%-18s %s (want %s)' % (name + ':', got, want))
    print('%s - screenshots in %s' % ('PASS' if ok else 'FAIL', OUT))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
