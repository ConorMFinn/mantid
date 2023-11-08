// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2019 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#include "IndirectPlotOptionsPresenter.h"

#include "MantidAPI/MatrixWorkspace.h"

using namespace Mantid::API;

namespace {

std::string OR(std::string const &lhs, std::string const &rhs) { return "(" + lhs + "|" + rhs + ")"; }

std::string NATURAL_NUMBER(std::size_t const &digits) {
  return OR("0", "[1-9][0-9]{," + std::to_string(digits - 1) + "}");
}

namespace Regexes {

std::string const SPACE = "[ ]*";
std::string const COMMA = SPACE + "," + SPACE;
std::string const MINUS = "\\-";

std::string const NUMBER = NATURAL_NUMBER(4);
std::string const NATURAL_RANGE = "(" + NUMBER + MINUS + NUMBER + ")";
std::string const NATURAL_OR_RANGE = OR(NATURAL_RANGE, NUMBER);
std::string const WORKSPACE_INDICES = "(" + NATURAL_OR_RANGE + "(" + COMMA + NATURAL_OR_RANGE + ")*)";

} // namespace Regexes
} // namespace

namespace MantidQt::CustomInterfaces {

IndirectPlotOptionsPresenter::IndirectPlotOptionsPresenter(
    IndirectPlotOptionsView *view, PlotWidget const &plotType, std::string const &fixedIndices,
    boost::optional<std::map<std::string, std::string>> const &availableActions)
    : QObject(nullptr), m_wsRemovedObserver(*this, &IndirectPlotOptionsPresenter::onWorkspaceRemoved),
      m_wsReplacedObserver(*this, &IndirectPlotOptionsPresenter::onWorkspaceReplaced), m_view(view),
      m_model(std::make_unique<IndirectPlotOptionsModel>(availableActions)) {
  setupPresenter(plotType, fixedIndices);
}

/// Used by the unit tests so that m_plotter can be mocked
IndirectPlotOptionsPresenter::IndirectPlotOptionsPresenter(IndirectPlotOptionsView *view,
                                                           IndirectPlotOptionsModel *model, PlotWidget const &plotType,
                                                           std::string const &fixedIndices)
    : QObject(nullptr), m_wsRemovedObserver(*this, &IndirectPlotOptionsPresenter::onWorkspaceRemoved),
      m_wsReplacedObserver(*this, &IndirectPlotOptionsPresenter::onWorkspaceReplaced), m_view(view), m_model(model) {
  setupPresenter(plotType, fixedIndices);
  m_view->subscribePresenter(this);
}

IndirectPlotOptionsPresenter::~IndirectPlotOptionsPresenter() { watchADS(false); }

void IndirectPlotOptionsPresenter::setupPresenter(PlotWidget const &plotType, std::string const &fixedIndices) {
  watchADS(true);

  connect(m_view, SIGNAL(selectedUnitChanged(std::string const &)), this, SLOT(unitChanged(std::string const &)));
  connect(m_view, SIGNAL(selectedIndicesChanged(std::string const &)), this, SLOT(indicesChanged(std::string const &)));

  connect(m_view, SIGNAL(plotSpectraClicked()), this, SLOT(plotSpectra()));
  connect(m_view, SIGNAL(plotBinsClicked()), this, SLOT(plotBins()));
  connect(m_view, SIGNAL(plotContourClicked()), this, SLOT(plotContour()));
  connect(m_view, SIGNAL(plotTiledClicked()), this, SLOT(plotTiled()));

  m_view->setIndicesRegex(QString::fromStdString(Regexes::WORKSPACE_INDICES));
  m_view->setPlotType(plotType, m_model->availableActions());
  m_view->setIndices(QString::fromStdString(fixedIndices));
  m_model->setFixedIndices(fixedIndices);

  m_plotType = plotType;
  setOptionsEnabled(false);
}

void IndirectPlotOptionsPresenter::watchADS(bool on) {
  auto &notificationCenter = AnalysisDataService::Instance().notificationCenter;
  if (on) {
    notificationCenter.addObserver(m_wsRemovedObserver);
    notificationCenter.addObserver(m_wsReplacedObserver);
  } else {
    notificationCenter.removeObserver(m_wsReplacedObserver);
    notificationCenter.removeObserver(m_wsRemovedObserver);
  }
}

void IndirectPlotOptionsPresenter::setPlotType(PlotWidget const &plotType) {
  m_plotType = plotType;
  m_view->setPlotType(plotType, m_model->availableActions());
}

void IndirectPlotOptionsPresenter::setPlotting(bool plotting) {
  m_view->setPlotButtonText(plotting ? "Plotting..."
                                     : QString::fromStdString(m_model->availableActions()["Plot Spectra"]));
  setOptionsEnabled(!plotting);
}

void IndirectPlotOptionsPresenter::setOptionsEnabled(bool enable) {
  m_view->setWorkspaceComboBoxEnabled(m_view->numberOfWorkspaces() > 1 ? enable : false);
  m_view->setIndicesLineEditEnabled(!m_model->indicesFixed() ? enable : false);
  m_view->setPlotButtonEnabled(enable);
  m_view->setUnitComboBoxEnabled(enable);
}

void IndirectPlotOptionsPresenter::onWorkspaceRemoved(WorkspacePreDeleteNotification_ptr nf) {
  // Ignore non matrix workspaces
  if (auto const removedWorkspace = std::dynamic_pointer_cast<MatrixWorkspace>(nf->object())) {
    auto const removedName = removedWorkspace->getName();
    if (removedName == m_view->selectedWorkspace().toStdString())
      m_model->removeWorkspace();
    m_view->removeWorkspace(QString::fromStdString(removedName));
  }
}

void IndirectPlotOptionsPresenter::onWorkspaceReplaced(WorkspaceBeforeReplaceNotification_ptr nf) {
  // Ignore non matrix workspaces
  if (auto const newWorkspace = std::dynamic_pointer_cast<MatrixWorkspace>(nf->newObject())) {
    auto const newName = newWorkspace->getName();
    if (newName == m_view->selectedWorkspace().toStdString())
      notifyWorkspaceChanged(newName);
  }
}

void IndirectPlotOptionsPresenter::setWorkspaces(std::vector<std::string> const &workspaces) {
  auto const workspaceNames = m_model->getAllWorkspaceNames(workspaces);
  m_view->setWorkspaces(workspaceNames);
  notifyWorkspaceChanged(workspaceNames.front());
}

void IndirectPlotOptionsPresenter::setWorkspace(std::string const &plotWorkspace) {
  bool success = m_model->setWorkspace(plotWorkspace);
  setOptionsEnabled(success);
  if (success && !m_model->indicesFixed())
    setIndices();
}

void IndirectPlotOptionsPresenter::clearWorkspaces() {
  m_model->removeWorkspace();
  m_view->clearWorkspaces();
  setOptionsEnabled(false);
}

void IndirectPlotOptionsPresenter::setUnit(std::string const &unit) {
  if (m_plotType == PlotWidget::SpectraUnit || m_plotType == PlotWidget::SpectraContourUnit) {
    m_model->setUnit(unit);
  }
}

void IndirectPlotOptionsPresenter::setIndices() {
  auto const selectedIndices = m_view->selectedIndices().toStdString();
  if (auto const indices = m_model->indices())
    indicesChanged(indices.get());
  else if (!selectedIndices.empty())
    indicesChanged(selectedIndices);
  else
    indicesChanged("0");
}

void IndirectPlotOptionsPresenter::notifyWorkspaceChanged(std::string const &workspaceName) {
  setWorkspace(workspaceName);
}

void IndirectPlotOptionsPresenter::unitChanged(std::string const &unit) { setUnit(unit); }

void IndirectPlotOptionsPresenter::indicesChanged(std::string const &indices) {
  auto const formattedIndices = m_model->formatIndices(indices);
  m_view->setIndices(QString::fromStdString(formattedIndices));
  m_view->setIndicesErrorLabelVisible(!m_model->setIndices(formattedIndices));

  if (!formattedIndices.empty())
    m_view->addIndicesSuggestion(QString::fromStdString(formattedIndices));
}

void IndirectPlotOptionsPresenter::plotSpectra() {
  if (validateWorkspaceSize(MantidAxis::Spectrum)) {
    setPlotting(true);
    m_model->plotSpectra();
    setPlotting(false);
  }
}

void IndirectPlotOptionsPresenter::plotBins() {
  if (validateWorkspaceSize(MantidAxis::Bin)) {
    auto const indicesString = m_view->selectedIndices().toStdString();
    if (m_model->validateIndices(indicesString, MantidAxis::Bin)) {
      setPlotting(true);
      m_model->plotBins(indicesString);
      setPlotting(false);
    } else {
      m_view->displayWarning("Plot Bins failed: Invalid bin indices provided.");
    }
  }
}

void IndirectPlotOptionsPresenter::plotContour() {
  setPlotting(true);
  m_model->plotContour();
  setPlotting(false);
}

void IndirectPlotOptionsPresenter::plotTiled() {
  if (validateWorkspaceSize(MantidAxis::Spectrum)) {
    setPlotting(true);
    m_model->plotTiled();
    setPlotting(false);
  }
}

bool IndirectPlotOptionsPresenter::validateWorkspaceSize(MantidAxis const &axisType) {
  auto const errorMessage = m_model->singleDataPoint(axisType);
  if (errorMessage) {
    m_view->displayWarning(QString::fromStdString(errorMessage.get()));
    return false;
  }
  return true;
}

} // namespace MantidQt::CustomInterfaces
