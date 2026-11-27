import importlib.util
from pathlib import Path
import tempfile
import unittest

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("report_imports", root / "tools/report-imports.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReportTests(unittest.TestCase):
    def test_pinned_database(self):
        libraries, symbols = module.sdk_symbols(root / "external/vita-headers/db/360")
        self.assertEqual(libraries[0xcae9ace6], "SceLibKernel")
        self.assertEqual(symbols[(0xcae9ace6, "function", 0x99ba173e)], "sceIoLseek")

    def test_scope_duplicates_unknowns_and_partial_lines(self):
        line = "[loader] UNRESOLVED_IMPORT kind=function library=0xCAE9ACE6 nid=0x99BA173E\n"
        rows = module.report(line * 2 +
                             "UNRESOLVED_IMPORT kind=variable library=0x12345678 nid=0x99ba173e\n"
                             "UNRESOLVED_IMPORT kind=function library=0xCAE9ACE6 nid=0x99BA\n",
                             {0xcae9ace6: "SceLibKernel"},
                             {(0xcae9ace6, "function", 0x99ba173e): "sceIoLseek"})
        self.assertEqual(len(rows), 2)
        self.assertIsNone(rows[0]["symbol"])
        self.assertEqual(rows[0]["kind"], "variable")
        self.assertEqual(rows[1]["symbol"], "sceIoLseek")
        self.assertEqual(rows[1]["log_occurrences"], 2)
        self.assertEqual(module.report("unrelated log", {}, {}), [])

    def test_database_library_and_symbol_kind_boundaries(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "fixture.yml").write_text(
                "      One:\n        nid: 0x1\n        functions:\n          func: 0x2\n"
                "        variables:\n          var: 0x2\n"
                "      Two:\n        nid: 0x3\n        functions:\n          other: 0x2\n")
            _, symbols = module.sdk_symbols(Path(directory))
            self.assertEqual(symbols[(1, "function", 2)], "func")
            self.assertEqual(symbols[(1, "variable", 2)], "var")
            self.assertEqual(symbols[(3, "function", 2)], "other")


if __name__ == "__main__":
    unittest.main()
