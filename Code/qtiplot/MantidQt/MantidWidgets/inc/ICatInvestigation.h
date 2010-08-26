#ifndef MANTIDWIDGETS_ICATINVESTIGATION_H_
#define MANTIDWIDGETS_ICATINVESTIGATION_H_

//----------------------
// Includes
//----------------------
#include "MantidQtMantidWidgets/ui_ICatInvestigation.h"
#include "MantidQtMantidWidgets/MantidWidget.h"
#include "MantidAPI/ITableWorkspace.h"
#include "MantidAPI/AlgorithmManager.h"
#include "WidgetDllOption.h"

namespace MantidQt
{
namespace MantidWidgets
{
class  ICatInvestigation : public QWidget
{
  Q_OBJECT

public:
  /// Constructor
  ICatInvestigation(long long investId,const QString &qRbNumber,const QString &qTitle,
	   const QString & qInstrument,Mantid::API::ITableWorkspace_sptr& m_ws2_sptr,QWidget *parent = 0);
signals:
   ///this signal prints error messge to log window
   
void error(const QString&,int param=0);
   /// loadraw asynchronous execution.
   void loadRawAsynch(const QString&,const QString&);
  
   /// loadnexus asynchronous execution
   void loadNexusAsynch(const QString&,const QString&);

   /// signal for downloading data files
   void executeDownload(std::vector<std::string>&);

   //signal for uncontrolled loading of 
   void executeLoadAlgorithm(const QString&, const QString&, const QString&);

private:
  /// Initialize the layout
  virtual void initLayout();
  /// Populate the investigation tree widget
  void populateInvestigationTreeWidget();
  /// execute getdatafIles algorithm
  Mantid::API::ITableWorkspace_sptr executeGetdataFiles();

  /// executes getdatasets algorithm
   Mantid::API::ITableWorkspace_sptr executeGetdataSets();
 
  /// Poulates the data files tree widget 
  void populateinvestigationWidget(Mantid::API::ITableWorkspace_sptr ws_sptr,const QString& type,bool bEnable);

  /// executes the downlaod file algorithm
   bool executeDownloadDataFiles(const std::vector<std::string>& fileNames,std::vector<std::string>& fileLocs);
   
   /// get the selected file name
	void getSelectedFileNames(std::vector<std::string>& fileNames );

	/// checks the file is of raw extn
	bool isRawFile(const QString& fileName);
	/// checks the file is of  nxs extn
	bool isNexusFile(const QString& fileName);

	/// executes loadRaw algorithm
	void executeLoadRaw(const QString& fileName,const QString& wsName);

	/// executes loadRaw algorithm
	void executeLoadNexus(const QString& fileName,const QString& wsName);

	//if the loading is controlled
	bool isLoadingControlled();

	/// execute the algorithm
	bool execute(const QString& algName,const int& version,const QString& filepath,const QString& wsName);

	/// This method loads raw/nexus data
	void loadData( const QString& filePath);


  private slots:
	  /// investigation Clicked
	void investigationClicked(QTreeWidgetItem *, int);
	///cancel button clicked
	void onCancel();
	/// Download button clicked
	void onDownload();
    /// tree widget item named defaults clicked
	void investigationWidgetItemExpanded(QTreeWidgetItem* item);
    /// table item selected 
	void tableItemSelected(QTableWidgetItem* item);
	/// load button clicked
	void onLoad();
	/// select all files button clciked
	void onSelectAllFiles();
	/// if data file checkbox selected
	bool isDataFilesChecked();
	 /// This method checks the selected data file exis in the downlaoded list
	bool isFileExistsInDownlodedList(const std::string& selectedFile,std::string& loadPath);

	/// getting the file locations
	void setfileLocations(const std::vector<std::string>&);
	//handler for helpbutton
	void helpButtonClicked();
	
private:
  //The form generated by Qt Designer
  Ui::ICatInvestigation m_uiForm;

   ///investigation id
  long long m_invstId;
  /// RbNumber
  QString m_RbNumber;
  /// Title
  QString m_Title;
  /// Instrument
  QString m_Instrument;
  /// data files workspace 
  Mantid::API::ITableWorkspace_sptr m_datafilesws_sptr;
   /// filtered data files workspace pointer
  Mantid::API::ITableWorkspace_sptr m_filteredws_sptr;
  /// shared pointer to datasets workspace
  Mantid::API::ITableWorkspace_sptr m_datasetsws_sptr;
  /// shared pointer to investigation data like abstarct,facility userand samle name 
  Mantid::API::ITableWorkspace_sptr m_investws_sptr;
  std::vector<std::string> m_downloadedFileList;

};

}
}

#endif //MANTIDQTCUSTOMINTERFACES_ICATINVESTIGATION_H_
