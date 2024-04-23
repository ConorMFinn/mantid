// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
//------------------------------------------------------
// Includes
//------------------------------------------------------
#include "MantidQtWidgets/Common/WorkspaceMultiSelector.h"
#include "MantidQtWidgets/Common/InterfaceUtils.h"
#include "MantidQtWidgets/Common/WorkspaceUtils.h"

#include <Poco/AutoPtr.h>
#include <Poco/NObserver.h>
#include <Poco/Notification.h>
#include <Poco/NotificationCenter.h>

#include "MantidAPI/AlgorithmManager.h"
#include "MantidAPI/MatrixWorkspace.h"
#include "MantidAPI/WorkspaceGroup.h"

#include <QCompleter>
#include <QDebug>
#include <QHeaderView>
#include <QLineEdit>
#include <QStyledItemDelegate>

constexpr short namesCol = 0;
constexpr short indexCol = 1;
using namespace MantidQt::MantidWidgets::InterfaceUtils;
using namespace MantidQt::MantidWidgets::WorkspaceUtils;

namespace {

QStringList headerLabels = {"Workspace Name", "Ws Index"};

const auto SPECTRA_LIST = getRegexValidatorString(SpectraValidator);
} // namespace

/**
 * Default constructor
 * @param parent :: A widget to act as this widget's parent (default = NULL)
 * @param init :: If true then the widget will make calls to the framework
 * (default = true)
 */
namespace MantidQt {
namespace MantidWidgets {

WorkspaceMultiSelector::WorkspaceMultiSelector(QWidget *parent, bool init)
    : QTableWidget(parent), m_addObserver(*this, &WorkspaceMultiSelector::handleAddEvent),
      m_remObserver(*this, &WorkspaceMultiSelector::handleRemEvent),
      m_clearObserver(*this, &WorkspaceMultiSelector::handleClearEvent),
      m_renameObserver(*this, &WorkspaceMultiSelector::handleRenameEvent),
      m_replaceObserver(*this, &WorkspaceMultiSelector::handleReplaceEvent), m_init(init), m_workspaceTypes(),
      m_showGroups(false), m_suffix() {

  if (init) {
    connectObservers();
  }
}

/**
 * Destructor for WorkspaceMultiSelector
 * De-subscribes this object from the Poco NotificationCentre
 */
WorkspaceMultiSelector::~WorkspaceMultiSelector() { disconnectObservers(); }

/**
 * Setup table dimensions and headers from the parent class.
 */
void WorkspaceMultiSelector::setupTable() {
  this->setRowCount(0);
  this->setColumnCount(headerLabels.size());
  this->verticalHeader()->setVisible(false);
  this->horizontalHeader()->setVisible(true);
  this->setHorizontalHeaderLabels(headerLabels);
  this->setItemDelegateForColumn(1, new RegexInputDelegate(this, SPECTRA_LIST));
  this->setSelectionMode(QAbstractItemView::ExtendedSelection);
  this->setSortingEnabled(true);
  this->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}
/**
 * De-subscribes this object from the Poco NotificationCentre
 */
void WorkspaceMultiSelector::disconnectObservers() {
  if (m_init) {
    Mantid::API::AnalysisDataService::Instance().notificationCenter.removeObserver(m_addObserver);
    Mantid::API::AnalysisDataService::Instance().notificationCenter.removeObserver(m_remObserver);
    Mantid::API::AnalysisDataService::Instance().notificationCenter.removeObserver(m_clearObserver);
    Mantid::API::AnalysisDataService::Instance().notificationCenter.removeObserver(m_renameObserver);
    Mantid::API::AnalysisDataService::Instance().notificationCenter.removeObserver(m_replaceObserver);
    m_init = false;
  }
}

/**
 * Subscribes this object to the Poco NotificationCentre
 */
void WorkspaceMultiSelector::connectObservers() {
  Mantid::API::AnalysisDataServiceImpl &ads = Mantid::API::AnalysisDataService::Instance();
  ads.notificationCenter.addObserver(m_addObserver);
  ads.notificationCenter.addObserver(m_remObserver);
  ads.notificationCenter.addObserver(m_renameObserver);
  ads.notificationCenter.addObserver(m_clearObserver);
  ads.notificationCenter.addObserver(m_replaceObserver);
  refresh();
  m_init = true;
}

QStringList WorkspaceMultiSelector::getWorkspaceTypes() const { return m_workspaceTypes; }

void WorkspaceMultiSelector::setWorkspaceTypes(const QStringList &types) {
  if (types != m_workspaceTypes) {
    m_workspaceTypes = types;
    if (m_init) {
      refresh();
    }
  }
}

bool WorkspaceMultiSelector::showWorkspaceGroups() const { return m_showGroups; }

void WorkspaceMultiSelector::showWorkspaceGroups(bool show) {
  if (show != m_showGroups) {
    m_showGroups = show;
    if (m_init) {
      refresh();
    }
  }
}

bool WorkspaceMultiSelector::isValid() const {
  const QTableWidgetItem *item = currentItem();
  return (item != nullptr);
}

QStringList WorkspaceMultiSelector::getWSSuffixes() const { return m_suffix; }

void WorkspaceMultiSelector::setWSSuffixes(const QStringList &suffix) {
  if (suffix != m_suffix) {
    m_suffix = suffix;
    if (m_init) {
      refresh();
    }
  }
}

void WorkspaceMultiSelector::addItem(const std::string &name) {
  insertRow(rowCount());
  auto nameItem = std::make_unique<QTableWidgetItem>(QString::fromStdString(name));
  auto indexItem = std::make_unique<QTableWidgetItem>(QString::fromStdString(getIndexString(name)));

  nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);

  setItem(rowCount() - 1, namesCol, nameItem.release());
  setItem(rowCount() - 1, indexCol, indexItem.release());
}

void WorkspaceMultiSelector::renameItem(const std::string &newName, int row) {
  // here is assuming item has already been deemed eligible
  this->item(row, namesCol)->setText(QString::fromStdString(newName));
  this->item(row, indexCol)->setText(QString::fromStdString(getIndexString(newName)));
}

void WorkspaceMultiSelector::addItems(const std::vector<std::string> &names) {
  for (auto const &name : names) {
    if (checkEligibility(name))
      addItem(name);
  }
}

stringPairVec WorkspaceMultiSelector::retrieveSelectedNameIndexPairs() {

  auto selIndexes = selectedIndexes();
  stringPairVec nameIndexPairVec;
  nameIndexPairVec.reserve(static_cast<std::size_t>(selIndexes.size()));

  for (auto const &index : selIndexes) {
    std::string txt = item(index.row(), namesCol)->text().toStdString();
    if (!txt.empty()) {
      std::string idx = item(index.row(), indexCol)->text().toStdString();
      nameIndexPairVec.push_back(std::make_pair(txt, idx));
    }
  }
  nameIndexPairVec.shrink_to_fit();
  return nameIndexPairVec;
}

void WorkspaceMultiSelector::resetIndexRangeToDefault() {
  auto selIndex = this->selectedIndexes();
  if (!selIndex.isEmpty()) {
    for (auto &index : selIndex) {
      std::string selName = this->item(index.row(), namesCol)->text().toStdString();
      this->item(index.row(), indexCol)->setText(QString::fromStdString(getIndexString(selName)));
    }
  }
}

void WorkspaceMultiSelector::unifyRange() {
  auto selIndex = this->selectedIndexes();
  if (!selIndex.isEmpty()) {
    auto rangeFirst = this->item(selIndex.takeFirst().row(), indexCol)->text();
    for (auto &index : selIndex) {
      this->item(index.row(), indexCol)->setText(rangeFirst);
    }
  }
}

void WorkspaceMultiSelector::handleAddEvent(Mantid::API::WorkspaceAddNotification_ptr pNf) {
  const std::lock_guard<std::mutex> lock(m_adsMutex);
  if (checkEligibility(pNf->objectName())) {
    addItem(pNf->objectName());
  }
}

void WorkspaceMultiSelector::handleRemEvent(Mantid::API::WorkspacePostDeleteNotification_ptr pNf) {
  const std::lock_guard<std::mutex> lock(m_adsMutex);
  QString name = QString::fromStdString(pNf->objectName());
  auto items = findItems(name, Qt::MatchExactly);
  if (!items.isEmpty()) {
    for (auto &item : items)
      removeRow(item->row());
  }
  if (rowCount() == 0) {
    emit emptied();
  }
}

void WorkspaceMultiSelector::handleClearEvent(Mantid::API::ClearADSNotification_ptr) {
  const std::lock_guard<std::mutex> lock(m_adsMutex);
  this->clearContents();
  while (rowCount() > 0) {
    removeRow(0);
  }
  emit emptied();
}

void WorkspaceMultiSelector::handleRenameEvent(Mantid::API::WorkspaceRenameNotification_ptr pNf) {
  const std::lock_guard<std::mutex> lock(m_adsMutex);

  QString newName = QString::fromStdString(pNf->newObjectName());
  QString currName = QString::fromStdString(pNf->objectName());

  bool eligible = checkEligibility(pNf->newObjectName());
  auto currItems = findItems(currName, Qt::MatchExactly);
  auto newItems = findItems(newName, Qt::MatchExactly);

  if (eligible) {
    if (!currItems.isEmpty() && newItems.isEmpty())
      renameItem(pNf->newObjectName(), currItems.first()->row());
    else if (currItems.isEmpty() && newItems.isEmpty())
      addItem(pNf->newObjectName());
    else if (!currItems.isEmpty() && !newItems.isEmpty()) {
      // list reduction w. redundancies
      removeRow(currItems.first()->row());
      renameItem(pNf->newObjectName(), newItems.first()->row());
    }
  } else {
    if (!currItems.isEmpty())
      removeRow(currItems.first()->row());
  }
}

void WorkspaceMultiSelector::handleReplaceEvent(Mantid::API::WorkspaceAfterReplaceNotification_ptr pNf) {
  const std::lock_guard<std::mutex> lock(m_adsMutex);
  QString name = QString::fromStdString(pNf->objectName());
  bool eligible = checkEligibility(pNf->objectName());
  auto items = findItems(name, Qt::MatchExactly);

  if ((eligible && !items.isEmpty()) || (!eligible && items.isEmpty()))
    return;
  else if (items.isEmpty() && eligible) {
    addItem(pNf->objectName());
  } else { // (inside && !eligible)
    removeRow(items.first()->row());
  }
}

bool WorkspaceMultiSelector::checkEligibility(const std::string &name) const {
  auto &ads = Mantid::API::AnalysisDataService::Instance();
  auto workspace = ads.retrieve(name);
  if ((!m_workspaceTypes.empty()) && m_workspaceTypes.indexOf(QString::fromStdString(workspace->id())) == -1)
    return false;
  else if (!hasValidSuffix(name))
    return false;
  else if (!m_showGroups)
    return std::dynamic_pointer_cast<Mantid::API::WorkspaceGroup>(workspace) == nullptr;
  return true;
}

bool WorkspaceMultiSelector::hasValidSuffix(const std::string &name) const {
  if (m_suffix.isEmpty())
    return true;
  if (name.find_last_of("_") < name.size())
    return m_suffix.contains(QString::fromStdString(name.substr(name.find_last_of("_"))));
  return false;
}

void WorkspaceMultiSelector::refresh() {
  const std::lock_guard<std::mutex> lock(m_adsMutex);
  this->clearContents();
  auto &ads = Mantid::API::AnalysisDataService::Instance();
  auto items = ads.getObjectNames();
  addItems(items);
}

/**
 * Called when there is an interaction with the widget.
 */
void WorkspaceMultiSelector::focusInEvent(QFocusEvent *) { emit focussed(); }

} // namespace MantidWidgets
} // namespace MantidQt
