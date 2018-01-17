#ifndef MANTIDQTCUSTOMINTERFACES_ENGGDIFFRACTION_GSASFITTINGMODEL_H_
#define MANTIDQTCUSTOMINTERFACES_ENGGDIFFRACTION_GSASFITTINGMODEL_H_

#include "DllConfig.h"
#include "IEnggDiffGSASFittingModel.h"
#include "RunMap.h"

namespace MantidQt {
namespace CustomInterfaces {

class MANTIDQT_ENGGDIFFRACTION_DLL EnggDiffGSASFittingModel
    : public IEnggDiffGSASFittingModel {

public:
  bool doPawleyRefinement(const int runNumber, const size_t bank,
                          const std::string &instParamFile,
                          const std::vector<std::string> &phaseFiles,
                          const std::string &pathToGSASII,
                          const std::string &GSASIIProjectFile,
                          const double dMin,
                          const double negativeWeight) override;

  bool doRietveldRefinement(const int runNumber, const size_t bank,
                            const std::string &instParamFile,
                            const std::vector<std::string> &phaseFiles,
                            const std::string &pathToGSASII,
                            const std::string &GSASIIProjectFile) override;

  boost::optional<Mantid::API::MatrixWorkspace_sptr>
  getFittedPeaks(const int runNumber, const size_t bank) const override;

  boost::optional<Mantid::API::MatrixWorkspace_sptr>
  getFocusedWorkspace(const int runNumber, const size_t bank) const override;

  boost::optional<Mantid::API::ITableWorkspace_sptr>
  getLatticeParams(const int runNumber, const size_t bank) const override;

  std::vector<std::pair<int, size_t>> getRunLabels() const override;

  boost::optional<double> getRwp(const int runNumber,
                                 const size_t bank) const override;

  bool hasFittedPeaksForRun(const int runNumber,
                            const size_t bank) const override;

  bool loadFocusedRun(const std::string &filename) override;

protected:
  /**
   Get whether a focused run has been loaded with a given runNumber and bank ID.
   Used for testing - exposed by the dummy class in EnggDiffGSASFittingModelTest
   @param runNumber The run number to check for
   @param bank The bank to check for
   @return Whether the model contains a focused run with these parameters
   */
  bool hasFocusedRun(const int runNumber, const size_t bank) const;

private:
  static const size_t MAX_BANKS = 2;

  RunMap<MAX_BANKS, Mantid::API::MatrixWorkspace_sptr> m_focusedWorkspaceMap;
};

} // CustomInterfaces
} // MantidQt

#endif // MANTIDQTCUSTOMINTERFACES_ENGGDIFFRACTION_GSASFITTINGMODEL_H_
