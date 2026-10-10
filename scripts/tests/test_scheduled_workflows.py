import copy
from datetime import datetime, timedelta, timezone
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('scheduled', Path(__file__).parents[1] / 'scheduled_workflows.py')
scheduled = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scheduled)


def run_record(path='.github/workflows/fuzzing.yml', run_id=17):
    return {'id': run_id, 'run_attempt': 1, 'workflow_id': 29, 'event': 'schedule',
            'head_branch': 'main', 'path': path, 'head_sha': 'a' * 40,
            'repository': {'full_name': scheduled.REPOSITORY},
            'head_repository': {'full_name': scheduled.REPOSITORY},
            'status': 'completed', 'conclusion': 'failure',
            'html_url': f'https://github.com/{scheduled.REPOSITORY}/actions/runs/{run_id}',
            'created_at': '2026-10-12T02:00:00Z', 'updated_at': '2026-10-12T02:30:00Z'}


class FakeGitHub:
    def __init__(self, run=None):
        self.run = run or run_record()
        self.issues = []
        self.writes = []
        self.registered_path = self.run['path']
        self.write_error = False

    def request(self, route, body=None, method='GET'):
        if method != 'GET':
            if self.write_error:
                raise RuntimeError('GitHub API failed: HTTP 403')
            self.writes.append((route, copy.deepcopy(body), method))
            if method == 'POST':
                issue = dict(body, number=41, state='open', html_url='https://example.test/issues/41')
                self.issues.append(issue)
            else:
                issue = self.issues[0]
                issue.update(body)
            return copy.deepcopy(issue)
        if route == '':
            return {'default_branch': 'main'}
        if route.startswith('actions/runs/'):
            return copy.deepcopy(self.run)
        if route == 'actions/workflows/29':
            return {'path': self.registered_path}
        raise AssertionError(route)

    def pages(self, route, key=None):
        if route.startswith('issues?'):
            return copy.deepcopy(self.issues)
        raise AssertionError(route)


class ReporterTests(unittest.TestCase):
    def test_failure_creates_issue_with_original_run_identity(self):
        api = FakeGitHub()
        result = scheduled.report_failure(api, 17, apply=True)
        self.assertTrue(result['applied'])
        body = api.issues[0]['body']
        for expected in ('fuzzing.yml', 'a' * 40, '/runs/17/attempts/1', '2026-10-12T02:00:00Z', '`failure`'):
            self.assertIn(expected, body)
        self.assertEqual(len(api.writes), 1)

    def test_preview_never_writes(self):
        api = FakeGitHub()
        result = scheduled.report_failure(api, 17)
        self.assertEqual(result['status'], 'preview')
        self.assertFalse(api.writes)

    def test_controlled_replay_is_explicitly_historical(self):
        api = FakeGitHub()
        result = scheduled.report_failure(api, 17, apply=True, replay=True)
        self.assertTrue(result['controlled_replay'])
        self.assertIn('not a new scheduled incident', api.issues[0]['body'])

    def test_non_scheduled_or_untrusted_runs_are_ignored(self):
        changes = [{'event': 'pull_request'}, {'event': 'workflow_dispatch'},
                   {'head_branch': 'feature/anything'},
                   {'repository': {'full_name': 'someone/fork'}},
                   {'head_repository': {'full_name': 'someone/fork'}},
                   {'path': '.github/workflows/notify-ecosystem.yml'},
                   {'path': '.github/workflows/test-integration.yml'}]
        for change in changes:
            with self.subTest(change=change):
                api = FakeGitHub(dict(run_record(), **change))
                self.assertTrue(scheduled.report_failure(api, 17, apply=True)['status'].startswith('ignored'))
                self.assertFalse(api.writes)

    def test_success_and_running_jobs_are_not_reported(self):
        for change in ({'conclusion': 'success'}, {'status': 'in_progress', 'conclusion': None}):
            api = FakeGitHub(dict(run_record(), **change))
            self.assertTrue(scheduled.report_failure(api, 17, apply=True)['status'].startswith('ignored'))
            self.assertFalse(api.writes)

    def test_cancelled_timeout_and_startup_failures_are_reported(self):
        for conclusion in ('cancelled', 'timed_out', 'startup_failure', 'action_required', 'skipped'):
            api = FakeGitHub(dict(run_record(), conclusion=conclusion))
            self.assertEqual(scheduled.report_failure(api, 17, apply=True)['status'], 'reported')

    def test_duplicate_delivery_does_not_write_or_reopen(self):
        api = FakeGitHub()
        scheduled.report_failure(api, 17, apply=True)
        api.issues[0]['state'] = 'closed'
        result = scheduled.report_failure(api, 17, apply=True)
        self.assertEqual(result['status'], 'already recorded')
        self.assertEqual(len(api.writes), 1)
        self.assertEqual(api.issues[0]['state'], 'closed')

    def test_new_attempt_updates_same_issue_preserving_history(self):
        api = FakeGitHub()
        scheduled.report_failure(api, 17, apply=True)
        api.issues[0]['state'] = 'closed'
        api.run['run_attempt'] = 2
        scheduled.report_failure(api, 17, attempt=2, apply=True)
        self.assertEqual(len(api.issues), 1)
        self.assertEqual(api.issues[0]['state'], 'open')
        self.assertIn('scheduled-run:17:attempt:1', api.issues[0]['body'])
        self.assertIn('scheduled-run:17:attempt:2', api.issues[0]['body'])

    def test_another_failed_run_updates_same_workflow_issue(self):
        api = FakeGitHub()
        scheduled.report_failure(api, 17, apply=True)
        api.run = run_record(run_id=18)
        scheduled.report_failure(api, 18, apply=True)
        self.assertEqual(len(api.issues), 1)
        self.assertEqual(len(api.writes), 2)

    def test_api_errors_are_not_success(self):
        api = FakeGitHub()
        api.write_error = True
        with self.assertRaisesRegex(RuntimeError, '403'):
            scheduled.report_failure(api, 17, apply=True)

    def test_workflow_id_and_immutable_sha_are_verified(self):
        api = FakeGitHub()
        api.registered_path = '.github/workflows/notify-ecosystem.yml'
        with self.assertRaisesRegex(ValueError, 'workflow ID'):
            scheduled.report_failure(api, 17, apply=True)
        api = FakeGitHub(dict(run_record(), head_sha='main'))
        with self.assertRaisesRegex(ValueError, 'immutable'):
            scheduled.report_failure(api, 17, apply=True)

    def test_ambiguous_tracking_issues_fail_visibly(self):
        api = FakeGitHub()
        scheduled.report_failure(api, 17, apply=True)
        api.issues.append(copy.deepcopy(api.issues[0]))
        with self.assertRaisesRegex(ValueError, 'Multiple tracking issues'):
            scheduled.report_failure(api, 17, apply=True)


class ObservationTests(unittest.TestCase):
    def setUp(self):
        self.start = datetime(2026, 10, 11, tzinfo=timezone.utc)
        self.end = self.start + timedelta(days=7)
        self.runs = [dict(run_record(path, i + 1), conclusion='success', first_attempt_conclusion='success',
                          jobs_verified=True, artifacts_verified=True)
                     for i, path in enumerate(scheduled.WORKFLOWS)]

    def evaluate(self, runs=None, end=None, active=None):
        return scheduled.evaluate_window(self.start, end or self.end,
                                         self.runs if runs is None else runs,
                                         scheduled.WORKFLOWS if active is None else active)

    def test_complete_window_requires_each_retained_workflow(self):
        self.assertTrue(self.evaluate()['scheduled_run_checks_passed'])
        self.assertFalse(self.evaluate(runs=self.runs[:-1])['scheduled_run_checks_passed'])

    def test_empty_or_partial_window_never_passes(self):
        self.assertFalse(self.evaluate(runs=[])['scheduled_run_checks_passed'])
        self.assertFalse(self.evaluate(end=self.end - timedelta(seconds=1))['scheduled_run_checks_passed'])

    def test_successful_rerun_does_not_hide_first_attempt_failure(self):
        self.runs[0].update(run_attempt=2, first_attempt_conclusion='failure')
        self.assertFalse(self.evaluate()['scheduled_run_checks_passed'])

    def test_failures_cancellations_skips_and_incomplete_runs_prevent_acceptance(self):
        for conclusion in ('failure', 'cancelled', 'skipped', None):
            runs = copy.deepcopy(self.runs)
            runs.append(dict(runs[0], id=99, conclusion=conclusion))
            self.assertFalse(self.evaluate(runs=runs)['scheduled_run_checks_passed'])

    def test_green_wrapper_without_jobs_or_artifacts_does_not_pass(self):
        for field in ('jobs_verified', 'artifacts_verified'):
            runs = copy.deepcopy(self.runs)
            runs[0][field] = False
            self.assertFalse(self.evaluate(runs=runs)['scheduled_run_checks_passed'])

    def test_inactive_workflow_does_not_pass(self):
        self.assertFalse(self.evaluate(active=list(scheduled.WORKFLOWS)[:-1])['scheduled_run_checks_passed'])

    def test_runs_outside_window_or_completing_later_do_not_qualify(self):
        for key, value in (('created_at', '2026-10-10T00:00:00Z'), ('updated_at', '2026-10-19T00:00:00Z')):
            runs = copy.deepcopy(self.runs)
            runs[0][key] = value
            self.assertFalse(self.evaluate(runs=runs)['scheduled_run_checks_passed'])

    def test_timezone_is_required_and_normalized(self):
        with self.assertRaisesRegex(ValueError, 'timezone'):
            scheduled.timestamp('2026-10-11T00:00:00')
        self.assertEqual(scheduled.timestamp('2026-10-11T09:00:00+09:00'), self.start)


if __name__ == '__main__':
    unittest.main()
