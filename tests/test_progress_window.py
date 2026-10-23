"""tools/progress_window.ps1: how the installer's window understands what the tools print (the pure functions)."""
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parent.parent
POWERSHELL = shutil.which('powershell') or shutil.which('pwsh')


def run(code):
    script = f". '{ROOT / 'tools' / 'progress_window.ps1'}'; [Console]::OutputEncoding = [Text.Encoding]::UTF8; {code}"
    r = subprocess.run([POWERSHELL, '-NoProfile', '-Command', script], capture_output=True, text=True, encoding='utf-8')
    return r.stdout.strip()


@unittest.skipUnless(POWERSHELL, 'needs PowerShell')
class ToolLineTests(unittest.TestCase):
    def test_bar_line(self):
        out = run("$t = ConvertFrom-ToolLine '[######----]  21%  about 2 h 05 min left  (compiling main: 3 of 9 files)'; "
                  "\"$($t.Kind)|$($t.Percent)|$(Format-ProgressLeft $t.Left 'es')|$(Format-ProgressLabel $t.Label 'en')\"")
        self.assertEqual(out, 'bar|21|unas 2 h 05 min|Compiling: part 3 of 9')

    def test_estimating_bar_has_no_time(self):
        out = run("$t = ConvertFrom-ToolLine '[----------]   1%  estimating the time left  (linking)'; "
                  "\"$($t.Percent)|[$(Format-ProgressLeft $t.Left 'es')]|$(Format-ProgressLabel $t.Label 'es')\"")
        self.assertEqual(out, '1|[]|Uniendo todo en un solo programa')

    def test_other_lines(self):
        out = run("(ConvertFrom-ToolLine '    msvc.zip: 40% of 570 MB').Kind; (ConvertFrom-ToolLine '[4/9] Copy the program').Index; "
                  "(ConvertFrom-ToolLine 'imports: 0 strong').Kind")
        self.assertEqual(out.split(), ['download', '4', 'text'])


if __name__ == '__main__':
    unittest.main()
