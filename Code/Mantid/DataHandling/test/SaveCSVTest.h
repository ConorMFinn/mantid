#ifndef SAVECSVTEST_H_
#define SAVECSVTEST_H_

#include <cxxtest/TestSuite.h>
#include <fstream>

#include "../inc/SaveCSV.h"
#include "../../API/inc/WorkspaceFactory.h"
#include "../../DataObjects/inc/Workspace1D.h"
#include "../../API/inc/AnalysisDataService.h"

#include "boost/filesystem.hpp"  // used to delete file from disk

using namespace Mantid::Kernel;
using namespace Mantid::DataHandling;
using namespace Mantid::DataObjects;


// Notice, the SaveCSV algorithm currently does not create
// an output workspace and therefore no tests related to the 
// output workspace is performed.

// Notice also that currently no tests have been added to test
// this class when trying to save a 2D workspace with SaveCSV. 

class SaveCSVTest : public CxxTest::TestSuite
{
public: 
  
  SaveCSVTest()
  {
    // specify name of file to save 1D-workspace to
    
    outputFile = "testOfSaveCSV.csv";
    algToBeTested.setProperty("Filename", outputFile);
    

    // create dummy 1D-workspace
    
    std::vector<double> lVecX; for(double d=0.0; d<0.95; d=d+0.1) lVecX.push_back(d);
    std::vector<double> lVecY; for(double d=0.0; d<0.95; d=d+0.1) lVecY.push_back(d);
    std::vector<double> lVecE; for(double d=0.0; d<0.95; d=d+0.1) lVecE.push_back(d);

    WorkspaceFactory *factory = WorkspaceFactory::Instance();
    
    Workspace *localWorkspace = factory->create("Workspace1D");
    Workspace1D *localWorkspace1D = dynamic_cast<Workspace1D*>(localWorkspace);

    localWorkspace1D->setX(lVecX);
    localWorkspace1D->setData(lVecY, lVecE);

    AnalysisDataService *data = AnalysisDataService::Instance();
    data->add("testSpace", localWorkspace);
    
    algToBeTested.setProperty("InputWorkspace", "testSpace"); 
  }
  
  
  void testInit()
  {
    std::string result;
    StatusCode status = algToBeTested.getProperty("Filename", result);
    TS_ASSERT( ! status.isFailure() );
    TS_ASSERT( ! result.compare(outputFile));

    status = algToBeTested.initialize();
    TS_ASSERT( ! status.isFailure() );
    TS_ASSERT( algToBeTested.isInitialized() );
  }
  
  
  void testExec()
  {
    if ( !algToBeTested.isInitialized() ) algToBeTested.initialize();
  
    
    StatusCode status = algToBeTested.execute();
    TS_ASSERT( ! status.isFailure() );
    TS_ASSERT( algToBeTested.isExecuted() );    
   
    
    // has the algorithm written a file to disk?
    
    TS_ASSERT( boost::filesystem::exists(outputFile) );
    
    
    // Do a few tests to see if the content of outputFile is what you
    // expect.
     
    std::ifstream in(outputFile.c_str());
    
    double d1, d2, d3;
    std::string seperator;
    std::string number_plus_comma;
    
    in >> d1 >> seperator >> d2 >> seperator >> d3 >> number_plus_comma;
    
    in.close();
    
    TS_ASSERT( ! seperator.compare(",") );
    TS_ASSERT( ! number_plus_comma.compare("0.1,") );
    
    
    // remove file created by this algorithm
    
    boost::filesystem::remove_all(outputFile);    
  
  }
  
  void testFinal()
  {
    if ( !algToBeTested.isInitialized() ) algToBeTested.initialize();
    
    // The final() method doesn't do anything at the moment, but test anyway
    StatusCode status = algToBeTested.finalize();
    TS_ASSERT( ! status.isFailure() );
    TS_ASSERT( algToBeTested.isFinalized() );
  }
  
private:
  SaveCSV algToBeTested;
  std::string outputFile;
  
};
  
#endif /*SAVECSVTEST_H_*/
