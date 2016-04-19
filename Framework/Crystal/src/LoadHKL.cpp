#include "MantidAPI/FileProperty.h"
#include "MantidCrystal/LoadHKL.h"
#include "MantidCrystal/AnvredCorrection.h"
#include "MantidGeometry/Instrument/Detector.h"
#include "MantidKernel/Utils.h"
#include <fstream>

using namespace Mantid::Geometry;
using namespace Mantid::DataObjects;
using namespace Mantid::Kernel;
using namespace Mantid::API;
using namespace Mantid::PhysicalConstants;

namespace Mantid {
namespace Crystal {

// Register the algorithm into the AlgorithmFactory
DECLARE_ALGORITHM(LoadHKL)

//----------------------------------------------------------------------------------------------
/** Constructor
 */
LoadHKL::LoadHKL() {}

//----------------------------------------------------------------------------------------------
/** Destructor
 */
LoadHKL::~LoadHKL() {}

//----------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------
/** Initialize the algorithm's properties.
 */
void LoadHKL::init() {
  declareProperty(Kernel::make_unique<FileProperty>("Filename", "",
                                                    FileProperty::Load, ".hkl"),
                  "Path to an hkl file to save.");

  declareProperty(make_unique<WorkspaceProperty<PeaksWorkspace>>(
                      "OutputWorkspace", "", Direction::Output),
                  "Name of the output workspace.");

}

//----------------------------------------------------------------------------------------------
/** Execute the algorithm.
 */
void LoadHKL::exec() {

  std::string filename = getPropertyValue("Filename");
  PeaksWorkspace_sptr ws(new PeaksWorkspace());
  bool cosines = false;

  std::fstream in;
  in.open(filename.c_str(), std::ios::in);

  // Anvred write from Art Schultz
  // hklFile.write('%4d%4d%4d%8.2f%8.2f%4d%8.4f%7.4f%7d%7d%7.4f%4d%9.5f%9.4f\n'
  //    % (H, K, L, FSQ, SIGFSQ, hstnum, WL, TBAR, CURHST, SEQNUM, TRANSMISSION,
  //    DN, TWOTH, DSP))
  // HKL is flipped by -1 due to different q convention in ISAW vs mantid.
  // Default for kf-ki has -q
  double qSign = -1.0;
  std::string convention = ConfigService::Instance().getString("Q.convention");
  if (convention == "Crystallography")
    qSign = 1.0;
  Instrument_sptr inst(new Geometry::Instrument);
  Detector *detector = new Detector("det1", -1, nullptr);
  detector->setPos(0.0, 0.0, 0.0);
  inst->add(detector); // This takes care of deletion
  inst->markAsDetector(detector);
  Mantid::Geometry::ObjComponent *sample =
      new Mantid::Geometry::ObjComponent("Sample");
  inst->add(sample); // This takes care of deletion
  inst->markAsSamplePos(sample);
  Mantid::Geometry::ObjComponent *source =
      new Mantid::Geometry::ObjComponent("Source");
  source->setPos(0.0, 0.0, -1.0);
  inst->add(source); // This takes care of deletion
  inst->markAsSource(source);

  std::string line;
  bool first = true;
  double mu1 = 0.0, mu2 = 0.0, wl1 = 0.0, wl2 = 0.0, sc1 = 0.0, astar1 = 0.0;
  do {
    getline(in, line);
    if (line.length() > 125) cosines = true;
    double h = atof(line.substr(0, 4).c_str());
    double k = atof(line.substr(4, 4).c_str());
    double l = atof(line.substr(8, 4).c_str());
    if (h == 0.0 && k == 0 && l == 0)
      break;
    double Inti = atof(line.substr(12, 8).c_str());
    double SigI = atof(line.substr(20, 8).c_str());
    double wl = atof(line.substr(32, 8).c_str());
    double tbar, trans, scattering;
    int run, bank;
    if (cosines) {
      tbar = atof(line.substr(40, 8).c_str()); // tbar
      run = atoi(line.substr(102, 6).c_str());
      trans = atof(line.substr(114, 7).c_str()); // transmission
      bank = atoi(line.substr(121, 4).c_str());
      scattering = atof(line.substr(125, 9).c_str());
    } else {
      tbar = atof(line.substr(40, 7).c_str()); // tbar
      run = atoi(line.substr(47, 7).c_str());
      trans = atof(line.substr(61, 7).c_str()); // transmission
      bank = atoi(line.substr(68, 4).c_str());
      scattering = atof(line.substr(72, 9).c_str());
    }

    if (first) {
      mu1 = -std::log(trans) / tbar;
      wl1 = wl / 1.8;
      sc1 = scattering;
      astar1 = 1.0 / trans;
      first = false;
    } else {
      mu2 = -std::log(trans) / tbar;
      wl2 = wl / 1.8;
    }

    Peak peak(inst, scattering, wl);
    peak.setHKL(qSign * h, qSign * k, qSign * l);
    peak.setIntensity(Inti);
    peak.setSigmaIntensity(SigI);
    peak.setRunNumber(run);
    std::ostringstream oss;
    oss << "bank" << bank;
    std::string bankName = oss.str();

    peak.setBankName(bankName);
    if (cosines) {
      int col = atoi(line.substr(142, 7).c_str());
      int row = atoi(line.substr(149, 7).c_str());
      peak.setCol(col);
      peak.setRow(row);
    }
    ws->addPeak(peak);

  } while (!in.eof());

  in.close();
  // solve 2 linear equations to find amu and smu
  double amu = (mu2 - 1.0 * mu1) / (-1.0 * wl1 + wl2);
  double smu = mu1 - wl1 * amu;
  double theta = sc1 * radtodeg_half;
  int i = static_cast<int>(theta / 5.);
  double x0, x1, x2;
  gsl_poly_solve_cubic(pc[2][i] / pc[3][i], pc[1][i] / pc[3][i],
                       (pc[0][i] - astar1) / pc[3][i], &x0, &x1, &x2);
  double radius = 0.0;
  if (x0 > 0)
    radius = x0;
  else if (x1 > 0)
    radius = x1;
  else if (x2 > 0)
    radius = x2;
  gsl_poly_solve_cubic(pc[2][i + 1] / pc[3][i + 1], pc[1][i + 1] / pc[3][i + 1],
                       (pc[0][i + 1] - astar1) / pc[3][i + 1], &x0, &x1, &x2);
  double radius1 = 0.0;
  if (x0 > 0)
    radius1 = x0;
  else if (x1 > 0)
    radius1 = x1;
  else if (x2 > 0)
    radius1 = x2;
  double frac =
      theta -
      static_cast<double>(static_cast<int>(theta / 5.)) * 5.; // theta%5.
  frac = frac / 5.;
  radius = radius * (1 - frac) + radius1 * frac;
  radius /= mu1;
  g_log.notice() << "LinearScatteringCoef = " << smu
                 << " LinearAbsorptionCoef = " << amu << " Radius = " << radius
                 << " calculated from tbar and transmission of 2 peaks\n";
  API::Run &mrun = ws->mutableRun();
  mrun.addProperty<double>("Radius", radius, true);
  NeutronAtom neutron(static_cast<uint16_t>(EMPTY_DBL()),
                      static_cast<uint16_t>(0), 0.0, 0.0, smu, 0.0, smu, amu);
  Object shape = ws->sample().getShape(); // copy
  shape.setMaterial(Material("SetInLoadHKL", neutron, 1.0));
  ws->mutableSample().setShape(shape);

  setProperty("OutputWorkspace",
              boost::dynamic_pointer_cast<PeaksWorkspace>(ws));
}

} // namespace Mantid
} // namespace Crystal
