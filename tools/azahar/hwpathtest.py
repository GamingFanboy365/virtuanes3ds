#!/usr/bin/env python3
#
# Runs the code that otherwise only runs on a real 3DS in Azahar, to keep
# the hardware fixes from coming undone: the sound mixing thread, the GPU
# waits, the HOME menu hooks, Exit and closing from the HOME menu. Each of
# them once hung or crashed a New 3DS XL while VirtuaNES ran fine in Azahar,
# because Azahar has neither CSND sound nor a HOME menu, and the app turns
# sound off under emulators.
#
# It needs a `make HWTEST=1` build, which takes the real-hardware paths in
# Azahar with only the CSND service calls skipped, logs to the SD card
# (TRACE=1) and simulates the HOME menu events that sdmc:/hwtest.txt asks
# for. Never release that build.
#
#   tools/build.sh clean && tools/build.sh HWTEST=1 && tools/azahar/hwpathtest.py
#
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoketest  # noqa: E402  (its test ROM)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, '.azahar', 'hwpathtest')
SDMC = os.path.join(ROOT, '.azahar', 'headless', '.local', 'share', 'azahar-emu', 'sdmc')
APP = os.path.join(ROOT, 'virtuanes_3ds.cia')

# Start a game: the .cia adds the "Nintendo 3DS" folder above the ROM.
START = ['wait:10', 'key:down', 'wait:1', 'key:a', 'wait:8']
# The in-game menu (tap the bottom screen), then Exit (the last item) and Yes.
MENU = ['tap:200:360', 'wait:2']
EXIT = MENU + ['key:up', 'wait:1', 'key:a', 'wait:2', 'key:down', 'wait:1', 'key:a', 'wait:8']


def mixed_at_frame(trace, frame):
    m = re.search(r'frame %d: mixed (\d+) sound blocks' % frame, trace)
    return int(m.group(1)) if m else None


def logged_number(trace, text):
    m = re.search(re.escape(text) + r' (\d+)', trace)
    return int(m.group(1)) if m else None


def check_running(trace, problems):
    if 'mixing thread: started' not in trace:
        problems.append('the sound mixing thread did not start')
    if 'sound started: isPlaying=1' not in trace:
        problems.append('the game did not start its sound')
    mixed = mixed_at_frame(trace, 120)
    if mixed is None:
        problems.append('the game did not reach frame 120 (hung at start-up?)')
    elif mixed < 120:
        problems.append('the sound thread mixed only %d blocks by frame 120 (want 2 a frame)' % mixed)


def check_exit(trace, log, problems, closed_by_system):
    if closed_by_system and 'system asked to close' not in trace:
        problems.append('the app did not see the close order (app3dsMainLoop)')
    if 'finalize: done' not in trace:
        problems.append('shutdown did not finish (hung on the way out?)')
    if not re.search(r'\d exit\n', trace):
        problems.append('the app did not reach exit()')
    if 'Cleaning up process' not in log and 'TerminateProcess' not in log:
        problems.append('the process did not end')


def check_play_and_exit(trace, log, problems):
    check_running(trace, problems)
    check_exit(trace, log, problems, closed_by_system=False)


def check_home_and_back(trace, log, problems):
    check_running(trace, problems)
    pressed = logged_number(trace, 'hwtest: HOME pressed, mixed')
    back = logged_number(trace, 'hwtest: back to the game, mixed')
    later = logged_number(trace, 'hwtest: 2 seconds after returning, mixed')
    if None in (pressed, back, later):
        problems.append('the simulated HOME menu visit did not complete')
        return
    if back - pressed > 10:
        problems.append('sound kept mixing in the HOME menu (%d blocks)' % (back - pressed))
    if later - back < 100:
        problems.append('sound did not resume after returning (%d blocks in 2 s)' % (later - back))


def check_closed(trace, log, problems):
    check_running(trace, problems)
    if 'hwtest: closing from the HOME menu' not in trace:
        problems.append('the simulated close did not happen')
    check_exit(trace, log, problems, closed_by_system=True)


CASES = [
    # name, sdmc:/hwtest.txt, steps, check
    ('play-and-exit', None, START + EXIT, check_play_and_exit),
    ('home-and-back', 'suspend 120', START + ['wait:4'], check_home_and_back),
    ('close-in-game', 'close-game 180', START + ['wait:6'], check_closed),
    ('close-in-menu', 'close-menu 60', START + MENU + ['wait:6'], check_closed),
]


def run_case(name, hwtest, steps, check, rom):
    out = os.path.join(OUT, name)
    os.makedirs(out, exist_ok=True)
    trace_path = os.path.join(SDMC, 'virtuanes_trace.txt')
    hwtest_path = os.path.join(SDMC, 'hwtest.txt')
    for path in (trace_path, hwtest_path):
        if os.path.exists(path):
            os.remove(path)
    os.makedirs(SDMC, exist_ok=True)
    if hwtest:
        with open(hwtest_path, 'w') as f:
            f.write(hwtest + '\n')

    subprocess.run([os.path.join(ROOT, 'tools', 'azahar', 'run.sh'),
                    '-a', APP, '-r', rom, '-o', out, '-t', '180', '--'] + steps)

    trace = open(trace_path, encoding='latin-1').read() if os.path.exists(trace_path) else ''
    log_path = os.path.join(out, 'azahar_log.txt')
    log = open(log_path, encoding='latin-1').read() if os.path.exists(log_path) else ''
    with open(os.path.join(out, 'virtuanes_trace.txt'), 'w') as f:
        f.write(trace)

    problems = []
    if not trace:
        problems.append('no trace: is this a `make HWTEST=1` build?')
    else:
        check(trace, log, problems)
    print('%-14s %s' % (name, 'PASS' if not problems else 'FAIL: ' + '; '.join(problems)))
    return not problems


def main():
    if not os.path.exists(APP):
        sys.exit('%s not found; build it with tools/build.sh HWTEST=1' % APP)
    with tempfile.TemporaryDirectory() as tmp:
        rom = os.path.join(tmp, 'hwpathtest.nes')
        with open(rom, 'wb') as f:
            f.write(smoketest.assemble())
        results = [run_case(name, hwtest, steps, check, rom)
                   for name, hwtest, steps, check in CASES]
    print('%d/%d passed; screenshots, logs and traces in %s' % (sum(results), len(results), OUT))
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
