from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[2]
class CompileAPI(unittest.TestCase):
    def test_exported_macros(self):
        umbrella = (ROOT / "include" / "ariadne-testing.hpp").read_text()
        self.assertIn('#include "testing/compile.hpp"', umbrella)
        header = (ROOT / "include" / "testing" / "compile.hpp").read_text()
        for macro in ("ARIADNE_TEST_CONCEPT", "ARIADNE_TEST_TRAIT", "ARIADNE_TEST_SAME_TYPE"):
            self.assertIn("#define " + macro, header)
        self.assertIn("static_assert", header)
