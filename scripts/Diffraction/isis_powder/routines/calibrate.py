# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
#   NScD Oak Ridge National Laboratory, European Spallation Source,
#   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
# SPDX - License - Identifier: GPL - 3.0 +
import mantid.simpleapi as mantid

import isis_powder.routines.common as common
from isis_powder.routines.common_enums import INPUT_BATCHING


def create_van(instrument, run_details, absorb):
    """
    Creates a splined vanadium run for the following instrument. Requires the run_details for the
    vanadium workspace we will process and whether to apply absorption corrections.
    :param instrument: The instrument object that will be used to supply various instrument specific methods
    :param run_details: The run details associated with this vanadium run
    :param absorb: Boolean flag whether to apply absorption corrections
    :return: Processed workspace group in dSpacing (but not splined)
    """
    van = run_details.vanadium_run_numbers
    # Always sum a range of inputs as its a vanadium run over multiple captures
    input_van_ws_list = common.load_current_normalised_ws_list(
        run_number_string=van, instrument=instrument, input_batching=INPUT_BATCHING.Summed
    )
    corrected_van_ws = input_van_ws_list[0]  # As we asked for a summed ws there should only be one returned

    instrument.create_solid_angle_corrections(corrected_van_ws, run_details)

    if not (run_details.empty_inst_runs is None):
        summed_empty_inst = common.generate_summed_runs(empty_ws_string=run_details.empty_inst_runs, instrument=instrument)
        mantid.SaveNexus(Filename=run_details.summed_empty_inst_file_path, InputWorkspace=summed_empty_inst)
        corrected_van_ws = common.subtract_summed_runs(ws_to_correct=corrected_van_ws, empty_ws=summed_empty_inst)

    # Crop the tail end of the data on PEARL if they are not capturing slow neutrons
    corrected_van_ws = instrument._crop_raw_to_expected_tof_range(ws_to_crop=corrected_van_ws)

    if absorb:
        corrected_van_ws = instrument._apply_absorb_corrections(run_details=run_details, ws_to_correct=corrected_van_ws)
    else:
        # Assume that create_van only uses Vanadium runs
        mantid.SetSampleMaterial(InputWorkspace=corrected_van_ws, ChemicalFormula="V")

    mantid.ApplyDiffCal(InstrumentWorkspace=corrected_van_ws, CalibrationFile=run_details.offset_file_path)
    aligned_ws = mantid.ConvertUnits(InputWorkspace=corrected_van_ws, Target="dSpacing")
    solid_angle = instrument.get_solid_angle_corrections(run_details.run_number, run_details)
    if solid_angle:
        aligned_ws = mantid.Divide(LHSWorkspace=aligned_ws, RHSWorkspace=solid_angle)
        mantid.DeleteWorkspace(solid_angle)
    focused_vanadium = mantid.DiffractionFocussing(InputWorkspace=aligned_ws, GroupingFileName=run_details.grouping_file_path)
    # convert back to TOF based on engineered detector positions
    mantid.ApplyDiffCal(InstrumentWorkspace=focused_vanadium, ClearCalibration=True)
    focused_spectra = common.extract_ws_spectra(focused_vanadium)
    focused_spectra = instrument._crop_van_to_expected_tof_range(focused_spectra)

    d_spacing_group, tof_group = instrument._output_focused_ws(processed_spectra=focused_spectra, run_details=run_details)
    _create_vanadium_splines(focused_spectra, instrument, run_details)

    common.keep_single_ws_unit(d_spacing_group=d_spacing_group, tof_group=tof_group, unit_to_keep=instrument._get_unit_to_keep())

    common.remove_intermediate_workspace(corrected_van_ws)
    common.remove_intermediate_workspace(aligned_ws)
    common.remove_intermediate_workspace(focused_vanadium)
    common.remove_intermediate_workspace(focused_spectra)

    return d_spacing_group


def create_van_per_detector(instrument, run_details, absorb, test=False):
    """
    Creates a splined vanadium run for the following instrument. Requires the run_details for the
    vanadium workspace we will process and whether to apply absorption corrections.
    :param instrument: The instrument object that will be used to supply various instrument specific methods
    :param run_details: The run details associated with this vanadium run
    :param absorb: Boolean flag whether to apply absorption corrections
    :param: test: Only true when running automated tests
    :return: Processed workspace group in dSpacing (but not splined)
    """
    van = run_details.vanadium_run_numbers
    # Always sum a range of inputs as its a vanadium run over multiple captures
    input_van_ws_list = common.load_current_normalised_ws_list(run_number_string=van, instrument=instrument,
                                                               input_batching=INPUT_BATCHING.Summed)
    input_van_ws = input_van_ws_list[0]  # As we asked for a summed ws there should only be one returned

    if test:
        input_van_ws = crop_to_small_ws_for_test(input_van_ws)

    instrument.create_solid_angle_corrections(input_van_ws, run_details)

    if not (run_details.empty_inst_runs is None):
        summed_empty = common.generate_summed_runs(empty_ws_string=run_details.empty_inst_runs, instrument=instrument)
        if test:
            summed_empty = crop_to_small_ws_for_test(summed_empty)
        if not test:
            mantid.SaveNexus(Filename=run_details.summed_empty_inst_file_path, InputWorkspace=summed_empty)
        corrected_van_ws = common.subtract_summed_runs(ws_to_correct=input_van_ws, empty_ws=summed_empty)

    corrected_van_ws = instrument._crop_raw_to_expected_tof_range(ws_to_crop=corrected_van_ws)

    if absorb:
        corrected_van_ws = instrument._apply_absorb_corrections(run_details=run_details,
                                                                ws_to_correct=corrected_van_ws)
    else:
        # Assume that create_van only uses Vanadium runs
        mantid.SetSampleMaterial(InputWorkspace=corrected_van_ws, ChemicalFormula='V')

    mantid.ApplyDiffCal(InstrumentWorkspace=corrected_van_ws,
                        CalibrationFile=run_details.offset_file_path)
    aligned_ws = mantid.ConvertUnits(InputWorkspace=corrected_van_ws, Target="dSpacing")
    solid_angle = instrument.get_solid_angle_corrections(run_details.run_number, run_details)
    if solid_angle:
        aligned_ws = mantid.Divide(LHSWorkspace=aligned_ws, RHSWorkspace=solid_angle)
        mantid.DeleteWorkspace(solid_angle)

    # Do not clear the calibration using ApplyDiffCal()
    # convert back to TOF based on calibrated detector positions
    # this is unfocused data so the calibration is still valid

    run_number = str(run_details.output_run_string)
    ext = run_details.file_extension if run_details.file_extension else ""
    d_spacing_out_name = run_number + ext + "-ResultD"
    tof_out_name = run_number + ext + "-ResultTOF"

    d_spacing_group = mantid.ConvertUnits(InputWorkspace=aligned_ws,
                                          OutputWorkspace=d_spacing_out_name, Target="dSpacing")
    tof_group = mantid.ConvertUnits(InputWorkspace=aligned_ws,
                                    OutputWorkspace=tof_out_name, Target="TOF")
    _create_vanadium_splines_one_ws(aligned_ws, instrument, run_details)

    common.keep_single_ws_unit(d_spacing_group=d_spacing_group, tof_group=tof_group,
                               unit_to_keep=instrument._get_unit_to_keep())
    common.remove_intermediate_workspace(corrected_van_ws)

    return d_spacing_group


def _create_vanadium_splines(focused_spectra, instrument, run_details):
    splined_ws_list = instrument._spline_vanadium_ws(focused_spectra)
    out_spline_van_file_path = run_details.splined_vanadium_file_path
    append = False
    for ws in splined_ws_list:
        mantid.SaveNexus(Filename=out_spline_van_file_path, InputWorkspace=ws, Append=append)
        append = True
    # Group for user convenience
    group_name = "Van_spline_data"
    tt_mode = instrument._get_current_tt_mode()
    if tt_mode:
        group_name = group_name + "_" + tt_mode

    mantid.GroupWorkspaces(InputWorkspaces=splined_ws_list, OutputWorkspace=group_name)


def _create_vanadium_splines_one_ws(vanadium_splines, instrument, run_details):
    mantid.ExtractMonitors(InputWorkspace=vanadium_splines,
                           DetectorWorkspace="vanadium_splines",
                           MonitorWorkspace="vanadium_monitors")

    from mantid.api import AnalysisDataService as ADS
    if instrument._inst_settings.masking_file_name is not None:
        import os
        masking_file_path = os.path.join(instrument.calibration_dir,
                                         instrument._inst_settings.masking_file_name)
        bragg_mask_list = common.read_masking_file(masking_file_path)
        vanadium_splines = ADS.retrieve("vanadium_splines")
        for bank_number, peaks_on_bank in enumerate(bragg_mask_list):
            ws_indices_on_bank_to_mask = []
            for workspace_index in range(vanadium_splines.getNumberHistograms()):
                # assuming that each spectrum only has one detector ID
                detector = vanadium_splines.getInstrument().getDetector(vanadium_splines.getSpectrum(workspace_index).getDetectorIDs()[0])
                bank_name_containing_detector = detector.getFullName().split('/')[1]
                if bank_name_containing_detector == f"bank{bank_number+1}":
                    ws_indices_on_bank_to_mask.append(workspace_index)
            for mask_params in peaks_on_bank:
                vanadium_splines = mantid.MaskBins(InputWorkspace="vanadium_splines",
                                                   XMin=mask_params[0], XMax=mask_params[1],
                                                   InputWorkspaceIndexSet=ws_indices_on_bank_to_mask)

    vanadium_splines.clearMonitorWorkspace()
    vanadium_splines = mantid.RemoveMaskedSpectra(InputWorkspace=vanadium_splines)
    out_name = "van_{}".format(run_details.vanadium_run_numbers)
    mantid.ConvertUnits(InputWorkspace=vanadium_splines, Target="TOF", OutputWorkspace=out_name)
    mantid.SplineBackground(InputWorkspace=out_name, WorkspaceIndex=0,
                            EndWorkspaceIndex=ADS.retrieve(out_name).getNumberHistograms()-1,
                            NCoeff=instrument._inst_settings.spline_coeff,
                            OutputWorkspace=out_name, EnableLogging=False)
    out_spline_van_file_path = run_details.splined_vanadium_file_path
    mantid.SaveNexus(Filename=out_spline_van_file_path, InputWorkspace=out_name)


def crop_to_small_ws_for_test(input_workspace):
    return input_workspace  # dummy function to protect production code and overridden during testing
