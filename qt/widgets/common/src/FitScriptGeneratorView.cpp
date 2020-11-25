// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#include "MantidQtWidgets/Common/FitScriptGeneratorView.h"
#include "MantidQtWidgets/Common/FitScriptGeneratorPresenter.h"

namespace MantidQt {
namespace MantidWidgets {

FitScriptGeneratorView::FitScriptGeneratorView(QWidget *parent)
    : API::MantidWidget(parent), m_presenter() {
  m_ui.setupUi(this);
  connectUiSignals();
}

FitScriptGeneratorView::~FitScriptGeneratorView() {}

void FitScriptGeneratorView::connectUiSignals() {
  connect(m_ui.pbRemove, SIGNAL(clicked()), this, SLOT(onRemoveClicked()));
}

void FitScriptGeneratorView::subscribePresenter(
    FitScriptGeneratorPresenter *presenter) {
  m_presenter = presenter;
}

void FitScriptGeneratorView::onRemoveClicked() {
  m_presenter->notifyPresenter(ViewEvent::RemoveClicked);
}

} // namespace MantidWidgets
} // namespace MantidQt
