from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
PUBLIC = ROOT / "include" / "testing"


class CompileAPI(unittest.TestCase):
    def test_exported_macros(self):
        umbrella = (ROOT / "include" / "ariadne-testing.hpp").read_text()
        self.assertIn('#include "testing/macros.hpp"', umbrella)
        self.assertIn('#include "testing/reporters.hpp"', umbrella)
        self.assertNotIn('#include "testing/compile.hpp"', umbrella)
        self.assertNotIn('#include "testing/checks.hpp"', umbrella)

        # These are retired headers, not compatibility aliases.
        self.assertFalse((PUBLIC / "compile.hpp").exists())
        self.assertFalse((PUBLIC / "checks.hpp").exists())

        header = (PUBLIC / "macros.hpp").read_text()
        for macro in (
            "ARIADNE_TESTING_COMPILE_CONCEPT",
            "ARIADNE_TESTING_COMPILE_TRAIT",
            "ARIADNE_TESTING_COMPILE_SAME_TYPE",
            "ARIADNE_TESTING_PROVE_CTX",
            "ARIADNE_TESTING_EXPECT_EQUAL_CTX",
            "ARIADNE_TESTING_ADD_TEST_TO",
            "ARIADNE_TESTING_ADD_VARIANT_TEST",
        ):
            self.assertRegex(header, r"(?m)^#define\s+" + macro + r"\b")
        self.assertIn("static_assert", header)

        # Test the actual architectural invariant, not the previous path:
        # every public Testing macro is defined exactly once in macros.hpp.
        declaration = re.compile(r"(?m)^#define\s+(ARIADNE_TESTING_[A-Z0-9_]+)\b")
        public = [
            name for name in declaration.findall(header)
            if not name.endswith("_HPP")
        ]
        self.assertGreaterEqual(len(public), 86)
        self.assertEqual(len(public), len(set(public)))
        for path in PUBLIC.glob("*.hpp"):
            if path.name == "macros.hpp":
                continue
            misplaced = [
                name for name in declaration.findall(path.read_text())
                if not name.endswith("_HPP")
            ]
            self.assertEqual(misplaced, [], f"Macro definitions outside macros.hpp: {path}")
