// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2014 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "MantidKernel/System.h"

#include "DllConfig.h"

#include "IALCPeakFittingModel.h"
#include "IALCPeakFittingModelSubscriber.h"
#include "IALCPeakFittingView.h"
#include "IALCPeakFittingViewSubscriber.h"

namespace MantidQt::CustomInterfaces {

/** ALCPeakFittingPresenter : Presenter for Peak Fitting step of ALC interface.
 */
class MANTIDQT_MUONINTERFACE_DLL ALCPeakFittingPresenter : public IALCPeakFittingModelSubscriber,
                                                           public IALCPeakFittingViewSubscriber {
  //  Q_OBJECT

public:
  ALCPeakFittingPresenter(IALCPeakFittingView *view, IALCPeakFittingModel *model);

  void initialize();

  // IALCPeakFittingModelSubscriber Overrides
  void dataChanged() const override;
  void fittedPeaksChanged() const override;
  void errorInModel(std::string const &message) const override;

  // IALCPeakFittingViewSubscriber Overrides
  void onFitRequested() override;
  void onCurrentFunctionChanged() override;
  void onPeakPickerChanged() override;
  void onParameterChanged(std::string const &functionIndex, std::string const &parameter) override;
  void onPlotGuessClicked() override;

private:
  //  /// Fit the data using the peaks from the view, and update them
  //  void fit();
  //
  //  /// Executed when user selects a function in a Function Browser
  //  void onCurrentFunctionChanged();
  //
  //  /// Executed when Peak Picker if moved/resized
  //  void onPeakPickerChanged();
  //
  //  /// Executed when user changes parameter in Function Browser
  //  void onParameterChanged(std::string const &funcIndex);

  //  void onFittedPeaksChanged();
  //  void onDataChanged();

  //  /// Executed when user clicks "Plot guess"
  //  void onPlotGuessClicked();

  /// Plot guess on graph
  bool plotGuessOnGraph();

  /// Remove plot from graph
  void removePlot(std::string const &plotName);

  /// Associated view
  IALCPeakFittingView *const m_view;

  /// Associated model
  IALCPeakFittingModel *const m_model;

  /// Whether guess is currently plotted
  bool m_guessPlotted;
};

} // namespace MantidQt::CustomInterfaces
