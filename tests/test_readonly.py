"""Read-only/in-memory checks: no TF access, no temporary directories, no network.

Run from the installation package: py -B tests\test_readonly.py
Requires Pillow for bitmap checks. Does not replace the full test_tools suite.
"""
import io
import unittest
from pathlib import Path
from unittest import mock

import test_tools as shared


class ReadOnlyChecks(unittest.TestCase):
    def test_font_ink_all_22_samples(self):
        # Reuse the exact full-suite assertion without its filesystem fixtures.
        shared.FontAndIds().test_cjk_ink_not_clipped()

    def test_lowercase_id_rejected_before_any_write(self):
        students = "studentId,class,displayName\nstud01,2A,陳大文\n"
        teachers = "teacherId,displayName\n01,李老師\n"

        def read_memory(path, *args, **kwargs):
            if path.name == "readonly-students.csv":
                return io.StringIO(students)
            if path.name == "readonly-teachers.csv":
                return io.StringIO(teachers)
            raise AssertionError("Unexpected filesystem read")

        with mock.patch.object(Path, "open", read_memory), mock.patch.object(
            Path, "mkdir", side_effect=AssertionError("Unexpected filesystem write")
        ) as write:
            with self.assertRaisesRegex(ValueError, "5位"):
                shared.generate(Path("readonly-students.csv"), Path("readonly-teachers.csv"),
                         Path("not-created"))
            write.assert_not_called()


if __name__ == "__main__":
    unittest.main(verbosity=2)