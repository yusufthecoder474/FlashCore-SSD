"""Rebuild and regenerate evidence without third-party Python packages.

Run from any directory. Pass --sanitizers for supported GCC/Clang or MSVC builds.
The script does not delete existing build directories. For a clean rebuild,
choose a new --build directory. Each subprocess has its exact arguments, exit
status, stdout and stderr captured in results/reproduction.json.
"""
import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, default=ROOT / 'build-verified')
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--disable-leak-check', action='store_true',
                        help='only for environments where LeakSanitizer cannot access /proc')
    args = parser.parse_args()
    args.build = args.build.resolve()
    results = ROOT / 'results'
    results.mkdir(exist_ok=True)
    cmake = shutil.which('cmake')
    ctest = shutil.which('ctest')
    if not cmake or not ctest:
        raise SystemExit('cmake and ctest must be on PATH')
    records = []
    environment = os.environ.copy()
    environment['UBSAN_OPTIONS'] = 'halt_on_error=1'
    if args.disable_leak_check:
        environment['ASAN_OPTIONS'] = 'detect_leaks=0'

    def run(command, expected=0):
        process = subprocess.run(list(map(str, command)), capture_output=True,
                                 text=True, cwd=ROOT, env=environment)
        records.append({'command': list(map(str, command)), 'cwd': str(ROOT),
                        'exit': process.returncode, 'expected_exit': expected,
                        'stdout': process.stdout, 'stderr': process.stderr})
        (results / 'reproduction.json').write_text(json.dumps({
            'platform': platform.platform(), 'python': sys.version,
            'sanitizers': args.sanitizers, 'leak_check_disabled': args.disable_leak_check,
            'commands': records}, indent=2) + '\n')
        if process.returncode != expected:
            raise RuntimeError(f'command failed: {command}\n{process.stdout}\n{process.stderr}')
        print('PASS:', ' '.join(map(str, command)))

    def executable(directory, name):
        path = directory / (name + '.exe' if platform.system() == 'Windows' else name)
        return path if path.is_file() else directory / 'Release' / (name + '.exe')

    run([cmake, '-S', ROOT, '-B', args.build, '-DCMAKE_BUILD_TYPE=Release',
         '-DFLASHCORE_SANITIZE=' + ('ON' if args.sanitizers else 'OFF')])
    run([cmake, '--build', args.build, '--config', 'Release', '--parallel', '2'])
    run([ctest, '--test-dir', args.build, '-C', 'Release', '--output-on-failure'])
    exe = executable(args.build, 'flashcore')
    run([exe, 'demo', '--output', results / 'demo.csv'])
    run([exe, 'test-workload', '--input', ROOT / 'workloads/trim-reproducer.csv'])
    run([executable(args.build, 'fault_campaign')])
    # Use the committed frozen controls. Never retune after inspecting held-out results.
    run([sys.executable, ROOT / 'python/run_validation.py', '--exe', exe,
         '--mode', 'evaluation', '--output', results])
    bug = args.build.parent / (args.build.name + '-bug')
    run([cmake, '-S', ROOT, '-B', bug, '-DFLASHCORE_BUG_TRIM=ON', '-DCMAKE_BUILD_TYPE=Release'])
    run([cmake, '--build', bug, '--config', 'Release', '--parallel', '2'])
    run([executable(bug, 'flashcore_tests'), 'trim'], expected=1)
    run([executable(bug, 'flashcore'), 'test-workload', '--input',
         ROOT / 'workloads/trim-reproducer.csv', '--seed', '0'], expected=1)
    print('Evidence written to', results / 'reproduction.json')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError) as error:
        raise SystemExit('REPRODUCTION FAILED: ' + str(error))
