# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2023 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +
from typing import Sequence
import numpy as np
from enum import Enum
import mantid.simpleapi as mantid
from mantid.api import FunctionFactory, AnalysisDataService as ADS
from mantid.kernel import logger
from FindGoniometerFromUB import getSignMaxAbsValInCol
from mantid.geometry import CrystalStructure, SpaceGroupFactory, ReflectionGenerator, ReflectionConditionFilter
from os import path

from abc import ABC, abstractmethod


class PEAK_TYPE(Enum):
    FOUND = "found"
    PREDICT = "predicted"
    PREDICT_SAT = "predicted_sat"


class INTEGRATION_TYPE(Enum):
    MD = "int_MD"
    MD_OPTIMAL_RADIUS = "int_MD_opt"
    SKEW = "int_skew"


class BaseSX(ABC):
    def __init__(self, vanadium_runno: str, file_ext: str):
        self.runs = dict()
        self.van_runno = vanadium_runno
        self.van_ws = None
        self.gonio_axes = None
        self.sample_dict = None
        self.n_mcevents = 1200
        self.file_ext = file_ext  # file extension

    # --- decorator to apply to all runs is run=None ---
    def default_apply_to_all_runs(func):
        def apply(self, *args, **kwargs):
            if "run" in kwargs and kwargs["run"] is not None:
                func(self, *args, **kwargs)
            else:
                for run in self.runs.keys():
                    func(self, *args, **kwargs, run=run)

        return apply

    # --- getters ---

    def get_ws(self, run):
        try:
            return BaseSX.retrieve(self.runs[str(run)]["ws"])
        except:
            return None

    def get_ws_name(self, run):
        ws = self.get_ws(run)
        if ws is not None:
            return ws.name()

    def get_md(self, run):
        try:
            return BaseSX.retrieve(self.runs[str(run)]["MD"])
        except:
            return None

    def get_md_name(self, run):
        ws = self.get_md(run)
        if ws is not None:
            return ws.name()

    def get_peaks(self, run, peak_type, integration_type=None):
        if integration_type is None:
            fieldname = peak_type.value
        else:
            fieldname = "_".join([peak_type.value, integration_type.value])
        try:
            return BaseSX.retrieve(self.runs[str(run)][fieldname])
        except:
            return None

    def get_peaks_name(self, run, peak_type, integration_type=None):
        ws = self.get_peaks(run, peak_type, integration_type)
        if ws is not None:
            return ws.name()

    @staticmethod
    def has_sample(ws):
        return BaseSX.retrieve(ws).sample().getMaterial().numberDensity > 1e-15

    # --- setters ---

    def set_van_ws(self, van_ws):
        self.van_ws = BaseSX.retrieve(van_ws).name()

    def set_ws(self, run, ws):
        run_str = str(run)
        if run_str not in self.runs:
            self.runs[run_str] = {"ws": BaseSX.retrieve(ws).name()}
        else:
            self.runs[run_str]["ws"] = BaseSX.retrieve(ws).name()

    def set_peaks(self, run, peaks, peak_type=PEAK_TYPE.FOUND, integration_type=None):
        if integration_type is None:
            fieldname = peak_type.value
        else:
            fieldname = "_".join([peak_type.value, integration_type.value])
        self.runs[str(run)][fieldname] = BaseSX.retrieve(peaks).name()

    def set_md(self, run, ws_md):
        self.runs[str(run)]["MD"] = BaseSX.retrieve(ws_md).name()

    def set_sample(self, **kwargs):
        self.sample_dict = kwargs

    def set_goniometer_axes(self, *args):
        """
        :param axes: collection of axes - e.g. ([x,y,z,hand], [x,y,z,hand]) - in same order to be passed to SetSample
        """
        # convert to strings
        self.gonio_axes = [",".join([str(elem) for elem in axis]) for axis in args]

    def set_mc_abs_nevents(self, nevents):
        self.n_mcevents = nevents

    # --- abstract methods ---

    @abstractmethod
    def process_data(self, runs: Sequence[str], *args):
        raise NotImplementedError()

    @abstractmethod
    def load_run(self, runno):
        raise NotImplementedError()

    @abstractmethod
    def process_vanadium(self):
        raise NotImplementedError()

    # --- methods ---

    def delete_run_data(self, run, del_MD=False):
        run = str(run)
        fields = ["ws", "MD"] if del_MD else ["ws"]
        for field in fields:
            if self.runs[run][field]:
                mantid.DeleteWorkspace(self.runs[run][field])
                self.runs[run][field] = None

    def _set_goniometer_on_ws(self, ws, angles):
        """
        :param ws: workspace or name
        :param angles: list of angles or motor names
        :return:
        """
        if self.gonio_axes is not None:
            axis_dict = {f"Axis{iax}": ",".join([str(angles[iax]), self.gonio_axes[iax]]) for iax in range(len(angles))}
            mantid.SetGoniometer(Workspace=ws, EnableLogging=False, **axis_dict)

    @staticmethod
    def convert_ws_to_MD(wsname, md_name=None, frame="Q (lab frame)"):
        if md_name is None:
            md_name = wsname + "_MD"
        xunit = BaseSX.get_xunit(wsname)  # get initial xunit
        BaseSX._normalise_by_bin_width_in_k(wsname)  # normalise by bin-width in K = 2pi/lambda
        wsMD = mantid.ConvertToDiffractionMDWorkspace(
            InputWorkspace=wsname,
            OutputWorkspace=md_name,
            LorentzCorrection=True,
            OneEventPerBin=False,
            OutputDimensions=frame,
            EnableLogging=False,
        )
        BaseSX._normalise_by_bin_width_in_k(wsname, undo=True)  # normalise by bin-width in K = 2pi/lambda
        mantid.ConvertUnits(InputWorkspace=wsname, OutputWorkspace=wsname, Target=xunit, EnableLogging=False)
        mantid.DeleteWorkspace("PreprocessedDetectorsWS")
        return wsMD

    @default_apply_to_all_runs
    def convert_to_MD(self, run=None, frame="Q (lab frame)"):
        wsname = self.get_ws_name(run)
        md_name = wsname + "_MD"
        if frame == "HKL" and not BaseSX.retrieve(wsname).sample().hasOrientedLattice():
            peaks = self.get_peaks(run, PEAK_TYPE.FOUND)
            if peaks is not None and peaks.sample().hasOrientedLattice():
                mantid.SetUB(wsname, UB=peaks.sample().getOrientedLattice().getUB())
            else:
                logger.error(f"No UB specified for {wsname} or found peaks - cannot transform to HKL.")
                return
        BaseSX.convert_ws_to_MD(wsname, md_name, frame)
        self.set_md(run, md_name)

    @default_apply_to_all_runs
    def load_isaw_ub(self, isaw_file: str, run=None, tol=0.15):
        ws = self.get_ws_name(run)
        try:
            mantid.LoadIsawUB(InputWorkspace=ws, Filename=isaw_file)
            peaks = self.get_peaks(run, PEAK_TYPE.FOUND)
            if peaks is not None:
                mantid.LoadIsawUB(InputWorkspace=peaks, Filename=isaw_file)
                mantid.IndexPeaks(PeaksWorkspace=peaks, Tolerance=tol, RoundHKLs=True)
        except:
            print(f"LoadIsawUB failed for run {run}")

    def find_ub_using_lattice_params(self, global_B, tol=0.15, **kwargs):
        if global_B:
            peaks = [self.get_peaks_name(run, PEAK_TYPE.FOUND) for run in self.runs.keys()]
            mantid.FindGlobalBMatrix(PeakWorkspaces=peaks, Tolerance=tol, **kwargs)
        else:
            for run in self.runs.keys():
                peaks = self.get_peaks_name(run, PEAK_TYPE.FOUND)
                mantid.FindUBUsingLatticeParameters(PeaksWorkspace=peaks, Tolerance=tol, **kwargs)
                mantid.IndexPeaks(PeaksWorkspace=peaks, Tolerance=tol, RoundHKLs=True)

    @default_apply_to_all_runs
    def calculate_U_matrix(self, run=None, **kwargs):
        mantid.CalculateUMatrix(PeaksWorkspace=self.get_peaks_name(run, PEAK_TYPE.FOUND), **kwargs)

    @default_apply_to_all_runs
    def calibrate_sample_pos(self, tol=0.15, run=None):
        peaks = self.get_peaks(run, PEAK_TYPE.FOUND)
        mantid.IndexPeaks(PeaksWorkspace=peaks, Tolerance=tol, RoundHKLs=True)
        mantid.OptimizeCrystalPlacement(
            PeaksWorkspace=peaks,
            ModifiedPeaksWorkspace=peaks,
            FitInfoTable=run + "_sample_pos_fit_info",
            AdjustSampleOffsets=True,
            OptimizeGoniometerTilt=True,
            MaxSamplePositionChangeMeters=0.01,
            MaxIndexingError=tol,
        )
        mantid.IndexPeaks(PeaksWorkspace=peaks, Tolerance=tol, RoundHKLs=True)

    @default_apply_to_all_runs
    def predict_peaks(self, peak_type=PEAK_TYPE.PREDICT, run=None, **kwargs):
        input_ws = None
        ws = self.get_ws(run)
        peaks = self.get_peaks(run, PEAK_TYPE.FOUND)
        if peaks is not None and peaks.sample().hasOrientedLattice():
            input_ws = peaks
        elif ws is not None and ws.sample().hasOrientedLattice():
            input_ws = ws
        if input_ws is not None:
            out_peaks_name = "_".join([input_ws.name(), peak_type.value])
            if peak_type == PEAK_TYPE.PREDICT:
                mantid.PredictPeaks(InputWorkspace=input_ws, OutputWorkspace=out_peaks_name, EnableLogging=False, **kwargs)
            else:
                pred_peaks = self.get_peaks(run, PEAK_TYPE.PREDICT)
                if pred_peaks is not None and pred_peaks.sample().hasOrientedLattice():
                    mantid.PredictFractionalPeaks(Peaks=pred_peaks, FracPeaks=out_peaks_name, EnableLogging=False, **kwargs)
            self.set_peaks(run, out_peaks_name, peak_type)

    @staticmethod
    def get_back_to_back_exponential_func(pk, ws, ispec):
        tof = pk.getTOF()
        func = FunctionFactory.Instance().createPeakFunction("BackToBackExponential")
        func.setParameter("X0", tof)  # set centre
        func.setMatrixWorkspace(ws, ispec, 0.0, 0.0)  # calc A,B,S based on peak cen
        return func

    @staticmethod
    def convert_dTOF_to_dQ_for_peak(dtof, pk):
        tof = pk.getTOF()
        modQ = 2 * np.pi / pk.getDSpacing()
        return (modQ / tof) * dtof

    @staticmethod
    def get_radius(pk, ws, ispec, scale):
        func = BaseSX.get_back_to_back_exponential_func(pk, ws, ispec)
        dtof = scale / func.getParameterValue("B")
        return BaseSX.convert_dTOF_to_dQ_for_peak(dtof, pk)

    @staticmethod
    def integrate_peaks_MD(wsMD, peaks, out_peaks, **kwargs):
        default_kwargs = {"IntegrateIfOnEdge": False, "UseOnePercentBackgroundCorrection": False}
        kwargs = {**default_kwargs, **kwargs}
        peaks_int = mantid.IntegratePeaksMD(
            InputWorkspace=wsMD,
            PeaksWorkspace=peaks,
            OutputWorkspace=out_peaks,
            **kwargs,
        )
        return peaks_int

    def integrate_peaks_MD_optimal_radius(self, wsMD, peaks, out_peaks, dq=0.01, scale=5, ws=None, **kwargs):
        # note this is not a static method so that the static `get_radius` can be overridden
        peaks = BaseSX.retrieve(peaks)
        use_empty_inst = ws is None
        if use_empty_inst:
            ws = mantid.LoadEmptyInstrument(
                InstrumentName=peaks.getInstrument().getFullName(), OutputWorkspace="empty", EnableLogging=False
            )
            axis = ws.getAxis(0)
            axis.setUnit("TOF")
        else:
            ws = BaseSX.retrieve(ws)
            mantid.ConvertUnits(
                InputWorkspace=ws, OutputWorkspace=ws, Target="TOF", EnableLogging=False
            )  # needs to be in TOF for setting B
        ispecs = ws.getIndicesFromDetectorIDs(peaks.column("DetID"))
        rads = [self.get_radius(pk, ws, ispecs[ipk], scale) for ipk, pk in enumerate(peaks)]
        bin_edges = np.arange(min(rads), max(rads) + dq, dq)
        ibins = np.digitize(rads, bin_edges[:-1])
        peaks_int = mantid.CloneWorkspace(InputWorkspace=peaks, OutputWorkspace=out_peaks, EnableLogging=False)
        mantid.DeleteTableRows(TableWorkspace=peaks_int, Rows=list(range(peaks_int.getNumberPeaks())), EnableLogging=False)
        for ibin in np.unique(ibins):
            rad = bin_edges[ibin]
            ipks = np.where(ibins == ibin)[0]
            irows_to_del = set(range(peaks.getNumberPeaks())) - set(ipks)
            peaks_subset = mantid.CloneWorkspace(InputWorkspace=peaks, OutputWorkspace=peaks.name() + "_rad", EnableLogging=False)
            mantid.DeleteTableRows(TableWorkspace=peaks_subset, Rows=list(irows_to_del), EnableLogging=False)
            # integrate
            peaks_subset = BaseSX.integrate_peaks_MD(
                wsMD,
                peaks_subset.name(),
                peaks_subset.name(),
                PeakRadius=rad,
                BackgroundInnerRadius=rad,
                BackgroundOuterRadius=rad * (2 ** (1 / 3)),
                **kwargs,
            )
            peaks_int = mantid.CombinePeaksWorkspaces(
                LHSWorkspace=peaks_int, RHSWorkspace=peaks_subset, OutputWorkspace=peaks_int.name(), EnableLogging=False
            )
            mantid.DeleteWorkspace(peaks_subset, EnableLogging=False)
        if use_empty_inst:
            mantid.DeleteWorkspace(ws, EnableLogging=False)
        return peaks_int

    @staticmethod
    def integrate_peaks_skew(ws, peaks, out_peaks, **kwargs):
        peaks_int = mantid.IntegratePeaksSkew(InputWorkspace=ws, PeaksWorkspace=peaks, OutputWorkspace=out_peaks, **kwargs)
        return peaks_int

    @default_apply_to_all_runs
    def integrate_data(self, integration_type, peak_type, tol=0, run=None, **kwargs):
        pk_table = self.get_peaks(run, peak_type)
        if peak_type == PEAK_TYPE.FOUND and tol > 0:
            mantid.IndexPeaks(PeaksWorkspace=pk_table, Tolerance=tol, RoundHKLs=True)
            BaseSX.remove_unindexed_peaks(pk_table)
        # integrate
        peak_int_name = "_".join([BaseSX.retrieve(pk_table).name(), integration_type.value])
        ws = self.get_ws(run)
        ws_md = self.get_md(run)
        if integration_type == INTEGRATION_TYPE.MD and ws_md is not None:
            BaseSX.integrate_peaks_MD(ws_md, pk_table, peak_int_name, **kwargs)
        elif integration_type == INTEGRATION_TYPE.MD_OPTIMAL_RADIUS and ws_md is not None:
            if ws is not None:
                kwargs["ws"] = ws  # use this workspace to avoid loading in empty
            self.integrate_peaks_MD_optimal_radius(ws_md, pk_table, peak_int_name, **kwargs)
        else:
            BaseSX.integrate_peaks_skew(ws, pk_table, peak_int_name, **kwargs)
        # store result
        self.set_peaks(run, peak_int_name, peak_type, integration_type)

    def save_peak_table(self, run, peak_type, integration_type, save_dir, save_format, run_ref=None):
        peaks = self.get_peaks_name(run, peak_type, integration_type)
        if run_ref is not None and run_ref != run:
            self.make_UB_consistent(self.get_peaks_name(run_ref, peak_type, integration_type), peaks)
        filepath = path.join(save_dir, "_".join([peaks, save_format])) + ".int"
        mantid.SaveReflections(InputWorkspace=peaks, Filename=filepath, Format=save_format, SplitFiles=False)
        mantid.SaveNexus(InputWorkspace=peaks, Filename=filepath[:-3] + "nxs")

    def save_all_peaks(self, peak_type, integration_type, save_dir, save_format, run_ref=None):
        runs = self.runs.keys()
        if len(runs) > 1:
            # get name for peak table from range of runs integrated
            min_ws = min(runs, key=lambda k: int("".join(filter(str.isdigit, k))))
            max_ws = max(runs, key=lambda k: int("".join(filter(str.isdigit, k))))
            all_peaks = f'{"-".join([min_ws, max_ws])}_{peak_type.value}_{integration_type.value}'
            mantid.CreatePeaksWorkspace(InstrumentWorkspace=self.van_ws, NumberOfPeaks=0, OutputWorkspace=all_peaks)
            for run in runs:
                peaks = self.get_peaks(run, peak_type, integration_type)
                if run_ref is not None and run_ref != run:
                    self.make_UB_consistent(self.get_peaks(run_ref, peak_type, integration_type), peaks)
                mantid.CombinePeaksWorkspaces(LHSWorkspace=all_peaks, RHSWorkspace=peaks, OutputWorkspace=all_peaks)
            filepath = path.join(save_dir, "_".join([all_peaks, save_format])) + ".int"
            mantid.SaveReflections(InputWorkspace=all_peaks, Filename=filepath, Format=save_format, SplitFiles=False)
            mantid.SaveNexus(InputWorkspace=all_peaks, Filename=filepath[:-3] + "nxs")

    def _is_vanadium_processed(self):
        return self.van_ws is not None and ADS.doesExist(self.van_ws)

    @staticmethod
    def _normalise_by_bin_width_in_k(wsname, undo=False):
        ws = mantid.ConvertUnits(InputWorkspace=wsname, OutputWorkspace=wsname, Target="Momentum", EnableLogging=False)
        si = ws.spectrumInfo()
        for ispec in range(ws.getNumberHistograms()):
            if si.hasDetectors(ispec) and not si.isMonitor(ispec):
                # divide y and e by bin-width
                xedges = ws.readX(ispec)
                dx = np.diff(xedges)
                scale = dx if not undo else 1 / dx
                ws.setY(ispec, ws.readY(ispec) / scale)
                ws.setE(ispec, ws.readE(ispec) / scale)

    @staticmethod
    def _minus_workspaces(ws_lhs, ws_rhs):
        mantid.RebinToWorkspace(
            WorkspaceToRebin=ws_rhs, WorkspaceToMatch=ws_lhs, OutputWorkspace=ws_rhs, PreserveEvents=False, EnableLogging=False
        )
        mantid.Minus(LHSWorkspace=ws_lhs, RHSWorkspace=ws_rhs, OutputWorkspace=ws_lhs, EnableLogging=False)
        mantid.ReplaceSpecialValues(
            InputWorkspace=ws_lhs,
            OutputWorkspace=ws_lhs,
            NaNValue=0,
            InfinityValue=0,
            BigNumberThreshold=1e15,
            SmallNumberThreshold=1e-15,
            EnableLogging=False,
        )

    @staticmethod
    def _divide_workspaces(ws_lhs, ws_rhs):
        mantid.RebinToWorkspace(
            WorkspaceToRebin=ws_rhs, WorkspaceToMatch=ws_lhs, OutputWorkspace=ws_rhs, PreserveEvents=False, EnableLogging=False
        )
        mantid.Divide(LHSWorkspace=ws_lhs, RHSWorkspace=ws_rhs, OutputWorkspace=ws_lhs, EnableLogging=False)
        mantid.ReplaceSpecialValues(
            InputWorkspace=ws_lhs,
            OutputWorkspace=ws_lhs,
            NaNValue=0,
            InfinityValue=0,
            BigNumberThreshold=1e15,
            SmallNumberThreshold=1e-15,
            EnableLogging=False,
        )

    @staticmethod
    def make_UB_consistent(ws_ref, ws):
        # compare U matrix to perform TransformHKL to preserve indexing
        U_ref = BaseSX.retrieve(ws_ref).sample().getOrientedLattice().getU()
        U = BaseSX.retrieve(ws).sample().getOrientedLattice().getU()
        # find transform required  ( U_ref = U T^-1) - see TransformHKL docs for details
        transform = np.linalg.inv(getSignMaxAbsValInCol(np.matmul(np.linalg.inv(U), U_ref)))
        try:
            mantid.TransformHKL(PeaksWorkspace=ws, HKLTransform=transform, FindError=True, EnableLogging=False)
        except ValueError:
            # probably not enough peaks to optimize UB to find error on lattice parameters (will be set to 0)
            mantid.TransformHKL(PeaksWorkspace=ws, HKLTransform=transform, FindError=False, EnableLogging=False)

    @staticmethod
    def crop_ws(ws, xmin, xmax, xunit="Wavelength"):
        wsname = BaseSX.retrieve(ws).name()
        ws_xunit = BaseSX.get_xunit(ws)
        if not ws_xunit == xunit:
            mantid.ConvertUnits(InputWorkspace=wsname, OutputWorkspace=wsname, Target=xunit, EnableLogging=False)
        mantid.CropWorkspace(InputWorkspace=wsname, OutputWorkspace=wsname, XMin=xmin, XMax=xmax, EnableLogging=False)
        if not ws_xunit == xunit:
            mantid.ConvertUnits(InputWorkspace=wsname, OutputWorkspace=wsname, Target=ws_xunit, EnableLogging=False)

    @staticmethod
    def find_sx_peaks(ws, bg=None, nstd=None, out_peaks=None, **kwargs):
        default_kwargs = {"XResolution": 200, "PhiResolution": 2, "TwoThetaResolution": 2}
        kwargs = {**default_kwargs, **kwargs}  # will overwrite default with provided if duplicate keys
        ws = BaseSX.retrieve(ws)
        if out_peaks is None:
            out_peaks = ws.name() + "_peaks"
        if bg is None:
            ymaxs = np.max(ws.extractY(), axis=1)  # max in each spectrum
            avg = np.median(ymaxs)
            std = (np.percentile(ymaxs, 90) - avg) / 1.2815
            bg = avg + nstd * std
        elif nstd is None:
            return
        # get unit to convert back to after peaks found
        xunit = BaseSX.get_xunit(ws)
        if not xunit == "TOF":
            ws = mantid.ConvertUnits(
                InputWorkspace=ws, OutputWorkspace=ws.name(), Target="TOF", EnableLogging=False
            )  # FindSXPeaks requires TOF
        # extract y data (to use to determine to determine threshold)
        mantid.FindSXPeaks(
            InputWorkspace=ws,
            PeakFindingStrategy="AllPeaks",
            AbsoluteBackground=bg,
            ResolutionStrategy="AbsoluteResolution",
            OutputWorkspace=out_peaks,
            EnableLogging=False,
            **kwargs,
        )
        if not xunit == "TOF":
            mantid.ConvertUnits(InputWorkspace=ws, OutputWorkspace=ws.name(), Target=xunit, EnableLogging=False)
        return out_peaks

    @staticmethod
    def get_xunit(ws):
        ws = BaseSX.retrieve(ws)
        xname = ws.getXDimension().name
        if xname == "Time-of-flight":
            return "TOF"
        else:
            return xname.replace("-", "")

    @staticmethod
    def remove_forbidden_peaks(peaks, hm_symbol):
        spgr = SpaceGroupFactory.createSpaceGroup(hm_symbol)
        iremove = []
        for ipk, pk in enumerate(BaseSX.retrieve(peaks)):
            if not spgr.isAllowedReflection(pk.getIntHKL()):
                iremove.append(ipk)
        mantid.DeleteTableRows(TableWorkspace=peaks, Rows=iremove, EnableLogging=False)

    @staticmethod
    def remove_peaks_near_powder_line(peaks, resolution=0.05, dmin=0.5, dmax=10, phase="Al", dlist=None, structure=None):
        if not dlist:
            xtal = None
            if structure:
                xtal = structure  # use CrystalStructure provided
            else:
                xtal_structures = {
                    "Cu": ["3.6149 3.6149 3.6149", "F m -3 m", "Cu 0 0 0 1.0 0.05"],
                    "Al": ["4.0495 4.0495 4.0495", "F m -3 m", "Al 0 0 0 1.0 0.05"],
                    "Si": ["5.431 5.431 5.431", "F d -3 m", "Si 0 0 0 1.0 0.05"],
                    "Ti-alpha": ["2.934 2.934 4.703 90 90 120", "P 63/m m c", "Ti 1/3 2/3 0.25 1.0 0.05"],
                    "Ti-beta": ["3.2657 3.2657 3.2657", "I m -3 m", "Ti 0 0 0 1.0 0.05"],
                }
                xtal = CrystalStructure(*xtal_structures[phase])
            # generate list of reflections
            generator = ReflectionGenerator(xtal, ReflectionConditionFilter.StructureFactor)
            hkls = generator.getUniqueHKLs(dmin, dmax)
            dlist = generator.getDValues(hkls)
        # remove peaks within delta_d/d of the peaks in dlist
        peaks = BaseSX.retrieve(peaks)
        dspacings = np.array(peaks.column("DSpacing"))
        iremove = []
        for dpk in dlist:
            ipks = np.where(abs(dspacings - dpk) < resolution * dpk)[0]
            if ipks.size > 0:
                iremove.extend(ipks)
        mantid.DeleteTableRows(TableWorkspace=peaks, Rows=[int(irow) for irow in iremove], EnableLogging=False)
        return dlist  # list of dspacing of powder peaks

    @staticmethod
    def remove_duplicate_peaks_by_hkl(peaks):
        peaks = BaseSX.retrieve(peaks)
        hkl = np.array([peaks.column("h"), peaks.column("k"), peaks.column("l")])
        hkl_int = np.round(hkl)
        hkl_unq, idx, ncnts = np.unique(hkl_int, axis=1, return_inverse=True, return_counts=True)
        irows_to_del = []
        for iunq in np.where(ncnts > 1)[0]:
            # ignore unindexed peaks
            if hkl_unq[:, iunq].any():
                iduplicates = np.where(idx == iunq)[0].tolist()
                dhkl_sq = np.sum((hkl[:, iduplicates] - hkl_int[:, iduplicates]) ** 2, axis=0)
                iduplicates.pop(np.argmin(dhkl_sq))
                irows_to_del.extend(iduplicates)
        mantid.DeleteTableRows(TableWorkspace=peaks, Rows=irows_to_del, EnableLogging=False)

    @staticmethod
    def remove_duplicate_peaks_by_qlab(peaks, q_tol=0.05):
        """
        Will keep lowest dSpacing peak (often best approx. to peak centre)
        """
        peaks = BaseSX.retrieve(peaks)
        peaks = mantid.SortPeaksWorkspace(
            InputWorkspace=peaks, OutputWorkspace=peaks.name(), ColumnNameToSortBy="DSpacing", EnableLogging=False
        )
        irows_to_del = []
        for ipk in range(peaks.getNumberPeaks() - 1):
            pk = peaks.getPeak(ipk)
            pk_dSpacing = pk.getDSpacing()
            d_tol = q_tol * (pk_dSpacing**2) / (2 * np.pi)
            istep = 1
            while ipk + istep < peaks.getNumberPeaks() and abs(pk_dSpacing - peaks.getPeak(ipk + istep).getDSpacing()) <= d_tol:
                dQ = pk.getQLabFrame() - peaks.getPeak(ipk + istep).getQLabFrame()
                if dQ.norm() <= q_tol:
                    irows_to_del.append(ipk + istep)
                istep += 1
        mantid.DeleteTableRows(TableWorkspace=peaks, Rows=irows_to_del, EnableLogging=False)

    @staticmethod
    def remove_unindexed_peaks(peaks):
        """
        Will keep lowest dSpacing peak (often best approx. to peak centre)
        """
        return mantid.FilterPeaks(
            InputWorkspace=peaks,
            OutputWorkspace=BaseSX.retrieve(peaks).name(),
            FilterVariable="h^2+k^2+l^2",
            FilterValue=0,
            Operator=">",
            EnableLogging=False,
        )

    @staticmethod
    def remove_non_integrated_peaks(peaks):
        return mantid.FilterPeaks(
            InputWorkspace=peaks,
            OutputWorkspace=BaseSX.retrieve(peaks).name(),
            FilterVariable="Signal/Noise",
            FilterValue=0,
            Operator=">",
            EnableLogging=False,
        )

    @staticmethod
    def retrieve(ws):
        if isinstance(ws, str):
            return ADS.retrieve(ws)
        else:
            return ws
