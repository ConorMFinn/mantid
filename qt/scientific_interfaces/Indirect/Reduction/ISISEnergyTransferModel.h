// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2023 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "DllConfig.h"
#include "ISISEnergyTransferData.h"
#include "ISISEnergyTransferValidator.h"
#include "MantidAPI/Algorithm.h"
#include "MantidQtWidgets/Common/BatchAlgorithmRunner.h"
#include <typeinfo>

using namespace Mantid::API;

namespace MantidQt {
namespace CustomInterfaces {
class MANTIDQT_INDIRECT_DLL IETModel {
public:
  IETModel();
  ~IETModel() = default;

  void setInstrumentProperties(IAlgorithm_sptr const &reductionAlg, InstrumentData const &instData);
  void setInputProperties(IAlgorithm_sptr const &reductionAlg, IETInputData const &inputData);
  void setConversionProperties(IAlgorithm_sptr const &reductionAlg, IETConversionData const &conversionData,
                               std::string const &instrument);
  void setBackgroundProperties(IAlgorithm_sptr const &reductionAlg, IETBackgroundData const &backgroundData);
  void setRebinProperties(IAlgorithm_sptr const &reductionAlg, IETRebinData const &rebinData);
  void setAnalysisProperties(IAlgorithm_sptr const &reductionAlg, IETAnalysisData const &analysisData);
  void setGroupingProperties(IAlgorithm_sptr const &reductionAlg, IETGroupingData const &groupingData,
                             IETConversionData const &conversionData);
  void setOutputProperties(IAlgorithm_sptr const &reductionAlg, IETOutputData const &outputData,
                           std::string const &outputGroupName);
  std::string getOuputGroupName(InstrumentData const &instData, std::string const &inputFiles);

  std::vector<std::string> validateRunData(IETRunData const &runData, std::size_t const &defaultSpectraMin,
                                           std::size_t const &defaultSpectraMax);
  std::vector<std::string> validatePlotData(IETPlotData plotData);

  std::string runIETAlgorithm(MantidQt::API::BatchAlgorithmRunner *batchAlgoRunner, InstrumentData instData,
                              IETRunData runParams);
  void plotRawFile(MantidQt::API::BatchAlgorithmRunner *batchAlgoRunner, InstrumentData instData, IETPlotData plotData);

  void saveWorkspace(std::string const &workspaceName, IETSaveData saveData);
  void saveDaveGroup(std::string const &workspaceName, std::string const &outputName);
  void saveAclimax(std::string const &workspaceName, std::string const &outputName,
                   std::string const &xUnits = "DeltaE_inWavenumber");
  void save(std::string const &algorithmName, std::string const &workspaceName, std::string const &outputName,
            int const version = -1, std::string const &separator = "");

  std::pair<std::string, std::string> createGrouping(IETGroupingData const &groupingData,
                                                     IETConversionData const &conversionParams);

  std::string getDetectorGroupingString(int spectraMin, int spectraMax, int nGroups);
  void createGroupingWorkspace(std::string const &instrumentName, std::string const &analyser,
                               std::string const &customGrouping, std::string const &outputName);
  double loadDetailedBalance(std::string const &filename);

  std::vector<std::string> groupWorkspaces(std::string groupName, std::string intrument, std::string groupOption,
                                           bool shouldGroup);
  void ungroupWorkspace(std::string const &workspaceName);
  void groupWorkspaceBySampleChanger(std::string const &workspaceName);
};
} // namespace CustomInterfaces
} // namespace MantidQt