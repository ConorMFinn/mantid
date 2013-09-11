#ifndef MANTIDQTCUSTOMINTERFACES_STRETCH_H_
#define MANTIDQTCUSTOMINTERFACES_STRETCH_H_

#include "ui_Stretch.h"
#include "MantidQtCustomInterfaces/IndirectBayesTab.h"

namespace MantidQt
{
	namespace CustomInterfaces
	{
		class DLLExport Stretch : public IndirectBayesTab
		{
			Q_OBJECT

		public:
			Stretch(QWidget * parent = 0);

		private:
			virtual void help();
			virtual void validate();
			virtual void run();

			//The ui form
			Ui::Stretch m_uiForm;

		};
	} // namespace CustomInterfaces
} // namespace MantidQt

#endif
