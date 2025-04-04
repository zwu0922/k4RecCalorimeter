#include "SplitClusters.h"

// Key4HEP
#include "k4Interface/IGeoSvc.h"

// FCC Detectors
#include "detectorCommon/DetUtils_k4geo.h"

// DD4hep
#include "DD4hep/Detector.h"

// ROOT
#include "TH1F.h"
#include "TH2F.h"
#include "TLorentzVector.h"
#include "TVector3.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

DECLARE_COMPONENT(SplitClusters)

SplitClusters::SplitClusters(const std::string& name, ISvcLocator* svcLoc) : Gaudi::Algorithm(name, svcLoc) {
  declareProperty("clusters", m_clusters, "Input clusters (input)");

  declareProperty("neigboursTool", m_neighboursTool, "Handle for tool to retrieve cell neighbours");

  declareProperty("outClusters", m_newClusters, "Output clusters (output)");
  declareProperty("outCells", m_newCells, "Output cells (output)");

  declareProperty("positionsECalBarrelTool", m_cellPositionsECalBarrelTool,
                  "Handle for tool to retrieve cell positions in ECal Barrel");
  declareProperty("positionsHCalBarrelTool", m_cellPositionsHCalBarrelTool,
                  "Handle for tool to retrieve cell positions in HCal Barrel");
  declareProperty("positionsHCalBarrelNoSegTool", m_cellPositionsHCalBarrelNoSegTool,
                  "Handle for tool to retrieve cell positions in HCal Barrel without DD4hep segmentation");
}

StatusCode SplitClusters::initialize() {
  StatusCode sc = Gaudi::Algorithm::initialize();
  if (sc.isFailure())
    return sc;
  m_geoSvc = service("GeoSvc");
  if (!m_geoSvc) {
    error() << "Unable to locate Geometry service." << endmsg;
    return StatusCode::FAILURE;
  }

  m_decoderECal = m_geoSvc->getDetector()->readout(m_readoutECal).idSpec().decoder();
  m_decoderHCal = m_geoSvc->getDetector()->readout(m_readoutHCal).idSpec().decoder();

  // Read neighbours map
  if (!m_neighboursTool.retrieve()) {
    error() << "Unable to retrieve the cells neighbours tool!!!" << endmsg;
    return StatusCode::FAILURE;
  }

  // Check if cell position ECal Barrel tool available
  if (!m_cellPositionsECalBarrelTool.retrieve()) {
    error() << "Unable to retrieve ECal Barrel cell positions tool!!!" << endmsg;
    return StatusCode::FAILURE;
  }
  // Check if cell position HCal Barrel tool available
  if (!m_cellPositionsHCalBarrelTool.retrieve()) {
    error() << "Unable to retrieve HCal Barrel cell positions tool!!!" << endmsg;
    if (!m_cellPositionsHCalBarrelNoSegTool.retrieve()) {
      error() << "Also unable to retrieve HCal Barrel no segmentation cell positions tool!!!" << endmsg;
      return StatusCode::FAILURE;
    }
  }

  info() << "SplitClusters initialized" << endmsg;

  return StatusCode::SUCCESS;
}

StatusCode SplitClusters::execute(const EventContext&) const {
  // Get the input collection with Geant4 hits
  const edm4hep::ClusterCollection* clusters = m_clusters.get();
  debug() << "Input Cluster collection size: " << clusters->size() << endmsg;
  // Output collections
  auto edmClusters = m_newClusters.createAndPut();
  edm4hep::CalorimeterHitCollection* edmClusterCells = new edm4hep::CalorimeterHitCollection();

  debug() << "Loop through " << clusters->size() << " clusters, " << endmsg;
  std::map<uint64_t, int> allCells;
  uint totSplitClusters = 0;
  uint totCellsBefore = 0;
  uint totCellsAfter = 0;
  double totEnergyBefore = 0.;
  double totEnergyAfter = 0.;

  for (auto cluster : *clusters) {
    // sanity checks
    totEnergyBefore += cluster.getEnergy();
    totCellsBefore += cluster.hits_size();

    std::map<uint64_t, int> cellsType;
    std::vector<std::pair<uint64_t, double>> cellsEnergy;
    std::map<uint64_t, TLorentzVector> cellsPosition;
    // used later for new cluster building
    std::map<uint, std::vector<std::pair<uint64_t, int>>> preClusterCollection;
    std::map<uint, TLorentzVector> clusterPositions;

    // number of new clusters
    uint newClusters = 0;
    std::vector<std::pair<uint64_t, double>> newSeeds;

    // Loop over cluster cells
    for (auto it = cluster.hits_begin(); it != cluster.hits_end(); ++it) {
      auto cell = *it;
      cellsType.emplace(cell.getCellID(), cell.getType());
      cellsEnergy.push_back(std::make_pair(cell.getCellID(), cell.getEnergy()));

      // get cell position by cellID
      // identify calo system
      auto systemId = m_decoder->get(cell.getCellID(), "system");
      dd4hep::Position posCell;
      if (systemId == 5) // ECAL BARREL system id
        posCell = m_cellPositionsECalBarrelTool->xyzPosition(cell.getCellID());
      else if (systemId == 8) { // HCAL BARREL system id
        if (m_noSegmentationHCalUsed)
          posCell = m_cellPositionsHCalBarrelNoSegTool->xyzPosition(cell.getCellID());
        else {
          posCell = m_cellPositionsHCalBarrelTool->xyzPosition(cell.getCellID());
        }
      } else
        warning() << "No cell positions tool found for system id " << systemId << ". " << endmsg;

      cellsPosition.emplace(std::pair<uint64_t, TLorentzVector>(
          cell.getCellID(), TLorentzVector(posCell.X(), posCell.Y(), posCell.Z(), cell.getEnergy())));
      allCells.emplace(cell.getCellID(), cell.getType());
    }

    // sort cells by energy
    std::sort(cellsEnergy.begin(), cellsEnergy.end(),
              [](const std::pair<uint64_t, double>& lhs, const std::pair<uint64_t, double>& rhs) {
                return lhs.second < rhs.second;
              });

    debug() << "..... with " << cellsEnergy.size() << " cells:" << endmsg;

    // loop through cells, find neighbouring cells with status "neighbour" (above 2nd topo-cluster threshold)
    for (auto cell : cellsEnergy) {
      // test if cell is seed
      auto itCell = cellsType.find(cell.first);
      auto fCell = *itCell;

      if (itCell != cellsType.end() && fCell.second == 1 && cell.second > m_threshold) {

        verbose() << "..... ... cell is seed type. " << fCell.second << endmsg;
        // start counting neighbours
        int countNeighbours = 0;
        auto neighboursVector = m_neighboursTool->neighbours(fCell.first);
        verbose() << "..... ... found " << neighboursVector.size() << " neighbours." << endmsg;
        // test if neighbouring cells are of type 2, and lower energy
        for (auto nCellId : neighboursVector) {
          // check if neighbours are hit
          auto itNeighbour = cellsType.find(nCellId);
          auto fNeighbour = *itNeighbour;
          // if the neighbour is of type 2, count up
          if (fNeighbour.second == 2) {
            countNeighbours++;
          }
          // if neighbour is seed, check energy
          else if (fNeighbour.second == 1) {
            auto fNeighbourEnergy = std::find_if(
                cellsEnergy.begin(), cellsEnergy.end(),
                [&nCellId](const std::pair<uint64_t, double>& elem) { return bool(elem.first == nCellId); });
            auto fNEnergy = *fNeighbourEnergy;
            if (fNEnergy.second > cell.second) {
              // checking next cell of cluster
              verbose() << "Neighbouring cell with higher energy found. " << endmsg;
              break;
            }
            // if cell is of lower energy, but seed, count as valid neighbour
            else {
              countNeighbours++;
            }
          }
        }
        if (countNeighbours > 4) {
          debug() << "..... ... found " << countNeighbours << " neighbouring, type 2 cells. " << endmsg;
          // collect cells to be used as seeds for new cluster
          newSeeds.push_back(std::make_pair(cell.first, cell.second));
          newClusters++;
        } else {
          verbose() << "..... cell with energy " << cell.second << ", does not have >4 neighbouring cells." << endmsg;
        }
      }
    }
    if (newClusters != newSeeds.size()) {
      error() << "Number of new seeds not num found cells qualifying for sub-cluster!!! " << endmsg;
    }
    verbose() << "Elements in cells types before sub-cluster building: " << cellsType.size() << endmsg;

    // Build new clusters, if more than 2 new seeds have been found.
    if (newClusters > 1) {
      totSplitClusters++;

      debug() << "..... split cluster into " << newSeeds.size() << ". " << endmsg;
      debug() << "################################### " << endmsg;
      debug() << "##  Start building sub-clusters ###" << endmsg;
      debug() << "################################### " << endmsg;

      // build clusters in multiple iterations
      int iter = 0;
      uint clusterID = clusters->size() + 1;
      std::map<uint64_t, uint> clusterOfCell;
      // map of clusterID to next neighbours vector to find next cells
      std::map<uint, std::vector<std::vector<std::pair<uint64_t, uint>>>> mapVecNextNeighbours;

      debug() << "Iteration 0: " << endmsg;
      while (iter == 0) {
        for (auto& seed : newSeeds) {
          // start cluster with seed, add to all maps, erase from vector
          clusterOfCell.emplace(seed.first, clusterID);

          preClusterCollection[clusterID].reserve(cluster.hits_size());
          clusterPositions.emplace(clusterID, cellsPosition[seed.first]);

          debug() << "Number of cells in clusters before filling : " << clusterOfCell.size() << endmsg;
          debug() << "Old Cluster (" << clusterID << ") position(x,y,z) / energy(GeV) : ("
                  << clusterPositions[clusterID].X() << ", " << clusterPositions[clusterID].X() << ", "
                  << clusterPositions[clusterID].X() << ") " << clusterPositions[clusterID].Energy() << " . " << endmsg;

          // collect neighbouring cells has type, in parallel for each seed!!!
          auto vec = SplitClusters::searchForNeighbours(seed.first, clusterID, cellsType, clusterOfCell, cellsPosition,
                                                        clusterPositions);
          std::vector<std::vector<std::pair<uint64_t, uint>>> vecVec;
          vecVec.resize(1000); // number of maximum iterations
          vecVec.insert(vecVec.begin(), vec);

          mapVecNextNeighbours.emplace(clusterID, vecVec);

          debug() << "Found " << mapVecNextNeighbours[clusterID][0].size() << " more neighbours.." << endmsg;
          debug() << "Left cells in vector " << cellsType.size() << ". " << endmsg;
          debug() << "New Cluster (" << clusterID << ") position(x,y,z) / energy(GeV) : ("
                  << clusterPositions[clusterID].X() << ", " << clusterPositions[clusterID].X() << ", "
                  << clusterPositions[clusterID].X() << ") " << clusterPositions[clusterID].Energy() << " . " << endmsg;
          //
          clusterID++;
        }
        iter++;
      }

      debug() << "Start iteration: ";

      while (iter > 0) {
        // iterate for adding cells to clusters
        debug() << iter << endmsg;
        bool foundNewNeighbours = false;
        // loop through new clusters for every iteration
        for (uint newCluster = 1; newCluster <= newSeeds.size(); newCluster++) {
          clusterID = clusters->size() + newCluster;
          // if neighbours have been found, continue...
          if (mapVecNextNeighbours[clusterID][iter - 1].size() > 0) {
            foundNewNeighbours = true;
            debug() << mapVecNextNeighbours[clusterID][iter - 1].size()
                    << ".. neighbours assigned to clusterId : " << clusterID << endmsg;
            for (auto& id : mapVecNextNeighbours[clusterID][iter - 1]) {
              if (id.first == 0) {
                error() << "Building of cluster is stopped due to missing id in neighbours map." << endmsg;
                return StatusCode::FAILURE;
              }
              if (id.second != clusterID) {
                error() << "Building of cluster is stopped due to wrong cell id in neighbours map." << endmsg;
                return StatusCode::FAILURE;
              }
              // find next neighbours
              auto vec = SplitClusters::searchForNeighbours(id.first, clusterID, cellsType, clusterOfCell,
                                                            cellsPosition, clusterPositions);
              // add the next neighbours at end of already found neighbours in this round.
              verbose() << "Size before additional vec : " << mapVecNextNeighbours[clusterID][iter].size() << endmsg;
              mapVecNextNeighbours[clusterID][iter].reserve(mapVecNextNeighbours[clusterID][iter].size() + vec.size());
              mapVecNextNeighbours[clusterID][iter].insert(mapVecNextNeighbours[clusterID][iter].end(),
                                                           std::make_move_iterator(vec.begin()),
                                                           std::make_move_iterator(vec.end()));
              verbose() << "Size after additional vec : " << mapVecNextNeighbours[clusterID][iter].size() << endmsg;
            }
          }
        }
        iter++;
        if (!foundNewNeighbours) {
          debug() << "Stopped cluster building at iteration : " << iter - 1 << endmsg;
          iter = -1;
        }
      }
      // TEST NEW CLUSTERS
      uint allClusteredCells = 0;
      for (auto i : clusterOfCell) {
        allClusteredCells++;
        preClusterCollection[i.second].push_back(std::make_pair(i.first, allCells[i.first]));
        cellsType.erase(i.first);
      }
      // in case not all cells have been assigned to new cluster, fill into seperate cluster and mark them with cell
      // type=4.
      if (allClusteredCells != cluster.hits_size()) {
        warning() << "NUMBER OF CELLS BEFORE " << cluster.hits_size() << " AND AFTER CLUSTER SPLITTING (map) "
                  << clusterOfCell.size() << "!!" << endmsg;
        warning() << "Elements in cells types after sub-cluster building: " << cellsType.size() << endmsg;

        auto l_cluster = edmClusters->create();
        double posX = 0.;
        double posY = 0.;
        double posZ = 0.;
        double energy = 0.;
        std::map<uint64_t, int>::iterator it;
        for (it = cellsType.begin(); it != cellsType.end(); it++) {
          totCellsAfter++;
          dd4hep::DDSegmentation::CellID cID = it->first;
          auto newCell = edmClusterCells->create();
          auto fNeighbourEnergy =
              std::find_if(cellsEnergy.begin(), cellsEnergy.end(), [&cID](const std::pair<uint64_t, double>& elem) {
                return bool(elem.first == uint64_t(cID));
              });
          auto fNEnergy = *fNeighbourEnergy;
          newCell.setEnergy(fNEnergy.second);
          newCell.setCellID(cID);
          // get cell position by cellID
          // identify calo system
          auto systemId = m_decoder->get(cID, "system");
          dd4hep::Position posCell;
          if (systemId == 5) // ECAL BARREL system id
            posCell = m_cellPositionsECalBarrelTool->xyzPosition(cID);
          else if (systemId == 8) { // HCAL BARREL system id
            if (m_noSegmentationHCalUsed)
              posCell = m_cellPositionsHCalBarrelNoSegTool->xyzPosition(cID);
            else {
              posCell = m_cellPositionsHCalBarrelTool->xyzPosition(cID);
            }
          }
          posX += posCell.X() * newCell.getEnergy();
          posY += posCell.Y() * newCell.getEnergy();
          posZ += posCell.Z() * newCell.getEnergy();
          // left over cells
          newCell.setType(4);
          energy += fNEnergy.second;
        }
        l_cluster.setType(3);
        l_cluster.setEnergy(energy);
        auto clusterPosition = edm4hep::Vector3f(posX / energy, posY / energy, posZ / energy);
        l_cluster.setPosition(clusterPosition);
        totEnergyAfter += energy;

        debug() << "Left-over cluster energy:     " << l_cluster.getEnergy() << endmsg;
      }

      // fill clusters into edm format
      for (auto i : preClusterCollection) {
        edm4hep::MutableCluster local_cluster;
        double posX = 0.;
        double posY = 0.;
        double posZ = 0.;
        double energy = 0.;
        std::map<int, int> system;

        for (auto pair : i.second) {
          totCellsAfter++;
          dd4hep::DDSegmentation::CellID cID = pair.first;
          // get CaloHit by cellID
          auto newCell = edmClusterCells->create();
          auto fNeighbourEnergy =
              std::find_if(cellsEnergy.begin(), cellsEnergy.end(), [&cID](const std::pair<uint64_t, double>& elem) {
                return bool(elem.first == uint64_t(cID));
              });
          auto fNEnergy = *fNeighbourEnergy;
          newCell.setEnergy(fNEnergy.second);
          newCell.setCellID(cID);
          newCell.setType(pair.second);
          energy += fNEnergy.second;

          // get cell position by cellID
          // identify calo system
          auto systemId = m_decoder->get(cID, "system");
          system[int(systemId)]++;
          dd4hep::Position posCell;
          if (systemId == 5) // ECAL BARREL system id
            posCell = m_cellPositionsECalBarrelTool->xyzPosition(cID);
          else if (systemId == 8) { // HCAL BARREL system id
            if (m_noSegmentationHCalUsed)
              posCell = m_cellPositionsHCalBarrelNoSegTool->xyzPosition(cID);
            else {
              posCell = m_cellPositionsHCalBarrelTool->xyzPosition(cID);
            }
          } else
            warning() << "No cell positions tool found for system id " << systemId << ". " << endmsg;

          posX += posCell.X() * newCell.getEnergy();
          posY += posCell.Y() * newCell.getEnergy();
          posZ += posCell.Z() * newCell.getEnergy();

          local_cluster.addToHits(newCell);
          auto check = allCells.erase(cID);
          if (check != 1)
            error() << "Cell id is not deleted from map. " << endmsg;
        }
        local_cluster.setEnergy(energy);
        auto clusterPosition = edm4hep::Vector3f(posX / energy, posY / energy, posZ / energy);
        local_cluster.setPosition(clusterPosition);
        local_cluster.setType(2);
        debug() << "Cluster energy:     " << local_cluster.getEnergy() << endmsg;
        totEnergyAfter += energy;
        edmClusters->push_back(local_cluster);
      }
      if (cellsType.size() > 0)
        info() << "Not all cluster cells have been assigned. " << cellsType.size() << endmsg;

      // clear maps
      clusterOfCell.clear();
      mapVecNextNeighbours.clear();
    }

    else {
      // fill cluster without changes
      auto clu = cluster.clone();
      clu.setType(1);
      totEnergyAfter += clu.getEnergy();
      edmClusters->push_back(clu);
      for (uint oldCells = 0; oldCells < clu.hits_size(); oldCells++) {
        totCellsAfter++;
        auto newCell = clu.getHits(oldCells).clone();
        edmClusterCells->push_back(newCell);
        auto check = allCells.erase(newCell.getCellID());
        if (check != 1)
          error() << "Cell id is not deleted from map. " << endmsg;
      }
    }

    // Clear maps and vectors
    cellsType.clear();
    cellsEnergy.clear();
    cellsPosition.clear();
    clusterPositions.clear();
    preClusterCollection.clear();
  }

  // sanity checks per event
  info() << "Split " << totSplitClusters << " clusters." << endmsg;
  if (allCells.size() > 0)
    info() << allCells.size() << " leftover cellIDs have been assigned to separate cluster. " << endmsg;
  // clear allCells map per event
  allCells.clear();

  if (int(totEnergyBefore) != int(totEnergyAfter))
    warning() << "After cluster splitting, energy ( " << totEnergyAfter << " ) is not what is was before ( "
              << totEnergyBefore << " )." << endmsg;
  if (totCellsBefore != totCellsAfter)
    warning() << "After cluster splitting, cells ( " << totCellsAfter << " ) is not what is was before ( "
              << totCellsBefore << " )." << endmsg;

  m_newCells.put(edmClusterCells);

  return StatusCode::SUCCESS;
}

std::vector<std::pair<uint64_t, uint>>
SplitClusters::searchForNeighbours(const uint64_t aCellId, const uint aClusterID,
                                   const std::map<uint64_t, int> aCellsType, std::map<uint64_t, uint>& aClusterOfCell,
                                   std::map<uint64_t, TLorentzVector> aCellPosition,
                                   std::map<uint, TLorentzVector>& aClusterPosition) const {
  // Fill vector to be returned, next cell ids and cluster id for which neighbours are found
  std::vector<std::pair<uint64_t, uint>> addedNeighbourIds;

  // Retrieve cellIds of neighbours
  auto neighboursVec = m_neighboursTool->neighbours(aCellId);
  if (neighboursVec.size() == 0) {
    error() << "No neighbours for cellID found! " << endmsg;
    error() << "to cellID :  " << aCellId << endmsg;
    error() << "in system:   " << m_decoder->get(aCellId, "system") << endmsg;
    addedNeighbourIds.resize(0);
    addedNeighbourIds.push_back(std::make_pair(0, 0));
  } else {
    verbose() << "For cluster: " << aClusterID << endmsg;
    // loop over neighbours
    for (auto& itr : neighboursVec) {
      uint64_t neighbourID = itr;
      // Find the neighbour in the Calo cells list
      auto itAllCellTypes = aCellsType.find(neighbourID);
      auto fCellType = *itAllCellTypes;
      auto itAllUsedCells = aClusterOfCell.find(neighbourID);
      auto fCellCluster = *itAllUsedCells;

      // If cell has type.. is found in the list of clustered cells
      if (itAllCellTypes != aCellsType.end()) {
        verbose() << "Found neighbour with CellID: " << neighbourID << endmsg;
        verbose() << "Neighbour is of cell type " << fCellType.second << ". " << endmsg;

        // and is not assigned to a cluster
        if (itAllUsedCells == aClusterOfCell.end()) {
          verbose() << "Add neighbour to cluster " << aClusterID << endmsg;
          // add neighbour to cells of cluster
          aClusterPosition[aClusterID] += aCellPosition[neighbourID]; // add lorentz vector
          aClusterOfCell.emplace(neighbourID, aClusterID);

          addedNeighbourIds.push_back(std::make_pair(neighbourID, aClusterID));
        }
        // and is already assigned to cluster, check if its assigned to different clusterID
        else if (itAllUsedCells != aClusterOfCell.end() && fCellCluster.second != aClusterID) {
          uint clusterIDToMerge = fCellCluster.second;
          verbose() << "This neighbour was found in cluster " << clusterIDToMerge << ", and cluster " << aClusterID
                    << ". It will be evaluate which one has higher geomertrical significance!" << endmsg;
          verbose() << "Distances to cluster core: "
                    << aClusterPosition[clusterIDToMerge].DeltaR(aCellPosition[neighbourID])
                    << ", and this cluster: " << aClusterPosition[aClusterID].DeltaR(aCellPosition[neighbourID])
                    << endmsg;

          // get distance of cell from cog of clusters, and test if the cell is closer to current cluster
          if (aClusterPosition[aClusterID].DeltaR(aCellPosition[neighbourID]) <=
              aClusterPosition[clusterIDToMerge].DeltaR(aCellPosition[neighbourID])) {
            verbose() << "Neighbour is assigned to cluster1. " << endmsg;
            addedNeighbourIds.push_back(std::make_pair(neighbourID, aClusterID));
            // remove the cell from the other cluster
            aClusterPosition[clusterIDToMerge] -= aCellPosition[neighbourID]; // remove lorentz vector
            // add cell to correct cluster
            aClusterPosition[aClusterID] += aCellPosition[neighbourID]; // add lorentz vector

            aClusterOfCell[neighbourID] = aClusterID;
          } else {
            verbose() << "Neighbour stays assigned to cluster2. " << endmsg;
            aClusterOfCell[neighbourID] = clusterIDToMerge;
          }
          // if the cell is assigned to current cluster.. nevermind.
        }
      }
    }
  }
  return addedNeighbourIds;
}

StatusCode SplitClusters::finalize() { return Gaudi::Algorithm::finalize(); }
