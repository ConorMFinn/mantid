# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2019 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +

from Engineering.gui.engineering_diffraction.tabs.common import create_error_message
from mantid.simpleapi import logger
from mantidqt.utils.asynchronous import AsyncTask


class FittingDataPresenter(object):
    def __init__(self, model, view):
        self.model = model
        self.view = view
        self.worker = None

        self.row_numbers = {}  # {ws_name: table_row}

        # Connect view signals to local methods
        self.view.set_on_load_clicked(self.on_load_clicked)
        self.view.set_enable_button_connection(self._enable_load_button)
        self.view.set_on_remove_selected_clicked(self._remove_selected_table_rows)
        self.view.set_on_remove_all_clicked(self._remove_all_table_rows)

    def on_load_clicked(self):
        if self._validate():
            filenames = self._get_filenames()
            self._start_load_worker(filenames)

    def remove_workspace(self, ws_name):
        if ws_name in self.get_loaded_workspaces():
            self.get_loaded_workspaces().pop(ws_name)
            self._remove_table_row(self.row_numbers.pop(ws_name))

    def rename_workspace(self, old_name, new_name):
        if old_name in self.get_loaded_workspaces():
            self.get_loaded_workspaces()[new_name] = self.get_loaded_workspaces().pop(
                old_name)
            self.row_numbers[new_name] = self.row_numbers.pop(old_name)

    def clear_workspaces(self):
        self.get_loaded_workspaces().clear()
        self.row_numbers.clear()

    def replace_workspace(self, name, workspace):
        if name in self.get_loaded_workspaces():
            self.get_loaded_workspaces()[name] = workspace

    def get_loaded_workspaces(self):
        return self.model.get_loaded_workspaces()

    def _start_load_worker(self, filenames):
        """
        Load one to many files into mantid that are tracked by the interface.
        :param filenames: Comma separated list of filenames to load
        """
        self.worker = AsyncTask(self.model.load_files, (filenames, ),
                                error_cb=self._on_worker_error,
                                finished_cb=self._emit_enable_button_signal,
                                success_cb=self._on_worker_success)
        self.worker.start()

    def _on_worker_error(self, _):
        logger.error("Error occurred when loading files.")
        self._emit_enable_button_signal()

    def _on_worker_success(self, _):
        self._remove_all_table_rows()
        self.row_numbers = {}
        for i, name in enumerate(self.get_loaded_workspaces()):
            self._add_row_to_table(name, i)

    def _enable_load_button(self, enabled):
        self.view.set_load_button_enabled(enabled)

    def _emit_enable_button_signal(self):
        self.view.sig_enable_load_button.emit(True)

    def _get_filenames(self):
        return self.view.get_filenames_to_load()

    def _is_searching(self):
        return self.view.is_searching()

    def _files_are_valid(self):
        return self.view.get_filenames_valid()

    def _validate(self):
        if self._is_searching():
            create_error_message(self.view, "Mantid is searching for files. Please wait.")
            return False
        elif not self._files_are_valid():
            create_error_message(self.view, "Entered files are not valid.")
            return False
        return True

    def _add_row_to_table(self, ws_name, row):
        words = ws_name.split("_")
        if len(words) == 4 and words[2] == "bank":
            self.view.add_table_row(words[1], words[3])
            self.row_numbers[ws_name] = row
        else:
            logger.warning(
                "The workspace '{}' was not in the correct naming format. Files should be named in the following way: "
                "INSTRUMENT_RUNNUMBER_bank_BANK.".format(ws_name)
            )

    def _remove_table_row(self, row_no):
        self.view.remove_table_row(row_no)

    def _remove_selected_table_rows(self):
        self.view.remove_selected()

    def _remove_all_table_rows(self):
        self.view.remove_all()
