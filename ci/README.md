# Coherence gates

Tracked by [common_system #701](https://github.com/kcenon/common_system/issues/701).

`ci/coherence.json` selects advisory or enforcing policy. The underlying validators
always return their real result. `scripts/run_coherence.py` saves the command,
source SHA, worktree state, stdout/stderr and raw exit code before applying policy.
An advisory workflow success does not mean the gate passed. Reports are uploaded
as workflow artifacts. `--strict` runs the same validator without advisory handling.

Run locally:

```sh
python3 scripts/run_coherence.py conformance
python3 scripts/run_coherence.py version-drift -- --verbose
python3 -m unittest discover -s scripts/tests -p 'test_*.py'
```

Common alone checks `DEPENDENCY_MATRIX.md`; downstream version checks compare
local FetchContent inputs and vcpkg overrides. Invalid manifests and unresolved
pinned versions fail. A report with zero comparisons must state its reviewed
reason; it is not evidence that two versions agreed.

The tag validator requires independently recorded release-merge provenance and
checks the port's stored SHA512 against the actual archive. Use a merged release
PR, a reviewed `ci/release-provenance.json` record, or `--release-commit` from the
release process. It never substitutes today's branch tip for an old release.
Identity-only validation is explicitly labeled and is not checksum validation.

```sh
python3 scripts/check_tag_reality.py --repo kcenon/network_system --tag v1.2.3 \
  --release-commit FULL_RELEASE_MERGE_SHA \
  --port-dir vcpkg-ports/kcenon-network-system
```

Select the port metadata revision that describes the artifact being checked.
Development metadata may describe an unreleased version; do not create or move
release tags just to pass a gate. Generated release port hashes are verified
before registry sync; a post-publication check is detection, not release approval.

`ci/coherence-source.json` records the shared source revision and file hashes.
Update shared scripts/helper tests in common first, run their behavior tests,
then propagate identical copies and refresh the manifest in every repository.

New gates remain advisory until remediation has landed and the raw commands
pass on current `develop` HEAD. Promotion is a separate change: remove advisory
handling via policy, verify negative fixtures fail, then add the exact emitted
check names to required-check settings. Preserve existing protections. YAML
changes alone do not apply repository settings.

## Default-branch protection and release versions

[Release policy](RELEASE_POLICY.md) documents the main-only PR/build ruleset,
its application and verification, and the existing coherence requirements on
`main` and `develop`. It also records the published v0.1.1 manifest mismatch and
the version/provenance contract for the next release. The deployable ruleset is
tracked in [`.github/rulesets/main.json`](../.github/rulesets/main.json).

## Dependency option migration

- `BUILD_WITH_COMMON_SYSTEM` → `KCENON_WITH_COMMON_SYSTEM`.
- `BUILD_WITH_THREAD_SYSTEM` → `KCENON_WITH_THREAD_SYSTEM`.
- `BUILD_WITH_LOGGER_SYSTEM` → `KCENON_WITH_LOGGER_SYSTEM`.
- `BUILD_WITH_CONTAINER_SYSTEM` → `KCENON_WITH_CONTAINER_SYSTEM`.

Canonical inputs take precedence with a warning on conflicts. Legacy-only inputs
remain supported, including changing OFF/ON in an existing cache. Defaults and
required-dependency guards are preserved. Effective values are mirrored in normal
CMake scope; parent inputs and existing cache values are not forcibly overwritten.
Remove an explicitly cached canonical choice with `cmake -U KCENON_WITH_<DEP>`
before returning control to a legacy alias. Source compile definitions keep their
existing names for compatibility. Overlay ports pass both spellings while their
release references may predate the shim; they must retain the legacy flag until
the selected published source supports the canonical one.

## Dispatch delivery

Deliver `coherence-receiver.yml` and its shared validation script on the default
`main` branch before common sends `coherence-check-v1`. The receiver validates
an accepted common lock revision and checks out the selected candidate/locked
SHA, then records strict raw results. `notify-ecosystem.yml` runs on trusted
main/develop pushes and uses `ECOSYSTEM_DISPATCH_TOKEN` to notify common and wait
for correlated cross-build and port-audit artifacts. Missing credentials or
receivers remain failures, not successful delivery evidence.

The common-owned [operations guide](https://github.com/kcenon/common_system/blob/ci/701-coherence-gates/ci/ECOSYSTEM_COHERENCE.md)
describes the routes, credential permissions, profiles, lock bootstrap, staged
release boundary and guarded promotion procedure. Refresh shared files from a
complete common checkout, using a reviewed immutable revision:

```sh
python3 ../common_system/scripts/sync_coherence.py --target . --source-revision FULL_COMMON_SHA
python3 ../common_system/scripts/sync_coherence.py --target . --source-revision FULL_COMMON_SHA --check
```

Release sync must reference the reviewed reusable workflow and validator commit.
`tag-reality-mode: advisory` records release identity/hash failures during rollout.
Promote it separately only after real release provenance and port hashes pass.
The release-triggered workflow checks an already published tag; its enforcing
prepublication boundary is the registry sync.

## Scheduled workflow operations

[Network #1170](https://github.com/kcenon/network_system/issues/1170) reduces six
schedules (two daily) to three weekly schedules:

| Workflow | UTC schedule | Purpose |
| --- | --- | --- |
| `sbom.yml` | Sunday 03:00 | Generate the software bill of materials |
| `fuzzing.yml` | Monday 02:00 | Exercise protocol parsers with libFuzzer |
| `cve-scan.yml` | Wednesday 04:23 | Trivy filesystem scan and license checks |

All three retain `workflow_dispatch`. Trivy retains its existing push/PR
triggers; SBOM retains its push/PR and release triggers.
`network-load-tests.yml` is manual-only, including its `update_baseline` input.
The retired `test-integration.yml` duplicated the integration workflow name and
invoked obsolete optional binaries. `integration-tests.yml` retains the
connection lifecycle, protocol integration, performance and error-handling
matrix, plus performance validation. Its build/test behavior is unchanged.

The retired `osv-scanner.yml` did not provide the intended dependency coverage.
In [manual validation](https://github.com/kcenon/network_system/actions/runs/38078410304),
OSV could not find an extractor for `vcpkg.json` (exit 127); its fallback found
zero packages and produced no SARIF artifact. Tolerated errors made the workflow
green without a meaningful scan. Retiring this dead workflow preserves the
working Trivy filesystem/secret and license checks and SBOM/Grype inventory scan.
Trivy found no language-specific dependency files, while Grype scanned 83 Syft
packages. These results do not establish complete C++/vcpkg dependency coverage;
adding a supported, resolved dependency inventory is separate work.

The fuzz job allows 45 minutes: five scheduled targets consume 25 minutes before
setup, dependency builds and artifact uploads. Its former 30-minute deadline
cancelled the fifth target in the original August 31 scheduled run. Target
selection and the scheduled 300-second duration per target are unchanged.

`scheduled-workflow-health.yml` inspects completed default-branch scheduled runs
of these three workflows. With the automatic `GITHUB_TOKEN`, it creates or
updates one marked issue per workflow for unsuccessful runs, including timeout,
startup failure and cancellation. It retains each run/attempt identity, reopens
the issue for a new failure, and does not duplicate an already recorded attempt.
Successful reruns do not remove historical failures or close the tracking issue.
The reporter checks out only its own default-branch revision. It never checks
out the triggering run, and only its reporting job has `issues: write`.

For a controlled replay, manually dispatch **Scheduled Workflow Health** from
`main` with an existing scheduled run ID. The default `dry_run: true` validates
and previews without changing issues. Set it to false to exercise actual
delivery. Record the original run date and label the evidence as a replay;
it is not a new scheduled failure or seven-day acceptance evidence. Replaying
the same attempt should return `already recorded`. Inspect the
`scheduled-health-report` artifact; API/reporting failures fail the job.

After delivery to default `main`, enable each retained inactive workflow with
`gh workflow enable <file> --repo kcenon/network_system`, then dispatch all three
once and inspect their original logs and artifacts. Historical/dynamic workflow
API entries can outlive files: reconcile the raw inactive-workflow listing with
tracked default-branch files instead of re-enabling deleted workflows.

Start the observation window after the final main delivery and activation:

```sh
python3 scripts/scheduled_workflows.py audit \
  --since 2026-10-11T00:00:00Z --output scheduled-observation.json
```

Replace the example timestamp with the recorded deployment/activation time.
The read-only audit requires seven full days, a real successful scheduled run
of every retained workflow, successful original-attempt jobs, retained result
artifacts and active workflow registrations. It rejects empty windows, missing
workflows, cancellations, skips and failures hidden by a later successful rerun.
Exit 0 establishes these metadata checks only: review the original logs and
artifact contents, reporter delivery and workflow inventory before acceptance.
Existing tolerated failures in scanners/fuzzing remain visible limitations;
artifact presence alone does not establish complete scan or fuzz coverage.

Keep #1170 open while observation is pending. Reset the qualifying window after
a failed scheduled run or material workflow repair. The separate missing-secret
notification in common_system#701 is not a prerequisite for this local work.
