# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2023 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +
from mantid.api import (
    DataProcessorAlgorithm,
    AlgorithmFactory,
    MatrixWorkspaceProperty,
    IPeaksWorkspaceProperty,
    WorkspaceUnitValidator,
    Progress,
)
from mantid.kernel import (
    Direction,
    IntBoundedValidator,
    EnabledWhenProperty,
    PropertyCriterion,
    logger,
)
import numpy as np
from scipy.ndimage import convolve, label, maximum_position, binary_closing, sum_labels
from IntegratePeaksSkew import InstrumentArrayConverter, get_fwhm_from_back_to_back_params


class FindSXPeaksConvolve(DataProcessorAlgorithm):
    def name(self):
        return "FindSXPeaksConvolve"

    def category(self):
        return "Diffraction\\Reduction"

    def seeAlso(self):
        return ["FindSXPeaks"]

    def summary(self):
        return "Find single-crystal Bragg peaks in MatrixWorkspaces for instruments comprising rectangular detctors (such as SXD at ISIS)."

    def PyInit(self):
        # Input
        self.declareProperty(
            MatrixWorkspaceProperty(
                name="InputWorkspace", defaultValue="", direction=Direction.Input, validator=WorkspaceUnitValidator("TOF")
            ),
            doc="A MatrixWorkspace to integrate (x-axis must be TOF).",
        )
        self.declareProperty(
            IPeaksWorkspaceProperty(name="PeaksWorkspace", defaultValue="", direction=Direction.Output),
            doc="A PeaksWorkspace containing the peaks to integrate.",
        )
        self.declareProperty(
            name="ThresholdIoverSigma",
            defaultValue=5.0,
            direction=Direction.Input,
            doc="Threhsold value for I/sigma used to identify statistically significant peaks.",
        )
        #   window parameters
        self.declareProperty(
            name="NRows",
            defaultValue=5,
            direction=Direction.Input,
            validator=IntBoundedValidator(lower=3),
            doc="Number of row components in the detector to use in the convolution kernel. "
            "For WISH row components correspond to pixels along a single tube.",
        )
        self.declareProperty(
            name="NCols",
            defaultValue=5,
            direction=Direction.Input,
            validator=IntBoundedValidator(lower=3),
            doc="Number of column components in the detector to use in the convolution kernel. "
            "For WISH column components correspond to tubes.",
        )
        self.declareProperty(
            name="NBins",
            defaultValue=10,
            direction=Direction.Input,
            validator=IntBoundedValidator(lower=3),
            doc="Number of TOF bins to use in the convolution kernel.",
        )
        self.declareProperty(
            name="GetNBinsFromBackToBackParams",
            defaultValue=False,
            direction=Direction.Input,
            doc="If true the number of TOF bins used in the convolution kernel will be calculated from the FWHM of the "
            "BackToBackExponential peak using parameters defined in the instrument parameters.xml file.",
        )
        self.declareProperty(
            name="NFWHM",
            defaultValue=4,
            direction=Direction.Input,
            doc="If GetNBinsFromBackToBackParams=True then the number of TOF bins will be NFWHM x FWHM of the "
            "BackToBackExponential peak at the center of each detector panel at the middle of the spectrum.",
        )
        use_nBins = EnabledWhenProperty("GetNBinsFromBackToBackParams", PropertyCriterion.IsDefault)
        self.setPropertySettings("NBins", use_nBins)
        use_nfwhm = EnabledWhenProperty("GetNBinsFromBackToBackParams", PropertyCriterion.IsNotDefault)
        self.setPropertySettings("NFWHM", use_nfwhm)
        # peak validation
        self.declareProperty(
            name="MinFracSize",
            defaultValue=0.125,
            direction=Direction.Input,
            doc="Minimum peak size as a fraction of the kernel size.",
        )

    def PyExec(self):
        # get input
        ws = self.getProperty("InputWorkspace").value
        threshold_i_over_sig = self.getProperty("ThresholdIoverSigma").value
        nrows = self.getProperty("NRows").value
        ncols = self.getProperty("NCols").value
        nfwhm = self.getProperty("NFWHM").value
        get_nbins_from_b2bexp_params = self.getProperty("GetNBinsFromBackToBackParams").value
        min_frac_size = self.getProperty("MinFracSize").value

        # create ouput table workspace
        peaks = self.exec_child_alg("CreatePeaksWorkspace", InstrumentWorkspace=ws, NumberOfPeaks=0, OutputWorkspace="_peaks")

        array_converter = InstrumentArrayConverter(ws)
        banks = ws.getInstrument().findRectDetectors()
        prog_reporter = Progress(self, start=0.0, end=1.0, nreports=len(banks))
        for bank in banks:
            prog_reporter.report(f"Searching in {bank.getName()}")
            # add a dummy peak at center of detector (used in PeakData to get data arrays) - will be deleted after
            irow_to_del = peaks.getNumberPeaks()
            detid = bank[bank.xpixels() // 2][bank.ypixels() // 2].getID()
            ispec = ws.getIndicesFromDetectorIDs([detid])[0]
            xspec = ws.readX(ispec)
            icen = len(xspec) // 2
            self.exec_child_alg("AddPeak", PeaksWorkspace=peaks, RunWorkspace=ws, TOF=xspec[icen], DetectorID=detid)
            dummy_pk = peaks.getPeak(irow_to_del)

            # get nbins of kernel
            if get_nbins_from_b2bexp_params:
                fwhm = get_fwhm_from_back_to_back_params(dummy_pk, ws, detid)
                bin_width = xspec[icen + 1] - xspec[icen]
                nbins = max(3, int(nfwhm * fwhm / bin_width)) if fwhm is not None else self.getProperty("NBins")
            else:
                nbins = self.getProperty("NBins")

            # make a kernel with background subtraction shell of approx. same number of elements as peak region
            nrows_bg = max(1, nrows // 8)  # padding either side of e.g. nrows for bg shell
            ncols_bg = max(1, ncols // 8)
            nbins_bg = max(1, nbins // 8)
            kernel = np.zeros((nrows + 2 * nrows_bg, ncols + 2 * ncols_bg, nbins + 2 * nbins_bg))
            kernel[nrows_bg:-nrows_bg, ncols_bg:-ncols_bg, nbins_bg:-nbins_bg] = 1
            bg_mask = np.logical_not(kernel)
            kernel[bg_mask] = -(nrows * ncols * nbins) / bg_mask.sum()  # such that kernel.sum() = 0

            # get data in detector coords
            peak_data = array_converter.get_peak_data(dummy_pk, detid, bank.getName(), bank.xpixels(), bank.ypixels(), 1, 1)
            _, y, e = peak_data.get_data_arrays()  # 3d arrays [rows x cols x tof]

            # perform convolutions to integrate kernel/shoebox
            # pad with nearest so don't get peaks at edge when -ve values go outside data extent
            yconv = convolve(input=y, weights=kernel, mode="nearest")
            econv = np.sqrt(convolve(input=e**2, weights=abs(kernel), mode="nearest"))
            with np.errstate(divide="ignore", invalid="ignore"):
                intens_over_sig = yconv / econv  # ignore 0/0 (produces NaN)

            pk_mask = intens_over_sig > threshold_i_over_sig
            pk_mask = binary_closing(pk_mask)
            labels, nlabels = label(pk_mask)
            # identify labels of peaks above min size
            min_size = int(min_frac_size * kernel.size)
            nbins = sum_labels(pk_mask, labels, range(1, nlabels + 1))
            ilabels = np.flatnonzero(nbins > min_size) + 1

            # find index of maximum bin in peak region
            imaxs = maximum_position(y, labels, ilabels)
            imaxs_ioversig = maximum_position(intens_over_sig, labels, ilabels)

            # add peaks to table
            for ipk in range(len(imaxs)):
                irow, icol, itof = imaxs[ipk]
                self.exec_child_alg(
                    "AddPeak", PeaksWorkspace=peaks, RunWorkspace=ws, TOF=xspec[itof], DetectorID=int(peak_data.detids[irow, icol])
                )
                # set intensity of peak (rough estimate)
                pk = peaks.getPeak(peaks.getNumberPeaks() - 1)
                bin_width = xspec[itof + 1] - xspec[itof]
                pk.setIntensity(yconv[imaxs_ioversig[ipk]] * bin_width)
                pk.setSigmaIntensity(econv[imaxs_ioversig[ipk]] * bin_width)

            # remove dummy peak
            self.exec_child_alg("DeleteTableRows", TableWorkspace=peaks, Rows=[irow_to_del])

            # report number of peaks found
            logger.notice(f"Found {peaks.getNumberPeaks() - irow_to_del} peaks in {bank.getName()}")

        # assign output
        self.setProperty("PeaksWorkspace", peaks)

    def exec_child_alg(self, alg_name, **kwargs):
        alg = self.createChildAlgorithm(alg_name, enableLogging=False)
        alg.initialize()
        for prop, value in kwargs.items():
            alg.setProperty(prop, value)
        alg.execute()
        if "OutputWorkspace" in alg.outputProperties():
            return alg.getProperty("OutputWorkspace").value
        else:
            return None


# register algorithm with mantid
AlgorithmFactory.subscribe(FindSXPeaksConvolve)
