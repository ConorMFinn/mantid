# Mantid Repository : https://github.com/mantidproject/mantid#
# Copyright &copy; 2021 ISIS Rutherford Appleton Laboratory UKRI,
#   NScD Oak Ridge National Laboratory, European Spallation Source,
#   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
# SPDX - License - Identifier: GPL - 3.0 +
#  This file is part of the mantid workbench.
from .view import RegionSelectorView
from ..observers.observing_presenter import ObservingPresenter
from ..sliceviewer.models.dimensions import Dimensions
from ..sliceviewer.models.workspaceinfo import WorkspaceInfo, WS_TYPE
from ..sliceviewer.presenters.base_presenter import SliceViewerBasePresenter


class RegionSelector(ObservingPresenter, SliceViewerBasePresenter):
    def __init__(self, ws=None, parent=None, view=None):
        if ws and WorkspaceInfo.get_ws_type(ws) != WS_TYPE.MATRIX:
            raise NotImplementedError("Only Matrix Workspaces are currently supported by the region selector.")

        self.view = view if view else RegionSelectorView(self, parent)
        super().__init__(ws, self.view._data_view)

        if ws:
            self._initialise_dimensions(ws)
            self._set_workspace(ws)

    def dimensions_changed(self) -> None:
        self.new_plot()

    def slicepoint_changed(self) -> None:
        pass

    def canvas_clicked(self, event) -> None:
        pass

    def zoom_pan_clicked(self, active) -> None:
        pass

    def new_plot(self, *args, **kwargs):
        if self.model.ws:
            self.new_plot_matrix()

    def nonorthogonal_axes(self, state: bool) -> None:
        pass

    def update_workspace(self, workspace) -> None:
        if not self.model.ws:
            self._initialise_dimensions(workspace)

        self._set_workspace(workspace)

    def _initialise_dimensions(self, workspace):
        self.view.create_dimensions(dims_info=Dimensions.get_dimensions_info(workspace))
        self.view.create_axes_orthogonal(
            redraw_on_zoom=not WorkspaceInfo.can_support_dynamic_rebinning(workspace))

    def _set_workspace(self, workspace):
        self.model.ws = workspace
        self.view.set_workspace(workspace)
        self.new_plot()
