// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2014 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "DllConfig.h"
#include "MantidQtWidgets/Plotting/PreviewPlot.h"

#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QSplitter>
#include <QString>
#include <QWidget>
#include <string>

namespace MantidQt {
namespace CustomInterfaces {

class IALFAnalysisPresenter;

class MANTIDQT_DIRECT_DLL IALFAnalysisView {
public:
  virtual ~IALFAnalysisView() = default;
  virtual QWidget *getView() = 0;
  virtual void subscribePresenter(IALFAnalysisPresenter *presenter) = 0;
  virtual std::pair<double, double> getRange() = 0;
  virtual void addSpectrum(const std::string &wsName) = 0;
  virtual void addFitSpectrum(const std::string &wsName) = 0;
  virtual void displayWarning(const std::string &message) = 0;
  virtual void setupPlotFitSplitter(const double &start, const double &end) = 0;
  virtual QWidget *createFitPane(const double &start, const double &end) = 0;
  virtual void setPeakCentre(const double centre) = 0;
  virtual double peakCentre() const = 0;
  virtual void setPeakCentreStatus(const std::string &status) = 0;
};

class MANTIDQT_DIRECT_DLL ALFAnalysisView final : public QWidget, public IALFAnalysisView {
  Q_OBJECT

public:
  explicit ALFAnalysisView(const double &start, const double &end, QWidget *parent = nullptr);

  QWidget *getView() override;

  void subscribePresenter(IALFAnalysisPresenter *presenter) override;

  std::pair<double, double> getRange() override;
  void addSpectrum(const std::string &wsName) override;
  void addFitSpectrum(const std::string &wsName) override;
  void displayWarning(const std::string &message) override;

  void setPeakCentre(const double centre) override;
  double peakCentre() const override;

  void setPeakCentreStatus(const std::string &status) override;
public slots:
  void notifyPeakCentreEditingFinished();
  void notifyUpdateEstimateClicked();
  void notifyFitClicked();

protected:
  void setupPlotFitSplitter(const double &start, const double &end) override;
  QWidget *createFitPane(const double &start, const double &end) override;

private:
  QWidget *setupFitRangeWidget(const double start, const double end);
  QWidget *setupFitButtonsWidget();
  QWidget *setupPeakCentreWidget(const double centre);

  MantidWidgets::PreviewPlot *m_plot;
  QLineEdit *m_start, *m_end;
  QSplitter *m_fitPlotLayout;
  QPushButton *m_fitButton;
  QPushButton *m_updateEstimateButton;
  QLineEdit *m_peakCentre;
  QLabel *m_fitStatus;

  IALFAnalysisPresenter *m_presenter;
};
} // namespace CustomInterfaces
} // namespace MantidQt
