"""
This module provides functionality for fingerprint testing of multiple simulations.

The main function is :py:func:`run_fingerprint_tests`. It allows running multiple fingerprint tests matching the provided
filter criteria. Fingerprint tests check for regressions in the simulation execution trajectory.
"""

import hashlib
import logging
import re

from inet.simulation.task import *

from inet.common.ide import *
from inet.project.inet import *
from inet.simulation.project import *
from inet.test.fingerprint.store import *
from inet.test.simulation import *

_logger = logging.getLogger(__name__)
all_fingerprint_ingredients = ["tplx", "~tNl", "~tND", "tyf"]
all_fingerprint_ingredients_append_args = {
    "~tNl": {},
    "~tND": {"--**.checksumMode=\"computed\"",
             "--**.fcsMode=\"computed\""},
    "tyf" : {"--cmdenv-fake-gui=true",
             "--cmdenv-fake-gui-before-event-probability=0.1",
             "--cmdenv-fake-gui-after-event-probability=0.1",
             "--cmdenv-fake-gui-on-hold-probability=0.1",
             "--cmdenv-fake-gui-on-hold-numsteps=3",
             "--cmdenv-fake-gui-on-simtime-probability=0.1",
             "--cmdenv-fake-gui-on-simtime-numsteps=3",
             "--**.fadeOutMode=\"animationTime\"",
             "--**.signalAnimationSpeedChangeTimeMode=\"animationTime\""}}

def get_ingredients_append_args(ingredients):
    global all_fingerprint_ingredients_append_args
    append_args = ["--fingerprintcalculator-class", "inet::FingerprintCalculator"] if "~" in ingredients else []
    return append_args + list(all_fingerprint_ingredients_append_args[ingredients]) if ingredients in all_fingerprint_ingredients_append_args else []

class FingerprintTestTaskResult(SimulationTestTaskResult):
    def __init__(self, expected_fingerprint=None, calculated_fingerprint=None, **kwargs):
        super().__init__(**kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs
        self.expected_fingerprint = expected_fingerprint
        self.calculated_fingerprint = calculated_fingerprint
        self.fingerprint_mismatch = expected_fingerprint != calculated_fingerprint

    def __repr__(self):
        return "Fingerprint test result: " + self.get_description()

    def get_description(self, **kwargs):
        return super().get_description(**kwargs) + (" calculated " + str(self.calculated_fingerprint) + " (correct " + str(self.expected_fingerprint) + ")" if self.fingerprint_mismatch else "")

    def print_result(self, test_result_filter=None, exclude_test_result_filter="SKIP", complete_error_message=True, output_stream=sys.stdout, **kwargs):
        if matches_filter(self.result, test_result_filter, exclude_test_result_filter, True):
            print(self.get_description(complete_error_message=complete_error_message), file=output_stream)

    def run_baseline_fingerprint_test(self, baseline_simulation_project=inet_baseline_project, **kwargs):
        simulation_task = self.simulation_result.simulation_task
        simulation_config = self.simulation_result.simulation_task.simulation_config
        multiple_test_tasks = get_fingerprint_test_tasks(simulation_project=baseline_simulation_project, ingredients_list=[self.task.ingredients], full_match=True,
                                                         working_directory_filter=simulation_config.working_directory, ini_file_filter=simulation_config.ini_file, config_filter=simulation_config.config, run_number=simulation_task.run_number,
                                                         **kwargs)
        assert(len(multiple_test_tasks.tasks) == 1)
        multiple_test_results = multiple_test_tasks.run(**kwargs)
        return multiple_test_results.results[0]

class MultipleFingerprintTestTaskResults(MultipleTestTaskResults):
    def __init__(self, **kwargs):
        super().__init(**kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs

class FingerprintTestTask(SimulationTestTask):
    def __init__(self, sim_time_limit=None, ingredients="tplx", fingerprint=None, test_result=None, name="fingerprint test", action="Checking fingerprint", task_result_class=FingerprintTestTaskResult, **kwargs):
        super().__init__(name=name, action=action, task_result_class=task_result_class, **kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs
        self.sim_time_limit = sim_time_limit
        self.ingredients = ingredients
        self.fingerprint = fingerprint
        self.test_result = test_result

    def __repr__(self):
        return repr(self)

    def get_hash(self, **kwargs):
        hasher = hashlib.sha256()
        digest = super().get_hash(**kwargs)
        if digest:
            hasher.update(digest)
        else:
            return None
        hasher.update(self.ingredients.encode("utf-8"))
        return hasher.digest()

    def run(self, test_result_filter=None, exclude_test_result_filter="SKIP", output_stream=sys.stdout, **kwargs):
        if self.fingerprint and matches_filter(self.test_result, test_result_filter, exclude_test_result_filter, True):
            simulation_project = self.simulation_task.simulation_config.simulation_project
            return super().run(append_args=self.get_append_args(simulation_project, str(self.fingerprint)) + get_ingredients_append_args(self.ingredients), output_stream=output_stream, **kwargs)
        else:
            if matches_filter("SKIP", test_result_filter, exclude_test_result_filter, True):
                print("Running " + self.simulation_task.get_parameters_string(**kwargs), end=" ", file=output_stream)
            return FingerprintTestTaskResult(task=self, result="SKIP", expected_result=self.test_result, reason="Correct fingerprint not found")

    def check_simulation_task_result(self, simulation_task_result, **kwargs):
        expected_fingerprint = self.fingerprint
        expected_result = self.test_result
        if simulation_task_result.result == "CANCEL":
            result = "CANCEL"
            reason = "Cancel by user"
            calculated_fingerprint = None
        else:
            calculated_fingerprint = get_calculated_fingerprint(simulation_task_result, self.fingerprint.ingredients)
            if calculated_fingerprint is None:
                result = "ERROR"
                reason = "Calculated fingerprint not found"
            elif expected_fingerprint != calculated_fingerprint:
                result = "FAIL"
                reason = "Fingerprint mismatch"
            else:
                result = "PASS"
                reason = None
        return FingerprintTestTaskResult(task=self, simulation_task_result=simulation_task_result, expected_fingerprint=expected_fingerprint, calculated_fingerprint=calculated_fingerprint, result=result, expected_result=expected_result, reason=reason)

    def get_append_args(self, simulation_project, fingerprint_arg):
        return ["--fingerprint", fingerprint_arg, "--vector-recording", "false", "--scalar-recording", "false"]

class FingerprintTestGroupTask(MultipleTestTasks):
    def __init__(self, sim_time_limit=None, **kwargs):
        super().__init__(**kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs
        for fingerprint_test_task in self.tasks:
            assert fingerprint_test_task.sim_time_limit == sim_time_limit
        self.sim_time_limit = sim_time_limit

    def __repr__(self):
        return repr(self)

    def run(self, output_stream=sys.stdout, **kwargs):
        if len(self.tasks) == 1:
            return self.tasks[0].run(output_stream=output_stream, **kwargs)
        else:
            fingerprint_test_results = []
            skipped_fingerprint_test_tasks = list(filter(lambda fingerprint_test_task: fingerprint_test_task.fingerprint is None, self.tasks))
            for skipped_fingerprint_test_task in skipped_fingerprint_test_tasks:
                fingerprint_test_results.append(skipped_fingerprint_test_task.run(**kwargs))
            checked_fingerprint_test_tasks = list(filter(lambda fingerprint_test_task: fingerprint_test_task.fingerprint, self.tasks))
            if checked_fingerprint_test_tasks:
                simulation_task = checked_fingerprint_test_tasks[0].simulation_task
                simulation_project = simulation_task.simulation_config.simulation_project
                fingerprint_arg = ",".join(map(lambda e: str(e.fingerprint), checked_fingerprint_test_tasks))
                append_args = checked_fingerprint_test_tasks[0].get_append_args(simulation_project, fingerprint_arg)
                ingredients_append_args = set()
                for checked_fingerprint_test_task in checked_fingerprint_test_tasks:
                    ingredients_append_args = ingredients_append_args.union(get_ingredients_append_args(checked_fingerprint_test_task.ingredients))
                simulation_result = simulation_task.run(print_end=" ", sim_time_limit=sim_time_limit or self.sim_time_limit, output_stream=output_stream, append_args=append_args + list(ingredients_append_args), **kwargs)
                fingerprint_test_group_results = check_fingerprint_test_group(simulation_result, self, **kwargs)
                fingerprint_test_results += fingerprint_test_group_results.results
            return MultipleTestResults(self.tasks, fingerprint_test_results)

class MultipleFingerprintTestTasks(MultipleSimulationTestTasks):
    def __init__(self, multiple_simulation_tasks=None, name="fingerprint test", **kwargs):
        super().__init__(name=name, **kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs
        self.multiple_simulation_tasks = multiple_simulation_tasks

class FingerprintTrajectoryTestTask(TestTask):
    def __init__(self, simulation_task, sim_time_limit, ingredients, **kwargs):
        super().__init__(simulation_task, **kwargs)
        self.sim_time_limit = sim_time_limit
        self.ingredients = ingredients
        # TODO

class FingerprintTrajectoryTestTaskResult(TestTaskResult):
    def __init__(self):
        # TODO
        pass

def get_calculated_fingerprint(simulation_result, ingredients):
    stdout = simulation_result.subprocess_result.stdout
    stderr = simulation_result.subprocess_result.stderr
    match = re.search(r"Fingerprint successfully verified:.*? ([0-9a-f]{4}-[0-9a-f]{4})/" + ingredients, stdout)
    if match:
        value = match.groups()[0]
    else:
        match = re.search(r"Fingerprint mismatch! calculated:.*? ([0-9a-f]{4}-[0-9a-f]{4})/" + ingredients + ".*expected", stdout)
        if match:
            value = match.groups()[0]
        else:
            match = re.search(r"Fingerprint mismatch! calculated:.*? ([0-9a-f]{4}-[0-9a-f]{4})/" + ingredients + ".*expected", stderr)
            if match:
                value = match.groups()[0]
            else:
                return None
    return Fingerprint(value, ingredients)

def check_fingerprint_test_group(simulation_result, fingerprint_test_group, **kwargs):
    fingerprint_test_results = []
    for fingerprint_test_task in fingerprint_test_group.tasks:
        if fingerprint_test_task.fingerprint:
            fingerprint_test_result = check_fingerprint_test(fingerprint_test_task, simulation_result, **kwargs)
            fingerprint_test_results.append(fingerprint_test_result)
    return MultipleTestTaskResults(fingerprint_test_group.tasks, fingerprint_test_results)

def select_fingerprint_by_sim_time_limit(fingerprint_entries, largest):
    sim_time_limits = list(map(lambda fingerprint_entry: fingerprint_entry["sim_time_limit"], fingerprint_entries))
    sim_time_limits.sort(reverse=largest, key=convert_to_seconds)
    smallest_sim_time_limit = sim_time_limits[0]
    sorted_fingerprint_entries = list(filter(lambda fingerprint_entry: fingerprint_entry["sim_time_limit"] == smallest_sim_time_limit, fingerprint_entries))
    sorted_fingerprint_entries.sort(reverse=True, key=lambda fingerprint_entry: fingerprint_entry["timestamp"])
    return sorted_fingerprint_entries[0]

def select_fingerprint_with_smallest_sim_time_limit(fingerprint_entries):
    return select_fingerprint_by_sim_time_limit(fingerprint_entries, False)

def select_fingerprint_with_largest_sim_time_limit(fingerprint_entries):
    return select_fingerprint_by_sim_time_limit(fingerprint_entries, True)

def get_fingerprint_test_task(simulation_task, ingredients="tplx", sim_time_limit=None, select_fingerprint=select_fingerprint_with_smallest_sim_time_limit, **kwargs):
    # TODO take sim_time_limit into account and also select_fingerprint
    if sim_time_limit is None:
        sim_time_limit = simulation_task.sim_time_limit
    simulation_config = simulation_task.simulation_config
    correct_fingerprint_store = get_correct_fingerprint_store(simulation_config.simulation_project)
    stored_fingerprint_entries = correct_fingerprint_store.filter_entries(ingredients=ingredients, sim_time_limit=sim_time_limit,
                                                                          working_directory=simulation_config.working_directory, ini_file=simulation_config.ini_file, config=simulation_config.config, run_number=simulation_task.run_number)
    selected_fingerprint_entry = select_fingerprint(stored_fingerprint_entries) if stored_fingerprint_entries else None
    if selected_fingerprint_entry:
        sim_time_limit = selected_fingerprint_entry["sim_time_limit"]
        fingerprint = selected_fingerprint_entry["fingerprint"]
        test_result = selected_fingerprint_entry["test_result"]
        simulation_task.sim_time_limit = sim_time_limit
        return FingerprintTestTask(simulation_task=simulation_task, sim_time_limit=sim_time_limit, ingredients=ingredients, fingerprint=Fingerprint(fingerprint, ingredients), test_result=test_result)
    else:
        simulation_task.sim_time_limit = sim_time_limit
        fingerprint_test_task = FingerprintTestTask(simulation_task=simulation_task, sim_time_limit=sim_time_limit, ingredients=ingredients, test_result=None)
    return fingerprint_test_task

def collect_fingerprint_test_groups(simulation_task, ingredients_list=["tplx"], sim_time_limit=None, **kwargs):
    fingerprint_test_tasks = []
    for ingredients in ingredients_list:
        fingerprint_test_task = get_fingerprint_test_task(simulation_task, ingredients=ingredients, sim_time_limit=sim_time_limit, **kwargs)
        if fingerprint_test_task and fingerprint_test_task.sim_time_limit:
            fingerprint_test_tasks.append(fingerprint_test_task)
    def get_sim_time_limit(element):
        return element.sim_time_limit
    def get_fingerprint_test_group_task(fingerprint_test_tasks):
        return FingerprintTestGroupTask(sim_time_limit=fingerprint_test_tasks[0].sim_time_limit, tasks=list(fingerprint_test_tasks))
    sorted_fingerprint_test_tasks = sorted(fingerprint_test_tasks, key=get_sim_time_limit)
    grouped_fingerprint_test_tasks = [list(it) for k, it in itertools.groupby(sorted_fingerprint_test_tasks, get_sim_time_limit)]
    return list(map(get_fingerprint_test_group_task, grouped_fingerprint_test_tasks))

def get_fingerprint_test_tasks(**kwargs):
    """
    Returns multiple fingerprint test tasks matching the provided filter criteria. The returned tasks can be run by
    calling the :py:meth:`run <inet.common.task.MultipleTasks.run>` method.

    Parameters:
        kwargs (dict):
            The filter criteria parameters are inherited from the :py:meth:`get_simulation_tasks <inet.simulation.task.get_simulation_tasks>` method.

    Returns (:py:class:`MultipleFingerprintTestTasks`):
        an object that contains a list of :py:class:`FingerprintTestTask` objects matching the provided filter criteria.
        The result can be run (and re-run) without providing additional parameters.
    """
    def get_tasks(**kwargs):
        multiple_simulation_tasks = get_simulation_tasks(**kwargs)
        fingerprint_test_groups = []
        for simulation_task in multiple_simulation_tasks.tasks:
            simulation_config = simulation_task.simulation_config
            fingerprint_test_groups += collect_fingerprint_test_groups(simulation_task, **dict(kwargs, pass_keyboard_interrupt=True))
        return MultipleFingerprintTestTasks(multiple_simulation_tasks=multiple_simulation_tasks, tasks=fingerprint_test_groups, **dict(kwargs, simulation_project=multiple_simulation_tasks.simulation_project))
    return get_tasks(**kwargs)

def run_fingerprint_tests(**kwargs):
    """
    Runs one or more fingerprint tests that match the provided filter criteria.

    Parameters:
        kwargs (dict):
            The filter criteria parameters are inherited from the :py:func:`get_fingerprint_test_tasks` function.

    Returns (:py:class:`MultipleFingerprintTestTaskResults`):
        an object that contains a list of :py:class:`FingerprintTestTaskResult` objects. Each object describes the result of running one test task.
    """
    multiple_fingerprint_test_tasks = get_fingerprint_test_tasks(**kwargs)
    return multiple_fingerprint_test_tasks.run(**kwargs)

def print_correct_fingerprints(**kwargs):
    multiple_test_tasks = get_fingerprint_test_tasks(**kwargs)
    for test_task in multiple_test_tasks.tasks:
        print(test_task.simulation_task.get_parameters_string(**kwargs) + " " + COLOR_GREEN + str(test_task.fingerprint) + COLOR_RESET)

def print_missing_correct_fingerprints(simulation_project=None, ingredients="tplx", num_runs=1):
    if simulation_project is None:
        simulation_project = get_default_simulation_project()
    correct_fingerprint_store = get_correct_fingerprint_store(simulation_project)
    for simulation_config in get_all_simulation_configs(simulation_project):
        if not simulation_config.abstract and not simulation_config.emulation:
            for run_number in range(0, num_runs or simulation_config.num_runs):
                simulation_task = SimulationRun(simulation_config, run_number=run_number)
                if not simulation_task.is_interactive():
                    stored_fingerprint_entries = correct_fingerprint_store.filter_entries(ingredients=ingredients, working_directory=simulation_config.working_directory, ini_file=simulation_config.ini_file, config=simulation_config.config, run_number=run_number)
                    if len(stored_fingerprint_entries) == 0:
                        print(simulation_task.get_parameters_string())

def insert_missing_correct_fingerprints(source_ingredients, target_ingredients, simulation_project=None, **kwargs):
    if simulation_project is None:
        simulation_project = get_default_simulation_project()
    correct_fingerprint_store = get_correct_fingerprint_store(simulation_project)
    multiple_simulation_tasks = get_simulation_tasks(**kwargs)
    for simulation_task in multiple_simulation_tasks.tasks:
        simulation_config = simulation_task.simulation_config
        simulation_project = simulation_config.simulation_project
        working_directory = simulation_config.working_directory
        ini_file = simulation_config.ini_file
        config = simulation_config.config
        run_number = simulation_task.run_number
        for source_entry in correct_fingerprint_store.filter_entries(ingredients=source_ingredients, working_directory=working_directory, ini_file=ini_file, config=config, run_number=run_number):
            sim_time_limit = source_entry["sim_time_limit"]
            target_entries = correct_fingerprint_store.filter_entries(ingredients=target_ingredients, working_directory=working_directory, ini_file=ini_file, config=config, run_number=run_number, sim_time_limit=sim_time_limit)
            if len(target_entries) == 0:
                correct_fingerprint_store.insert_fingerprint("0000-0000/" + target_ingredients, ingredients=target_ingredients, working_directory=working_directory, ini_file=ini_file, config=config, run_number=run_number, sim_time_limit=sim_time_limit, test_result=source_entry["test_result"])

class FingerprintUpdateTask(SimulationUpdateTask):
    def __init__(self, action="Updating fingerprint", **kwargs):
        super().__init__(action=action, **kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs

    def run_protected(self, ingredients="tplx", sim_time_limit=None, output_stream=sys.stdout, **kwargs):
        simulation_config = self.simulation_task.simulation_config
        correct_fingerprint_store = get_correct_fingerprint_store(simulation_config.simulation_project)
        stored_fingerprint_entries = correct_fingerprint_store.filter_entries(ingredients=ingredients, sim_time_limit=sim_time_limit,
                                                                              working_directory=simulation_config.working_directory, ini_file=simulation_config.ini_file, config=simulation_config.config, run_number=self.simulation_task.run_number)
        if len(stored_fingerprint_entries) == 1:
            stored_fingerprint_entry = stored_fingerprint_entries[0]
            if sim_time_limit is None:
                sim_time_limit = stored_fingerprint_entry["sim_time_limit"]
            correct_fingerprint = Fingerprint(stored_fingerprint_entry["fingerprint"], stored_fingerprint_entry["ingredients"])
        else:
            correct_fingerprint = None
        fingerprint_arg = "0000-0000/" + ingredients
        append_args = ["--fingerprint", fingerprint_arg, "--vector-recording", "false", "--scalar-recording", "false"] + get_ingredients_append_args(ingredients)
        if sim_time_limit is not None:
            self.simulation_task.sim_time_limit = sim_time_limit
        simulation_task_result = self.simulation_task.run_protected(sim_time_limit=sim_time_limit, output_stream=output_stream, append_args=append_args, **kwargs)
        calculated_fingerprint = get_calculated_fingerprint(simulation_task_result, ingredients)
        return FingerprintUpdateTaskResult(task=self, simulation_task_result=simulation_task_result, correct_fingerprint=correct_fingerprint, calculated_fingerprint=calculated_fingerprint)

class MultipleFingerprintUpdateTasks(MultipleSimulationUpdateTasks):
    def __init__(self, multiple_simulation_tasks=None, name="update fingerprint", **kwargs):
        super().__init__(name=name, **kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs
        self.multiple_simulation_tasks = multiple_simulation_tasks

    def run(self, simulation_project=None, concurrent=None, build=None, **kwargs):
        if concurrent is None:
            concurrent = self.multiple_simulation_tasks.concurrent
        simulation_project = simulation_project or self.multiple_simulation_tasks.simulation_project
        if build if build is not None else get_default_build_argument():
            build_project(simulation_project=simulation_project, **kwargs)
        multiple_fingerprint_update_results = super().run(**kwargs)
        correct_fingerprint_store = get_correct_fingerprint_store(simulation_project)
        for fingerprint_update_result in multiple_fingerprint_update_results.results:
            fingerprint_update_task = fingerprint_update_result.task
            simulation_task = fingerprint_update_task.simulation_task
            simulation_config = simulation_task.simulation_config
            calculated_fingerprint = fingerprint_update_result.calculated_fingerprint
            if calculated_fingerprint is not None:
                correct_fingerprint_store.update_fingerprint(calculated_fingerprint.fingerprint, ingredients=calculated_fingerprint.ingredients, test_result="PASS", sim_time_limit=simulation_task.sim_time_limit,
                                                             working_directory=simulation_config.working_directory, ini_file=simulation_config.ini_file, config=simulation_config.config, run_number=simulation_task.run_number)
        correct_fingerprint_store.write()
        return multiple_fingerprint_update_results

class FingerprintUpdateTaskResult(SimulationUpdateTaskResult):
    def __init__(self, correct_fingerprint=None, calculated_fingerprint=None, reason=None, **kwargs):
        super().__init__(**kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs
        self.calculated_fingerprint = calculated_fingerprint
        if calculated_fingerprint is None:
            self.result = "ERROR"
            self.reason = reason or "Calculated fingerprint not found"
            self.color = COLOR_RED
        elif correct_fingerprint is None:
            self.result = "INSERT"
            self.reason = None
            self.color = COLOR_YELLOW
        elif calculated_fingerprint != correct_fingerprint:
            self.result = "UPDATE" if calculated_fingerprint else "ERROR"
            self.reason = None
            self.color = COLOR_YELLOW
        else:
            self.result = "KEEP"
            self.reason = None
            self.color = COLOR_GREEN
        self.expected = self.result == self.expected_result

    def get_description(self, complete_error_message=True, include_parameters=False, **kwargs):
        return (self.task.simulation_task.get_parameters_string() + " " if include_parameters else "") + \
               self.color + self.result + COLOR_RESET + " " + str(self.calculated_fingerprint) + \
               ((" " + self.simulation_task_result.get_error_message(complete_error_message=complete_error_message)) if self.simulation_task_result and self.simulation_task_result.result == "ERROR" else "") + \
               (" (" + self.reason + ")" if self.reason else "")

    def __repr__(self):
        return "Fingerprint update result: " + self.get_description(include_parameters=True)

    def print_result(self, complete_error_message=True, output_stream=sys.stdout, **kwargs):
        print(self.get_description(complete_error_message=complete_error_message), file=output_stream)

class MultipleFingerprintUpdateTaskResults(MultipleUpdateTaskResults):
    def __init__(self, multiple_fingerprint_update_tasks, fingerprint_update_results, elapsed_wall_time=None, **kwargs):
        super().__init__(**kwargs)
        self.locals = locals()
        self.locals.pop("self")
        self.kwargs = kwargs
        self.multiple_fingerprint_update_tasks = multiple_fingerprint_update_tasks
        self.fingerprint_update_results = fingerprint_update_results
        self.elapsed_wall_time = elapsed_wall_time
        self.num_different_results = 0
        self.num_keep = self.count_results("KEEP")
        self.num_insert = self.count_results("INSERT")
        self.num_update = self.count_results("UPDATE")
        self.num_error = self.count_results("ERROR")

    def __repr__(self):
        if len(self.fingerprint_update_results) == 1:
            return self.fingerprint_update_results[0].__repr__()
        else:
            details = self.get_details(exclude_result_filter="KEEP", include_parameters=True)
            return ("" if details.strip() == "" else "\nDetails:\n" + details + "\n\n") + \
                    "Fingerprint update summary: " + self.get_summary()

    def count_results(self, result):
        num = sum(e.result == result for e in self.fingerprint_update_results)
        if num != 0:
            self.num_different_results += 1
        return num

    def get_result_class_texts(self, result, color, num):
        texts = []
        if num != 0:
            texts += [(color + str(num) + " " + result + COLOR_RESET if num != 0 else "")]
        return texts

    def get_summary(self):
        if len(self.fingerprint_update_results) == 1:
            return self.fingerprint_update_results[0].get_description()
        else:
            texts = []
            if self.num_different_results != 1:
                texts.append(str(len(self.fingerprint_update_results)) + " TOTAL")
            texts += self.get_result_class_texts("KEEP", COLOR_GREEN, self.num_keep)
            texts += self.get_result_class_texts("INSERT", COLOR_YELLOW, self.num_insert)
            texts += self.get_result_class_texts("UPDATE", COLOR_YELLOW, self.num_update)
            texts += self.get_result_class_texts("ERROR", COLOR_RED, self.num_error)
            return ", ".join(texts) + (" in " + str(datetime.timedelta(seconds=self.elapsed_wall_time)) if self.elapsed_wall_time else "")

    def get_details(self, separator="\n  ", result_filter=None, exclude_result_filter=None, **kwargs):
        texts = []
        def matches_simulation_result(simulation_result, result):
            return simulation_result and simulation_result.result == result and \
                   matches_filter(result, result_filter, exclude_result_filter, True)
        for simulation_result in filter(lambda simulation_result: matches_simulation_result(simulation_result, "KEEP"), self.fingerprint_update_results):
            texts.append(simulation_result.get_description(**kwargs))
        for simulation_result in filter(lambda simulation_result: matches_simulation_result(simulation_result, "INSERT"), self.fingerprint_update_results):
            texts.append(simulation_result.get_description(**kwargs))
        for simulation_result in filter(lambda simulation_result: matches_simulation_result(simulation_result, "UPDATE"), self.fingerprint_update_results):
            texts.append(simulation_result.get_description(**kwargs))
        for simulation_result in filter(lambda simulation_result: matches_simulation_result(simulation_result, "ERROR"), self.fingerprint_update_results):
            texts.append(simulation_result.get_description(**kwargs))
        return "  " + separator.join(texts)

def get_update_correct_fingerprint_tasks(**kwargs):
    """
    Returns multiple fingerprint update tasks matching the provided filter criteria. The returned tasks can be run by
    calling the :py:meth:`run <inet.common.task.MultipleTasks.run>` method.

    Parameters:
        kwargs (dict):
            The filter criteria parameters are inherited from the :py:meth:`get_simulation_tasks <inet.simulation.task.get_simulation_tasks>` method.

    Returns (:py:class:`MultipleFingerprintUpdateTasks`):
        an object that contains a list of :py:class:`FingerprintUpdateTask` objects matching the provided filter criteria.
        The result can be run (and re-run) without providing additional parameters.
    """
    fingerprint_update_tasks = []
    multiple_simulation_tasks = get_simulation_tasks(**kwargs)
    for simulation_task in multiple_simulation_tasks.tasks:
        fingerprint_update_task = FingerprintUpdateTask(simulation_task=simulation_task, **kwargs)
        fingerprint_update_tasks.append(fingerprint_update_task)
    return MultipleFingerprintUpdateTasks(multiple_simulation_tasks, tasks=fingerprint_update_tasks, **kwargs)

def update_correct_fingerprints(**kwargs):
    """
    Updates the stored correct fingerprints in the database for one or more fingerprint tests that match the provided
    filter criteria.

    Parameters:
        kwargs (dict):
            The filter criteria parameters are inherited from the :py:func:`get_update_correct_fingerprint_tasks` function.

    Returns (:py:class:`MultipleFingerprintUpdateTaskResults`):
        an object that contains a list of :py:class:`FingerprintUpdateTaskResult` objects. Each object describes the result of running one update task.
    """
    multiple_fingerprint_update_tasks = get_update_correct_fingerprint_tasks(**kwargs)
    return multiple_fingerprint_update_tasks.run(**kwargs)

def remove_correct_fingerprint(simulation_task, ingredients=None, **kwargs):
    simulation_config = simulation_task.simulation_config
    correct_fingerprint_store = get_correct_fingerprint_store(simulation_config.simulation_project)
    correct_fingerprint_store.remove_fingerprints(ingredients=ingredients, sim_time_limit=simulation_task.sim_time_limit,
                                                  working_directory=simulation_config.working_directory, ini_file=simulation_config.ini_file, config=simulation_config.config, run_number=simulation_task.run_number)

def remove_correct_fingerprints(**kwargs):
    _logger.info("Removing correct fingerprints")
    multiple_simulation_tasks = get_simulation_tasks(**kwargs)
    for simulation_task in multiple_simulation_tasks.tasks:
        remove_correct_fingerprint(simulation_task)

def remove_extra_correct_fingerprints(simulation_project=None, **kwargs):
    if simulation_project is None:
        simulation_project = get_default_simulation_project()
    correct_fingerprint_store = get_correct_fingerprint_store(simulation_project)
    for entry in correct_fingerprint_store.get_entries():
        found = False
        for simulation_config in get_all_simulation_configs(simulation_project):
            if simulation_config.working_directory == entry["working_directory"] and \
               simulation_config.ini_file == entry["ini_file"] and \
               simulation_config.config == entry["config"]:
                found = True
        if not found:
            correct_fingerprint_store.remove_entry(entry)
