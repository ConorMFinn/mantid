#include "MantidQtCustomInterfaces/IndirectMolDyn.h"

#include "MantidAPI/AlgorithmManager.h"

#include <QFileInfo>
#include <QString>

using namespace Mantid::API;

namespace MantidQt
{
	namespace CustomInterfaces
	{
		IndirectMolDyn::IndirectMolDyn(QWidget * parent) :
			IndirectSimulationTab(parent)
		{
			m_uiForm.setupUi(parent);

      connect(m_uiForm.ckResolution, SIGNAL(toggled(bool)), m_uiForm.dsResolution, SLOT(setEnabled(bool)));
		}

		/**
     * Validate the form to check the program can be run
     *
     * @return :: Whether the form was valid
     */
		bool IndirectMolDyn::validate()
		{
			QString filename = m_uiForm.mwRun->getFirstFilename();
			QFileInfo finfo(filename);
			QString ext = finfo.extension().toLower();

			if(ext != "dat" && ext != "cdl")
			{
				emit showMessageBox("File is not of expected type.\n File type must be .dat or .cdl");
				return false;
			}

      if(m_uiForm.ckResolution->isChecked() && !m_uiForm.dsResolution->isValid())
      {
				emit showMessageBox("Invalid resolution file.");
        return false;
      }

      bool energyIsNumber = false;
      m_uiForm.leMaxEnergy->text().toDouble(&energyIsNumber);
      if(!energyIsNumber)
      {
				emit showMessageBox("Max energy must be a number.");
        return false;
      }

			return true;
		}

		/**
     * Collect the settings on the GUI and run the MolDyn algorithm.
     */
		void IndirectMolDyn::run()
		{
      // Get filename and base filename (for naming output workspace group)
      QString filename = m_uiForm.mwRun->getFirstFilename();
      QFileInfo fi(filename);
      QString baseName = fi.baseName();

      // Setup algorithm
      IAlgorithm_sptr molDynAlg = AlgorithmManager::Instance().create("MolDyn");
      molDynAlg->setProperty("Filename", filename.toStdString());
      molDynAlg->setProperty("Functions", m_uiForm.leFunctionNames->text().toStdString());
      molDynAlg->setProperty("Verbose", m_uiForm.ckVerbose->isChecked());
      molDynAlg->setProperty("Save", m_uiForm.ckSave->isChecked());
      molDynAlg->setProperty("Plot", m_uiForm.cbPlot->currentText().toStdString());
      molDynAlg->setProperty("MaxEnergy", m_uiForm.leMaxEnergy->text().toStdString());
      molDynAlg->setProperty("OutputWorkspace", baseName.toStdString());

      // Set instrument resolution options
      if(m_uiForm.ckResolution->isChecked())
        molDynAlg->setProperty("Resolution", m_uiForm.dsResolution->getCurrentDataName().toStdString());

      runAlgorithm(molDynAlg);
		}

		/**
     * Set the data selectors to use the default save directory
     * when browsing for input files.
     *
     * @param settings :: The settings to loading into the interface
     */
		void IndirectMolDyn::loadSettings(const QSettings& settings)
		{
			m_uiForm.mwRun->readSettings(settings.group());
		}

	} // namespace CustomInterfaces
} // namespace MantidQt
