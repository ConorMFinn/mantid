// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
#include "MainWindowPresenter.h"
#include "../Common/Decoder.h"
#include "../Common/Encoder.h"
#include "GUI/Common/IMessageHandler.h"
#include "GUI/Runs/IRunsPresenter.h"
#include "IMainWindowView.h"
#include "MantidQtWidgets/Common/HelpWindow.h"
#include "MantidQtWidgets/Common/QtJSONUtils.h"
#include "Reduction/Batch.h"

#include <QFileDialog>

namespace MantidQt {
namespace CustomInterfaces {
namespace ISISReflectometry {

/** Constructor
 * @param view :: [input] The view we are managing
 * @param messageHandler :: Interface to a class that displays messages to
 * the user
 * @param batchPresenterFactory :: [input] A factory to create the batches
 * we will manage
 */
MainWindowPresenter::MainWindowPresenter(
    IMainWindowView *view, IMessageHandler *messageHandler,
    BatchPresenterFactory batchPresenterFactory)
    : m_view(view), m_messageHandler(messageHandler),
      m_batchPresenterFactory(std::move(batchPresenterFactory)) {
  view->subscribe(this);
  for (auto *batchView : m_view->batches())
    addNewBatch(batchView);
}

void MainWindowPresenter::notifyNewBatchRequested() {
  auto *newBatchView = m_view->newBatch();
  addNewBatch(newBatchView);
}

void MainWindowPresenter::notifyCloseBatchRequested(int batchIndex) {
  if (m_batchPresenters[batchIndex]->isAutoreducing() ||
      m_batchPresenters[batchIndex]->isProcessing()) {
    m_messageHandler->giveUserCritical(
        "Cannot close batch while processing or autoprocessing is in progress",
        "Error");
    return;
  }

  if (m_batchPresenters[batchIndex]->requestClose()) {
    m_batchPresenters.erase(m_batchPresenters.begin() + batchIndex);
    m_view->removeBatch(batchIndex);
  }
}

void MainWindowPresenter::notifyAutoreductionResumed() {
  for (auto batchPresenter : m_batchPresenters) {
    batchPresenter->anyBatchAutoreductionResumed();
  }
  m_view->batchProcessingResumed();
}

void MainWindowPresenter::notifyAutoreductionPaused() {
  for (auto batchPresenter : m_batchPresenters) {
    batchPresenter->anyBatchAutoreductionResumed();
  }
  m_view->batchProcessingPaused();
}

void MainWindowPresenter::notifyProcessingResumed() {
  m_view->batchProcessingResumed();
}

void MainWindowPresenter::notifyProcessingPaused() {
  m_view->batchProcessingPaused();
}

void MainWindowPresenter::notifyHelpPressed() { showHelp(); }

bool MainWindowPresenter::isAnyBatchProcessing() const {
  for (auto batchPresenter : m_batchPresenters) {
    if (batchPresenter->isProcessing())
      return true;
  }
  return false;
}

bool MainWindowPresenter::isAnyBatchAutoreducing() const {
  for (auto batchPresenter : m_batchPresenters) {
    if (batchPresenter->isAutoreducing())
      return true;
  }
  return false;
}

void MainWindowPresenter::addNewBatch(IBatchView *batchView) {
  m_batchPresenters.emplace_back(m_batchPresenterFactory.make(batchView));
  m_batchPresenters.back()->acceptMainPresenter(this);

  // starts in the paused state
  m_batchPresenters.back()->reductionPaused();

  // Ensure autoreduce button is enabled/disabled correctly for the new batch
  if (isAnyBatchAutoreducing())
    m_batchPresenters.back()->anyBatchAutoreductionResumed();
  else
    m_batchPresenters.back()->anyBatchAutoreductionPaused();
}

void MainWindowPresenter::showHelp() {
  MantidQt::API::HelpWindow::showCustomInterface(nullptr,
                                                 QString("ISIS Reflectometry"));
}

void MainWindowPresenter::notifySaveBatchRequested(int tabIndex) {
  auto filename = QFileDialog::getSaveFileName();
  if (filename == "")
    return;
  Encoder encoder;
  IBatchPresenter *batchPresenter = m_batchPresenters[tabIndex].get();
  auto map = encoder.encodeBatch(batchPresenter, m_view, false);
  MantidQt::API::saveJSONToFile(filename, map);
}

void MainWindowPresenter::notifyLoadBatchRequested(int tabIndex) {
  auto filename = QFileDialog::getOpenFileName();
  if (filename == "")
    return;
  auto map = MantidQt::API::loadJSONFromFile(filename);
  IBatchPresenter *batchPresenter = m_batchPresenters[tabIndex].get();
  Decoder decoder;
  decoder.decodeBatch(batchPresenter, m_view, map);
}
} // namespace ISISReflectometry
} // namespace CustomInterfaces
} // namespace MantidQt
