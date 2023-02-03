// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "MantidKernel/DateAndTime.h"

namespace Mantid {
namespace Kernel {

/**
 * Class holding a start/end time and a destination for splitting
 * event lists and logs.
 *
 * The start/stop times are saved internally as DateAndTime, for
 * fastest event list splitting.
 *
 * Author: Janik Zikovsky, SNS
 */
class MANTID_KERNEL_DLL SplittingInterval : public TimeInterval {
public:
  /// Default constructor
  SplittingInterval();

  SplittingInterval(const Types::Core::DateAndTime &start, const Types::Core::DateAndTime &stop, const int index = 0);

  Types::Core::DateAndTime start() const;
  Types::Core::DateAndTime stop() const;

  double duration() const;

  int index() const;

  bool overlaps(const SplittingInterval &b) const;
  /// @cond
  SplittingInterval operator&(const SplittingInterval &b) const;
  /// @endcond
  SplittingInterval operator|(const SplittingInterval &b) const;

private:
  /// Index of the destination
  int m_index;
};

/**
 * A typedef for splitting events according their pulse time.
 * It is a vector of SplittingInterval classes.
 *
 */
using SplittingIntervalVec = std::vector<SplittingInterval>;

// -------------- Operators ---------------------
MANTID_KERNEL_DLL SplittingIntervalVec operator+(const SplittingIntervalVec &a, const SplittingIntervalVec &b);
MANTID_KERNEL_DLL SplittingIntervalVec operator&(const SplittingIntervalVec &a, const SplittingIntervalVec &b);
MANTID_KERNEL_DLL SplittingIntervalVec operator|(const SplittingIntervalVec &a, const SplittingIntervalVec &b);
MANTID_KERNEL_DLL SplittingIntervalVec operator~(const SplittingIntervalVec &a);

} // Namespace Kernel
} // Namespace Mantid
