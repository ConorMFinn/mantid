#ifndef Chebpoly_h
#define Chebpoly_h

/*!
  \class ChebPoly
  \brief Adds the ablity to put a cheb-poly on a class
  \version 1.0
  \date August 2006
  \author S. Ansell
*/
class ChebPoly  
{
  
 private:
  
  typedef std::vector<double> storeType;                 ///< The storage type
  typedef std::vector<double>::const_iterator storeIter; ///< Typedef for an iterator over the storage

  int polyN;    ///< number of polynominal units  
  int Npts;     ///< number of points
  const std::vector<double>& X;      ///< X-coord
  const std::vector<double>& Y;      ///< Y-coord
  const std::vector<double>& Err;    ///< Error values 
  std::vector<double> Coef;          ///< Coefficients

						 
  void chebft();                     ///< Do the fit?
  double fit(double) const;          ///< Another fitting functions?
  double polint(const storeIter,const storeIter,const int,double) const;  ///< Integrate the polynomial?
  void resizeYfit(const int);        ///< Resize the Y fit
 
 public:
  
  ChebPoly(const storeType&,const storeType&,const storeType&);  ///< Constructor
  ChebPoly(const ChebPoly&);                                     ///< Copy Constructor
  ChebPoly& operator=(const ChebPoly&);                          ///< Copy assignment operator
  ~ChebPoly();                                                   ///< Destructor

  void chebypol();                  ///< Do the fit?
  double chi();                     ///< Get the chi^2
  double value(const double) const; ///< Get the value

};


#endif





