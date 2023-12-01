// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "DllConfig.h"
#include "MantidAPI/CompositeFunction.h"
#include "MantidAPI/IFunction.h"
#include "ParameterEstimation.h"

namespace MantidQt {
namespace CustomInterfaces {
namespace IDA {
class MANTIDQT_INELASTIC_DLL IDAFunctionParameterEstimation {

public:
  using ParameterEstimateSetter =
      std::function<void(Mantid::API::IFunction_sptr &function, const DataForParameterEstimation &estimationData)>;
  using ParameterEstimator =
      std::function<std::unordered_map<std::string, double>(Mantid::MantidVec const &, Mantid::MantidVec const &)>;

  IDAFunctionParameterEstimation(std::unordered_map<std::string, ParameterEstimator> estimators);
  void addParameterEstimationFunction(std::string const &functionName, ParameterEstimateSetter function);
  void estimateFunctionParameters(Mantid::API::IFunction_sptr &function,
                                  const DataForParameterEstimation &estimationData);

private:
  void estimateSingleFunctionParameters(Mantid::API::CompositeFunction_sptr const &composite,
                                        Mantid::API::IFunction_sptr &function,
                                        const DataForParameterEstimation &estimationData,
                                        std::size_t const functionIndex = 0u);

  std::map<std::string, ParameterEstimateSetter> m_funcMap;
};

} // namespace IDA
} // namespace CustomInterfaces
} // namespace MantidQt
