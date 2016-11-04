from __future__ import (absolute_import, division, print_function)

import mantid.simpleapi as mantid
import os

from isis_powder.abstract_inst import AbstractInst
from isis_powder.polaris_routines import polaris_calib_factory

import isis_powder.common as common


class Polaris(AbstractInst):

    _lower_lambda_range = 0.0
    _upper_lambda_range = 0.0  # TODO populate this

    _focus_tof_binning_params = "TODO"  # TODO

    _create_van_calib_tof_binning = {}  # TODO with calibration
    _calibration_grouping_names = "BankN"

    def __init__(self, user_name=None, calibration_dir=None, raw_data_dir=None, output_dir=None,
                 input_file_ext=".raw", sample_empty_name=None):  # TODO move TT_mode PEARL specific

        super(Polaris, self).__init__(user_name=user_name, calibration_dir=calibration_dir, raw_data_dir=raw_data_dir,
                                      output_dir=output_dir, default_input_ext=input_file_ext, tt_mode=tt_mode)

        self._grouping_file_path = None  # TODO support this
        self._cal_file_path = None  # TODO support this

        self._sample_empty = sample_empty_name

    # Abstract implementation
    def _get_lambda_range(self):
        return self._lower_lambda_range, self._upper_lambda_range

    def _get_focus_tof_binning(self):
        return self._focus_tof_binning_params

    def _get_create_van_tof_binning(self):
        return self._create_van_calib_tof_binning

    def _get_default_group_names(self):
        return self._calibration_grouping_names

    def _get_calibration_full_paths(self, cycle):
        # TODO implement this properly
        temp_variable = polaris_calib_factory.get_calibration_file(cycle)

        calibration_dir = self.calibration_dir

        calibration_full_path = calibration_dir + temp_variable
        grouping_full_path = calibration_dir + temp_variable
        vanadium_absorb_full_path = None
        vanadium_full_path = None

        calibration_details = {"calibration": calibration_full_path,
                               "grouping": grouping_full_path,
                               "vanadium_absorption": vanadium_absorb_full_path,
                               "vanadium": vanadium_full_path}

        return calibration_details

    @staticmethod
    def _generate_inst_file_name(run_number):
        return "POL" + str(run_number)  # TODO check this is correct

    @staticmethod
    def _get_instrument_alg_save_ranges(instrument=''):
        alg_range = 5
        save_range = 0  # TODO set save range
        return alg_range, save_range

    @staticmethod
    def _get_cycle_information(run_number):
        return {"cycle": "",  # TODO implement properly
                "instrument_version": ""}

    def _normalise_ws(self, ws_to_correct, monitor_ws=None, spline_terms=20):
        normalised_ws = mantid.NormaliseByCurrent(InputWorkspace=ws_to_correct)
        return normalised_ws

    def _mask_noisy_detectors(self, vanadium_ws):
        summed_van_ws = mantid.Integration(InputWorkspace=vanadium_ws)
        # TODO do they want this masking detectors with too high a contribution?
        mantid.MaskDetectorsIf(InputWorkspace=summed_van_ws, InputCalFile=self._grouping_file_path,
                               OutputCalFile=self._cal_file_path, Mode="DeselectIf", Operator="LessEqual", Value=10)

    def _calculate_solid_angle_efficiency_corrections(self, vanadium_ws):
        solid_angle_ws = mantid.SolidAngle(InputWorkspace=vanadium_ws)
        solid_angle_multiplicand = mantid.CreateSingleValuedWorkspace(DataValue=str(100))
        solid_angle_ws = mantid.Multiply(LHSWorkspace=solid_angle_ws, RHSWorkspace=solid_angle_multiplicand)
        common.remove_intermediate_workspace(solid_angle_multiplicand)

        efficiency_ws = mantid.Divide(LHSWorkspace=vanadium_ws, RHSWorkspace=solid_angle_ws)
        efficiency_ws = mantid.ConvertUnits(InputWorkspace=efficiency_ws, Target="Wavelength")
        efficiency_ws = mantid.Integration(InputWorkspace=efficiency_ws,
                                           RangeLower=self._lower_lambda_range, RangeUpper=self._upper_lambda_range)

        corrections_ws = mantid.Multiply(LHSWorkspace=solid_angle_ws, RHSWorkspace=efficiency_ws)
        corrections_divisor_ws = mantid.CreateSingleValuedWorkspace(DataValue=str(100000))
        corrections_ws = mantid.Divide(LHSWorkspace=corrections_ws, RHSWorkspace=corrections_divisor_ws)

        common.remove_intermediate_workspace(corrections_divisor_ws)
        common.remove_intermediate_workspace(solid_angle_ws)
        common.remove_intermediate_workspace(efficiency_ws)

        return corrections_ws

    def _subtract_sample_empty(self, input_sample):
        # TODO move this to be generated by calib factory so we don't have to use the full fname
        if self._sample_empty is not None:
            empty_sample_path = self.calibration_dir + self._sample_empty
            empty_sample = mantid.Load(Filename=empty_sample_path)
            empty_sample = self._normalise_ws(empty_sample)
            input_sample = mantid.Minus(LHSWorkspace=input_sample, RHSWorkspace=empty_sample)
            common.remove_intermediate_workspace(empty_sample)
        return input_sample

    def _apply_solid_angle_efficiency_corr(self, ws_to_correct, vanadium_ws_path):
        vanadium_ws = mantid.Load(Filename=vanadium_ws_path)
        vanadium_ws = self._normalise_ws(vanadium_ws)
        corrections = self._calculate_solid_angle_efficiency_corrections(vanadium_ws)

        corrected_ws = mantid.Divide(LHSWorkspace=ws_to_correct, RHSWorkspace=corrections)
        common.remove_intermediate_workspace(vanadium_ws)
        common.remove_intermediate_workspace(corrections)

        return corrected_ws
