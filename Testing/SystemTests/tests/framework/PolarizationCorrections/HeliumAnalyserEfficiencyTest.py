# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2024 ISIS Rutherford Appleton Laboratory UKRI,
#   NScD Oak Ridge National Laboratory, European Spallation Source,
#   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
# SPDX - License - Identifier: GPL - 3.0 +

from mantid.simpleapi import *
from PolarizationCorrectionsBase import PolarizationCorrectionsBase


class HeliumAnalyserEfficiencyTest(PolarizationCorrectionsBase):
    def __init__(self):
        PolarizationCorrectionsBase.__init__(self)
        self.reference_basename = "HeliumAnalyser"
        self.input_filename = "ZOOM00038249.nxs"

    def _run_test(self):
        run = Load(self.input_filename)

        pre_processed = self._prepare_workspace(run)

        HeliumAnalyserEfficiency(
            pre_processed, "00,10,11,01", OutputWorkspace="efficiency", OutputFitCurves="curves", OutputFitParameters="params"
        )

    def _validate(self):
        result_eff = "efficiency"
        reference_eff = f"{self.reference_basename}EfficiencyReference.nxs"
        result_curves = "curves"
        reference_curves = f"{self.reference_basename}CurvesReference.nxs"
        result_params = "params"
        reference_params = f"{self.reference_basename}ParamsReference.nxs"

        is_efficiency_match = self._validate_workspace(result_eff, reference_eff)
        is_curves_match = self._validate_workspace(result_curves, reference_curves)
        is_params_match = self._validate_workspace(result_params, reference_params)

        return is_efficiency_match and is_curves_match and is_params_match


class HeliumAnalyserEfficiencyUnpolarisedTest(HeliumAnalyserEfficiencyTest):
    def __init__(self):
        HeliumAnalyserEfficiencyTest.__init__(self)
        self.reference_basename = "UnpolHeliumAnalyser"
        self.input_filename = "ZOOM00038253.nxs"
