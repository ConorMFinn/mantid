// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +

#include "MantidAlgorithms/CorelliPowderCalibrationApply.h"
#include "MantidAPI/FileProperty.h"
#include "MantidAPI/ITableWorkspace.h"
#include "MantidAPI/InstrumentValidator.h"
#include "MantidAPI/MatrixWorkspace.h"
#include "MantidGeometry/Instrument.h"
#include "MantidGeometry/Instrument/ComponentInfo.h"
#include "MantidGeometry/Instrument/DetectorInfo.h"
#include "MantidGeometry/Objects/IObject.h"
#include "MantidKernel/Logger.h"

namespace Mantid {
    namespace Algorithms {

        using namespace Kernel;
        using namespace API;
        namespace {
        Logger logger("CorelliPowderCalibrationApply");
        }

        DECLARE_ALGORITHM(CorelliPowderCalibrationApply)

        /**
         * @brief Initialization
         * 
         */
        void CorelliPowderCalibrationApply::init() {
            
            //
            // InputWorkspace
            // [Input, Required, MatrixWorkspace or EventsWorkspace]
            // workspace to which the calibration should be applied
            // Question: Can we only use a inputworkspace for in&out given that
            //           the size of CORELLI workspace is fairly large
            auto wsValidator = std::make_shared<InstrumentValidator>();
            declareProperty(
                std::make_unique<WorkspaceProperty<MatrixWorkspace>>(
                    "InputWorkspace",
                    "",
                    Direction::InOut,
                    PropertyMode::Mandatory,
                    wsValidator),
                "Input workspace for calibration");

            //
            // CalibrationTable
            // [Input, Optional, TableWorkspace]
            // workspace resulting from uploading
            declareProperty(
                std::make_unique<WorkspaceProperty<ITableWorkspace>>(
                    "CalibrationTable",
                    "",
                    Direction::Input,
                    PropertyMode::Optional),
                "TableWorkspace containing calibration table");

            //
            // DatabaseDirectory
            // [Input, Optional, string]
            // absolute path to the database.
            // Question:  can we drop this option?
            //            it might be a better idea to keep one type of input instead
            //            of trying to create a "smart" input detector at c++ level,
            //            which can be error prone (espeically user errors)
            declareProperty(
                std::make_unique<FileProperty>(
                    "DatabaseDirectory",
                    "/SNS/CORELLI",
                    FileProperty::Directory),
                "absolute path to the CORELLI database");

            //
            // OutputWorkspace
            // if emtpy, InputWorkspace will be calibrated.
            // Question: similar to the question for inputworkspace,
            //           we might need some input form the CIS about
            //           their expectations
            declareProperty(
                std::make_unique<WorkspaceProperty<MatrixWorkspace>>(
                    "OutputWorkspace",
                    "",
                    Direction::Output,
                    PropertyMode::Optional),
                "Calibrated input workspace clone");
        }

        /**
         * @brief Validate algorithm inputs
         * 
         * @return std::map<std::string, std::string> 
         */
        std::map<std::string, std::string>
        CorelliPowderCalibrationApply::validateInputs() {
            std::map<std::string, std::string> issues;
            inputWS = getProperty("InputWorkspace");
            
            // 1_check: input workspace is from CORELLI
            if (inputWS->getInstrument()->getName() != "CORELLI") {
                issues["InputWorkspace"] = "CORELLI only algorithm, aborting";
            }

            // 2_check: headers of calibration table
            calTable = getProperty("CalibrationTable");
            auto refCalTableHeader = CorelliCalibration::calibrationTableColumnNames;
            std::vector<std::string> colnames = calTable->getColumnNames();
            if (colnames.size() != refCalTableHeader.size()) {
                issues["CalibrationTable"] = "Headers of input calibration table does not match required";
            }
            for (size_t i=0; i<colnames.size(); i++){
                if (colnames[i] != refCalTableHeader[i]){
                    issues["CalibrationTable"] = "Header mismatch at " + std::to_string(i);
                    break;
                }
            }

            // 3_check: any other things to check

            return issues;
        }
    
        /**
         * @brief Executes the algorithm.
         * 
         */
        void CorelliPowderCalibrationApply::exec(){
            g_log.notice() << "Start applying CORELLI calibration\n";

            // Get input arguments
            inputWS = getProperty("InputWorkspace");
            calTable = getProperty("CalibrationTable");
            const std::string dbDir = getProperty("DatabaseDirectory");
            if (isDefault("DatabaseDirectory")) {
                g_log.notice() << "Using default database directory\n";
            } 

            //
            outputWS = getProperty("OutputWorkspace");
            if (outputWS != inputWS) {
                outputWS = inputWS->clone();
                }

            // Parse calibration table

            // Translate each component in the instrument
            // [source, sample, bank1,.. bank92]
            // dev reference:
            // https://github.com/mantidproject/mantid/blob/f0fad29ee0496901b8d453c738ea948644c7c6a6/Framework/Reflectometry/src/SpecularReflectionPositionCorrect2.cpp#L241
            auto moveAlg = createChildAlgorithm("MoveInstrumentComponent");
            moveAlg -> initialize();
            // Todo ...
            moveAlg->execute();

            // Rotate each component in the instrument
            auto rotateAlg = createChildAlgorithm("RotateInstrumentComponent");
            rotateAlg -> initialize();
            // Todo ...
            rotateAlg->execute();

            // Config output
            setProperty("OutputWorkspace", outputWS);

            g_log.notice() << "Finished applying CORELLI calibration\n";
        }

    } // namespace Algorithm

} // namespace Mantid
