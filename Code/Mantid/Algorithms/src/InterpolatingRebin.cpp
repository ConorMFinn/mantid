//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidAlgorithms/InterpolatingRebin.h"
#include "MantidKernel/VectorHelper.h"
#include <gsl/gsl_errno.h>
#include <gsl/gsl_interp.h>
#include <gsl/gsl_spline.h>

namespace Mantid
{
  namespace Algorithms
  {

    // Register the class into the algorithm factory
    DECLARE_ALGORITHM(InterpolatingRebin)

    using namespace Kernel;
    using namespace API;

    /** Only calls its parent's (SimpleRebin) init()
    *
    */
    void InterpolatingRebin::init()
    {
      SimpleRebin::init();
    }

    /** Executes the rebin algorithm
    *
    *  @throw runtime_error Thrown if the bin range does not intersect the range of the input workspace
    */
    void InterpolatingRebin::exec()
    {
      // retrieve the properties
      std::vector<double> rb_params=getProperty("Params");

      // Get the input workspace
      MatrixWorkspace_sptr inputW = getProperty("InputWorkspace");

      //this calculation requires a distribution workspace but deal with the situation when we don't get this
      const bool distCon = ! inputW->isDistribution();
      if (distCon)
      {
        g_log.debug() << "Converting the input workspace to a distribution\n";
        WorkspaceHelpers::makeDistribution(inputW);
      }

      DataObjects::Histogram1D::RCtype XValues_new;
      // create new output X axis
      const int ntcnew =
        VectorHelper::createAxisFromRebinParams(rb_params,XValues_new.access());

      const int nHists = inputW->getNumberHistograms();
      // make output Workspace the same type is the input, but with new length of signal array
      MatrixWorkspace_sptr outputW =
        WorkspaceFactory::Instance().create(inputW, nHists, ntcnew, ntcnew-1);
      // Copy over the 'vertical' axis
      if (inputW->axes() > 1) outputW->replaceAxis( 1, inputW->getAxis(1)->clone(outputW.get()) );

      //evaluate the rebinned data
      outputXandEValues(inputW, XValues_new, outputW);

      //check if there was a convert to distribution done previously
      if (distCon)
      {
        g_log.debug() << "Converting the input and output workspaces _from_ distributions\n";
        WorkspaceHelpers::makeDistribution(inputW, false);
        // the calculation produces a distribution workspace but if they passed a non-distribution workspace they maybe not expect it, so convert back to the same form that was given
        WorkspaceHelpers::makeDistribution(outputW, false);
      }
      outputW->isDistribution( ! distCon );

      // Now propagate any masking correctly to the output workspace
      // More efficient to have this in a separate loop because 
      // MatrixWorkspace::maskBins blocks multi-threading
      for (int i=0; i <  nHists; ++i)
      {
        if ( inputW->hasMaskedBins(i) )  // Does the current spectrum have any masked bins?
        {
          this->propagateMasks(inputW,outputW,i);
        }
      }

      for (int i=0; i < outputW->axes(); ++i)
      {
        outputW->getAxis(i)->unit() = inputW->getAxis(i)->unit();        
      }
      
      // Assign it to the output workspace property
      setProperty("OutputWorkspace",outputW);
    }
    /** Calls the interpolation function for each histogram in the workspace
    *  @param[in] inputW workspace with un-interpolated data
    *  @param[in] XValues_new new x-values to interpolated to
    *  @param[out] outputW this will contain the interpolated data, the lengths of the histograms must corrospond with the number of x-values in XValues_new
    */
    void InterpolatingRebin::outputXandEValues(API::MatrixWorkspace_const_sptr inputW, const DataObjects::Histogram1D::RCtype &XValues_new, API::MatrixWorkspace_sptr outputW)
    {
      g_log.debug() << "Preparing to calculate y-values using splines and estimate errors\n";

      // prepare to use GSL functions but don't let them terminate Mantid
      gsl_error_handler_t * old_handler = gsl_set_error_handler(NULL);

      const int histnumber = inputW->getNumberHistograms();
      Progress prog(this,0.0,1.0,histnumber);
      PARALLEL_FOR2(inputW,outputW)
      for (int hist=0; hist <  histnumber;++hist)
      {
        PARALLEL_START_INTERUPT_REGION
        // get const references to input Workspace arrays (no copying)
        const MantidVec& XValues = inputW->readX(hist);
        const MantidVec& YValues = inputW->readY(hist);
        const MantidVec& YErrors = inputW->readE(hist);

        //get references to output workspace data (no copying)
        MantidVec& YValues_new=outputW->dataY(hist);
        MantidVec& YErrors_new=outputW->dataE(hist);

        try
        {
          // output data arrays are implicitly filled by function
          cubicInterpolation(XValues, YValues, YErrors,
            *XValues_new, YValues_new, YErrors_new);
        }
        catch (std::exception& ex)
        {
          g_log.error() << "Error in rebin function: " << ex.what() << std::endl;
          throw;
        }
        
        // Populate the output workspace X values
        outputW->setX(hist, XValues_new);
        
        prog.report();
        PARALLEL_END_INTERUPT_REGION
      }
      PARALLEL_CHECK_INTERUPT_REGION

      gsl_set_error_handler(old_handler);
    }

    /**Uses cubic splines to interpolate the mean rate of change of the integral
    *  over the inputed data bins to that for the user supplied bins. 
    *  Note that this algorithm was inplemented to provide a little more resolution
    *  on high count rate data. Whether it is more accurate than the standard rebin
    *  for all, or your, application needs more thought.
    *  The input data must be a distribution (proportional to the rate of change e.g.
    *  raw_counts/bin_widths) but note that these mean rate of counts data
    *  are integrals not (instanteously) sampled data. The error values on each point
    *  are a weighted mean of the error values from the surrounding input data. This
    *  makes sense if the interpolation error is low compared to the statistical
    *  errors on each input data point. The weighting is inversely proportional to
    *  the distance from the original data point to the new interpolated one.
    *
    *  @param[in] xOld the x-values of the data that will be intepolated
    *  @param[in] yOld the data's y-values that corrospond to the x-values, must be 1 element shorter than xOld.
    *  @param[in] eOld the error on each y-value, must be same length as yOld.
    *  @param[in] xNew x-values to rebin to, must be monotonically increasing
    *  @param[out] yNew is overwritten with the algorithm output. Must be allocated and 1 element shorter than xnew.
    *  @param[out] eNew is overwritten with errors from the errors on the nearest input data points. Must be allocated with the same number of points as ynew
    *  @throw runtime_error if there is a problem executing one of the GSL functions
    *  @throw invalid_argument if any output x-values are outside the range of input x-values
    **/
    void InterpolatingRebin::cubicInterpolation(const MantidVec &xOld, const MantidVec &yOld, const MantidVec &eOld,
          const MantidVec& xNew, MantidVec &yNew, MantidVec &eNew) const
    {
      // Make sure y and e vectors are of correct sizes
      const size_t size_old = yOld.size();
      if (size_old != (xOld.size() - 1) || size_old != eOld.size() )
        throw std::runtime_error("rebin: y and error vectors should be of same size & 1 shorter than x");
      const size_t size_new = yNew.size();
      if (size_new != (xNew.size() - 1) || size_new != eNew.size() )
        throw std::runtime_error("rebin: y and error vectors should be of same size & 1 shorter than x");
   
      // get the bin centres of the input data
      std::vector<double> xCensOld(size_new);
      VectorHelper::convertToBinCentre(xOld, xCensOld);
      // the centres of the output data
      std::vector<double> xCensNew(size_new);
      VectorHelper::convertToBinCentre(xNew, xCensNew);

      // find the range of input values whose x-values just suround the output x-values
      size_t oldIn1 =
        std::lower_bound(xCensOld.begin(), xCensOld.end(), xCensNew.front())
        - xCensOld.begin() - 1;
      size_t oldIn2 =
        std::lower_bound(xCensOld.begin(), xCensOld.end(), xCensNew.back())
        - xCensOld.begin();
      //bring one point before and one point after into the inpolation to reduce any errors coming in from the edge
      oldIn1 --;
      oldIn2 ++;

      //check that the end points are all within the arrays
      if ( oldIn1<0 || oldIn2>=size_old || oldIn1>oldIn2 )
      {
        throw std::invalid_argument("Problem with the requested x-values to intepolate to: There must be at\nleast two input data points below the range of intepolation points and\ntwo higher. Also the intepolation points must have monatomically increasing x-values.");
      }

      //get the GSL to allocate the memory, if this wasn't already done
      gsl_interp_accel *acc = gsl_interp_accel_alloc();
      const size_t nPoints = oldIn2 - oldIn1 + 1;
      gsl_spline *spline = gsl_spline_alloc(gsl_interp_cspline, nPoints);
      
      if ( ! acc || ! spline ||
      //GSL calculates the splines
        gsl_spline_init(spline, &xCensOld[oldIn1], &yOld[oldIn1], nPoints) )
      {
        throw std::runtime_error("Error setting up GSL spline functions");
      }
      
      for ( size_t i = 0; i < size_new; ++i )
      {
        yNew[i] = gsl_spline_eval(spline, xCensNew[i], acc);
        //(basic) error estimate the based on a weighted mean of the errors of the surrounding input data points
        eNew[i] = estimateError(xCensOld, eOld, xCensNew[i]);
      }
      
      //for GSL to clear up its memory use
      gsl_spline_free (spline);
      gsl_interp_accel_free (acc);
    }
    /**Estimates the error on each interpolated point by assuming it is similar to the errors in
    *  near by input data points. Output points with the same x-value as an input point have the
    *  same error as the input point. Points between input points have a error value that is a 
    *  weighted mean of the closest input points
    *  @param[in] xsOld x-values of the input data around the point of interested
    *  @param[in] esOld error values for the same points in the input data as xsOld
    *  @param[in] xNew the value of x for at the point of interest
    *  @return the estimated error at that point
    */
    double InterpolatingRebin::estimateError(const std::vector<double>& xsOld, const std::vector<double>& esOld,
          const double xNew) const
    {  
      //get the index of the first point that is higher in x, we'll base some of the error estimate on the error on this point 
      const size_t indAbove =
        std::lower_bound(xsOld.begin(), xsOld.end(), xNew) - xsOld.begin();
      
      const double error1 = esOld[indAbove];
      // ratio of weightings will be inversely proportional to the distance between the points
      double weight1 = xsOld[indAbove] - xNew;
      //check if the points are close enough agnoring any spurious effects that can occur with exact comparisons of floating point numbers
      if ( weight1 < 1e-100 )
      {
        // the point is on an input point, all the weight is on this point ignore the other
        return error1;
      }
      weight1 = 1/weight1;

      // if p were zero lower_bound must have found xCensNew <= xCensOld.front() but in that situation we should have exited before now
      const double error2 = esOld[indAbove-1];
      double weight2 =  xNew - xsOld[indAbove-1];
      if ( weight2 < 1e-100 )
      {
        // the point is on an input point, all the weight is on this point ignore the other
        return error2;
      }
      weight2 = 1/weight2;

      return ( weight1*error1 + weight2*error2 )/( weight1 + weight2 );
    }


  } // namespace Algorithm
} // namespace Mantid
