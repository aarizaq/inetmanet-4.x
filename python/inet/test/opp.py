"""
This module provides functionality for running multiple tests using the :command:`opp_test` command.

The main function is :py:func:`run_opp_tests`. It allows running multiple tests matching the provided
filter criteria.
"""

import builtins
import glob
import logging
import signal
import subprocess

from inet.simulation.project import *
from inet.test.task import *

_logger = logging.getLogger(__name__)

class OppTestTask(TestTask):
    def __init__(self, simulation_project, working_directory, test_file_name, **kwargs):
        super().__init__(**kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs
        self.simulation_project = simulation_project
        self.working_directory = working_directory
        self.test_file_name = test_file_name

    def get_parameters_string(self, **kwargs):
        return self.test_file_name

    def run_protected(self, capture_output=True, **kwargs):
        executable = "sh"
        args = [executable, "runtest", self.test_file_name]
        subprocess_result = subprocess.run(args, cwd=self.working_directory, capture_output=capture_output, env=self.simulation_project.get_env())
        stdout = subprocess_result.stdout.decode("utf-8")
        stderr = subprocess_result.stderr.decode("utf-8")
        match = re.search(r"Aggregate result: (\w+)", stdout)
        if subprocess_result.returncode == signal.SIGINT.value or subprocess_result.returncode == -signal.SIGINT.value:
            return self.task_result_class(self, result="CANCEL", reason="Cancel by user")
        elif match and subprocess_result.returncode == 0:
            return self.task_result_class(self, result=match.group(1))
        else:
            return self.task_result_class(self, result="FAIL", reason=f"Non-zero exit code: {subprocess_result.returncode}", stdout=stdout, stderr=stderr)

def get_opp_test_tasks(test_folder, simulation_project=None, filter=".*", full_match=False, **kwargs):
    """
    Returns multiple opp test tasks matching the provided filter criteria. The returned tasks can be run by
    calling the :py:meth:`run <inet.common.task.MultipleTasks.run>` method.

    Parameters:
        kwargs (dict):
            TODO

    Returns (:py:class:`MultipleTestTasks`):
        an object that contains a list of :py:class:`OppTestTask` objects matching the provided filter criteria.
        The result can be run (and re-run) without providing additional parameters.
    """
    def create_test_task(test_file_name):
        return OppTestTask(simulation_project, simulation_project.get_full_path(test_folder), os.path.basename(test_file_name), task_result_class=TestTaskResult, **kwargs)
    if simulation_project is None:
        simulation_project = get_default_simulation_project()
    test_file_names = list(builtins.filter(lambda test_file_name: matches_filter(test_file_name, filter, None, full_match),
                                           glob.glob(os.path.join(simulation_project.get_full_path(test_folder), "*.test"))))
    test_tasks = list(map(create_test_task, test_file_names))
    return MultipleTestTasks(tasks=test_tasks, multiple_task_results_class=MultipleTestTaskResults, **kwargs)

def run_opp_tests(test_folder, **kwargs):
    """
    Runs one or more tests using the :command:`opp_test` command that match the provided filter criteria.

    Parameters:
        kwargs (dict):
            The filter criteria parameters are inherited from the :py:func:`get_opp_test_tasks` function.

    Returns (:py:class:`MultipleTestTaskResults`):
        an object that contains a list of :py:class:`TestTaskResult` objects. Each object describes the result of running one test task.
    """
    multiple_test_tasks = get_opp_test_tasks(test_folder, **kwargs)
    return multiple_test_tasks.run(**kwargs)
