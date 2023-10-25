// Mantid Repository : https://github.ervicecom/mantidproject/mantid
//
// Copyright &copy; 2023 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#include "ISISEnergyTransferModel.h"
#include "ISISEnergyTransferModelUtils.h"
#include "MantidAPI/AlgorithmManager.h"
#include "MantidAPI/AlgorithmRuntimeProps.h"
#include "MantidAPI/MatrixWorkspace.h"

#include <filesystem>

using namespace Mantid::API;

namespace MantidQt::CustomInterfaces {
IETModel::IETModel() {}

std::vector<std::string> IETModel::validateRunData(IETRunData const &runData, std::size_t const &defaultSpectraMin,
                                                   std::size_t const &defaultSpectraMax) {
  std::vector<std::string> errors;
  IETDataValidator validator;

  auto inputFiles = runData.getInputData().getInputFiles();
  bool isRunFileValid = !inputFiles.empty();
  std::string firstFileName = inputFiles.substr(0, inputFiles.find(','));

  std::string detectorError =
      validator.validateDetectorGrouping(runData.getGroupingData(), defaultSpectraMin, defaultSpectraMax);
  if (!detectorError.empty()) {
    errors.push_back(detectorError);
  }

  std::string analysisError = validator.validateAnalysisData(runData.getAnalysisData());
  if (!analysisError.empty()) {
    errors.push_back(analysisError);
  }

  std::string conversionError = validator.validateConversionData(runData.getConversionData());
  if (!conversionError.empty()) {
    errors.push_back(conversionError);
  }

  std::vector<std::string> backgroundErrors = validator.validateBackgroundData(
      runData.getBackgroundData(), runData.getConversionData(), firstFileName, isRunFileValid);
  errors.insert(errors.end(), backgroundErrors.begin(), backgroundErrors.end());

  errors.erase(std::remove(errors.begin(), errors.end(), ""), errors.end());

  return errors;
}

void IETModel::setInstrumentProperties(IAlgorithm_sptr const &reductionAlg, InstrumentData const &instData) {
  reductionAlg->setProperty("Instrument", instData.getInstrument());
  reductionAlg->setProperty("Analyser", instData.getAnalyser());
  reductionAlg->setProperty("Reflection", instData.getReflection());
}

void IETModel::setInputProperties(IAlgorithm_sptr const &reductionAlg, IETInputData const &inputData) {
  reductionAlg->setProperty("InputFiles", inputData.getInputFiles());
  reductionAlg->setProperty("SumFiles", inputData.getSumFiles());
  reductionAlg->setProperty("LoadLogFiles", inputData.getLoadLogFiles());
  if (inputData.getUseCalibration()) {
    reductionAlg->setProperty("CalibrationWorkspace", inputData.getCalibrationWorkspace());
  }
}

void IETModel::setConversionProperties(IAlgorithm_sptr const &reductionAlg, IETConversionData const &conversionData,
                                       std::string const &instrument) {
  std::vector<long> detectorRange;

  if (instrument == "IRIS" || instrument == "OSIRIS") {
    reductionAlg->setProperty("Efixed", conversionData.getEfixed());
  }
  detectorRange.emplace_back(conversionData.getSpectraMin());
  detectorRange.emplace_back(conversionData.getSpectraMax());
  reductionAlg->setProperty("SpectraRange", detectorRange);
}

void IETModel::setBackgroundProperties(IAlgorithm_sptr const &reductionAlg, IETBackgroundData const &backgroundData) {
  if (backgroundData.getRemoveBackground()) {
    std::vector<double> backgroundRange;
    backgroundRange.emplace_back(backgroundData.getBackgroundStart());
    backgroundRange.emplace_back(backgroundData.getBackgroundEnd());
    reductionAlg->setProperty("BackgroundRange", backgroundRange);
  }
}

void IETModel::setRebinProperties(IAlgorithm_sptr const &reductionAlg, IETRebinData const &rebinData) {
  if (rebinData.getShouldRebin()) {
    std::string rebin;

    if (rebinData.getRebinType() == IETRebinType::SINGLE) {
      rebin = std::to_string(rebinData.getRebinLow()) + "," + std::to_string(rebinData.getRebinWidth()) + "," +
              std::to_string(rebinData.getRebinHigh());
    } else {
      rebin = rebinData.getRebinString();
    }

    reductionAlg->setProperty("RebinString", rebin);
  }
}

void IETModel::setAnalysisProperties(IAlgorithm_sptr const &reductionAlg, IETAnalysisData const &analysisData) {
  if (analysisData.getUseDetailedBalance()) {
    reductionAlg->setProperty("DetailedBalance", analysisData.getDetailedBalance());
  }
  if (analysisData.getUseScaleFactor()) {
    reductionAlg->setProperty("ScaleFactor", analysisData.getScaleFactor());
  }
}

void IETModel::setGroupingProperties(IAlgorithm_sptr const &reductionAlg, IETGroupingData const &groupingData,
                                     IETConversionData const &conversionData) {
  std::pair<std::string, std::string> grouping = createGrouping(groupingData, conversionData);

  reductionAlg->setProperty("GroupingMethod", grouping.first);

  if (grouping.first == "File")
    reductionAlg->setProperty("MapFile", grouping.second);
  else if (grouping.first == "Custom")
    reductionAlg->setProperty("GroupingString", grouping.second);
}

void IETModel::setOutputProperties(IAlgorithm_sptr const &reductionAlg, IETOutputData const &outputData,
                                   std::string const &outputGroupName) {
  if (outputData.getUseDeltaEInWavenumber()) {
    reductionAlg->setProperty("UnitX", "DeltaE_inWavenumber");
  }
  reductionAlg->setProperty("FoldMultipleFrames", outputData.getFoldMultipleFrames());

  reductionAlg->setProperty("OutputWorkspace", outputGroupName);
}

std::string IETModel::getOuputGroupName(InstrumentData const &instData, std::string const &inputText) {
  std::string instrument = instData.getInstrument();
  std::string analyser = instData.getAnalyser();
  std::string reflection = instData.getReflection();

  return instrument + inputText + "_" + analyser + "_" + reflection + "_Reduced";
}

std::string IETModel::runIETAlgorithm(MantidQt::API::BatchAlgorithmRunner *batchAlgoRunner, InstrumentData instData,
                                      IETRunData runData) {
  auto reductionAlg = AlgorithmManager::Instance().create("ISISIndirectEnergyTransferWrapper");
  reductionAlg->initialize();

  setInstrumentProperties(reductionAlg, instData);
  setInputProperties(reductionAlg, runData.getInputData());
  setConversionProperties(reductionAlg, runData.getConversionData(), instData.getInstrument());
  setBackgroundProperties(reductionAlg, runData.getBackgroundData());
  setRebinProperties(reductionAlg, runData.getRebinData());
  setAnalysisProperties(reductionAlg, runData.getAnalysisData());
  setGroupingProperties(reductionAlg, runData.getGroupingData(), runData.getConversionData());

  std::string outputGroupName = getOuputGroupName(instData, runData.getInputData().getInputText());
  setOutputProperties(reductionAlg, runData.getOutputData(), outputGroupName);

  batchAlgoRunner->addAlgorithm(reductionAlg);
  batchAlgoRunner->executeBatchAsync();

  return outputGroupName;
}

std::pair<std::string, std::string> IETModel::createGrouping(const IETGroupingData &groupingData,
                                                             const IETConversionData &conversionData) {
  auto groupType = groupingData.getGroupingType();

  if (groupType == IETGroupingType::FILE) {
    return std::make_pair(IETGroupingType::FILE, groupingData.getGroupingMapFile());
  } else if (groupType == IETGroupingType::GROUPS) {
    std::string groupingString = getDetectorGroupingString(conversionData.getSpectraMin(),
                                                           conversionData.getSpectraMax(), groupingData.getNGroups());
    return std::make_pair(IETGroupingType::CUSTOM, groupingString);
  } else if (groupType == IETGroupingType::DEFAULT) {
    return std::make_pair(IETGroupingType::IPF, "");
  } else if (groupType == IETGroupingType::CUSTOM)
    return std::make_pair(IETGroupingType::CUSTOM, groupingData.getCustomGroups());
  else {
    // Catch All and Individual
    return std::make_pair(groupType, "");
  }
}

std::string IETModel::getDetectorGroupingString(int spectraMin, int spectraMax, int nGroups) {
  const unsigned int nSpectra = 1 + spectraMax - spectraMin;
  return createDetectorGroupingString(static_cast<std::size_t>(nSpectra), static_cast<std::size_t>(nGroups),
                                      static_cast<std::size_t>(spectraMin));
}

std::vector<std::string> IETModel::validatePlotData(IETPlotData plotParams) {
  std::vector<std::string> errors;

  const std::string inputFiles = plotParams.getInputData().getInputFiles();
  if (inputFiles.empty()) {
    errors.push_back("You must select a run file.");
  }

  IETDataValidator validator;

  bool isRunFileValid = !inputFiles.empty();
  std::string firstFileName = inputFiles.substr(0, inputFiles.find(','));

  std::string conversionError = validator.validateConversionData(plotParams.getConversionData());
  if (!conversionError.empty()) {
    errors.push_back(conversionError);
  }

  std::vector<std::string> backgroundErrors = validator.validateBackgroundData(
      plotParams.getBackgroundData(), plotParams.getConversionData(), firstFileName, isRunFileValid);
  errors.insert(errors.end(), backgroundErrors.begin(), backgroundErrors.end());

  errors.erase(std::remove(errors.begin(), errors.end(), ""), errors.end());

  return errors;
}

void IETModel::plotRawFile(MantidQt::API::BatchAlgorithmRunner *batchAlgoRunner, InstrumentData instData,
                           IETPlotData plotParams) {
  using Mantid::specnum_t;

  const std::string inputFiles = plotParams.getInputData().getInputFiles();

  int spectraMin = plotParams.getConversionData().getSpectraMin();
  int spectraMax = plotParams.getConversionData().getSpectraMax();

  std::string rawFile = inputFiles.substr(0, inputFiles.find(',')); // getting the name of the first file
  std::filesystem::path rawFileInfo(rawFile);
  std::string name = rawFileInfo.filename().string();

  auto loadAlg = loadAlgorithm(rawFile, name);
  if (instData.getInstrument() != "TOSCA") {
    if (loadAlg->existsProperty("LoadLogFiles")) {
      loadAlg->setProperty("LoadLogFiles", false);
    }
    loadAlg->setPropertyValue("SpectrumMin", std::to_string(spectraMin));
    loadAlg->setPropertyValue("SpectrumMax", std::to_string(spectraMax));
  }
  loadAlg->execute();

  auto inputFromRebin = std::make_unique<Mantid::API::AlgorithmRuntimeProps>();
  inputFromRebin->setPropertyValue("InputWorkspace", name);

  std::vector<specnum_t> detectorList;
  for (specnum_t i = spectraMin; i <= spectraMax; i++)
    detectorList.emplace_back(i);

  auto backgroundData = plotParams.getBackgroundData();
  if (backgroundData.getRemoveBackground()) {
    std::vector<double> range;
    range.emplace_back(backgroundData.getBackgroundStart());
    range.emplace_back(backgroundData.getBackgroundEnd());

    IAlgorithm_sptr calcBackAlg = AlgorithmManager::Instance().create("CalculateFlatBackground");
    calcBackAlg->initialize();
    calcBackAlg->setProperty("OutputWorkspace", name + "_bg");
    calcBackAlg->setProperty("Mode", "Mean");
    calcBackAlg->setProperty("StartX", range[0]);
    calcBackAlg->setProperty("EndX", range[1]);
    batchAlgoRunner->addAlgorithm(calcBackAlg, std::make_unique<Mantid::API::AlgorithmRuntimeProps>(*inputFromRebin));

    auto inputFromCalcBG = std::make_unique<Mantid::API::AlgorithmRuntimeProps>();
    inputFromCalcBG->setPropertyValue("InputWorkspace", name + "_bg");

    IAlgorithm_sptr groupAlg = AlgorithmManager::Instance().create("GroupDetectors");
    groupAlg->initialize();
    groupAlg->setProperty("OutputWorkspace", name + "_grp");
    groupAlg->setProperty("DetectorList", detectorList);
    batchAlgoRunner->addAlgorithm(groupAlg, std::move(inputFromCalcBG));

    IAlgorithm_sptr rawGroupAlg = AlgorithmManager::Instance().create("GroupDetectors");
    rawGroupAlg->initialize();
    rawGroupAlg->setProperty("OutputWorkspace", name + "_grp_raw");
    rawGroupAlg->setProperty("DetectorList", detectorList);
    batchAlgoRunner->addAlgorithm(rawGroupAlg, std::move(inputFromRebin));
  } else {
    IAlgorithm_sptr rawGroupAlg = AlgorithmManager::Instance().create("GroupDetectors");
    rawGroupAlg->initialize();
    rawGroupAlg->setProperty("OutputWorkspace", name + "_grp");
    rawGroupAlg->setProperty("DetectorList", detectorList);
    batchAlgoRunner->addAlgorithm(rawGroupAlg, std::move(inputFromRebin));
  }

  batchAlgoRunner->executeBatchAsync();
}

void IETModel::saveWorkspace(std::string const &workspaceName, IETSaveData saveTypes) {
  if (saveTypes.getNexus())
    save("SaveNexusProcessed", workspaceName, workspaceName + ".nxs");
  if (saveTypes.getSPE())
    save("SaveSPE", workspaceName, workspaceName + ".spe");
  if (saveTypes.getNXSPE())
    save("SaveNXSPE", workspaceName, workspaceName + ".nxspe");
  if (saveTypes.getASCII())
    save("SaveAscii", workspaceName, workspaceName + ".dat", 2);
  if (saveTypes.getAclimax())
    saveAclimax(workspaceName, workspaceName + "_aclimax.dat");
  if (saveTypes.getDaveGrp())
    saveDaveGroup(workspaceName, workspaceName + ".grp");
}

void IETModel::save(std::string const &algorithmName, std::string const &workspaceName, std::string const &outputName,
                    int const version, std::string const &separator) {
  auto saver = AlgorithmManager::Instance().create(algorithmName, version);
  saver->initialize();
  saver->setProperty("InputWorkspace", workspaceName);
  saver->setProperty("Filename", outputName);
  if (!separator.empty())
    saver->setProperty("Separator", separator);
  saver->execute();
}

void IETModel::saveAclimax(std::string const &workspaceName, std::string const &outputName, std::string const &xUnits) {
  auto const bins = xUnits == "DeltaE_inWavenumber" ? "24, -0.005, 4000" : "3, -0.005, 500"; // cm-1 or meV
  auto const temporaryName = workspaceName + "_aclimax_save_temp";

  auto rebin = AlgorithmManager::Instance().create("Rebin");
  rebin->initialize();
  rebin->setProperty("InputWorkspace", workspaceName);
  rebin->setProperty("OutputWorkspace", temporaryName);
  rebin->setProperty("Params", bins);
  rebin->execute();

  save("SaveAscii", temporaryName, outputName, -1, "Tab");
  deleteWorkspace(temporaryName);
}

void IETModel::saveDaveGroup(std::string const &workspaceName, std::string const &outputName) {
  auto const temporaryName = workspaceName + "_davegrp_save_temp";

  auto converter = AlgorithmManager::Instance().create("ConvertSpectrumAxis");
  converter->initialize();
  converter->setProperty("InputWorkspace", workspaceName);
  converter->setProperty("OutputWorkspace", temporaryName);
  converter->setProperty("Target", "ElasticQ");
  converter->setProperty("EMode", "Indirect");
  converter->execute();

  save("SaveDaveGrp", temporaryName, outputName);
  deleteWorkspace(temporaryName);
}

void IETModel::createGroupingWorkspace(std::string const &instrumentName, std::string const &analyser,
                                       std::string const &customGrouping, std::string const &outputName) {
  auto creator = AlgorithmManager::Instance().create("CreateGroupingWorkspace");
  creator->initialize();
  creator->setProperty("InstrumentName", instrumentName);
  creator->setProperty("ComponentName", analyser);
  creator->setProperty("CustomGroupingString", customGrouping);
  creator->setProperty("OutputWorkspace", outputName);

  creator->execute();
}

double IETModel::loadDetailedBalance(std::string const &filename) {
  std::vector<std::string> const logNames{"sample", "sample_top", "sample_bottom"};
  auto const detailedBalance = loadSampleLog(filename, logNames, 300.0);
  return detailedBalance;
}

std::vector<std::string> IETModel::groupWorkspaces(std::string const &groupName, std::string const &instrument,
                                                   std::string const &groupOption, bool const shouldGroup) {
  std::vector<std::string> outputWorkspaces;

  if (doesExistInADS(groupName)) {
    if (auto const outputGroup = getADSWorkspaceGroup(groupName)) {
      outputWorkspaces = outputGroup->getNames();

      if (instrument == "OSIRIS") {
        if (!shouldGroup) {
          ungroupWorkspace(outputGroup->getName());
        }
      } else {
        if (groupOption == IETGroupOption::UNGROUPED) {
          ungroupWorkspace(outputGroup->getName());
        } else if (groupOption == IETGroupOption::SAMPLECHANGERGROUPED) {
          groupWorkspaceBySampleChanger(outputGroup->getName());
          // If we are grouping by sample we want to ungroup the reduced group leaving only the sample grouped
          ungroupWorkspace(outputGroup->getName());
        }
      }
    }
  }

  return outputWorkspaces;
}

void IETModel::ungroupWorkspace(std::string const &workspaceName) {
  auto ungroup = AlgorithmManager::Instance().create("UnGroupWorkspace");
  ungroup->initialize();
  ungroup->setProperty("InputWorkspace", workspaceName);
  ungroup->execute();
}

void IETModel::groupWorkspaceBySampleChanger(std::string const &workspaceName) {
  auto group = AlgorithmManager::Instance().create("GroupBySampleChangerPosition");
  group->initialize();
  group->setProperty("InputWorkspace", workspaceName);
  std::string prefix = workspaceName;
  prefix.erase(workspaceName.find("_Reduced"), 8);
  group->setProperty("OutputGroupPrefix", prefix);
  group->setProperty("OutputGroupSuffix", "Reduced");
  group->execute();
}

} // namespace MantidQt::CustomInterfaces