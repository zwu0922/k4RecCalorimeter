#ifndef RECCALORIMETER_LAYEREDCALOTOWERTOOL_H
#define RECCALORIMETER_LAYEREDCALOTOWERTOOL_H

// Gaudi
#include "GaudiKernel/AlgTool.h"

// k4geo
#include "detectorSegmentations/FCCSWGridPhiEta_k4geo.h"

// k4FWCore
#include "k4FWCore/DataHandle.h"
#include "k4Interface/ITowerTool.h"
class IGeoSvc;

// edm4hep
namespace edm4hep {
class CalorimeterHitCollection;
class Cluster;
class MutableCluster;
} // namespace edm4hep

// DD4hep
namespace dd4hep {
namespace DDSegmentation {
  class Segmentation;
}
} // namespace dd4hep

/** @class LayeredCaloTowerTool
 * Reconstruction/RecCalorimeter/src/components/LayeredCaloTowerTool.h
 * LayeredCaloTowerTool.h
 *
 *  Tool building the calorimeter towers for the sliding window algorithm.
 *  Towers are built of cells in eta-phi, summed over all radial layers.
 *  A tower contains all cells within certain eta and phi (tower size: '\b
 * deltaEtaTower', '\b deltaPhiTower').
 *  Distance in r plays no role, however `\b radiusForPosition` needs to be
 *  defined (e.g. to inner radius of the detector) for the cluster position
 *  calculation. By default the radius is equal to 1.
 *
 *  This tool creates towers from a single cell collection (from one
 * calorimeter).
 *
 *  It will only consider cells within the defined layers of the calorimeter, if the layers are defined by 'layer'
 * bitfield. By default it uses 0 to 130th layer.
 *
 *  For more explanation please [see reconstruction documentation](@ref
 * md_reconstruction_doc_reccalorimeter).
 *
 *  @author Anna Zaborowska
 *  @author Jana Faltova
 */

class LayeredCaloTowerTool : public AlgTool, virtual public ITowerTool {
public:
  LayeredCaloTowerTool(const std::string& type, const std::string& name, const IInterface* parent);
  virtual ~LayeredCaloTowerTool() = default;
  /**  Initialize.
   *   @return status code
   */
  virtual StatusCode initialize() final;
  /**  Finalize.
   *   @return status code
   */
  virtual StatusCode finalize() final;
  /**  Find number of calorimeter towers.
   *   Number of towers in phi is calculated from full azimuthal angle (2 pi)
   * and the size of tower in phi ('\b deltaPhiTower').
   *   Number of towers in eta is calculated from maximum detector eta ('\b
   * etaMax`) and the size of tower in eta ('\b deltaEtaTower').
   *   @param[out] nEta number of towers in eta.
   *   @param[out] nPhi number of towers in phi.
   */
  virtual void towersNumber(int& nEta, int& nPhi) final;
  /**  Build calorimeter towers.
   *   Tower is segmented in eta and phi, with the energy from all layers
   *   (no segmentation).
   *   @param[out] aTowers Calorimeter towers.
   *   @return Size of the cell collection.
   */
  virtual uint buildTowers(std::vector<std::vector<float>>& aTowers, bool fillTowerCells = true) final;
  /**  Get the radius (in mm) for the position calculation.
   *   Reconstructed cluster has eta and phi position, without the radial
   * coordinate. The cluster in EDM contains
   * Cartesian position, so the radial position (e.g. the inner radius of the
   * calorimeter) needs to be specified. By default it is equal to 1.
   *   @return Radius
   */
  virtual float radiusForPosition() const final;
  /**  Get the tower IDs in eta.
   *   @param[in] aEta Position of the calorimeter cell in eta
   *   @return ID (eta) of a tower
   */
  virtual uint idEta(float aEta) const final;
  /**  Get the tower IDs in phi.
   *   Tower IDs are shifted so they start at 0 (middle of cell with ID=0 is
   * phi=0, phi is defined form -pi to pi). No
   * segmentation offset is taken into account.
   *   @param[in] aPhi Position of the calorimeter cell in phi
   *   @return ID (phi) of a tower
   */
  virtual uint idPhi(float aPhi) const final;
  /**  Get the eta position of the centre of the tower.
   *   Tower IDs are shifted so they start at 0 (middle of cell with ID=0 is
   * eta=0). No segmentation offset is taken into account.
   *   @param[in] aIdEta ID (eta) of a tower
   *   @return Position of the centre of the tower
   */
  virtual float eta(int aIdEta) const final;
  /**  Get the phi position of the centre of the tower.
   *   @param[in] aIdPhi ID (phi) of a tower
   *   @return Position of the centre of the tower
   */
  virtual float phi(int aIdPhi) const final;
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
  std::shared_ptr<dd4hep::DDSegmentation::BitFieldCoder> m_decoder;

private:
  /// Handle for calo cells (input collection)
  mutable k4FWCore::DataHandle<edm4hep::CalorimeterHitCollection> m_cells{"calo/cells", Gaudi::DataHandle::Reader,
                                                                          this};
  /// Pointer to the geometry service
  ServiceHandle<IGeoSvc> m_geoSvc;
  /// Name of the detector readout
  Gaudi::Property<std::string> m_readoutName{this, "readoutName", "", "Name of the detector readout"};
  /// PhiEta segmentation (owned by DD4hep)
  dd4hep::DDSegmentation::FCCSWGridPhiEta_k4geo* m_segmentation;
  /// Radius used to calculate cluster position from eta and phi (in mm)
  Gaudi::Property<double> m_radius{this, "radiusForPosition", 1.0,
                                   "Radius used to calculate cluster position from eta and phi (in mm)"};
  /// Maximum eta of the detector
  float m_etaMax;
  /// Maximum phi of the detector
  float m_phiMax;

  /// Size of the tower in eta
  Gaudi::Property<float> m_deltaEtaTower{this, "deltaEtaTower", 0.01, "Size of the tower in eta"};
  /// Size of the tower in phi
  Gaudi::Property<float> m_deltaPhiTower{this, "deltaPhiTower", 0.01, "Size of the tower in phi"};
  // Minimum layer (0 = first layer)
  Gaudi::Property<float> m_minimumLayer{this, "minimumLayer", 0, "Minimum cell layer"};
  // Maximum layer (130 = last layer in ECalBarrel inclined)
  Gaudi::Property<float> m_maximumLayer{this, "maximumLayer", 130, "Maximum cell layer"};
  // Restrict tower building in layers if bitfield: layer exists

  Gaudi::Property<float> m_addLayerRestriction{this, "addLayerRestriction", true, "set layer restriction on/off"};
  /// number of towers in eta (calculated from m_deltaEtaTower and m_etaMax)
  int m_nEtaTower;
  /// Number of towers in phi (calculated from m_deltaPhiTower)
  int m_nPhiTower;
};

#endif /* RECCALORIMETER_LAYEREDCALOTOWERTOOL_H */
