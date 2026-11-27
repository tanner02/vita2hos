import importlib.util
from pathlib import Path
import tempfile
import unittest

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("report_imports", root / "tools/report-imports.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReportTests(unittest.TestCase):
    def test_argument_registers_are_optional_and_first_use_only(self):
        prefix = ("UNSUPPORTED_CALL kind=function library=0x12345678 "
                  "nid=0x11111111 stub=0x81000000 return_address=0x81002001")
        registers = " r0=0xABCDEF01 r1=0x22223333 r2=0x44444444 r3=0x55555555"
        rows = module.called_imports(prefix + registers + "\n" + prefix + "\n", {}, {})
        self.assertEqual(rows[0]["first_argument_registers"], {
            "r0": "0xabcdef01", "r1": "0x22223333", "r2": "0x44444444", "r3": "0x55555555"})
        self.assertEqual(rows[0]["log_occurrences"], 2)
        rows = module.called_imports(prefix + "\n" + prefix + registers + "\n", {}, {})
        self.assertNotIn("first_argument_registers", rows[0])

    def test_legacy_context_switch_orphans_and_variables(self):
        rows = module.report(
            "Could not resolve NID 0x11111111, export not found!\n"
            "Resolving imports for library NID 0xCAE9ACE6\n"
            "Could not resolve NID 0x99BA173E, export not found!\n"
            "Resolving imports for library NID 0x12345678\n"
            "Trying to resolve variable NID 0x99BA173E\n", {}, {})
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0]["library_nid"], "0x12345678")
        self.assertEqual(rows[0]["kind"], "variable")
        self.assertEqual(rows[1]["library_nid"], "0xcae9ace6")
        self.assertEqual(rows[1]["evidence"], "legacy_context")

    def test_mixed_formats_do_not_double_count(self):
        legacy = ("Resolving imports for library NID 0xCAE9ACE6\n"
                  "Could not resolve NID 0x99BA173E, export not found!\n")
        structured = "UNRESOLVED_IMPORT kind=function library=0xCAE9ACE6 nid=0x99BA173E\n"
        rows = module.report(legacy + structured, {}, {})
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["log_occurrences"], 1)
        self.assertEqual(rows[0]["evidence"], "structured")
        rows = module.report(legacy + structured * 2, {}, {})
        self.assertEqual(rows[0]["log_occurrences"], 2)

    def test_first_runtime_uses_are_separate_from_binding_records(self):
        line = ("UNSUPPORTED_CALL kind=function library=0xCAE9ACE6 nid=0x15E2A45D "
                "stub=0x81001000 return_address=0x81002001\n")
        rows = module.called_imports(line * 2, {0xcae9ace6: "SceLibKernel"},
                                    {(0xcae9ace6, "function", 0x15e2a45d): "sceKernelCallModuleExit"})
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["log_occurrences"], 2)
        self.assertEqual(rows[0]["first_return_address"], "0x81002001")
        self.assertEqual(rows[0]["symbol"], "sceKernelCallModuleExit")
        self.assertEqual(module.report(line, {}, {}), [])

    def test_runtime_site_scope_partial_lines_and_first_address(self):
        prefix = "UNSUPPORTED_CALL kind=function library=0x12345678 nid=0x11111111 "
        log = prefix + "stub=0x81001000 return_address=0x81002001\n"
        log += prefix + "stub=0x81001000 return_address=0x81003001\n"
        log += prefix + "stub=0x81001010 return_address=0x123456789abcdef0\n"
        log += prefix + "stub=0x81001020 return_address=0x1234\n"
        rows = module.called_imports(log, {}, {})
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0]["first_return_address"], "0x81002001")
        self.assertIsNone(rows[0]["symbol"])
        self.assertEqual(rows[1]["first_return_address"], "0x123456789abcdef0")
        self.assertEqual(module.called_imports("UNRESOLVED_IMPORT kind=function library=0x12345678 nid=0x11111111", {}, {}), [])

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
