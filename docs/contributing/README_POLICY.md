---
doc_id: "NET-GUID-031"
doc_title: "README Editing Policy"
doc_version: "1.0.0"
doc_date: "2026-10-11"
doc_status: "Released"
project: "network_system"
category: "GUID"
---

# README Editing Policy

The English and Korean READMEs are landing pages. Keep their instructions and
limitations aligned. Put detailed procedures in guides, retaining working links.

## Local checks

Run from the repository root with Python 3.9 or later:

```bash
python3 scripts/readme_lint.py README.md README.kr.md
python3 -m unittest discover -s scripts/tests -p 'test_readme_lint.py'
```

The linter uses the Python standard library and makes no network requests.
No arguments checks both READMEs. Any violation or unreadable input returns exit 1;
invalid CLI arguments return exit 2. Diagnostics include the file and source line.
Documentation Audit runs these checks as blocking steps before fetching the existing audit tool.

## Landing-page rules

- At most 300 physical lines, including blanks and code fences. Do not pad content
  or pack multiple prose lines together to manipulate the limit or density.
- In the first 20 physical lines, show `Status: active`, `maintenance`, or
  `experimental` and a linked published release version. Confirm the release on
  GitHub when editing; offline lint checks the link form, not live release existence.
- Keep a usable introduction, requirements, one C++/CMake example, and guide links.
  Technical version numbers, RFC identifiers, ports, and configuration are allowed.
- Describe observable behavior rather than unsupported quality assertions. The
  linter checks English marketing/safety terms and common Korean equivalents.
  Passing a phrase list is not a substitute for editorial review or translation review.
- Put all measured performance, coverage, and migration percentages in the relevant
  evidence document, not the README. This repository deliberately uses the stricter
  **no headline figures** alternative: source comments never exempt a figure.
  Inline code formatting also does not exempt a prose claim.
- Only GitHub Actions workflow badges for this repository are allowed. Each image
  must reference an existing `.github/workflows/*.yml` or `*.yaml` file. Use ordinary
  links for the license and Codecov. Markdown reference images and HTML images
  receive the same checks as inline images.

Density is `100 * matching source lines / total physical lines`. A line containing
both a qualifier and a figure counts once; fences, indented code, link destinations,
and HTML comments do not contribute matches. Density must not exceed 1.0.
Individual forbidden claims fail regardless of density, so valid landing pages have
zero matching lines. Fenced sample code is excluded because numeric literals and
fixture strings are not performance claims. Reviewers must not hide claims in code.

## Evidence and review

[Benchmark documentation](../BENCHMARKS.md) distinguishes historical summaries,
raw artifacts, benchmark sources, and suggested reproduction commands. A source file
or edit timestamp is not a measured result or measurement date. Do not invent missing
provenance or use synthetic measurements as network throughput.

Check moved links relative to their new document. Validate changed C++/CMake examples
against installed public packages. Keep pre-release API qualifications, historical
release errata, dependency setup, and asynchronous lifecycle handling when shortening.

Do not describe sanitizer jobs as proof of defect freedom. See actual workflow
failure policy, test exclusions, and suppression files, including project-specific
LSan suppressions. Do not equate coverage configuration targets with measured coverage.
