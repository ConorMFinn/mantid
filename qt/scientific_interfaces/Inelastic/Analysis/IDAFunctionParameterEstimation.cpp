// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#include "IDAFunctionParameterEstimation.h"
#include "MantidAPI/IFunction.h"

#include <math.h>

namespace MantidQt::CustomInterfaces::IDA {

IDAFunctionParameterEstimation::ParameterEstimateSetter
parameterEstimateSetter(IDAFunctionParameterEstimation::ParameterEstimator estimator) {
  return [estimator](Mantid::API::IFunction_sptr &function, const DataForParameterEstimation &estimationData) {
    auto const y = estimationData.y;
    auto const x = estimationData.x;
    if (x.size() != 2 || y.size() != 2) {
      return;
    }
    auto const parameterValues = estimator(x, y);

    for (auto it = parameterValues.cbegin(); it != parameterValues.cend(); ++it) {
      function->setParameter(it->first, it->second);
    }
  };
}

IDAFunctionParameterEstimation::IDAFunctionParameterEstimation(
    std::unordered_map<std::string, ParameterEstimator> estimators) {
  for (auto it = estimators.cbegin(); it != estimators.cend(); ++it) {
    addParameterEstimationFunction(it->first, parameterEstimateSetter(it->second));
  }
}

// Add function name and estimation function to the stored function map.
void IDAFunctionParameterEstimation::addParameterEstimationFunction(std::string const &functionName,
                                                                    ParameterEstimateSetter function) {
  m_funcMap.insert(std::make_pair(functionName, std::move(function)));
}
// Estimate the function parameters for the input function
// If the input function exists in the stored map it will update the function
// parameters in-place.
void IDAFunctionParameterEstimation::estimateFunctionParameters(Mantid::API::IFunction_sptr &function,
                                                                const DataForParameterEstimation &estimationData) {
  if (!function) {
    return;
  }

  if (auto composite = std::dynamic_pointer_cast<Mantid::API::CompositeFunction>(function)) {
    for (auto i = 0u; i < composite->nFunctions(); ++i) {
      estimateSingleFunctionParameters(composite->getFunction(i), estimationData);
    }
  } else {
    estimateSingleFunctionParameters(function, estimationData);
  }
}

void IDAFunctionParameterEstimation::estimateSingleFunctionParameters(
    Mantid::API::IFunction_sptr &function, const DataForParameterEstimation &estimationData) {
  if (function) {
    std::string functionName = function->name();
    if (m_funcMap.find(functionName) != m_funcMap.end()) {
      m_funcMap[functionName](function, estimationData);
    }
  }
}

} // namespace MantidQt::CustomInterfaces::IDA
