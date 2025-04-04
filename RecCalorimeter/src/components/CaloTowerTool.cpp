#include "CaloTowerTool.h"

// k4FWCore
#include "k4Interface/IGeoSvc.h"

// edm4hep
#include "edm4hep/CalorimeterHitCollection.h"
#include "edm4hep/Cluster.h"
#include "edm4hep/MutableCluster.h"

// DD4hep
#include "DD4hep/Detector.h"
#include "DD4hep/Readout.h"

DECLARE_COMPONENT(CaloTowerTool)

CaloTowerTool::CaloTowerTool(const std::string& type, const std::string& name, const IInterface* parent)
    : AlgTool(type, name, parent), m_geoSvc("GeoSvc", name) {
  declareProperty("ecalBarrelCells", m_ecalBarrelCells, "");
  declareProperty("ecalEndcapCells", m_ecalEndcapCells, "");
  declareProperty("ecalFwdCells", m_ecalFwdCells, "");
  declareProperty("hcalBarrelCells", m_hcalBarrelCells, "");
  declareProperty("hcalExtBarrelCells", m_hcalExtBarrelCells, "");
  declareProperty("hcalEndcapCells", m_hcalEndcapCells, "");
  declareProperty("hcalFwdCells", m_hcalFwdCells, "");
  declareInterface<ITowerTool>(this);
}

StatusCode CaloTowerTool::initialize() {
  if (AlgTool::initialize().isFailure()) {
    return StatusCode::FAILURE;
  }

  if (!m_geoSvc) {
    error() << "Unable to locate Geometry Service. "
            << "Make sure you have GeoSvc and SimSvc in the right order in the configuration." << endmsg;
    return StatusCode::FAILURE;
  }

  // check if readouts exist & retrieve PhiEta segmentations
  // if readout does not exist, reconstruction without this calorimeter part will be performed
  std::pair<dd4hep::DDSegmentation::Segmentation*, SegmentationType> tmpPair;
  info() << "Retrieving Ecal barrel segmentation" << endmsg;
  tmpPair = retrieveSegmentation(m_ecalBarrelReadoutName);
  m_ecalBarrelSegmentation = tmpPair.first;
  m_ecalBarrelSegmentationType = tmpPair.second;
  if (tmpPair.first != nullptr && tmpPair.second == SegmentationType::kWrong) {
    error() << "Wrong type of segmentation" << endmsg;
    return StatusCode::FAILURE;
  }
  if (m_useHalfTower) {
    m_decoder = m_geoSvc->getDetector()->readout(m_ecalBarrelReadoutName).idSpec().decoder();
  }
  info() << "Retrieving Ecal endcap segmentation" << endmsg;
  tmpPair = retrieveSegmentation(m_ecalEndcapReadoutName);
  m_ecalEndcapSegmentation = tmpPair.first;
  m_ecalEndcapSegmentationType = tmpPair.second;
  if (tmpPair.first != nullptr && tmpPair.second == SegmentationType::kWrong) {
    error() << "Wrong type of segmentation" << endmsg;
    return StatusCode::FAILURE;
  }
  info() << "Retrieving Ecal forward segmentation" << endmsg;
  tmpPair = retrieveSegmentation(m_ecalFwdReadoutName);
  m_ecalFwdSegmentation = tmpPair.first;
  m_ecalFwdSegmentationType = tmpPair.second;
  if (tmpPair.first != nullptr && tmpPair.second == SegmentationType::kWrong) {
    error() << "Wrong type of segmentation" << endmsg;
    return StatusCode::FAILURE;
  }
  info() << "Retrieving Hcal barrel segmentation" << endmsg;
  tmpPair = retrieveSegmentation(m_hcalBarrelReadoutName);
  m_hcalBarrelSegmentation = tmpPair.first;
  m_hcalBarrelSegmentationType = tmpPair.second;
  if (tmpPair.first != nullptr && tmpPair.second == SegmentationType::kWrong) {
    error() << "Wrong type of segmentation" << endmsg;
    return StatusCode::FAILURE;
  }
  info() << "Retrieving Hcal extended barrel segmentation" << endmsg;
  tmpPair = retrieveSegmentation(m_hcalExtBarrelReadoutName);
  m_hcalExtBarrelSegmentation = tmpPair.first;
  m_hcalExtBarrelSegmentationType = tmpPair.second;
  if (tmpPair.first != nullptr && tmpPair.second == SegmentationType::kWrong) {
    error() << "Wrong type of segmentation" << endmsg;
    return StatusCode::FAILURE;
  }
  info() << "Retrieving Hcal endcap segmentation" << endmsg;
  tmpPair = retrieveSegmentation(m_hcalEndcapReadoutName);
  m_hcalEndcapSegmentation = tmpPair.first;
  m_hcalEndcapSegmentationType = tmpPair.second;
  if (tmpPair.first != nullptr && tmpPair.second == SegmentationType::kWrong) {
    error() << "Wrong type of segmentation" << endmsg;
    return StatusCode::FAILURE;
  }
  info() << "Retrieving Hcal forward segmentation" << endmsg;
  tmpPair = retrieveSegmentation(m_hcalFwdReadoutName);
  m_hcalFwdSegmentation = tmpPair.first;
  m_hcalFwdSegmentationType = tmpPair.second;
  if (tmpPair.first != nullptr && tmpPair.second == SegmentationType::kWrong) {
    error() << "Wrong type of segmentation" << endmsg;
    return StatusCode::FAILURE;
  }

  return StatusCode::SUCCESS;
}

StatusCode CaloTowerTool::finalize() {
  for (auto& towerInMap : m_cellsInTowers) {
    towerInMap.second.clear();
  }
  return AlgTool::finalize();
}

std::pair<double, double> CaloTowerTool::retrievePhiEtaExtrema(dd4hep::DDSegmentation::Segmentation* aSegmentation,
                                                               SegmentationType aType) {
  double phiMax = -1;
  double etaMax = -1;
  dd4hep::DDSegmentation::FCCSWGridPhiEta_k4geo* segmentation = nullptr;
  if (aSegmentation != nullptr) {
    switch (aType) {
    case SegmentationType::kPhiEta: {
      info() << "== Retrieving phi-eta segmentation " << aSegmentation->name() << endmsg;
      segmentation = dynamic_cast<dd4hep::DDSegmentation::FCCSWGridPhiEta_k4geo*>(aSegmentation);
      phiMax = fabs(segmentation->offsetPhi()) + M_PI / (double)segmentation->phiBins();
      etaMax = fabs(segmentation->offsetEta()) + segmentation->gridSizeEta() * 0.5;
      break;
    }
    case SegmentationType::kMulti: {
      double phi = -1;
      double eta = -1;
      info() << "== Retrieving multi segmentation " << aSegmentation->name() << endmsg;
      for (const auto& subSegm :
           dynamic_cast<dd4hep::DDSegmentation::MultiSegmentation*>(aSegmentation)->subSegmentations()) {
        segmentation = dynamic_cast<dd4hep::DDSegmentation::FCCSWGridPhiEta_k4geo*>(subSegm.segmentation);
        phi = fabs(segmentation->offsetPhi()) + M_PI / (double)segmentation->phiBins();
        eta = fabs(segmentation->offsetEta()) + segmentation->gridSizeEta() * 0.5;
        if (eta > etaMax) {
          etaMax = eta;
        }
        if (phi > phiMax) {
          phiMax = phi;
        }
      }
      break;
    }
    case SegmentationType::kWrong: {
      info() << "== Retrieving WRONG segmentation" << endmsg;
      phiMax = -1;
      etaMax = -1;
      break;
    }
    }
  }
  return std::make_pair(phiMax, etaMax);
}

void CaloTowerTool::towersNumber(int& nEta, int& nPhi) {

  std::vector<double> listPhiMax;
  std::vector<double> listEtaMax;
  listPhiMax.reserve(7);
  listEtaMax.reserve(7);

  std::pair<double, double> tmpPair;
  tmpPair = retrievePhiEtaExtrema(m_ecalBarrelSegmentation, m_ecalBarrelSegmentationType);
  listPhiMax.push_back(tmpPair.first);
  listEtaMax.push_back(tmpPair.second);
  tmpPair = retrievePhiEtaExtrema(m_ecalEndcapSegmentation, m_ecalEndcapSegmentationType);
  listPhiMax.push_back(tmpPair.first);
  listEtaMax.push_back(tmpPair.second);
  tmpPair = retrievePhiEtaExtrema(m_ecalFwdSegmentation, m_ecalFwdSegmentationType);
  listPhiMax.push_back(tmpPair.first);
  listEtaMax.push_back(tmpPair.second);
  tmpPair = retrievePhiEtaExtrema(m_hcalBarrelSegmentation, m_hcalBarrelSegmentationType);
  listPhiMax.push_back(tmpPair.first);
  listEtaMax.push_back(tmpPair.second);
  tmpPair = retrievePhiEtaExtrema(m_hcalExtBarrelSegmentation, m_hcalExtBarrelSegmentationType);
  listPhiMax.push_back(tmpPair.first);
  listEtaMax.push_back(tmpPair.second);
  tmpPair = retrievePhiEtaExtrema(m_hcalEndcapSegmentation, m_hcalEndcapSegmentationType);
  listPhiMax.push_back(tmpPair.first);
  listEtaMax.push_back(tmpPair.second);
  tmpPair = retrievePhiEtaExtrema(m_hcalFwdSegmentation, m_hcalFwdSegmentationType);
  listPhiMax.push_back(tmpPair.first);
  listEtaMax.push_back(tmpPair.second);

  // Maximum eta & phi of the calorimeter system
  m_phiMax = *std::max_element(listPhiMax.begin(), listPhiMax.end());
  m_etaMax = *std::max_element(listEtaMax.begin(), listEtaMax.end());
  debug() << "Detector limits: phiMax " << m_phiMax << " etaMax " << m_etaMax << endmsg;

  // very small number (epsilon) substructed from the edges to ensure correct division
  float epsilon = 0.0001;
  // number of phi bins
  m_nPhiTower = ceil(2 * (m_phiMax - epsilon) / m_deltaPhiTower);
  // number of eta bins (if eta maximum is defined)
  m_nEtaTower = ceil(2 * (m_etaMax - epsilon) / m_deltaEtaTower);
  debug() << "Towers: etaMax " << m_etaMax << ", deltaEtaTower " << m_deltaEtaTower << ", nEtaTower " << m_nEtaTower
          << endmsg;
  debug() << "Towers: phiMax " << m_phiMax << ", deltaPhiTower " << m_deltaPhiTower << ", nPhiTower " << m_nPhiTower
          << endmsg;

  nEta = m_nEtaTower;
  nPhi = m_nPhiTower;
}

uint CaloTowerTool::buildTowers(std::vector<std::vector<float>>& aTowers, bool fillTowersCells) {
  uint totalNumberOfCells = 0;
  for (auto& towerInMap : m_cellsInTowers) {
    towerInMap.second.clear();
  }
  // 1. ECAL barrel
  // Get the input collection with calorimeter cells
  const edm4hep::CalorimeterHitCollection* ecalBarrelCells = m_ecalBarrelCells.get();
  debug() << "Input Ecal barrel cell collection size: " << ecalBarrelCells->size() << endmsg;
  // Loop over a collection of calorimeter cells and build calo towers
  if (m_ecalBarrelSegmentation != nullptr) {
    CellsIntoTowers(aTowers, ecalBarrelCells, m_ecalBarrelSegmentation, m_ecalBarrelSegmentationType, fillTowersCells);
    totalNumberOfCells += ecalBarrelCells->size();
  }

  // 2. ECAL endcap calorimeter
  const edm4hep::CalorimeterHitCollection* ecalEndcapCells = m_ecalEndcapCells.get();
  debug() << "Input Ecal endcap cell collection size: " << ecalEndcapCells->size() << endmsg;
  // Loop over a collection of calorimeter cells and build calo towers
  if (m_ecalEndcapSegmentation != nullptr) {
    CellsIntoTowers(aTowers, ecalEndcapCells, m_ecalEndcapSegmentation, m_ecalEndcapSegmentationType, fillTowersCells);
    totalNumberOfCells += ecalEndcapCells->size();
  }

  // 3. ECAL forward calorimeter
  const edm4hep::CalorimeterHitCollection* ecalFwdCells = m_ecalFwdCells.get();
  debug() << "Input Ecal forward cell collection size: " << ecalFwdCells->size() << endmsg;
  // Loop over a collection of calorimeter cells and build calo towers
  if (m_ecalFwdSegmentation != nullptr) {
    CellsIntoTowers(aTowers, ecalFwdCells, m_ecalFwdSegmentation, m_ecalFwdSegmentationType, fillTowersCells);
    totalNumberOfCells += ecalFwdCells->size();
  }

  // 4. HCAL barrel
  const edm4hep::CalorimeterHitCollection* hcalBarrelCells = m_hcalBarrelCells.get();
  debug() << "Input hadronic barrel cell collection size: " << hcalBarrelCells->size() << endmsg;
  // Loop over a collection of calorimeter cells and build calo towers
  if (m_hcalBarrelSegmentation != nullptr) {
    CellsIntoTowers(aTowers, hcalBarrelCells, m_hcalBarrelSegmentation, m_hcalBarrelSegmentationType, fillTowersCells);
    totalNumberOfCells += hcalBarrelCells->size();
  }

  // 5. HCAL extended barrel
  const edm4hep::CalorimeterHitCollection* hcalExtBarrelCells = m_hcalExtBarrelCells.get();
  debug() << "Input hadronic extended barrel cell collection size: " << hcalExtBarrelCells->size() << endmsg;
  // Loop over a collection of calorimeter cells and build calo towers
  if (m_hcalExtBarrelSegmentation != nullptr) {
    CellsIntoTowers(aTowers, hcalExtBarrelCells, m_hcalExtBarrelSegmentation, m_hcalExtBarrelSegmentationType,
                    fillTowersCells);
    totalNumberOfCells += hcalExtBarrelCells->size();
  }

  // 6. HCAL endcap calorimeter
  const edm4hep::CalorimeterHitCollection* hcalEndcapCells = m_hcalEndcapCells.get();
  debug() << "Input Hcal endcap cell collection size: " << hcalEndcapCells->size() << endmsg;
  // Loop over a collection of calorimeter cells and build calo towers
  if (m_hcalEndcapSegmentation != nullptr) {
    CellsIntoTowers(aTowers, hcalEndcapCells, m_hcalEndcapSegmentation, m_hcalEndcapSegmentationType, fillTowersCells);
    totalNumberOfCells += hcalEndcapCells->size();
  }

  // 7. HCAL forward calorimeter
  const edm4hep::CalorimeterHitCollection* hcalFwdCells = m_hcalFwdCells.get();
  debug() << "Input Hcal forward cell collection size: " << hcalFwdCells->size() << endmsg;
  // Loop over a collection of calorimeter cells and build calo towers
  if (m_hcalFwdSegmentation != nullptr) {
    CellsIntoTowers(aTowers, hcalFwdCells, m_hcalFwdSegmentation, m_hcalFwdSegmentationType, fillTowersCells);
    totalNumberOfCells += hcalFwdCells->size();
  }
  return totalNumberOfCells;
}

uint CaloTowerTool::idEta(float aEta) const {
  uint id = floor((aEta + m_etaMax) / m_deltaEtaTower);
  return id;
}

uint CaloTowerTool::idPhi(float aPhi) const {
  uint id = floor((aPhi + m_phiMax) / m_deltaPhiTower);
  return id;
}

float CaloTowerTool::eta(int aIdEta) const {
  // middle of the tower
  return ((aIdEta + 0.5) * m_deltaEtaTower - m_etaMax);
}

float CaloTowerTool::phi(int aIdPhi) const {
  // middle of the tower
  return ((aIdPhi + 0.5) * m_deltaPhiTower - m_phiMax);
}

uint CaloTowerTool::phiNeighbour(int aIPhi) const {
  if (aIPhi < 0) {
    return m_nPhiTower + aIPhi;
  } else if (aIPhi >= m_nPhiTower) {
    return aIPhi % m_nPhiTower;
  }
  return aIPhi;
}

float CaloTowerTool::radiusForPosition() const { return m_radius; }

void CaloTowerTool::CellsIntoTowers(std::vector<std::vector<float>>& aTowers,
                                    const edm4hep::CalorimeterHitCollection* aCells,
                                    dd4hep::DDSegmentation::Segmentation* aSegmentation, SegmentationType aType,
                                    bool fillTowersCells) {
  // Loop over a collection of calorimeter cells and build calo towers
  // borders of the cell in eta/phi
  float etaCellMin = 0, etaCellMax = 0;
  float phiCellMin = 0, phiCellMax = 0;
  // tower index of the borders of the cell
  int iPhiMin = 0, iPhiMax = 0;
  int iEtaMin = 0, iEtaMax = 0;
  // fraction of cell area in eta/phi belonging to towers
  // Min - first tower, Max - last tower, Middle - middle tower(s)
  // If cell size <= tower size => first == last == middle tower, all fractions = 1
  // cell size > tower size => Sum of fractions = 1
  float ratioEta = 1.0, ratioPhi = 1.0;
  float fracEtaMin = 1.0, fracEtaMax = 1.0, fracEtaMiddle = 1.0;
  float fracPhiMin = 1.0, fracPhiMax = 1.0, fracPhiMiddle = 1.0;
  float epsilon = 0.0001;
  const dd4hep::DDSegmentation::FCCSWGridPhiEta_k4geo* segmentation = nullptr;
  const dd4hep::DDSegmentation::MultiSegmentation* multisegmentation = nullptr;
  if (aType == SegmentationType::kPhiEta) {
    segmentation = dynamic_cast<const dd4hep::DDSegmentation::FCCSWGridPhiEta_k4geo*>(aSegmentation);
  } else if (aType == SegmentationType::kMulti) {
    multisegmentation = dynamic_cast<const dd4hep::DDSegmentation::MultiSegmentation*>(aSegmentation);
  }
  bool pass = true;
  for (const auto& cell : *aCells) {
    pass = true;
    // if multisegmentation is used - first find out which segmentation to use
    if (aType == SegmentationType::kMulti) {
      segmentation = dynamic_cast<const dd4hep::DDSegmentation::FCCSWGridPhiEta_k4geo*>(
          &multisegmentation->subsegmentation(cell.getCellID()));
    }
    if (m_useHalfTower) {
      uint layerId = m_decoder->get(cell.getCellID(), "layer");
      if (layerId > m_max_layer) {
        pass = false;
      }
    }
    if (pass) {
      // find to which tower(s) the cell belongs
      float cellEta = segmentation->eta(cell.getCellID());
      float cellPhi = segmentation->phi(cell.getCellID());
      etaCellMin = cellEta - segmentation->gridSizeEta() * 0.5;
      etaCellMax = cellEta + segmentation->gridSizeEta() * 0.5;
      phiCellMin = cellPhi - M_PI / (double)segmentation->phiBins();
      phiCellMax = cellPhi + M_PI / (double)segmentation->phiBins();
      iEtaMin = idEta(etaCellMin + epsilon);
      iPhiMin = idPhi(phiCellMin + epsilon);
      iEtaMax = idEta(etaCellMax - epsilon);
      iPhiMax = idPhi(phiCellMax - epsilon);
      // if a cell is larger than a tower in eta/phi, calculate the fraction of
      // the cell area belonging to the first/last/middle towers
      fracEtaMin = 1.0;
      fracEtaMax = 1.0;
      fracEtaMiddle = 1.0;
      if (iEtaMin != iEtaMax) {
        fracEtaMin = fabs(eta(iEtaMin) + 0.5 * m_deltaEtaTower - etaCellMin) / segmentation->gridSizeEta();
        fracEtaMax = fabs(etaCellMax - eta(iEtaMax) + 0.5 * m_deltaEtaTower) / segmentation->gridSizeEta();
        if ((iEtaMax - iEtaMin - 1) != 0) {
          fracEtaMiddle = (1 - fracEtaMin - fracEtaMax) / float(iEtaMax - iEtaMin - 1);
        } else {
          fracEtaMiddle = 0.0;
        }
      }
      fracPhiMin = 1.0;
      fracPhiMax = 1.0;
      fracPhiMiddle = 1.0;
      if (iPhiMin != iPhiMax) {
        fracPhiMin =
            fabs(phi(iPhiMin) + 0.5 * m_deltaPhiTower - phiCellMin) / (2 * M_PI / (double)segmentation->phiBins());
        fracPhiMax =
            fabs(phiCellMax - phi(iPhiMax) + 0.5 * m_deltaPhiTower) / (2 * M_PI / (double)segmentation->phiBins());
        if ((iPhiMax - iPhiMin - 1) != 0) {
          fracPhiMiddle = (1 - fracPhiMin - fracPhiMax) / float(iPhiMax - iPhiMin - 1);
        } else {
          fracPhiMiddle = 0.0;
        }
      }

      // Loop through the appropriate towers and add transverse energy
      for (auto iEta = iEtaMin; iEta <= iEtaMax; iEta++) {
        if (iEta == iEtaMin) {
          ratioEta = fracEtaMin;
        } else if (iEta == iEtaMax) {
          ratioEta = fracEtaMax;
        } else {
          ratioEta = fracEtaMiddle;
        }
        for (auto iPhi = iPhiMin; iPhi <= iPhiMax; iPhi++) {
          if (iPhi == iPhiMin) {
            ratioPhi = fracPhiMin;
          } else if (iPhi == iPhiMax) {
            ratioPhi = fracPhiMax;
          } else {
            ratioPhi = fracPhiMiddle;
          }
          aTowers[iEta][phiNeighbour(iPhi)] +=
              cell.getEnergy() / cosh(segmentation->eta(cell.getCellID())) * ratioEta * ratioPhi;
          if (fillTowersCells) {
            m_cellsInTowers[std::make_pair(iEta, phiNeighbour(iPhi))].push_back(cell.clone());
            if (fabs(etaCellMin) < 1.5 && m_cellsInTowers[std::make_pair(iEta, phiNeighbour(iPhi))].size() > 8)
              verbose() << "NUM CELLs IN TOWER : " << m_cellsInTowers[std::make_pair(iEta, phiNeighbour(iPhi))].size()
                        << endmsg;
          }
        }
      }
    }
  }
}

std::pair<dd4hep::DDSegmentation::Segmentation*, CaloTowerTool::SegmentationType>
CaloTowerTool::retrieveSegmentation(std::string aReadoutName) {
  dd4hep::DDSegmentation::Segmentation* segmentation = nullptr;
  if (m_geoSvc->getDetector()->readouts().find(aReadoutName) == m_geoSvc->getDetector()->readouts().end()) {
    info() << "Readout does not exist! Please check if it is correct. Processing without it." << endmsg;
  } else {
    info() << "Readout " << aReadoutName << " found." << endmsg;
    segmentation = dynamic_cast<dd4hep::DDSegmentation::FCCSWGridPhiEta_k4geo*>(
        m_geoSvc->getDetector()->readout(aReadoutName).segmentation().segmentation());
    if (segmentation == nullptr) {
      segmentation = dynamic_cast<dd4hep::DDSegmentation::MultiSegmentation*>(
          m_geoSvc->getDetector()->readout(aReadoutName).segmentation().segmentation());
      if (segmentation == nullptr) {
        error() << "There is no phi-eta or multi- segmentation for the readout " << aReadoutName << " defined."
                << endmsg;
      } else {
        // check if multisegmentation contains only phi-eta sub-segmentations
        dd4hep::DDSegmentation::Segmentation* subsegmentation = nullptr;
        for (const auto& subSegm :
             dynamic_cast<dd4hep::DDSegmentation::MultiSegmentation*>(segmentation)->subSegmentations()) {
          subsegmentation = dynamic_cast<dd4hep::DDSegmentation::FCCSWGridPhiEta_k4geo*>(subSegm.segmentation);
          if (subsegmentation == nullptr) {
            error() << "At least one of the sub-segmentations in MultiSegmentation named " << aReadoutName
                    << " is not a phi-eta grid." << endmsg;
            return std::make_pair(nullptr, SegmentationType::kWrong);
          }
        }
        return std::make_pair(segmentation, SegmentationType::kMulti);
      }
    } else {
      return std::make_pair(segmentation, SegmentationType::kPhiEta);
    }
  }
  return std::make_pair(segmentation, SegmentationType::kWrong);
}

void CaloTowerTool::attachCells(float eta, float phi, uint halfEtaFin, uint halfPhiFin,
                                edm4hep::MutableCluster& aEdmCluster,
                                edm4hep::CalorimeterHitCollection* aEdmClusterCells, bool aEllipse) {
  int etaId = idEta(eta);
  int phiId = idPhi(phi);
  std::vector<dd4hep::DDSegmentation::CellID> seen_cellIDs;
  if (aEllipse) {
    for (int iEta = etaId - halfEtaFin; iEta <= int(etaId + halfEtaFin); iEta++) {
      for (int iPhi = phiId - halfPhiFin; iPhi <= int(phiId + halfPhiFin); iPhi++) {
        if (pow((etaId - iEta) / (halfEtaFin + 0.5), 2) + pow((phiId - iPhi) / (halfPhiFin + 0.5), 2) < 1) {
          for (auto cell : m_cellsInTowers[std::make_pair(iEta, phiNeighbour(iPhi))]) {
            if (std::find(seen_cellIDs.begin(), seen_cellIDs.end(), cell.getCellID()) !=
                seen_cellIDs.end()) { // towers can be smaller than cells in which case a cell belongs to several towers
              continue;
            }
            seen_cellIDs.push_back(cell.getCellID());
            auto cellclone = cell.clone();
            aEdmClusterCells->push_back(cellclone);
            aEdmCluster.addToHits(cellclone);
          }
        }
      }
    }
  } else {
    for (int iEta = etaId - halfEtaFin; iEta <= int(etaId + halfEtaFin); iEta++) {
      for (int iPhi = phiId - halfPhiFin; iPhi <= int(phiId + halfPhiFin); iPhi++) {
        for (auto cell : m_cellsInTowers[std::make_pair(iEta, phiNeighbour(iPhi))]) {
          if (std::find(seen_cellIDs.begin(), seen_cellIDs.end(), cell.getCellID()) !=
              seen_cellIDs.end()) { // towers can be smaller than cells in which case a cell belongs to several towers
            continue;
          }
          seen_cellIDs.push_back(cell.getCellID());
          auto cellclone = cell.clone();
          aEdmClusterCells->push_back(cellclone);
          aEdmCluster.addToHits(cellclone);
        }
      }
    }
  }
  return;
}
