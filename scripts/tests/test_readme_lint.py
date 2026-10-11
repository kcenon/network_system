from contextlib import redirect_stdout
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "readme_lint.py"
spec = importlib.util.spec_from_file_location("readme_lint", SCRIPT)
lint = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = lint
spec.loader.exec_module(lint)

HEADER = "# Network System\n\nStatus: active | Release: [v0.1.1](https://github.com/kcenon/network_system/releases/tag/v0.1.1)\n\n"
BADGE = "https://github.com/kcenon/network_system/actions/workflows/ci.yml/badge.svg"


class ReadmeLintTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        workflow = self.root / ".github/workflows/ci.yml"
        workflow.parent.mkdir(parents=True)
        workflow.write_text("name: CI\n")

    def report(self, text):
        return lint.lint(HEADER + text, self.root)

    def assertPasses(self, text):
        self.assertEqual(self.report(text).findings, [])

    def assertFails(self, text, diagnostic):
        self.assertTrue(any(diagnostic in item.message for item in self.report(text).findings), text)

    def test_technical_numbers_and_nonclaim_words(self):
        self.assertPasses("C++20, CMake 3.25+, TLS 1.2/1.3, HTTP/2, RFC 9000/9001/9002.\n"
                          "Listen on port 9090. Use GCC 13+, OpenSSL 3.0+, and ASIO 1.30.2+.\n"
                          "Pre-1.0 stability terms take effect after release.\n"
                          "A robustness_test.cpp fixture is not a reliability claim.\n")

    def test_english_qualifiers(self):
        for claim in ("production-ready", "Enterprise Grade", "battle tested", "blazing",
                      "world-class", "comprehensive", "robust", "seamless", "guaranteed",
                      "QUIC RFC compliant", "HIPAA", "RAII Grade A", "100% smart pointers",
                      "Zero manual memory management", "ASAN clean", "zero data races",
                      "Same performance as direct API"):
            with self.subTest(claim=claim):
                self.assertTrue(self.report(claim).findings)

    def test_korean_claims_and_figures(self):
        for claim in ("견고한 구현", "원활한 통합", "완벽한 지원", "누수 없음", "성능 동일",
                      "초당 100K 메시지", "80퍼센트", "10배 빠름", "0.5 마이크로초"):
            with self.subTest(claim=claim):
                self.assertTrue(self.report(claim).findings)

    def test_performance_formats(self):
        for claim in ("~769K msg/s", "1,300 ns", "3.2GHz", "0.5 µs", "0.8 μs",
                      "5ms", "2x faster", "2× faster", "100 MB/s", "1 GiB/sec",
                      "75-80% migrated", "80 percent", "~`769K` **msg/s**"):
            with self.subTest(claim=claim):
                self.assertFails(claim, "measured figure")

    def test_source_comments_are_not_exemptions(self):
        for marker in ("<!-- source: nowhere -->", "<!-- source: docs/BENCHMARKS.md#synthetic-benchmarks (2026-10-11, Linux) -->"):
            self.assertFails("80% coverage " + marker, "measured figure")

    def test_inline_code_and_html_prose_are_checked(self):
        self.assertFails("Use `robust` networking", "unsupported qualifier")
        self.assertFails("<strong>100%</strong> coverage", "measured figure")
        self.assertFails("<details>\n<summary>Comprehensive support</summary>\n</details>", "unsupported qualifier")

    def test_fences_comments_and_link_destinations(self):
        self.assertPasses("```cpp\nint timeout_ms = 100; // robust\n```\n"
                          "~~~text\n100% comprehensive\n~~~\n"
                          "    robust fixture text\n"
                          "<!-- robust 100%\ncomprehensive -->\n"
                          "[Results](https://example.org/robust/100ms)\n")
        self.assertFails("````cpp\n```\nrobust\n````\nrobust", "unsupported qualifier")

    def test_blockquote_fences_and_claims(self):
        self.assertPasses("> ```cpp\n> // robust\n> ```\n")
        self.assertFails("> robust", "unsupported qualifier")

    def test_line_numbers_survive_comments_and_fences(self):
        text = HEADER + "<!-- hidden\ncomment -->\n```cpp\nint x=1;\n```\nrobust\n"
        report = lint.lint(text, self.root)
        self.assertEqual(next(f.line for f in report.findings if "unsupported qualifier" in f.message), 10)

    def test_existing_workflow_badges_in_markdown_forms(self):
        for image in (f"[![CI]({BADGE})](https://github.com/kcenon/network_system/actions/workflows/ci.yml)",
                      f'![CI](<{BADGE}?branch=main> "CI status")',
                      f"![CI][build]\n\n[build]: {BADGE}",
                      f"![CI][]\n\n[CI]: <{BADGE}>",
                      f"![CI]\n\n[CI]: {BADGE}",
                      f'<img src="{BADGE}" alt="CI">',
                      f'<img\n  src="{BADGE}"\n  alt="CI">'):
            with self.subTest(image=image):
                self.assertPasses(image)

    def test_badge_failures(self):
        for image in (f"![Missing]({BADGE.replace('ci.yml', 'missing.yml')})",
                      f"![Wrong repo]({BADGE.replace('network_system', 'other')})",
                      f"![HTTP]({BADGE.replace('https:', 'http:')})",
                      "![Static](https://img.shields.io/badge/coverage-80%25-green)",
                      "![CI][undefined]", "![CI](", "<img alt='missing source'>",
                      "<img src='https://codecov.io/example/badge.svg'>",
                      "<img\n src='https://codecov.io/example/badge.svg'\n>"):
            with self.subTest(image=image):
                self.assertFails(image, "existing repository workflow")

    def test_first_screen_information(self):
        for source in (HEADER.replace("active", "unknown"), HEADER.replace("Status:", "State:"),
                       HEADER.replace("[v0.1.1]", "[Latest]"), "\n" * 20 + HEADER,
                       HEADER.replace("[v0.1.1]", "[v1.0.0]"),
                       HEADER.replace("network_system/releases", "other/releases")):
            with self.subTest(source=source):
                self.assertTrue(lint.lint(source, self.root).findings)
        reference = HEADER.replace("[v0.1.1](https://github.com/kcenon/network_system/releases/tag/v0.1.1)", "[v0.1.1][release]")
        reference += "\n[release]: https://github.com/kcenon/network_system/releases/tag/v0.1.1\n"
        self.assertEqual(lint.lint(reference, self.root).findings, [])

    def test_line_limit_and_density(self):
        allowed = HEADER + "Documentation.\n" * (300 - len(HEADER.splitlines()))
        self.assertEqual(lint.lint(allowed, self.root).findings, [])
        self.assertTrue(any("line limit" in f.message for f in lint.lint(allowed + "Extra\n", self.root).findings))
        report = self.report("robust 100%\nrobust\n")
        self.assertEqual(report.claim_lines, 2)  # count unique matching source lines
        self.assertEqual(report.density, 100 * 2 / report.lines)
        self.assertTrue(any("density" in f.message for f in report.findings))

    def test_cli_multiple_files_and_exit_codes(self):
        good = self.root / "README.md"
        bad = self.root / "README.kr.md"
        good.write_text(HEADER)
        bad.write_text(HEADER + "robust\n")
        with redirect_stdout(io.StringIO()) as output:
            self.assertEqual(lint.main([str(good)]), 0)
            self.assertEqual(lint.main([str(good), str(bad)]), 1)
            self.assertEqual(lint.main([str(self.root / "absent")]), 1)
        self.assertIn(f"{bad}:5:", output.getvalue())
        result = subprocess.run([sys.executable, str(SCRIPT), str(bad)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)


if __name__ == "__main__":
    unittest.main()
