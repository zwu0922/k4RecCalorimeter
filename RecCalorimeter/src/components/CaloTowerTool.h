#ifndef RECCALORIMETER_CALOTOWERTOOL_H
#define RECCALORIMETER_CALOTOWERTOOL_H

// from Gaudi
#include "GaudiKernel/AlgTool.h"

// k4geo
#include "detectorSegmentations/FCCSWGridPhiEta_k4geo.h"

// k4FWCore
#include "k4FWCore/DataHandle.h"
#include "k4Interface/ITowerTool.h"
class IGeoSvc;

// dd4hep
#include "DDSegmentation/MultiSegmentation.h"

namespace dd4hep {
namespace DDSegmentation {
  class Segmentation;
  class BitFieldCoder;
} // namespace DDSegmentation
} // namespace dd4hep

// edm4hep
namespace edm4hep {
class CalorimeterHitCollection;
class CalorimeterHit;
class Cluster;
} // namespace edm4hep

/** @class CaloTowerTool Reconstruction/RecCalorimeter/src/components/CaloTowerTool.h
 * CaloTowerTool.h
 *
 *  Tool building the calorimeter towers for the sliding window algorithm.
 *  This tool runs over all calorimeter systems (ECAL barrel, HCAL barrel + extended barrel, calorimeter endcaps,
 * forward calorimeters). If not all systems are available or not wanted to be used, create an empty collection using
 * CreateDummyCellsCollection algorithm.
 *  Towers are built of cells in eta-phi, summed over all radial layers.
 *  A tower contains all cells within certain eta and phi (tower size: '\b deltaEtaTower', '\b deltaPhiTower').
 *  Distance in r plays no role, however `\b radiusForPosition` needs to be defined
 *  (e.g. to inner radius of the detector) for the cluster position calculation. By default the radius is equal to 1.
 *
 *  For more explanation please [see reconstruction documentation](@ref md_reconstruction_doc_reccalorimeter).
 *
 *  @author Anna Zaborowska
 *  @author Jana Faltova
 */

class CaloTowerTool : public AlgTool, virtual public ITowerTool {
public:
  CaloTowerTool(const std::string& type, const std::string& name, const IInterface* parent);
  virtual ~CaloTowerTool() = default;
  /**  Initialize.
   *   @return status code
   */
  virtual StatusCode initialize() final;
  /**  Finalize.
   *   @return status code
   */
  virtual StatusCode finalize() final;
  /**  Find number of calorimeter towers.
   *   Number of towers in phi is calculated from full azimuthal angle (2 pi) and the size of tower in phi ('\b
   * deltaPhiTower').
   *   Number of towers in eta is calculated from maximum detector eta ('\b etaMax`) and the size of tower in eta ('\b
   * deltaEtaTower').
   *   @param[out] nEta Number of towers in eta.
   *   @param[out] nPhi Number of towers in phi.
   */
  virtual void towersNumber(int& nEta, int& nPhi) final;
  /**  Build calorimeter towers.
   *   Tower is defined by a segment in eta and phi, with the energy from all layers (no r segmentation).
   *   @param[out] aTowers Calorimeter towers.
   *   @param[in] fillTowersCells Whether to fill maps of cells into towers, for later use in attachCells
   *   @return Size of the cell collection.
   */
  virtual uint buildTowers(std::vector<std::vector<float>>& aTowers, bool fillTowersCells = true) final;

  /**  Get the radius for the position calculation.
   *   @return Radius
   */
  virtual float radiusForPosition() const final;
  /**  Get the tower IDs in eta.
   *   @param[in] aEta Position of the calorimeter cell in eta
   *   @return ID (eta) of a tower
   */
  virtual uint idEta(float aEta) const final;
  /**  Get the tower IDs in phi.
   *   @param[in] aPhi Position of the calorimeter cell in phi
   *   @return ID (phi) of a tower
   */
  virtual uint idPhi(float aPhi) const final;
  /**  Get the eta position of the centre of the tower.
   *   @param[in] aIdEta ID (eta) of a tower
   *   @return Position of the centre of the tower
   */
  virtual float eta(int aIdEta) const final;
  /**  Get the phi position of the centre of the tower.
   *   @param[in] aIdPhi ID (phi) of a tower
   *   @return Position of the centre of the tower
   */
  virtual float phi(int aIdPhi) const final;
  /**  Find cells belonging to a cluster.
   *   @param[in] aEta Position of the middle tower of a cluster in eta
   *   @param[in] aPhi Position of the middle tower of a cluster in phi
   *   @param[in] aHalfEtaFinal Half size of cluster in eta (in units of tower size). Cluster size is 2*aHalfEtaFinal+1
   *   @param[in] aHalfPhiFinal Half size of cluster in phi (in units of tower size). Cluster size is 2*aHalfPhiFinal+1
   *   @param[out] aEdmCluster Cluster where cells are attached to
   */
  virtual void attachCells(float aEta, float aPhi, uint aHalfEtaFinal, uint aHalfPhiFinal,
                           edm4hep::MutableCluster& aEdmCluster, edm4hep::CalorimeterHitCollection* aEdmClusterCells,
                           bool aEllipse = false) final;

private:
  /// Type of the segmentation
  enum class SegmentationType { kWrong, kPhiEta, kMulti };
  /**  Correct way to access the neighbour of the phi tower, taking into account
   * the full coverage in phi.
   *   Full coverage means that first tower in phi, with ID = 0 is a direct
   * neighbour of the last tower in phi with ID = m_nPhiTower - 1).
   *   @param[in] aIPhi requested ID of a phi tower,
   *   may be < 0 or >=m_nPhiTower
   *   @return  ID of a tower - shifted and corrected
   * (in [0, m_nPhiTower) range)
   */
  uint phiNeighbour(int aIPhi) const;
  /**  This is where the cell info is filled into towers
   *   @param[in] aTowers Calorimeter towers.
   *   @param[in] aCells Calorimeter cells collection.
   *   @param[in] aSegmentation Segmentation of the calorimeter
   */
  void CellsIntoTowers(std::vector<std::vector<float>>& aTowers, const edm4hep::CalorimeterHitCollection* aCells,
                       dd4hep::DDSegmentation::Segmentation* aSegmentation, SegmentationType aType,
                       bool fillTowersCells);
  /**  Check if the readout name exists. If so, it returns the eta-phi segmentation.
   *   @param[in] aReadoutName Readout name to be retrieved
   */
  std::pair<double, double> retrievePhiEtaExtrema(dd4hep::DDSegmentation::Segmentation* aSegmentation,
                                                  SegmentationType aType);
  std::pair<dd4hep::DDSegmentation::Segmentation*, SegmentationType> retrieveSegmentation(std::string aReadoutName);
  /// Handle for electromagnetic barrel cells (input collection)
  mutable k4FWCore::DataHandle<edm4hep::CalorimeterHitCollection> m_ecalBarrelCells{"ecalBarrelCells",
                                                                                    Gaudi::DataHandle::Reader, this};
  /// Handle for ecal endcap calorimeter cells (input collection)
  mutable k4FWCore::DataHandle<edm4hep::CalorimeterHitCollection> m_ecalEndcapCells{"ecalEndcapCells",
                                                                                    Gaudi::DataHandle::Reader, this};
  /// Handle for ecal forward calorimeter cells (input collection)
  mutable k4FWCore::DataHandle<edm4hep::CalorimeterHitCollection> m_ecalFwdCells{"ecalFwdCells",
                                                                                 Gaudi::DataHandle::Reader, this};
  /// Handle for hadronic barrel cells (input collection)
  mutable k4FWCore::DataHandle<edm4hep::CalorimeterHitCollection> m_hcalBarrelCells{"hcalBarrelCells",
                                                                                    Gaudi::DataHandle::Reader, this};
  /// Handle for hadronic extended barrel cells (input collection)
  mutable k4FWCore::DataHandle<edm4hep::CalorimeterHitCollection> m_hcalExtBarrelCells{"hcalExtBarrelCells",
                                                                                       Gaudi::DataHandle::Reader, this};
  /// Handle for hcal endcap calorimeter cells (input collection)
  mutable k4FWCore::DataHandle<edm4hep::CalorimeterHitCollection> m_hcalEndcapCells{"hcalEndcapCells",
                                                                                    Gaudi::DataHandle::Reader, this};
  /// Handle for hcal forward calorimeter cells (input collection)
  mutable k4FWCore::DataHandle<edm4hep::CalorimeterHitCollection> m_hcalFwdCells{"hcalFwdCells",
                                                                                 Gaudi::DataHandle::Reader, this};
  /// Pointer to the geometry service
  ServiceHandle<IGeoSvc> m_geoSvc;
  /// Name of the electromagnetic barrel readout
  Gaudi::Property<std::string> m_ecalBarrelReadoutName{this, "ecalBarrelReadoutName", "",
                                                       "name of the ecal barrel readout"};
  /// Name of the ecal endcap calorimeter readout
  Gaudi::Property<std::string> m_ecalEndcapReadoutName{this, "ecalEndcapReadoutName", "",
                                                       "name of the ecal endcap readout"};
  /// Name of the ecal forward calorimeter readout
  Gaudi::Property<std::string> m_ecalFwdReadoutName{this, "ecalFwdReadoutName", "", "name of the ecal fwd readout"};
  /// Name of the hadronic barrel readout
  Gaudi::Property<std::string> m_hcalBarrelReadoutName{this, "hcalBarrelReadoutName", "",
                                                       "name of the hcal barrel readout"};
  /// Name of the hadronic extended barrel readout
  Gaudi::Property<std::string> m_hcalExtBarrelReadoutName{this, "hcalExtBarrelReadoutName", "",
                                                          "name of the hcal extended barrel readout"};
  /// Name of the hcal endcap calorimeter readout
  Gaudi::Property<std::string> m_hcalEndcapReadoutName{this, "hcalEndcapReadoutName", "",
                                                       "name of the hcal endcap readout"};
  /// Name of the hcal forward calorimeter readout
  Gaudi::Property<std::string> m_hcalFwdReadoutName{this, "hcalFwdReadoutName", "", "name of the hcal fwd readout"};
  /// PhiEta segmentation of the electromagnetic barrel (owned by DD4hep)
  dd4hep::DDSegmentation::Segmentation* m_ecalBarrelSegmentation;
  /// PhiEta segmentation of the ecal endcap calorimeter (owned by DD4hep)
  dd4hep::DDSegmentation::Segmentation* m_ecalEndcapSegmentation;
  /// PhiEta segmentation of the ecal forward calorimeter (owned by DD4hep)
  dd4hep::DDSegmentation::Segmentation* m_ecalFwdSegmentation;
  /// PhiEta segmentation of the hadronic barrel (owned by DD4hep)
  dd4hep::DDSegmentation::Segmentation* m_hcalBarrelSegmentation;
  /// PhiEta segmentation of the hadronic extended barrel (owned by DD4hep)
  dd4hep::DDSegmentation::Segmentation* m_hcalExtBarrelSegmentation;
  /// PhiEta segmentation of the hcal endcap calorimeter (owned by DD4hep)
  dd4hep::DDSegmentation::Segmentation* m_hcalEndcapSegmentation;
  /// PhiEta segmentation of the hcal forward calorimeter (owned by DD4hep)
  dd4hep::DDSegmentation::Segmentation* m_hcalFwdSegmentation;
  /// Type of segmentation of the electromagnetic barrel
  SegmentationType m_ecalBarrelSegmentationType;
  /// Type of segmentation of the ecal endcap calorimeter
  SegmentationType m_ecalEndcapSegmentationType;
  /// Type of segmentation of the ecal forward calorimeter
  SegmentationType m_ecalFwdSegmentationType;
  /// Type of segmentation of the hadronic barrel
  SegmentationType m_hcalBarrelSegmentationType;
  /// Type of segmentation of the hadronic extended barrel
  SegmentationType m_hcalExtBarrelSegmentationType;
  /// Type of segmentation of the hcal endcap calorimeter
  SegmentationType m_hcalEndcapSegmentationType;
  /// Type of segmentation of the hcal forward calorimeter
  SegmentationType m_hcalFwdSegmentationType;
  /// decoder: only for barrel
  dd4hep::DDSegmentation::BitFieldCoder* m_decoder;
  /// Radius used to calculate cluster position from eta and phi (in mm)
  Gaudi::Property<double> m_radius{this, "radiusForPosition", 1.0,
                                   "Radius used to calculate cluster position from eta and phi (in mm)"};
  /// Maximum eta of detector
  float m_etaMax;
  /// Maximum phi of the detector
  float m_phiMax;
  /// Size of the tower in eta
  Gaudi::Property<float> m_deltaEtaTower{this, "deltaEtaTower", 0.01, "Size of the tower in eta"};
  /// Size of the tower in phi
  Gaudi::Property<float> m_deltaPhiTower{this, "deltaPhiTower", 0.01, "Size of the tower in phi"};
  /// number of towers in eta (calculated from m_deltaEtaTower and m_etaMax)
  int m_nEtaTower;
  /// Number of towers in phi (calculated from m_deltaPhiTower)
  int m_nPhiTower;
  /// map to cells contained within a tower so they can be attached to a reconstructed cluster (note that fraction of
  /// their energy assigned to a cluster is not acknowledged)
  std::map<std::pair<uint, uint>, std::vector<edm4hep::CalorimeterHit>> m_cellsInTowers;
  /// Use only a part of the calorimeter (in depth)
  Gaudi::Property<bool> m_useHalfTower{this, "halfTower", false, "Use half tower"};
  Gaudi::Property<uint> m_max_layer{
      this, "max_layer", 6,
      "Specify which radial layer are used. The condition is 'if(cellLayer > m_max_layer) skip this cell'."};
};

#endif /* RECCALORIMETER_CALOTOWERTOOL_H */
