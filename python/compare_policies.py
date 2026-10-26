"""Aggregate independently validated rows; no combined efficiency score."""
import argparse
import csv
import statistics
from pathlib import Path

METRICS = ('waf', 'gc_migrations', 'gc_cycles', 'erase_min', 'erase_max',
           'erase_stddev', 'correctness_violations')


def summarize(rows):
    groups = {}
    for row in rows:
        key = (row['workload'], row['allocation'], row['policy'])
        groups.setdefault(key, []).append(row)
    lines = ['| Workload | Allocation | Policy | Metric | Mean | Median |',
             '|---|---|---|---|---:|---:|']
    for key, group in sorted(groups.items()):
        for metric in METRICS:
            values = [float(r[metric]) for r in group]
            lines.append('| ' + ' | '.join(key) + f' | {metric} | '
                         f'{statistics.mean(values):.5f} | {statistics.median(values):.5f} |')
    return '\n'.join(lines) + '\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('csv', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = summarize(list(csv.DictReader(args.csv.open(newline=''))))
    if args.output:
        args.output.write_text(result, encoding='utf-8')
    else:
        print(result)
