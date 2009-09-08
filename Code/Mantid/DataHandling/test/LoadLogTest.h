#ifndef LOADLOGTEST_H_
#define LOADLOGTEST_H_

#include <cxxtest/TestSuite.h>

#include "MantidDataHandling/LoadLog.h"
#include "MantidAPI/WorkspaceFactory.h"
#include "MantidAPI/Instrument.h"
#include "MantidDataObjects/Workspace2D.h"
#include "MantidAPI/AnalysisDataService.h"
#include "MantidKernel/Exception.h"
#include "MantidAPI/FrameworkManager.h"
#include "MantidAPI/Workspace.h"
#include "MantidAPI/Algorithm.h"
#include "MantidGeometry/Instrument/Component.h"
#include "MantidKernel/TimeSeriesProperty.h"
#include <vector>

using namespace Mantid::API;
using namespace Mantid::Kernel;
using namespace Mantid::DataHandling;
using namespace Mantid::DataObjects;

class LoadLogTest : public CxxTest::TestSuite
{
public: 
  
  LoadLogTest()
  {	
	  //initialise framework manager to allow logging
	//Mantid::API::FrameworkManager::Instance().initialize();
  }
  void testInit()
  {
    TS_ASSERT( !loader.isInitialized() );
    TS_ASSERT_THROWS_NOTHING(loader.initialize());    
    TS_ASSERT( loader.isInitialized() );
  }
  
  void testExecWithSingleLogFile()
  {
    if ( !loader.isInitialized() ) loader.initialize();

    // Path to test input file assumes Test directory checked out from SVN
    TS_ASSERT_THROWS_NOTHING(loader.setPropertyValue("Filename", "../../../../Test/Data/HRP37129_ICPevent.txt") )
    inputFile = loader.getPropertyValue("Filename");

    outputSpace = "LoadLogTest-singleLogFile";
    TS_ASSERT_THROWS(loader.setPropertyValue("Workspace", outputSpace), std::invalid_argument)
    // Create an empty workspace and put it in the AnalysisDataService
    Workspace_sptr ws = WorkspaceFactory::Instance().create("Workspace2D",1,1,1);

    TS_ASSERT_THROWS_NOTHING(AnalysisDataService::Instance().add(outputSpace, ws));    

	  std::string result;
    TS_ASSERT_THROWS_NOTHING( result = loader.getPropertyValue("Filename") )
    TS_ASSERT( ! result.compare(inputFile));

    TS_ASSERT_THROWS_NOTHING( result = loader.getPropertyValue("Workspace") )
    TS_ASSERT( ! result.compare(outputSpace));

	  TS_ASSERT_THROWS_NOTHING(loader.execute());    


    TS_ASSERT( loader.isExecuted() );    
    
    // Get back the saved workspace
    MatrixWorkspace_sptr output;
    TS_ASSERT_THROWS_NOTHING(output = boost::dynamic_pointer_cast<MatrixWorkspace>(AnalysisDataService::Instance().retrieve(outputSpace)));
    
	  boost::shared_ptr<Sample> sample = output->getSample();

    Property *l_property = sample->getLogData("HRP37129_ICPevent");
    TimeSeriesProperty<std::string> *l_timeSeries = dynamic_cast<TimeSeriesProperty<std::string>*>(l_property);

    std::string timeSeriesString = l_timeSeries->value();

    //std::cerr<<timeSeriesString<<'\n';

    // test that log file read in ok
    TS_ASSERT_EQUALS( timeSeriesString.substr(0,26), "2007-Nov-16 13:25:48   END" );
  }


  void testExecWithRawDatafile()
  {
      FrameworkManager::Instance();
    //if ( !loader.isInitialized() ) loader.initialize();

    LoadLog loaderRawFile;
    loaderRawFile.initialize();

	  // Path to test input file assumes Test directory checked out from SVN
    loaderRawFile.setPropertyValue("Filename", "../../../../Test/Data/HRP37125.RAW");
    inputFile = loaderRawFile.getPropertyValue("Filename");
    
    outputSpace = "LoadLogTestraw-datafile";
    // Create an empty workspace and put it in the AnalysisDataService
    Workspace_sptr ws = WorkspaceFactory::Instance().create("Workspace2D",1,1,1);

    TS_ASSERT_THROWS_NOTHING(AnalysisDataService::Instance().add(outputSpace, ws));    
    loaderRawFile.setPropertyValue("Workspace", outputSpace);

    std::string result;
    TS_ASSERT_THROWS_NOTHING( result = loaderRawFile.getPropertyValue("Filename") )
    TS_ASSERT( ! result.compare(inputFile));

    TS_ASSERT_THROWS_NOTHING( result = loaderRawFile.getPropertyValue("Workspace") )

    TS_ASSERT( ! result.compare(outputSpace));


	  TS_ASSERT_THROWS_NOTHING(loaderRawFile.execute());    

    TS_ASSERT( loaderRawFile.isExecuted() );    

    // Get back the saved workspace
    MatrixWorkspace_sptr output;
    TS_ASSERT_THROWS_NOTHING(output = boost::dynamic_pointer_cast<MatrixWorkspace>(AnalysisDataService::Instance().retrieve(outputSpace)));
   
    boost::shared_ptr<Sample> sample = output->getSample(); 

    // obtain the expected log files which should be in the same directory as the raw datafile

    Property *l_property = sample->getLogData( std::string("ICPevent") );
    TimeSeriesProperty<std::string> *l_timeSeriesString = dynamic_cast<TimeSeriesProperty<std::string>*>(l_property);
    std::string timeSeriesString = l_timeSeriesString->value();
    TS_ASSERT_EQUALS( timeSeriesString.substr(0,28), "2007-Nov-13 15:19:13   BEGIN" );

    l_property = sample->getLogData( std::string("cphs_6") );
    TimeSeriesProperty<double> *l_timeSeriesDouble = dynamic_cast<TimeSeriesProperty<double>*>(l_property);
    timeSeriesString = l_timeSeriesDouble->value();
    TS_ASSERT_EQUALS( timeSeriesString.substr(0,23), "2007-Nov-13 15:16:20  0" );

    l_property = sample->getLogData( std::string("PROP3") );
    l_timeSeriesDouble = dynamic_cast<TimeSeriesProperty<double>*>(l_property);
    timeSeriesString = l_timeSeriesDouble->value();
    TS_ASSERT_EQUALS( timeSeriesString.substr(0,23), "2007-Nov-13 15:16:20  0" );

    l_property = sample->getLogData( std::string("SE_He_Level") );
    l_timeSeriesDouble = dynamic_cast<TimeSeriesProperty<double>*>(l_property);
    timeSeriesString = l_timeSeriesDouble->value();
    TS_ASSERT_EQUALS( timeSeriesString.substr(0,24), "2007-Nov-13 15:17:08  -1" );

    l_property = sample->getLogData( std::string("TEMP1") );
    l_timeSeriesDouble = dynamic_cast<TimeSeriesProperty<double>*>(l_property);
    timeSeriesString = l_timeSeriesDouble->value();
    TS_ASSERT_EQUALS( timeSeriesString.substr(0,23), "2007-Nov-13 15:16:20  0" );
  }
  

  // Same idea as testExecWithRawDataFile() but testing on a raw file with the extension .s#
  // where # is some integer ranging from 01,02,...,99 I believe
  void testExecWithRawDatafile_s_type()
  {
    //if ( !loader.isInitialized() ) loader.initialize();

    LoadLog loaderRawFile;
    loaderRawFile.initialize();

	  // Path to test input file assumes Test directory checked out from SVN
    TS_ASSERT_THROWS_NOTHING( loaderRawFile.setPropertyValue("Filename", "../../../../Test/Data/HRP37129.S02") )
      inputFile = loaderRawFile.getPropertyValue("Filename");

    outputSpace = "LoadLogTest-rawdatafile_so_type";
    TS_ASSERT_THROWS( loaderRawFile.setPropertyValue("Workspace", outputSpace), std::invalid_argument)
    // Create an empty workspace and put it in the AnalysisDataService
    Workspace_sptr ws = WorkspaceFactory::Instance().create("Workspace1D",1,1,1);

    TS_ASSERT_THROWS_NOTHING(AnalysisDataService::Instance().add(outputSpace, ws));    

    std::string result;
    TS_ASSERT_THROWS_NOTHING( result = loaderRawFile.getPropertyValue("Filename") )
    TS_ASSERT( ! result.compare(inputFile));

    TS_ASSERT_THROWS_NOTHING( result = loaderRawFile.getPropertyValue("Workspace") )
    TS_ASSERT( ! result.compare(outputSpace));


	  TS_ASSERT_THROWS_NOTHING(loaderRawFile.execute());    

    TS_ASSERT( loaderRawFile.isExecuted() );    
    
    // Get back the saved workspace
    MatrixWorkspace_sptr output;
    TS_ASSERT_THROWS_NOTHING(output = boost::dynamic_pointer_cast<MatrixWorkspace>(AnalysisDataService::Instance().retrieve(outputSpace)));
   
    boost::shared_ptr<Sample> sample = output->getSample(); 

    // obtain the expected log files which should be in the same directory as the raw datafile

    Property *l_property = sample->getLogData( std::string("ICPevent") );
    TimeSeriesProperty<std::string> *l_timeSeriesString = dynamic_cast<TimeSeriesProperty<std::string>*>(l_property);
    std::string timeSeriesString = l_timeSeriesString->value();
    TS_ASSERT_EQUALS( timeSeriesString.substr(0,26), "2007-Nov-16 13:25:48   END" );
  }

  
private:
  LoadLog loader;
  std::string inputFile;
  std::string outputSpace;
  std::string inputSpace;
  
};
  
#endif /*LOADLOGTEST_H_*/
