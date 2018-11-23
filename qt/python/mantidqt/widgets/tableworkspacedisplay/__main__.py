# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +
#  This file is part of the mantid workbench.
#
#

# To Run - target this package with PyCharm, and __main__ will be executed

import matplotlib

matplotlib.use('Qt5Agg')

from qtpy.QtWidgets import QApplication  # noqa: F402

from mantid.simpleapi import Load  # noqa: F402
from mantidqt.widgets.tableworkspacedisplay.presenter import TableWorkspaceDisplay  # noqa: F402
from workbench.plotting.functions import plot  # noqa: F402

app = QApplication([])
DEEE_WS_MON = Load("SavedTableWorkspace.nxs")
# DEEE_WS_MON = Load("TOPAZ_3007.peaks.nxs")
# DEEE_WS_MON = Load("SmallPeakWS10.nxs")
window = TableWorkspaceDisplay(DEEE_WS_MON, plot)
app.exec_()
