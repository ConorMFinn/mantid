// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once
#include "Common/ValidationResult.h"
#include "LookupTableValidationError.h"
#include "Reduction/LookupRow.h"
#include <array>

namespace MantidQt {
namespace CustomInterfaces {
namespace ISISReflectometry {

class MANTIDQT_ISISREFLECTOMETRY_DLL LookupTableValidator {
public:
  using ContentType = std::vector<LookupRow::ValueArray>;
  using LookupTableRows = std::vector<LookupRow>;
  using ResultType = ValidationResult<LookupTableRows, LookupTableValidationError>;

  ResultType operator()(ContentType const &lookupTableContent, double thetaTolerance) const;

  ValidationResult<boost::blank, LookupCriteriaError> validateSearchCriteria(LookupTableRows lookupTable,
                                                                             double tolerance) const;

  void validateAllLookupRows(ContentType const &lookupTableContent, LookupTableRows &lookupTable,
                             std::vector<InvalidLookupRowCells> &validationErrors) const;

  int countWildcards(LookupTableRows const &lookupTable) const;

  void sortInPlaceByThetaThenTitleMatcher(LookupTableRows &lookupTable) const;

  bool hasUniqueSearchCriteria(LookupTableRows lookupTable, double tolerance) const;
  void appendSearchCriteriaErrorForAllRows(std::vector<InvalidLookupRowCells> &validationErrors,
                                           std::size_t rowCount) const;
};
} // namespace ISISReflectometry
} // namespace CustomInterfaces
} // namespace MantidQt
