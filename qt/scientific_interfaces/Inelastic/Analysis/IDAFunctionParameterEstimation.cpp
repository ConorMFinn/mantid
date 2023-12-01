// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#include "IDAFunctionParameterEstimation.h"
#include "MantidAPI/IFunction.h"

#include <math.h>

namespace {

std::string nameForParameterEstimator(Mantid::API::CompositeFunction_sptr const &composite,
                                      Mantid::API::IFunction_sptr &function, std::size_t const functionIndex) {
  auto functionName = function->name();
  if (composite) {
    // functionIndex returns the index of the first function with the given name. If the index is different,
    // we know that this is not the first function with this name in the composite function
    if (composite->functionIndex(functionName) != functionIndex) {
      functionName += "N";
    }
  }
  return functionName;
}

} // namespace

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
      estimateSingleFunctionParameters(composite, composite->getFunction(i), estimationData, i);
    }
  } else {
    estimateSingleFunctionParameters(nullptr, function, estimationData);
  }
}

void IDAFunctionParameterEstimation::estimateSingleFunctionParameters(
    Mantid::API::CompositeFunction_sptr const &composite, Mantid::API::IFunction_sptr &function,
    const DataForParameterEstimation &estimationData, std::size_t const functionIndex) {
  if (function) {
    auto const parameterEstimatorName = nameForParameterEstimator(composite, function, functionIndex);
    if (m_funcMap.find(parameterEstimatorName) != m_funcMap.end()) {
      m_funcMap[parameterEstimatorName](function, estimationData);
    }
  }
}

} // namespace MantidQt::CustomInterfaces::IDA
