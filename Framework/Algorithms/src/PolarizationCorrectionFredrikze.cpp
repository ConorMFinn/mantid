// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#include "MantidAlgorithms/PolarizationCorrectionFredrikze.h"
#include "MantidAPI/Axis.h"
#include "MantidAPI/MatrixWorkspace.h"
#include "MantidAPI/TextAxis.h"
#include "MantidAPI/WorkspaceGroup.h"
#include "MantidAPI/WorkspaceHistory.h"
#include "MantidAlgorithms/PolarizationCorrections/PolarizationCorrectionsHelpers.h"
#include "MantidAlgorithms/PolarizationCorrections/SpinStateValidator.h"
#include "MantidDataObjects/WorkspaceSingleValue.h"
#include "MantidGeometry/Instrument.h"
#include "MantidKernel/ListValidator.h"

#include <memory>

#include <algorithm>

using namespace Mantid::API;
using namespace Mantid::Kernel;
using namespace Mantid::Geometry;

namespace Prop {
constexpr auto PNR_LABEL("PNR");
constexpr auto PA_LABEL("PA");
constexpr auto EFFICIENCIES{"Efficiencies"};
constexpr auto INPUT_WORKSPACE{"InputWorkspace"};
constexpr auto OUTPUT_WORKSPACE{"OutputWorkspace"};
constexpr auto INPUT_SPIN_STATES("InputSpinStates");
constexpr auto OUTPUT_SPIN_STATES("OutputSpinStates");

// Default order for PA anaysis
static const std::vector<std::string> defaultOrderForPA = {
    Mantid::Algorithms::SpinStateConfigurationsFredrikze::PARA_PARA,
    Mantid::Algorithms::SpinStateConfigurationsFredrikze::PARA_ANTI,
    Mantid::Algorithms::SpinStateConfigurationsFredrikze::ANTI_PARA,
    Mantid::Algorithms::SpinStateConfigurationsFredrikze::ANTI_ANTI};
// Default order for PNR analysis
static const std::vector<std::string> defaultOrderForPNR = {Mantid::Algorithms::SpinStateConfigurationsFredrikze::PARA,
                                                            Mantid::Algorithms::SpinStateConfigurationsFredrikze::ANTI};

} // namespace Prop

namespace {

const std::string crhoLabel("Rho");

const std::string cppLabel("Pp");

const std::string cAlphaLabel("Alpha");

const std::string cApLabel("Ap");

std::vector<std::string> modes() {
  std::vector<std::string> modes;
  modes.emplace_back(Prop::PA_LABEL);
  modes.emplace_back(Prop::PNR_LABEL);
  return modes;
}

Instrument_const_sptr fetchInstrument(WorkspaceGroup const *const groupWS) {
  if (groupWS->size() == 0) {
    throw std::invalid_argument("Input group workspace has no children.");
  }
  Workspace_sptr firstWS = groupWS->getItem(0);
  MatrixWorkspace_sptr matrixWS = std::dynamic_pointer_cast<MatrixWorkspace>(firstWS);
  return matrixWS->getInstrument();
}

// Helper function to check valid spin states
bool isValidSpinState(const std::vector<std::string> &spinStates, const std::string &analysisMode) {
  if (analysisMode == Prop::PNR_LABEL) {
    // for PR, spinStates must be "p,a", "a,p", or empty vector
    return (spinStates.size() == 2 &&
            ((spinStates[0] == Mantid::Algorithms::SpinStateConfigurationsFredrikze::PARA &&
              spinStates[1] == Mantid::Algorithms::SpinStateConfigurationsFredrikze::ANTI) ||
             (spinStates[0] == Mantid::Algorithms::SpinStateConfigurationsFredrikze::ANTI &&
              spinStates[1] == Mantid::Algorithms::SpinStateConfigurationsFredrikze::PARA))) ||
           spinStates.empty();
  } else if (analysisMode == Prop::PA_LABEL) {
    // For PA, spinStates size must be 4 or empty
    return (spinStates.size() == 4 || spinStates.empty());
  }

  return false;
}

void validateInputWorkspace(WorkspaceGroup_sptr &ws, const std::string &inputStatesStr,
                            const std::string &outputStatesStr, const std::string &analysisMode) {
  MatrixWorkspace_sptr lastWS;
  for (size_t i = 0; i < ws->size(); ++i) {

    Workspace_sptr item = ws->getItem(i);

    if (MatrixWorkspace_sptr ws2d = std::dynamic_pointer_cast<MatrixWorkspace>(item)) {

      // X-units check
      auto wsUnit = ws2d->getAxis(0)->unit();
      auto expectedUnit = Units::Wavelength();
      if (wsUnit->unitID() != expectedUnit.unitID()) {
        throw std::invalid_argument("Input workspaces must have units of Wavelength");
      }

      // More detailed checks based on shape.
      if (lastWS) {
        if (lastWS->getNumberHistograms() != ws2d->getNumberHistograms()) {
          throw std::invalid_argument("Not all workspaces in the "
                                      "InputWorkspace WorkspaceGroup have the "
                                      "same number of spectrum");
        }
        if (lastWS->blocksize() != ws2d->blocksize()) {
          throw std::invalid_argument("Number of bins do not match between all "
                                      "workspaces in the InputWorkspace "
                                      "WorkspaceGroup");
        }

        auto &currentX = ws2d->x(0);
        auto &lastX = lastWS->x(0);
        auto xMatches = std::equal(lastX.cbegin(), lastX.cend(), currentX.cbegin());
        if (!xMatches) {
          throw std::invalid_argument("X-arrays do not match between all "
                                      "workspaces in the InputWorkspace "
                                      "WorkspaceGroup.");
        }

        const auto inputStates =
            Mantid::Algorithms::PolarizationCorrectionsHelpers::splitSpinStateString(inputStatesStr);
        const auto outputStates =
            Mantid::Algorithms::PolarizationCorrectionsHelpers::splitSpinStateString(outputStatesStr);

        if (!isValidSpinState(inputStates, analysisMode)) {
          throw std::invalid_argument("Invalid input spin state: " + inputStatesStr + " for " + analysisMode +
                                      " The possible values are 'pp,pa,ap,aa' for PA, or 'p,a' for PNR, in any order");
        }

        if (!isValidSpinState(outputStates, analysisMode)) {
          throw std::invalid_argument("Invalid output spin state: " + outputStatesStr + " for " + analysisMode +
                                      " The possible values are 'pp,pa,ap,aa' for PA, or 'p,a' for PNR, in any order");
        }
      }

      lastWS = ws2d; // Cache the last workspace so we can use it for comparison
                     // purposes.

    } else {
      std::stringstream messageBuffer;
      messageBuffer << "Item with index: " << i << "in the InputWorkspace is not a MatrixWorkspace";
      throw std::invalid_argument(messageBuffer.str());
    }
  }
}

/**
 * Map the input workspaces according to the specified input order.
 * @param inWS :: The input WorkspaceGroup.
 * @param order :: The vector of strings representing the order.
 * @return A map of spin state to MatrixWorkspace shared pointers.
 */
std::map<std::string, MatrixWorkspace_sptr> mapOrderToWorkspaces(const WorkspaceGroup_sptr &inWS,
                                                                 const std::vector<std::string> &order) {
  std::map<std::string, MatrixWorkspace_sptr> workspaceMap;

  for (size_t i = 0; i < order.size(); ++i) {
    workspaceMap[order[i]] = std::dynamic_pointer_cast<MatrixWorkspace>(inWS->getItem(i));
  }

  return workspaceMap;
}

/**
 * Map the corrected workspaces to the specified output order.
 * @param workspaces :: The map of spin state to MatrixWorkspace shared pointers.
 * @param order :: The vector of strings representing the output order.
 * @return A WorkspaceGroup with workspaces in the specified order.
 */
WorkspaceGroup_sptr mapWorkspacesToOrder(const std::map<std::string, MatrixWorkspace_sptr> &workspaces,
                                         const std::vector<std::string> &order) {
  auto dataOut = std::make_shared<WorkspaceGroup>();

  std::for_each(order.begin(), order.end(), [&](const auto &state) { dataOut->addWorkspace(workspaces.at(state)); });

  return dataOut;
}

using VecDouble = std::vector<double>;

} // namespace

namespace Mantid::Algorithms {

// Register the algorithm into the AlgorithmFactory
DECLARE_ALGORITHM(PolarizationCorrectionFredrikze)

//----------------------------------------------------------------------------------------------
/// Algorithm's name for identification. @see Algorithm::name
const std::string PolarizationCorrectionFredrikze::name() const { return "PolarizationCorrectionFredrikze"; }

/// Algorithm's version for identification. @see Algorithm::version
int PolarizationCorrectionFredrikze::version() const { return 1; }

/// Algorithm's category for identification. @see Algorithm::category
const std::string PolarizationCorrectionFredrikze::category() const { return "Reflectometry"; }

/**
 * @return Return the algorithm summary.
 */
const std::string PolarizationCorrectionFredrikze::summary() const {
  return "Makes corrections for polarization efficiencies of the polarizer and "
         "analyzer in a reflectometry neutron spectrometer.";
}

/**
 * Multiply a workspace by a constant value
 * @param lhsWS : WS to be multiplied
 * @param rhs : WS to multiply by (constant value)
 * @return Multiplied Workspace.
 */
MatrixWorkspace_sptr PolarizationCorrectionFredrikze::multiply(const MatrixWorkspace_sptr &lhsWS, const double &rhs) {
  auto multiply = this->createChildAlgorithm("Multiply");
  auto rhsWS = std::make_shared<DataObjects::WorkspaceSingleValue>(rhs);
  multiply->initialize();
  multiply->setProperty("LHSWorkspace", lhsWS);
  multiply->setProperty("RHSWorkspace", rhsWS);
  multiply->execute();
  MatrixWorkspace_sptr outWS = multiply->getProperty(Prop::OUTPUT_WORKSPACE);
  return outWS;
}

/**
 * Add a constant value to a workspace
 * @param lhsWS WS to add to
 * @param rhs Value to add
 * @return Summed workspace
 */
MatrixWorkspace_sptr PolarizationCorrectionFredrikze::add(const MatrixWorkspace_sptr &lhsWS, const double &rhs) {
  auto plus = this->createChildAlgorithm("Plus");
  auto rhsWS = std::make_shared<DataObjects::WorkspaceSingleValue>(rhs);
  plus->initialize();
  plus->setProperty("LHSWorkspace", lhsWS);
  plus->setProperty("RHSWorkspace", rhsWS);
  plus->execute();
  MatrixWorkspace_sptr outWS = plus->getProperty(Prop::OUTPUT_WORKSPACE);
  return outWS;
}

//----------------------------------------------------------------------------------------------
/** Initialize the algorithm's properties.
 */
void PolarizationCorrectionFredrikze::init() {
  declareProperty(
      std::make_unique<WorkspaceProperty<Mantid::API::WorkspaceGroup>>(Prop::INPUT_WORKSPACE, "", Direction::Input),
      "An input workspace to process.");

  auto propOptions = modes();
  declareProperty("PolarizationAnalysis", "PA", std::make_shared<StringListValidator>(propOptions),
                  "What Polarization mode will be used?\n"
                  "PNR: Polarized Neutron Reflectivity mode\n"
                  "PA: Full Polarization Analysis PNR-PA");

  declareProperty(
      std::make_unique<API::WorkspaceProperty<API::MatrixWorkspace>>(Prop::EFFICIENCIES, "", Kernel::Direction::Input),
      "A workspace containing the efficiency factors Pp, Ap, Rho and Alpha "
      "as histograms");

  declareProperty(
      std::make_unique<WorkspaceProperty<Mantid::API::WorkspaceGroup>>(Prop::OUTPUT_WORKSPACE, "", Direction::Output),
      "An output workspace.");

  const auto spinStateValidator =
      std::make_shared<SpinStateValidator>(std::unordered_set<int>{2, 4}, true, 'p', 'a', true);

  declareProperty(Prop::INPUT_SPIN_STATES, "", spinStateValidator,
                  "The order of spin states in the input workspace group. The possible values are 'pp,pa,ap,aa' or "
                  "'p,a', in any order.");

  declareProperty(Prop::OUTPUT_SPIN_STATES, "", spinStateValidator,
                  "The order of spin states in the output workspace group. The possible values are 'pp,pa,ap,aa' or "
                  "'p,a', in any order.");
}

WorkspaceGroup_sptr PolarizationCorrectionFredrikze::execPA(const WorkspaceGroup_sptr &inWS,
                                                            const std::vector<std::string> &inputOrder,
                                                            const std::vector<std::string> &outputOrder) {

  // If the input order vector is empty, use the default order
  const auto &effectiveInputOrder = inputOrder.empty() ? Prop::defaultOrderForPA : inputOrder;
  // Map the input workspaces according to the specified input order
  auto inputMap = mapOrderToWorkspaces(inWS, effectiveInputOrder);

  MatrixWorkspace_sptr Ipp = inputMap[SpinStateConfigurationsFredrikze::PARA_PARA];
  MatrixWorkspace_sptr Ipa = inputMap[SpinStateConfigurationsFredrikze::PARA_ANTI];
  MatrixWorkspace_sptr Iap = inputMap[SpinStateConfigurationsFredrikze::ANTI_PARA];
  MatrixWorkspace_sptr Iaa = inputMap[SpinStateConfigurationsFredrikze::ANTI_ANTI];

  Ipp->setTitle("Ipp");
  Iaa->setTitle("Iaa");
  Ipa->setTitle("Ipa");
  Iap->setTitle("Iap");

  const auto rho = this->getEfficiencyWorkspace(crhoLabel);
  const auto pp = this->getEfficiencyWorkspace(cppLabel);
  const auto alpha = this->getEfficiencyWorkspace(cAlphaLabel);
  const auto ap = this->getEfficiencyWorkspace(cApLabel);

  const auto A0 = (Iaa * pp * ap) + (Ipa * ap * rho * pp) + (Iap * ap * alpha * pp) + (Ipp * ap * alpha * rho * pp);
  const auto A1 = Iaa * pp;
  const auto A2 = Iap * pp;
  const auto A3 = Iaa * ap;
  const auto A4 = Ipa * ap;
  const auto A5 = Ipp * ap * alpha;
  const auto A6 = Iap * ap * alpha;
  const auto A7 = Ipp * pp * rho;
  const auto A8 = Ipa * pp * rho;

  const auto D = pp * ap * (rho + alpha + 1.0 + (rho * alpha));

  const auto nIpp = (A0 - A1 + A2 - A3 + A4 + A5 - A6 + A7 - A8 + Ipp + Iaa - Ipa - Iap) / D;
  const auto nIaa = (A0 + A1 - A2 + A3 - A4 - A5 + A6 - A7 + A8 + Ipp + Iaa - Ipa - Iap) / D;
  const auto nIap = (A0 - A1 + A2 + A3 - A4 - A5 + A6 + A7 - A8 - Ipp - Iaa + Ipa + Iap) / D;
  const auto nIpa = (A0 + A1 - A2 - A3 + A4 + A5 - A6 - A7 + A8 - Ipp - Iaa + Ipa + Iap) / D;

  // Map the corrected workspaces to the specified output order
  std::map<std::string, MatrixWorkspace_sptr> outputMap;
  outputMap[SpinStateConfigurationsFredrikze::PARA_PARA] = nIpp;
  outputMap[SpinStateConfigurationsFredrikze::PARA_ANTI] = nIpa;
  outputMap[SpinStateConfigurationsFredrikze::ANTI_PARA] = nIap;
  outputMap[SpinStateConfigurationsFredrikze::ANTI_ANTI] = nIaa;

  // If the output order vector is empty, use the default order
  const auto &effectiveOutputOrder = outputOrder.empty() ? Prop::defaultOrderForPA : outputOrder;
  auto dataOut = mapWorkspacesToOrder(outputMap, effectiveOutputOrder);

  size_t totalGroupEntries(dataOut->getNumberOfEntries());
  for (size_t i = 1; i < totalGroupEntries; i++) {
    auto alg = this->createChildAlgorithm("ReplaceSpecialValues");
    alg->setProperty(Prop::INPUT_WORKSPACE, dataOut->getItem(i));
    alg->setProperty(Prop::OUTPUT_WORKSPACE, "dataOut_" + std::to_string(i));
    alg->setProperty("NaNValue", 0.0);
    alg->setProperty("NaNError", 0.0);
    alg->setProperty("InfinityValue", 0.0);
    alg->setProperty("InfinityError", 0.0);
    alg->execute();
  }
  // Preserve the history of the inside workspaces
  nIpp->history().addHistory(Ipp->getHistory());
  nIaa->history().addHistory(Iaa->getHistory());
  nIpa->history().addHistory(Ipa->getHistory());
  nIap->history().addHistory(Iap->getHistory());

  return dataOut;
}

WorkspaceGroup_sptr PolarizationCorrectionFredrikze::execPNR(const WorkspaceGroup_sptr &inWS,
                                                             const std::vector<std::string> &inputOrder,
                                                             const std::vector<std::string> &outputOrder) {

  // If the input order vector is empty, use the default order
  const auto &effectiveInputOrder = inputOrder.empty() ? Prop::defaultOrderForPNR : inputOrder;
  // Map the input workspaces according to the specified input order
  auto inputMap = mapOrderToWorkspaces(inWS, effectiveInputOrder);

  MatrixWorkspace_sptr Ip = inputMap[SpinStateConfigurationsFredrikze::PARA];
  MatrixWorkspace_sptr Ia = inputMap[SpinStateConfigurationsFredrikze::ANTI];

  const auto rho = this->getEfficiencyWorkspace(crhoLabel);
  const auto pp = this->getEfficiencyWorkspace(cppLabel);

  const auto D = pp * (rho + 1);

  const auto nIp = (Ip * (rho * pp + 1.0) + Ia * (pp - 1.0)) / D;
  const auto nIa = (Ip * (rho * pp - 1.0) + Ia * (pp + 1.0)) / D;

  // Preserve the history of the inside workspaces
  nIp->history().addHistory(Ip->getHistory());
  nIa->history().addHistory(Ia->getHistory());

  std::map<std::string, MatrixWorkspace_sptr> outputMap;
  outputMap[SpinStateConfigurationsFredrikze::PARA] = nIp;
  outputMap[SpinStateConfigurationsFredrikze::ANTI] = nIa;

  // If the output order vector is empty, use the default order
  const auto &effectiveOutputOrder = outputOrder.empty() ? Prop::defaultOrderForPNR : outputOrder;
  auto dataOut = mapWorkspacesToOrder(outputMap, effectiveOutputOrder);

  return dataOut;
}

/** Extract a spectrum from the Efficiencies workspace as a 1D workspace.
 * @param label :: A label of the spectrum to extract.
 * @return :: A workspace with a single spectrum.
 */
std::shared_ptr<Mantid::API::MatrixWorkspace>
PolarizationCorrectionFredrikze::getEfficiencyWorkspace(const std::string &label) {
  MatrixWorkspace_sptr efficiencies = getProperty(Prop::EFFICIENCIES);
  const auto &axis = dynamic_cast<TextAxis &>(*efficiencies->getAxis(1));
  size_t index = axis.length();
  for (size_t i = 0; i < axis.length(); ++i) {
    if (axis.label(i) == label) {
      index = i;
      break;
    }
  }

  if (index == axis.length()) {
    // Check if we need to fetch polarization parameters from the instrument's
    // parameters
    static std::map<std::string, std::string> loadableProperties{
        {crhoLabel, "crho"}, {cppLabel, "cPp"}, {cApLabel, "cAp"}, {cAlphaLabel, "calpha"}};
    WorkspaceGroup_sptr inWS = getProperty(Prop::INPUT_WORKSPACE);
    Instrument_const_sptr instrument = fetchInstrument(inWS.get());
    auto vals = instrument->getStringParameter(loadableProperties[label]);
    if (vals.empty()) {
      throw std::invalid_argument("Efficiency property not found: " + label);
    }
    auto extract = createChildAlgorithm("CreatePolarizationEfficiencies");
    extract->initialize();
    extract->setProperty(Prop::INPUT_WORKSPACE, efficiencies);
    extract->setProperty(label, vals.front());
    extract->execute();
    MatrixWorkspace_sptr outWS = extract->getProperty(Prop::OUTPUT_WORKSPACE);
    return outWS;
  } else {
    auto extract = createChildAlgorithm("ExtractSingleSpectrum");
    extract->initialize();
    extract->setProperty(Prop::INPUT_WORKSPACE, efficiencies);
    extract->setProperty("WorkspaceIndex", static_cast<int>(index));
    extract->execute();
    MatrixWorkspace_sptr outWS = extract->getProperty(Prop::OUTPUT_WORKSPACE);
    return outWS;
  }
}

//----------------------------------------------------------------------------------------------
/** Execute the algorithm.
 */
void PolarizationCorrectionFredrikze::exec() {
  WorkspaceGroup_sptr inWS = getProperty(Prop::INPUT_WORKSPACE);
  const std::string analysisMode = getProperty("PolarizationAnalysis");
  const size_t nWorkspaces = inWS->size();

  const std::string inputStatesStr = getProperty(Prop::INPUT_SPIN_STATES);
  const std::string outputStatesStr = getProperty(Prop::OUTPUT_SPIN_STATES);

  const auto inputStates = PolarizationCorrectionsHelpers::splitSpinStateString(inputStatesStr);
  const auto outputStates = PolarizationCorrectionsHelpers::splitSpinStateString(outputStatesStr);

  validateInputWorkspace(inWS, inputStatesStr, outputStatesStr, analysisMode);

  WorkspaceGroup_sptr outWS;
  if (analysisMode == Prop::PA_LABEL) {
    if (nWorkspaces != 4) {
      throw std::invalid_argument("For PA analysis, input group must have 4 periods.");
    }
    g_log.notice("PA polarization correction");
    outWS = execPA(inWS, inputStates, outputStates);
  } else if (analysisMode == Prop::PNR_LABEL) {
    if (nWorkspaces != 2) {
      throw std::invalid_argument("For PNR analysis, input group must have 2 periods.");
    }
    outWS = execPNR(inWS, inputStates, outputStates);
    g_log.notice("PNR polarization correction");
  }

  this->setProperty(Prop::OUTPUT_WORKSPACE, outWS);
}

} // namespace Mantid::Algorithms
