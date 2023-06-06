# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2023 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +
from typing import Sequence
import numpy as np
import mantid.simpleapi as mantid
from mantid.kernel import logger
from Diffraction.single_crystal.base_sx import BaseSX
from mantid.api import AnalysisDataService as ADS

tof_min = 6000
tof_max = 99000
lambda_min = 0.8


class WishSX(BaseSX):
    def __init__(self, vanadium_runno=None, ext=".raw"):
        super().__init__(vanadium_runno)
        self.sphere_shape = """<sphere id="sphere">
                               <centre x="0.0"  y="0.0" z="0.0" />
                               <radius val="0.0025"/>
                               </sphere>"""  # sphere radius 2.5mm  - used for vanadium and NaCl
        self.ext = ext  # allow to load .nxs for testing purposes

    def process_data(self, runs: Sequence[str], *args):
        """
        Loads runs, set goniometer (based on positional args), normalises by vanadium and corrects for absorption
        :param runs: list of run numbers (string or int)
        :param args: positonal args (require one for each goniometer axis) - can be motor names or lists of angles (one for each run)
        :return: name of last loaded workspaces
        """
        gonio_angles = args
        for irun, run in enumerate(runs):
            wsname = self.load_run(run, self.ext)
            # set goniometer
            if self.gonio_axes is not None:
                if len(gonio_angles) != len(self.gonio_axes):
                    logger.warning("No goniometer will be applied as the number of goniometer angles doesn't match the number of axes set.")
                elif isinstance(gonio_angles[0], str):
                    # gonio_angles are a list of motor strings (same for all runs)
                    self._set_goniometer_on_ws(wsname, gonio_angles)
                else:
                    # gonio_angles is a list of individual or tuple motor angles for each run
                    self._set_goniometer_on_ws(wsname, gonio_angles[irun])
            # correct for empty counts and normalise by vanadium
            self._divide_workspaces(wsname, self.van_ws)  # van_ws has been converted to TOF
            # set sample (must be done after gonio to rotate shape) and correct for attenuation
            if self.sample_dict is not None:
                mantid.SetSample(wsname, EnableLogging=False, **self.sample_dict)
                mantid.ConvertUnits(InputWorkspace=wsname, OutputWorkspace=wsname, Target="Wavelength", EnableLogging=False)
                if "<sphere" in ADS.retrieve(wsname).sample().getShape().getShapeXML():
                    transmission = mantid.SphericalAbsorption(InputWorkspace=wsname, OutputWorkspace="transmission", EnableLogging=False)
                else:
                    transmission = mantid.MonteCarloAbsorption(
                        InputWorkspace=wsname, OutputWorkspace="transmission", EventsPerPoint=self.n_mcevents, EnableLogging=False
                    )
                self._divide_workspaces(wsname, transmission)
                mantid.DeleteWorkspace(transmission)
                mantid.ConvertUnits(InputWorkspace=wsname, OutputWorkspace=wsname, Target="TOF", EnableLogging=False)
            # save results in dictionary
            self.set_ws(run, wsname)
        return wsname

    @staticmethod
    def mask_detector_edges(ws, nedge=16, ntubes=2):
        # mask pixels on ends of tubes
        mantid.MaskBTP(Workspace=ws, Pixel=f"1-{nedge},{512-nedge}-512")
        # only mask tubes on panels with edge facing beam in/out
        tubes = np.r_[152 - (ntubes - 1) // 2 : 153, 76 - ntubes // 2 + 1 : 77]
        mantid.MaskBTP(Workspace=ws, Bank="5-6", Tube=",".join(str(tube) for tube in tubes))
        tubes = np.r_[1 : (ntubes + 1) // 2 + 1, 77 : 77 + ntubes // 2]
        mantid.MaskBTP(Workspace=ws, Bank="1,10", Tube=",".join(str(tube) for tube in tubes))

    @BaseSX.default_apply_to_all_runs
    def convert_to_MD(self, run=None, frame="Q (lab frame)"):
        WishSX.mask_detector_edges(self.get_ws_name(run), nedge=16, ntubes=2)
        super().convert_to_MD(run=run, frame=frame)

    def process_vanadium(self):
        # vanadium
        self.van_ws = self.load_run(self.van_runno, ext=self.ext)
        mantid.SmoothNeighbours(InputWorkspace=self.van_ws, OutputWorkspace=self.van_ws, Radius=3)
        mantid.SmoothData(InputWorkspace=self.van_ws, OutputWorkspace=self.van_ws, NPoints=301)
        # correct vanadium for absorption
        mantid.ConvertUnits(InputWorkspace=self.van_ws, OutputWorkspace=self.van_ws, Target="Wavelength", EnableLogging=False)
        mantid.SetSample(
            self.van_ws,
            Geometry={"Shape": "CSG", "Value": self.sphere_shape},
            Material={"ChemicalFormula": "V0.95-Nb0.05", "SampleNumberDensity": 0.0722},
        )
        vanadium_transmission = mantid.SphericalAbsorption(InputWorkspace=self.van_ws, OutputWorkspace="vanadium_transmission")
        self._divide_workspaces(self.van_ws, vanadium_transmission)
        mantid.DeleteWorkspace(vanadium_transmission)
        mantid.ConvertUnits(InputWorkspace=self.van_ws, OutputWorkspace=self.van_ws, Target="TOF", EnableLogging=False)

    @staticmethod
    def load_run(runno, ext=".raw"):
        wsname = "WISH000" + str(runno)
        mon_name = wsname + "_monitors"
        mantid.Load(Filename=wsname + ext, OutputWorkspace=wsname)
        mantid.ExtractMonitors(InputWorkspace=wsname, DetectorWorkspace=wsname, MonitorWorkspace=mon_name)
        mantid.CropWorkspace(InputWorkspace=wsname, OutputWorkspace=wsname, XMin=tof_min, XMax=tof_max)
        mantid.CropWorkspace(InputWorkspace=mon_name, OutputWorkspace=mon_name, XMin=tof_min, XMax=tof_max)
        mantid.ConvertUnits(InputWorkspace=wsname, OutputWorkspace=wsname, Target="Wavelength")
        mantid.ConvertUnits(InputWorkspace=mon_name, OutputWorkspace=mon_name, Target="Wavelength")
        mantid.NormaliseToMonitor(InputWorkspace=wsname, OutputWorkspace=wsname, MonitorWorkspaceIndex=3, MonitorWorkspace=mon_name)
        mantid.ReplaceSpecialValues(InputWorkspace=wsname, OutputWorkspace=wsname, NaNValue=0, InfinityValue=0)
        mantid.CropWorkspace(InputWorkspace=wsname, OutputWorkspace=wsname, XMin=lambda_min)
        mantid.NormaliseByCurrent(InputWorkspace=wsname, OutputWorkspace=wsname)
        mantid.ConvertUnits(InputWorkspace=wsname, OutputWorkspace=wsname, Target="TOF")
        return wsname

    @staticmethod
    def find_sx_peaks(ws, bg=None, nstd=None, lambda_min=1.25, lambda_max=7.0, nbunch=3, out_peaks=None, **kwargs):
        wsname = WishSX.retrieve(ws).name()
        ws_rb = wsname + "_rb"
        mantid.Rebunch(InputWorkspace=ws, OutputWorkspace=ws_rb, NBunch=nbunch, EnableLogging=False)
        WishSX.mask_detector_edges(ws_rb, nedge=16, ntube=3)
        WishSX.crop_ws(ws_rb, lambda_min, lambda_max, xunit="Wavelength")
        if out_peaks is None:
            out_peaks = wsname + "_peaks"  # need to do this so not "_rb_peaks"
        BaseSX.find_sx_peaks(ws_rb, bg, nstd, out_peaks, **kwargs)
        mantid.DeleteWorkspace(ws_rb, EnableLogging=False)
        return out_peaks

    @staticmethod
    def remove_peaks_on_edge(peaks, nedge_tube=2, nedge_pix=16):
        peaks = WishSX.retrieve(peaks)
        # filter peaks on edge of tubes
        row = np.array(peaks.column("Row"))
        iremove = np.where(np.logical_or(row < nedge_pix, row > 512 - nedge_pix))[0]
        mantid.DeleteTableRows(TableWorkspace=peaks, Rows=iremove)
        # filter out peaks on tubes at edge of detector banks
        col = np.array(peaks.column("Col"))
        bank = np.array([int(name[-2:]) for name in peaks.column("BankName")])
        iremove = np.where(np.logical_and(col < nedge_tube, bank == 1))[0]
        mantid.DeleteTableRows(TableWorkspace=peaks, Rows=iremove)
        iremove = np.where(np.logical_and(col > 152 - nedge_tube, bank == 5))[0]
        mantid.DeleteTableRows(TableWorkspace=peaks, Rows=iremove)
        iremove = np.where(np.logical_and(col < nedge_tube, bank == 10))[0]
        mantid.DeleteTableRows(TableWorkspace=peaks, Rows=iremove)
        iremove = np.where(np.logical_and(col > 152 - nedge_tube, bank == 6))[0]
        mantid.DeleteTableRows(TableWorkspace=peaks, Rows=iremove)
