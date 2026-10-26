"""CLI contracts, repeatability, and deliberately corrupted CSV rejection."""
import csv
import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from run_validation import execute, parse_csv

EXE = Path(sys.argv.pop(1)).resolve()


def encode(row):
    out = io.StringIO()
    writer = csv.DictWriter(out, fieldnames=row.keys())
    writer.writeheader()
    writer.writerow(row)
    return out.getvalue()


class Contracts(unittest.TestCase):
    def test_bad_csv_rejected(self):
        with self.assertRaises(ValueError):
            parse_csv('policy,waf\ngreedy,1.0\n')

    def test_corrupt_metrics_rejected(self):
        baseline = execute(EXE, ['demo'])
        for key, value in [('waf', '999'), ('correctness_violations', '1'),
                           ('physical_programs', '999'), ('wall_seconds', 'nan'),
                           ('host_successful', '-1'), ('free_pages', '999999'),
                           ('operation_errors', '1')]:
            with self.subTest(key=key):
                row = baseline.copy()
                row[key] = value
                with self.assertRaises(ValueError):
                    parse_csv(encode(row))

    def test_determinism(self):
        args = ['benchmark', '--seed', '73', '--policy', 'adaptive', '--workload', 'hotcold']
        a, b = execute(EXE, args), execute(EXE, args)
        for field in ('wall_seconds', 'simulator_throughput'):
            a.pop(field)
            b.pop(field)
        self.assertEqual(a, b)

    def test_policy_same_trace(self):
        a = execute(EXE, ['benchmark', '--seed', '88', '--policy', 'greedy'])
        b = execute(EXE, ['benchmark', '--seed', '88', '--policy', 'adaptive'])
        self.assertEqual(a['trace_hash'], b['trace_hash'])

    def test_invalid_options(self):
        for args in ([], ['wrong'], ['benchmark', '--seed', '-1'],
                     ['benchmark', '--seed', '4294967296'], ['benchmark', '--seed'],
                     ['benchmark', '--alloc', 'wrong'], ['benchmark', '--pages', '0'],
                     ['benchmark', '--policy', 'greedy', '--policy', 'adaptive'],
                     ['benchmark', '--weight', 'nan'], ['benchmark', '--decay', '0'],
                     ['benchmark', '--workload', 'wrong'], ['test-workload'],
                     ['demo', '--input', 'x'], ['benchmark', '--unknown', 'x']):
            with self.subTest(args=args):
                proc = subprocess.run([str(EXE), *args], capture_output=True)
                self.assertEqual(proc.returncode, 2)

    def test_csv_input_and_output(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'trace.csv'
            output = Path(directory) / 'metrics.csv'
            path.write_text('op,lpn,value\nW,0,18446744073709551615\nR,0,0\nT,0,0\nR,0,0\n')
            row = execute(EXE, ['test-workload', '--input', path, '--output', output])
            self.assertEqual(row, parse_csv(output.read_text()))
            self.assertEqual(row['host_successful'], '1')
            for contents in ('', 'bad\n', 'op,lpn,value\nX,0,0\n',
                             'op,lpn,value\nW,0,1,2\n', 'op,lpn,value\nW,-1,0\n'):
                path.write_text(contents)
                proc = subprocess.run([str(EXE), 'test-workload', '--input', str(path)], capture_output=True)
                self.assertEqual(proc.returncode, 2)

    def test_invalid_workload_address(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'trace.csv'
            path.write_text('op,lpn,value\nW,999999,1\n')
            proc = subprocess.run([str(EXE), 'test-workload', '--input', str(path)], capture_output=True, text=True)
            self.assertEqual(proc.returncode, 1)
            with self.assertRaises(ValueError):
                parse_csv(proc.stdout)


if __name__ == '__main__':
    unittest.main()
