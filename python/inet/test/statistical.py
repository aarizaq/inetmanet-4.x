"""
This module provides functionality for statistical testing of multiple simulations.

The main function is :py:func:`run_statistical_tests`. It allows running multiple statistical tests
matching the provided filter criteria. Statistical tests check if scalar results of the simulations
are the same as the saved baseline results. The baseline results can be found in the statistics folder 
of the simulation project.  For the INET Framework the media folder can be found at
https://github.com/inet-framework/statistics in a separate GitHub repository.
"""

import difflib
import glob
import logging
import pandas
import re
import shutil

from omnetpp.scave.results import *

from inet.simulation import *
from inet.test.fingerprint import *
from inet.test.simulation import *

_logger = logging.getLogger(__name__)

class StatisticalTestTask(SimulationTestTask):
    def __init__(self, simulation_config, run_number, name="statistical test", task_result_class=SimulationTestTaskResult, **kwargs):
        super().__init__(SimulationTask(simulation_config=simulation_config, run_number=run_number, name=name, **kwargs), task_result_class=task_result_class, **kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs

    def check_simulation_task_result(self, simulation_task_result, result_name_filter=None, exclude_result_name_filter=None, result_module_filter=None, exclude_result_module_filter=None, full_match=False, **kwargs):
        simulation_task = simulation_task_result.task
        simulation_config = simulation_task.simulation_config
        simulation_project = simulation_config.simulation_project
        working_directory = simulation_config.working_directory
        config = simulation_config.config
        current_results_directory = simulation_project.get_full_path(os.path.join(working_directory, "results"))
        stored_results_directory = simulation_project.get_full_path(os.path.join(simulation_project.statistics_folder, working_directory))
        scalars_match = False
        # KLUDGE TODO there's no way of knowing which scalar files are created by a particular simulation run
        #             the results folder should be different from the default so that all results can be identified
        for current_scalar_result_file_name in glob.glob(os.path.join(current_results_directory, "*.sca")):
            if re.search("/" + config + "-#", current_scalar_result_file_name):
                _logger.debug(f"Reading result file {current_scalar_result_file_name}")
                current_df = read_result_files(current_scalar_result_file_name, include_fields_as_scalars=True)
                current_df = get_scalars(current_df)
                if "runID" in current_df:
                    current_df.drop("runID", axis=1, inplace=True)
                scalar_file_name = os.path.basename(current_scalar_result_file_name)
                stored_scalar_result_file_name = os.path.join(stored_results_directory, scalar_file_name)
                scalar_result_diff_file_name = re.sub(".sca", ".diff", stored_scalar_result_file_name)
                if os.path.exists(scalar_result_diff_file_name):
                    os.remove(scalar_result_diff_file_name)
                if os.path.exists(stored_scalar_result_file_name):
                    _logger.debug(f"Reading result file {stored_scalar_result_file_name}")
                    stored_df = read_result_files(stored_scalar_result_file_name, include_fields_as_scalars=True)
                    stored_df = get_scalars(stored_df)
                    if "runID" in stored_df:
                        stored_df.drop("runID", axis=1, inplace=True)
                    scalars_match = current_df.equals(stored_df)
                    if not scalars_match:
                        with open(current_scalar_result_file_name, "r") as file:
                            current_scalar_result_file = file.readlines()
                        with open(stored_scalar_result_file_name, "r") as file:
                            stored_scalar_result_file = file.readlines()
                        with open(scalar_result_diff_file_name, "w") as file:
                            scalar_diff = "".join(difflib.ndiff(stored_scalar_result_file, current_scalar_result_file))
                            file.write(scalar_diff)
                        if current_df.empty:
                            return self.task_result_class(task=self, simulation_task_result=simulation_task_result, result="FAIL", reason="Current statistical results are empty")
                        elif stored_df.empty:
                            return self.task_result_class(task=self, simulation_task_result=simulation_task_result, result="FAIL", reason="Stored statistical results are empty")
                        else:
                            df = pandas.DataFrame()
                            df["module"] = stored_df["module"]
                            df["name"] = stored_df["name"]
                            df["stored_value"] = stored_df["value"]
                            df["current_value"] = current_df["value"]
                            df["absolute_error"] = df.apply(lambda row: abs(row["current_value"] - row["stored_value"]), axis=1)
                            df["relative_error"] = df.apply(lambda row: row["absolute_error"] / abs(row["stored_value"]) if row["stored_value"] != 0 else (float("inf") if row["current_value"] != 0 else 0), axis=1)
                            df = df[df.apply(lambda row: matches_filter(row["name"], result_name_filter, exclude_result_name_filter, full_match) and \
                                                         matches_filter(row["module"], result_module_filter, exclude_result_module_filter, full_match), axis=1)]
                            reason = df.loc[df["relative_error"].idxmax()].to_string()
                            reason = re.sub(" +", " = ", reason)
                            reason = re.sub("\\n", ", ", reason)
                            return self.task_result_class(task=self, simulation_task_result=simulation_task_result, result="FAIL", reason=reason)
                else:
                    return self.task_result_class(task=self, simulation_task_result=simulation_task_result, result="ERROR", reason="Stored statistical results are not found")
        return self.task_result_class(task=self, simulation_task_result=simulation_task_result, result="PASS")

def get_statistical_test_sim_time_limit(simulation_config, run_number=0):
    return simulation_config.sim_time_limit

def get_statistical_test_tasks(sim_time_limit=get_statistical_test_sim_time_limit, **kwargs):
    """
    Returns multiple statistical test tasks matching the provided filter criteria. The returned tasks can be run by
    calling the :py:meth:`run <inet.common.task.MultipleTasks.run>` method.

    Parameters:
        kwargs (dict):
            The filter criteria parameters are inherited from the :py:meth:`get_simulation_tasks <inet.simulation.task.get_simulation_tasks>` method.

    Returns (:py:class:`MultipleTestTasks`):
        an object that contains a list of :py:class:`StatisticalTestTask` objects matching the provided filter criteria.
        The result can be run (and re-run) without providing additional parameters.
    """
    # remove run_number=0 parameter and add the same to the github-job-inet_run_statistical-tests.sh
    return get_simulation_tasks(name="statistical test", run_number=0, sim_time_limit=sim_time_limit, simulation_task_class=StatisticalTestTask, multiple_simulation_tasks_class=MultipleSimulationTestTasks, **kwargs)

def run_statistical_tests(**kwargs):
    """
    Runs one or more statistical tests that match the provided filter criteria.

    Parameters:
        kwargs (dict):
            The filter criteria parameters are inherited from the :py:func:`get_statistical_test_tasks` function.

    Returns (:py:class:`MultipleSimulationTestTaskResults`):
        an object that contains a list of :py:class:`SimulationTestTaskResult` objects. Each object describes the result of running one test task.
    """
    multiple_statistical_test_tasks = get_statistical_test_tasks(**kwargs)
    return multiple_statistical_test_tasks.run(extra_args=["--**.param-recording=false"], **kwargs)

class StatisticalResultsUpdateTask(SimulationTask):
    def __init__(self, simulation_config, run_number, name="statistical results update", **kwargs):
        super().__init__(simulation_config=simulation_config, run_number=run_number, name=name, **kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs

    def run_protected(self, **kwargs):
        update_result = super().run_protected(**kwargs)
        simulation_project = self.simulation_config.simulation_project
        working_directory = self.simulation_config.working_directory
        source_results_directory = simulation_project.get_full_path(os.path.join(working_directory, "results"))
        target_results_directory = simulation_project.get_full_path(os.path.join(simulation_project.statistics_folder, working_directory))
        if not os.path.exists(target_results_directory):
            os.makedirs(target_results_directory)
        # KLUDGE TODO there's no way of knowing which scalar files are created by a particular simulation run
        #             the results folder should be different from the default so that all results can be identified
        for scalar_result_file_name in glob.glob(os.path.join(source_results_directory, "*.sca")):
            shutil.copy(scalar_result_file_name, target_results_directory)
        return update_result

def get_update_statistical_result_tasks(**kwargs):
    """
    Returns multiple update statisical results tasks matching the provided filter criteria. The returned tasks can be run by
    calling the :py:meth:`run <inet.common.task.MultipleTasks.run>` method.

    Parameters:
        kwargs (dict):
            The filter criteria parameters are inherited from the :py:meth:`get_simulation_tasks <inet.simulation.task.get_simulation_tasks>` method.

    Returns (:py:class:`MultipleUpdateTasks`):
        an object that contains a list of :py:class:`StatisticalResultsUpdateTask` objects matching the provided filter criteria.
        The result can be run (and re-run) without providing additional parameters.
    """
    return get_simulation_tasks(simulation_task_class=StatisticalResultsUpdateTask, **kwargs)

def update_statistical_results(sim_time_limit=get_statistical_test_sim_time_limit, **kwargs):
    """
    Updates the stored statistical results for one or more chart tests that match the provided filter criteria.

    Parameters:
        kwargs (dict):
            The filter criteria parameters are inherited from the :py:func:`get_update_statistical_result_tasks` function.

    Returns (:py:class:`MultipleUpdateTaskResults`):
        an object that contains a list of :py:class:`UpdateTaskResult` objects. Each object describes the result of running one update task.
    """
    multiple_update_statistical_result_tasks = get_update_statistical_result_tasks(sim_time_limit=sim_time_limit, **kwargs)
    return multiple_update_statistical_result_tasks.run(sim_time_limit=sim_time_limit, extra_args=["--**.param-recording=false"], **kwargs)
