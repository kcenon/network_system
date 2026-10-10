#!/usr/bin/env python3
"""Report scheduled failures and audit a non-vacuous seven-day observation window.

Uses gh authentication locally and the automatic GITHUB_TOKEN in Actions.
Only GitHub metadata is inspected; triggering workflow code is never executed.
"""
import argparse
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import re
import subprocess
import sys

REPOSITORY = 'kcenon/network_system'
WORKFLOWS = {
    '.github/workflows/cve-scan.yml': 'CVE Security Scan',
    '.github/workflows/osv-scanner.yml': 'OSV Vulnerability Scan',
    '.github/workflows/sbom.yml': 'SBOM Generation',
    '.github/workflows/fuzzing.yml': 'Fuzzing Tests',
}
ARTIFACTS = {
    'CVE Security Scan': ('security-report-',),
    'OSV Vulnerability Scan': ('osv-scan-results-',),
    'SBOM Generation': ('sbom-',),
    'Fuzzing Tests': ('fuzz-logs', 'fuzz-corpus'),
}


class GitHub:
    def request(self, route, body=None, method='GET'):
        endpoint = f'repos/{REPOSITORY}' + (f'/{route}' if route else '')
        args = ['gh', 'api', endpoint, '--method', method]
        if body is not None:
            args += ['--input', '-']
        result = subprocess.run(args, input=json.dumps(body) if body is not None else None,
                                text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(f'GitHub API failed for {route}: {result.stderr.strip()}')
        return json.loads(result.stdout)

    def pages(self, route, key=None):
        rows = []
        for page in range(1, 1001):
            sep = '&' if '?' in route else '?'
            data = self.request(f'{route}{sep}per_page=100&page={page}')
            batch = data[key] if key else data
            rows.extend(batch)
            if len(batch) < 100:
                return rows
        raise RuntimeError('Pagination limit reached; refusing an incomplete audit')


def timestamp(value):
    result = datetime.fromisoformat(value.replace('Z', '+00:00'))
    if result.tzinfo is None:
        raise ValueError('Timestamps must include a timezone')
    return result.astimezone(timezone.utc)


def workflow_path(run):
    return run.get('path', '').split('@', 1)[0]


def eligible(run, default_branch):
    return (run.get('event') == 'schedule'
            and run.get('head_branch') == default_branch
            and run.get('repository', {}).get('full_name') == REPOSITORY
            and run.get('head_repository', {}).get('full_name') == REPOSITORY
            and workflow_path(run) in WORKFLOWS)


def report_failure(api, run_id, attempt=None, apply=False, replay=False):
    route = f'actions/runs/{run_id}'
    if attempt is not None:
        route += f'/attempts/{attempt}'
    run = api.request(route)
    default = api.request('')['default_branch']
    result = {'run_id': run_id, 'run_attempt': run['run_attempt'],
              'run_url': run['html_url'], 'applied': False, 'controlled_replay': replay}
    if not eligible(run, default):
        return dict(result, status='ignored: not an allowlisted default-branch scheduled run')
    if run['status'] != 'completed' or run['conclusion'] == 'success':
        return dict(result, status='ignored: no completed unsuccessful run')
    path = workflow_path(run)
    registered = api.request(f'actions/workflows/{run["workflow_id"]}')
    if registered['path'] != path:
        raise ValueError('Run workflow ID does not match its allowlisted path')
    if not re.fullmatch('[0-9a-f]{40}', run['head_sha']):
        raise ValueError('Run has no immutable source SHA')
    marker = f'<!-- scheduled-workflow-health:{path} -->'
    entry_marker = f'<!-- scheduled-run:{run_id}:attempt:{run["run_attempt"]} -->'
    title = f'ci(schedule): {WORKFLOWS[path]} requires attention'
    matches = [issue for issue in api.pages('issues?state=all&sort=created&direction=desc')
               if 'pull_request' not in issue and (issue.get('body') or '').startswith(marker)]
    if len(matches) > 1:
        raise ValueError('Multiple tracking issues share a workflow marker')
    existing = matches[0] if matches else None
    if existing and entry_marker in existing['body']:
        return dict(result, status='already recorded', issue_url=existing['html_url'])
    intro = (f'{marker}\nScheduled runs of `{path}` did not succeed.\n\n'
             'Tracked under #1170. Review the original job logs and artifacts; '
             'a successful rerun does not erase a failed first attempt.\n')
    entry = (f'\n{entry_marker}\n'
             f'- Run: https://github.com/{REPOSITORY}/actions/runs/{run_id}/attempts/{run["run_attempt"]}\n'
             f'- Workflow: `{path}`\n- Source SHA: `{run["head_sha"]}`\n'
             f'- Attempt: {run["run_attempt"]}\n- Created: {run["created_at"]}\n'
             f'- Completed/updated: {run["updated_at"]}\n- Conclusion: `{run["conclusion"]}`\n')
    if replay:
        entry += '- Controlled manual replay of historical evidence; this is not a new scheduled incident.\n'
    body = (existing['body'] if existing else intro) + entry
    result.update(status='preview', workflow_path=path, source_sha=run['head_sha'],
                  conclusion=run['conclusion'], issue_number=existing['number'] if existing else None,
                  proposed_title=title, proposed_body=body)
    if apply:
        if existing:
            issue = api.request(f'issues/{existing["number"]}', {'body': body, 'state': 'open'}, 'PATCH')
        else:
            issue = api.request('issues', {'title': title, 'body': body}, 'POST')
        result.update(applied=True, status='reported', issue_url=issue['html_url'])
    return result


def evaluate_window(start, end, runs, active_paths):
    """Pure acceptance checks; run records include original attempts and job/artifact evidence."""
    reasons = []
    if end < start + timedelta(days=7):
        reasons.append('Seven full days have not elapsed')
    missing_active = set(WORKFLOWS) - set(active_paths)
    if missing_active:
        reasons.append('Retained workflows are inactive or missing: ' + ', '.join(sorted(missing_active)))
    observed = set()
    for run in runs:
        path = workflow_path(run)
        if path not in WORKFLOWS or not start <= timestamp(run['created_at']) <= end:
            continue
        if (run.get('status') != 'completed' or run.get('conclusion') != 'success'
                or run.get('first_attempt_conclusion') != 'success'):
            reasons.append(f'Run {run["id"]} did not succeed on its first attempt')
            continue
        if timestamp(run['updated_at']) > end:
            reasons.append(f'Run {run["id"]} was not complete within the observation window')
            continue
        if not run.get('jobs_verified') or not run.get('artifacts_verified'):
            reasons.append(f'Run {run["id"]} lacks successful jobs or retained result artifacts')
            continue
        observed.add(path)
    missing = set(WORKFLOWS) - observed
    if missing:
        reasons.append('No qualifying scheduled run: ' + ', '.join(sorted(missing)))
    return {'scheduled_run_checks_passed': not reasons, 'reasons': reasons,
            'workflows_observed': sorted(observed),
            'earliest_eligible_end': (start + timedelta(days=7)).isoformat()}


def audit_window(api, start, end):
    now = datetime.now(timezone.utc)
    if end > now or start > end:
        raise ValueError('Observation window must be ordered and cannot end in the future')
    default = api.request('')['default_branch']
    workflows = api.pages('actions/workflows', 'workflows')
    active = [w['path'] for w in workflows if w['state'] == 'active']
    # Filter only the lower bound server-side; compare exact UTC timestamps locally.
    runs = api.pages(f'actions/runs?event=schedule&created=>={start.strftime("%Y-%m-%d")}', 'workflow_runs')
    selected = []
    for run in runs:
        if not eligible(run, default) or not start <= timestamp(run['created_at']) <= end:
            continue
        first = api.request(f'actions/runs/{run["id"]}/attempts/1')
        jobs = api.pages(f'actions/runs/{run["id"]}/attempts/1/jobs', 'jobs')
        artifacts = api.pages(f'actions/runs/{run["id"]}/artifacts', 'artifacts')
        prefixes = ARTIFACTS[WORKFLOWS[workflow_path(run)]]
        available = [a for a in artifacts if not a['expired'] and a['size_in_bytes'] > 0]
        record = dict(run, first_attempt_conclusion=first['conclusion'], jobs=jobs, artifacts=artifacts,
                      jobs_verified=bool(jobs) and all(j['conclusion'] == 'success' for j in jobs),
                      artifacts_verified=all(any(a['name'].startswith(p) for a in available) for p in prefixes))
        selected.append(record)
    result = evaluate_window(start, end, selected, active)
    result.update(repository=REPOSITORY, observed_at=now.isoformat(), window_start=start.isoformat(),
                  window_end=end.isoformat(), workflows=workflows, runs=selected,
                  acceptance_complete=False,
                  remaining_manual_checks=['Inspect original logs and artifact contents, including tolerated failures',
                                           'Verify reporter delivery and tracked workflow inventory',
                                           'Confirm start follows final main delivery and activation; reset after material repairs'])
    return result


def positive(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError('Expected a positive integer')
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    report = commands.add_parser('report')
    report.add_argument('--run-id', type=positive, required=True)
    report.add_argument('--attempt', type=positive)
    report.add_argument('--apply', action='store_true', help='Create/update the tracking issue; default is preview')
    report.add_argument('--replay', action='store_true', help='Label controlled replay evidence explicitly')
    audit = commands.add_parser('audit')
    audit.add_argument('--since', required=True, help='UTC instant after final main delivery and activation')
    audit.add_argument('--until', help='Defaults to now; must not be in the future')
    for command in (report, audit):
        command.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        api = GitHub()
        if args.command == 'report':
            result = report_failure(api, args.run_id, args.attempt, args.apply, args.replay)
            code = 0
        else:
            end = timestamp(args.until) if args.until else datetime.now(timezone.utc)
            result = audit_window(api, timestamp(args.since), end)
            code = 0 if result['scheduled_run_checks_passed'] else 1
    except (ValueError, RuntimeError, KeyError) as exc:
        result, code = {'error': str(exc)}, 1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('proposed_body', 'runs', 'workflows')}))
    return code


if __name__ == '__main__':
    sys.exit(main())
