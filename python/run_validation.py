"""Standard-library-only validation, development selection, and held-out evaluation.

A nonzero CLI exit, malformed metric, or nonzero correctness count aborts the run.
Development seeds 11/12 never appear in evaluation seeds 101..105.
"""
import argparse
import csv
import io
import json
import math
import platform
import shutil
import statistics
import subprocess
from pathlib import Path
from compare_policies import summarize

ROOT = Path(__file__).resolve().parents[1]
WORKLOADS = ('sequential', 'uniform', 'hotcold', 'mixed')
POLICIES = ('greedy', 'adaptive')
ALLOCATIONS = ('basic', 'wear')
COUNTS = ('seed', 'blocks', 'pages_per_block', 'logical_pages', 'op_pages',
          'operations', 'trace_hash', 'host_attempted', 'host_successful',
          'physical_programs', 'gc_migrations', 'gc_cycles', 'erases',
          'invalid_ops', 'failed_writes', 'free_pages', 'erase_min', 'erase_max',
          'correctness_violations', 'operation_errors', 'threshold', 'decay', 'watermark')
FLOATS = ('erase_stddev', 'waf', 'wall_seconds', 'simulator_throughput', 'weight')


def parse_csv(text):
    reader = csv.DictReader(io.StringIO(text))
    required = set(COUNTS + FLOATS + ('policy', 'allocation', 'workload', 'erase_counts'))
    if reader.fieldnames is None or set(reader.fieldnames) != required:
        raise ValueError('missing, extra, or incorrect CSV columns')
    if len(reader.fieldnames) != len(required):
        raise ValueError('duplicate CSV columns')
    rows = list(reader)
    if len(rows) != 1:
        raise ValueError('expected exactly one metrics row')
    row = rows[0]
    if None in row or any(value is None for value in row.values()):
        raise ValueError('CSV row width mismatch')
    for key in COUNTS:
        if not row[key].isdigit():
            raise ValueError(f'invalid nonnegative integer: {key}')
    for key in FLOATS:
        value = float(row[key])
        if not math.isfinite(value) or value < 0:
            raise ValueError(f'invalid finite nonnegative metric: {key}')
    n = {key: int(row[key]) for key in COUNTS}
    if n['correctness_violations'] or n['operation_errors']:
        raise ValueError('correctness or operation failure in CSV')
    if n['host_attempted'] != n['host_successful'] + n['failed_writes']:
        raise ValueError('host write accounting mismatch')
    if n['physical_programs'] != n['host_successful'] + n['gc_migrations']:
        raise ValueError('physical program accounting mismatch')
    expected = n['physical_programs'] / n['host_successful'] if n['host_successful'] else 0
    if not math.isclose(float(row['waf']), expected, rel_tol=1e-9, abs_tol=1e-10):
        raise ValueError('WAF arithmetic mismatch')
    total = n['blocks'] * n['pages_per_block']
    if n['logical_pages'] + n['op_pages'] != total or n['free_pages'] > total:
        raise ValueError('geometry/free-space mismatch')
    if n['erase_min'] > n['erase_max'] or n['gc_cycles'] != n['erases']:
        raise ValueError('erase accounting mismatch')
    if row['policy'] not in POLICIES or row['allocation'] not in ALLOCATIONS:
        raise ValueError('unrecognized policy/allocation')
    counts = row['erase_counts'].split('|')
    if len(counts) != n['blocks'] or any(not x.isdigit() for x in counts):
        raise ValueError('invalid per-block erase counts')
    erases = list(map(int, counts))
    if min(erases) != n['erase_min'] or max(erases) != n['erase_max']:
        raise ValueError('erase extrema mismatch')
    if not math.isclose(statistics.pstdev(erases), float(row['erase_stddev']), rel_tol=1e-9, abs_tol=1e-10):
        raise ValueError('erase spread mismatch')
    return row


def execute(exe, arguments):
    proc = subprocess.run([str(exe), *map(str, arguments)], capture_output=True, text=True)
    if proc.returncode:
        raise RuntimeError(f'CLI exit {proc.returncode}: {arguments}\n{proc.stdout}\n{proc.stderr}')
    return parse_csv(proc.stdout)


def locate(path, build):
    if path:
        exe = Path(path).resolve()
    else:
        cmake = shutil.which('cmake')
        if not cmake:
            raise RuntimeError('CMake not found; put it on PATH or pass --exe')
        subprocess.run([cmake, '-S', str(ROOT), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release'], check=True)
        subprocess.run([cmake, '--build', str(build), '--config', 'Release'], check=True)
        exe = build / ('flashcore.exe' if platform.system() == 'Windows' else 'flashcore')
        if not exe.exists():
            exe = build / 'Release' / 'flashcore.exe'
    if not exe.is_file():
        raise RuntimeError(f'executable missing: {exe}')
    return exe


def write_rows(path, rows):
    if not rows:
        raise ValueError('refusing empty result file')
    with path.open('w', newline='', encoding='utf-8') as file:
        writer = csv.DictWriter(file, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)


def tune(exe, out):
    # Small declared candidate set: all four controls vary; candidates are not AGC.
    candidates = [dict(weight=0.25, threshold=2, decay=128, watermark=0),
                  dict(weight=0.5, threshold=3, decay=256, watermark=1),
                  dict(weight=1.0, threshold=2, decay=512, watermark=1)]
    rows = []
    scores = []
    for candidate in candidates:
        group = []
        for workload in WORKLOADS:
            for seed in (11, 12):
                arguments = ['benchmark', '--policy', 'adaptive', '--alloc', 'wear',
                             '--seed', seed, '--workload', workload, '--operations', 2000]
                for key, value in candidate.items():
                    arguments += ['--' + key, value]
                row = execute(exe, arguments)
                rows.append(row)
                group.append(float(row['waf']))
        scores.append(statistics.mean(group))
    best = min(range(len(candidates)), key=lambda i: (scores[i], i))
    frozen = {'parameters': candidates[best], 'development_seeds': [11, 12],
              'evaluation_seeds': [101, 102, 103, 104, 105],
              'selection': 'lowest mean development WAF; first candidate wins ties',
              'candidate_mean_waf': scores, 'candidate_parameters': candidates,
              'geometry': {'blocks': 24, 'pages': 8, 'logical': 128},
              'operations': 5000,
              'wear_acceptability': 'adaptive mean erase stddev <= 1.10 * greedy, per workload/allocation; exploratory threshold'}
    write_rows(out / 'development.csv', rows)
    # Written before any held-out evaluation is launched.
    (out / 'frozen_parameters.json').write_text(json.dumps(frozen, indent=2) + '\n')
    return frozen


def evaluate(exe, out, frozen):
    rows = []
    for workload in WORKLOADS:
        for allocation in ALLOCATIONS:
            for seed in frozen['evaluation_seeds']:
                pair = []
                for policy in POLICIES:
                    args = ['benchmark', '--policy', policy, '--alloc', allocation,
                            '--seed', seed, '--workload', workload,
                            '--operations', frozen['operations']]
                    for key, value in frozen['parameters'].items():
                        args += ['--' + key, value]
                    row = execute(exe, args)
                    rows.append(row)
                    pair.append(row)
                if pair[0]['trace_hash'] != pair[1]['trace_hash']:
                    raise RuntimeError('policy traces differ')
    write_rows(out / 'evaluation.csv', rows)
    (out / 'comparison.md').write_text(summarize(rows))
    return rows


def compatibility(exe, out, frozen):
    rows = []
    for blocks in (8, 12, 16):
        for pages in (2, 4, 8):
            for op_blocks in (2, 3):
                logical = (blocks - op_blocks) * pages
                for policy in POLICIES:
                    for allocation in ALLOCATIONS:
                        args = ['benchmark', '--blocks', blocks, '--pages', pages,
                                '--logical', logical, '--policy', policy, '--alloc', allocation,
                                '--workload', 'mixed', '--seed', 201, '--operations', 1000]
                        for key, value in frozen['parameters'].items():
                            args += ['--' + key, value]
                        rows.append(execute(exe, args))
    write_rows(out / 'compatibility.csv', rows)
    unsupported = []
    for blocks, pages, logical in ((2, 4, 1), (4, 0, 1), (4, 2, 6), (0, 2, 1)):
        proc = subprocess.run([str(exe), 'inspect', '--blocks', str(blocks),
                               '--pages', str(pages), '--logical', str(logical)],
                              capture_output=True, text=True)
        if proc.returncode != 2:
            raise RuntimeError('unsupported geometry was not rejected')
        unsupported.append({'blocks': blocks, 'pages': pages, 'logical': logical,
                            'exit': proc.returncode, 'reason': proc.stderr.strip()})
    write_rows(out / 'unsupported.csv', unsupported)
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe')
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--output', type=Path, default=ROOT / 'results')
    parser.add_argument('--mode', choices=('development', 'evaluation', 'all'), default='all')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    exe = locate(args.exe, args.build)
    if args.mode in ('development', 'all'):
        frozen = tune(exe, args.output)
        print('Development complete; parameters frozen before evaluation:', frozen['parameters'])
    else:
        frozen = json.loads((args.output / 'frozen_parameters.json').read_text())
    if args.mode in ('evaluation', 'all'):
        rows = evaluate(exe, args.output, frozen)
        grid = compatibility(exe, args.output, frozen)
        report = (f'# Automated validation\n\n{len(rows)} held-out runs, '
                  f'{len(grid)} compatibility runs; zero correctness violations.\n\n'
                  'Warm-up: fill every logical page, reset counters, retain state/heat. '
                  'This is not a steady-state or hardware performance claim.\n\n' + summarize(rows))
        (args.output / 'validation-report.md').write_text(report)
        print(f'PASS: {len(rows)} held-out runs; {len(grid)} compatibility runs; 4 expected rejected geometries')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, RuntimeError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'VALIDATION FAILED: {error}')
