# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2023 ISIS Rutherford Appleton Laboratory UKRI,
#   NScD Oak Ridge National Laboratory, European Spallation Source,
#   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
# SPDX - License - Identifier: GPL - 3.0 +
import unittest
from mantid.simpleapi import (
    FindSXPeaksConvolve,
    LoadParameterFile,
    AnalysisDataService,
    SortPeaksWorkspace,
)
from testhelpers import WorkspaceCreationHelper
from numpy import array, sqrt


class FindSXPeaksConvolveTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # load empty instrument with RectangularDetector banks and create a peak table
        cls.ws = WorkspaceCreationHelper.create2DWorkspaceWithRectangularInstrument(1, 7, 13)  # nbanks, npix, nbins
        AnalysisDataService.addOrReplace("ws_rect", cls.ws)
        axis = cls.ws.getAxis(0)
        axis.setUnit("TOF")
        # fake peak centred on ispec=24 (detid=79) and TOF=5 - near middle of bank
        peak_1D = array([0, 0, 0, 1, 4, 6, 4, 1, 0, 0, 0, 0, 0])
        cls.ws.setY(24, cls.ws.readY(12) + peak_1D)
        for ispec in [17, 23, 24, 25, 31]:
            cls.ws.setY(ispec, cls.ws.readY(ispec) + peak_1D)
            cls.ws.setE(ispec, sqrt(cls.ws.readY(ispec)))
        # fake peak centred on ispec=12 (detid=61) and TOF=7 - near detector edge
        cls.ws.setY(12, cls.ws.readY(12) + peak_1D[::-1])
        for ispec in [5, 11, 12, 13, 19]:
            cls.ws.setY(ispec, cls.ws.readY(ispec) + peak_1D[::-1])
            cls.ws.setE(ispec, sqrt(cls.ws.readY(ispec)))
        # Add back-to-back exponential params
        xml_params = """
        <?xml version="1.0" encoding="UTF-8" ?>
        <parameter-file instrument = "basic_rect" valid-from = "2013-11-06T00:00:00">
        <component-link name = "bank1">
        <parameter name="BackToBackExponential:A" type="fitting">
        <formula eq="2" unit="TOF" result-unit="1/TOF" /> <fixed />
        </parameter>
        <parameter name="BackToBackExponential:B" type="fitting">
        <formula eq="2" unit="TOF" result-unit="1/TOF" /> <fixed />
        </parameter>
        <parameter name="BackToBackExponential:S" type="fitting">
        <formula eq="2" unit="TOF" result-unit="TOF" />
        </parameter>
        </component-link>
        </parameter-file>
        """
        LoadParameterFile(cls.ws, ParameterXML=xml_params)

    @classmethod
    def tearDownClass(cls):
        AnalysisDataService.clear()

    def _assert_found_correct_peaks(self, peak_ws):
        self.assertEqual(peak_ws.getNumberPeaks(), 2)
        peak_ws = SortPeaksWorkspace(
            InputWorkspace=peak_ws, OutputWorkspace=peak_ws.name(), ColumnNameToSortBy="DetID", SortAscending=False
        )
        pk = peak_ws.getPeak(0)
        self.assertEqual(pk.getDetectorID(), 79)
        self.assertEqual(pk.getTOF(), 5.0, delta=1e-8)
        self.assertAlmostEqual(pk.getIntensityOverSigma(), 6.0622, delta=1e-4)
        pk = peak_ws.getPeak(1)
        self.assertEqual(pk.getDetectorID(), 61)
        self.assertEqual(pk.getTOF(), 7.0, delta=1e-8)
        self.assertAlmostEqual(pk.getIntensityOverSigma(), 5.3981, delta=1e-4)

    def test_exec_specify_nbins(self):
        out = FindSXPeaksConvolve(
            InputWorkspace=self.ws, PeaksWorkspace="peaks1", NRows=3, NCols=3, NBins=5, ThresholdIoverSigma=3.0, MinFracSize=0.02
        )
        self._assert_found_correct_peak(out)

    def test_exec_get_nbins_from_back_to_back_params(self):
        out = FindSXPeaksConvolve(
            InputWorkspace=self.ws,
            PeaksWorkspace="peaks1",
            NRows=3,
            NCols=3,
            GetNBinsFromBackToBackParams=True,
            ThresholdIoverSigma=3.0,
            MinFracSize=0.02,
        )
        self._assert_found_correct_peak(out)

    def test_exec_IoverSigma_threshold(self):
        out = FindSXPeaksConvolve(InputWorkspace=self.ws, PeaksWorkspace="peaks3", NRows=3, NCols=3, NBins=5, ThresholdIoverSigma=100.0)
        self.assertEqual(out.getNumberPeaks(), 0)

    def test_exec_remove_on_edge(self):
        out = FindSXPeaksConvolve(
            InputWorkspace=self.ws,
            PeaksWorkspace="peaks1",
            NRows=3,
            NCols=3,
            NBins=5,
            ThresholdIoverSigma=3.0,
            MinFracSize=0.02,
            RemoveOnEdge=True,
        )
        self.assertEqual(out.getNumberPeaks(), 1)
        # check it's the correct peak
        pk = out.getPeak(0)
        self.assertEqual(pk.getDetectorID(), 79)

    def test_exec_min_frac_size(self):
        out = FindSXPeaksConvolve(
            InputWorkspace=self.ws, PeaksWorkspace="peaks1", NRows=3, NCols=3, NBins=5, ThresholdIoverSigma=3.0, MinFracSize=0.5
        )
        self.assertEqual(out.getNumberPeaks(), 0)


if __name__ == "__main__":
    unittest.main()
