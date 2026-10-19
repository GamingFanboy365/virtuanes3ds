#!/usr/bin/env python3
#
# Fails if the 3DS code brings back a known cause of bugs that only show
# on real hardware. Each of these once made VirtuaNES hang or crash on a
# New 3DS XL while it ran fine in Azahar, so the Azahar tests can't catch
# them on their own (tools/azahar/hwpathtest.py covers the rest).
#
#   tools/check_hardware_pitfalls.py
#
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SOURCE_DIRS = ['src/3ds', 'src/cores/virtuanes/3ds']

problems = []


def strip_comments(text):
    """Blanks out comments (keeping line breaks, so line numbers hold)."""
    def blank(match):
        return re.sub(r'[^\n]', ' ', match.group(0))
    return re.sub(r'//[^\n]*|/\*.*?\*/', blank, text, flags=re.S)


def sources():
    for directory in SOURCE_DIRS:
        for name in sorted(os.listdir(os.path.join(ROOT, directory))):
            if name.endswith(('.c', '.cpp', '.h')):
                path = os.path.join(directory, name)
                with open(os.path.join(ROOT, path), encoding='latin-1') as f:
                    yield path, strip_comments(f.read())


def report(path, text, pos, message):
    line = text.count('\n', 0, pos) + 1
    problems.append('%s:%d: %s' % (path, line, message))


for path, code in sources():
    for m in re.finditer(r'\bsvcCreateThread\s*\(', code):
        report(path, code, m.start(),
               'use threadCreate(), not svcCreateThread(): libctru stops the program '
               'when a raw thread touches newlib state such as errno')

    if path != 'src/3ds/3dsexit.cpp':
        for m in re.finditer(r'\baptMainLoop\s*\(', code):
            report(path, code, m.start(),
                   'call app3dsMainLoop(), not aptMainLoop(): libctru 2.x reports a close '
                   'from the HOME menu only through its return value, and ignoring it hangs '
                   'the app on "Closing software"')

    for m in re.finditer(r'\b(aptExit|srvExit|hidExit|fsExit|gspExit)\s*\(', code):
        report(path, code, m.start(),
               "don't call %s(): libctru's exit() does, in the right order; calling it "
               'as well made Exit end in "An error has occurred"' % m.group(1))

    if path == 'src/3ds/3dssound.cpp':
        # Every call into the CSND service goes through CSND_SERVICE(), so
        # HWTEST builds can skip it and still run everything around it.
        for m in re.finditer(r'\b(csnd[A-Z]\w*|CSND_[A-Z]\w*)\s*\(', code):
            if m.group(1) in ('CSND_SERVICE', 'CSND_TIMER', 'CSND_VOL'):
                continue
            before = code[code.rfind('\n', 0, m.start()) + 1:m.start()]
            if 'CSND_SERVICE(' not in before:
                report(path, code, m.start(),
                       'wrap %s() in CSND_SERVICE(), so tools/azahar/hwpathtest.py can run '
                       'this code in Azahar' % m.group(1))

with open(os.path.join(ROOT, 'src/3ds/3dssound.h'), encoding='latin-1') as f:
    header = strip_comments(f.read())
for field in ('isPlaying', 'terminateMixingThread'):
    if not re.search(r'\bvolatile\s+bool\s+%s\b' % field, header):
        problems.append('src/3ds/3dssound.h: snd3DS.%s must be volatile: the mixing thread '
                        'waits on it, and without volatile GCC reads it once and the thread '
                        'spins forever (no sound)' % field)

with open(os.path.join(ROOT, 'Makefile')) as f:
    makefile = f.read()
cflags = '\n'.join(re.findall(r'^CFLAGS\s*[:+]?=(?:[^\n]*\\\n)*[^\n]*', makefile, flags=re.M))
if '-Werror=return-type' not in cflags:
    problems.append('Makefile: CFLAGS needs -Werror=return-type: a non-void function that '
                    'falls off its end hung every game at start-up (snd3dsPlaySound)')
if re.search(r'(^|\s)-w(\s|\\|$)', cflags, flags=re.M):
    problems.append('Makefile: no -w in CFLAGS: it hides every warning, '
                    'including the errors -Werror turns on')

if problems:
    print('Known causes of real-hardware bugs found:\n')
    print('\n'.join(problems))
    sys.exit(1)
print('No known causes of real-hardware bugs found.')
