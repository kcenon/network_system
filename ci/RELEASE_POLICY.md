# Default-branch protection and release versions

Tracked by [#1169](https://github.com/kcenon/network_system/issues/1169).
The default branch remains `main`.

## Required checks and pull requests

The reviewed configuration is [`.github/rulesets/main.json`](../.github/rulesets/main.json).
The **Main pull request and CI gates** ruleset targets only the default branch.
It requires a pull request, zero approving reviews, an up-to-date branch, and
these check-run contexts from GitHub Actions (app ID `15368`):

| Required context | What it verifies |
| --- | --- |
| `Minimal Build (Fast Feedback)` | Minimal Linux build |
| `ubuntu-24.04 / gcc` | GCC build and Unix CTest |
| `ubuntu-24.04 / clang` | Clang build and Unix CTest |
| `macos-15 / clang` | macOS build and Unix CTest |
| `windows-2022 / msvc` | Windows build; this job currently does not run CTest |

`CI` is the workflow name, not a check-run context. These jobs run on every
pull request targeting `main` without path filters. Unix CTest propagates
failures and rejects empty test discovery. The build jobs retain their existing
vcpkg/fallback behavior; requiring them does not establish that every build
used the vcpkg path. Review the original steps and logs when validating a release.

The ruleset blocks force pushes and deletion and has no bypass actors. Code-owner
and last-push approvals are not required. Normal PR merges remain available to
the single maintainer after the required checks pass.

The separate [Coherence gates ruleset](https://github.com/kcenon/network_system/rules/24470649)
continues to require `cross-system conformance linter` and
`SOUP Version Drift Detection` on both `main` and `develop`. Preserve it when
updating the main-only ruleset. Advisory sanitizer/downstream jobs and the
cross-repository dispatch notification are not added as required checks here.
Their failures still require accurate reporting; they are not passing evidence.

## Applying and verifying the configuration

A committed JSON file does not apply GitHub settings. Use an authenticated
maintainer session with repository administration access; no additional Actions
secret is needed.

1. Save the current ruleset list, each ruleset's details, and effective rules for
   `main` and `develop`. List with
   `gh api repos/kcenon/network_system/rulesets?includes_parents=true`
   (quote the endpoint in shells that expand `?`).
2. Validate the exact check contexts and app IDs on a fresh PR. Confirm successful
   build/test steps, rather than relying on a workflow name or summary job.
3. Find **Main pull request and CI gates** by name and repository source. If absent,
   create it with `gh api --method POST repos/kcenon/network_system/rulesets --input .github/rulesets/main.json`.
   If exactly one exists, update that ID with `--method PUT` at
   `repos/kcenon/network_system/rulesets/ID`. Investigate duplicates instead of
   creating another ruleset. Do not update the separate coherence ruleset.
4. Read back the saved ID and `repos/kcenon/network_system/rules/branches/main`.
   Check active enforcement, default-branch targeting, all rules and app IDs,
   zero approvals, and an empty bypass list. Confirm `develop` rules are unchanged.
5. From a scratch clone, create an empty commit directly atop freshly fetched
   `main`, without an associated PR. Attempt an ordinary fast-forward push to
   `main`. Save the rejection naming the pull-request rule and confirm the remote
   SHA is unchanged. A dry run, authentication error, stale base, or missing-check
   rejection alone does not prove PR enforcement. If unexpectedly accepted,
   record the failure and repair the rules; do not force-reset history.
6. Verify an ordinary PR merge after all required checks pass with zero approvals.
   Never test protection by force-pushing or deleting `main`; inspect those rules
   through the effective configuration.

Keep rule snapshots, commit SHAs, original logs, run URLs, and push output with
the change record. Revalidate contexts before any future job rename. GitHub's
[ruleset API](https://docs.github.com/en/rest/repos/rules) and
[available rules](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets)
describe the server-side settings.

## Published versions and historical erratum

The following source comparison was verified on 2026-10-11 (KST). The two
manifests are `vcpkg.json` and `vcpkg-ports/kcenon-network-system/vcpkg.json`;
vcpkg `port-version` is a packaging revision, not the project's SemVer.

| Source | CMake project version | Root manifest | Network overlay manifest |
| --- | --- | --- | --- |
| [v0.1.0](https://github.com/kcenon/network_system/tree/df8516d4e24996aff8973272e75d5f97941c0d75) | 0.1.0 | 0.1.0 | 0.1.0 |
| [v0.1.1](https://github.com/kcenon/network_system/tree/082a99914947c82d8354c7f7d9dacc99dbb6a135) | 0.1.1 | 0.1.0 | 0.1.0 |
| [main at this audit](https://github.com/kcenon/network_system/tree/ca3a005af8e622ab36ade03f1aefb2035e8d580b) | 0.1.1 | 0.1.1 | 0.1.1 |

The latest published release is **v0.1.1**. Its tagged CMake version agrees with
the release name, but both tagged vcpkg manifests still declare **0.1.0**. Current
development metadata agrees at **0.1.1**; that correction does not change the
historical release contents. Preserve the existing tags and assets and disclose
the mismatch rather than claiming the old package metadata was repaired.

**v1.0.0 remains unpublished.** Historical changelog headings such as v1.5.0 and
v2.0.0 are not evidence of GitHub releases or tags. The root changelog's
`v2.0.0...HEAD` comparison and unpublished v1.0.0 release link are not reliable
published-release references. Use the [actual releases](https://github.com/kcenon/network_system/releases)
and the comparison above for publication status. Broader changelog reconciliation,
coverage policy, upstream prerequisites, and the next release remain under
[#1165](https://github.com/kcenon/network_system/issues/1165).

## Contract for the next release

Before publishing a new stable release:

1. Complete the release prerequisites in #1165, including its measured coverage
   policy and upstream release requirements.
2. Deliver a release PR through the protected default branch. Its reviewed source
   must give the same `X.Y.Z` version in CMake and both network manifests.
3. Verify the actual release merge commit and record independent provenance.
   Create a new `vX.Y.Z` tag on that exact commit; do not infer it from today's tip
   or move an existing tag. Release names and README statements must identify the
   published version accurately, separately from any unreleased candidate.
4. Verify archive checksums and port references against the published artifact.
   Inspect the raw tag-reality result; an advisory green workflow is insufficient.

These are release policy requirements, not a claim that the existing release
workflow mechanically enforces every step. Keep its separate enforcement work
under #1165. Branch protection can be complete while historical package
consistency and release provenance remain unresolved; do not report full #1169
completion solely because the ruleset is active.
