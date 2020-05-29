#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include "CrystalBase.hpp"


// Note (data layout of Matrix class):
// (i) mat(i,j)
//     resize(nRow,nCol); nRow = dim. of coordinates, nCol = # of atoms
//       j ->
//     i    x0  x1 ... x[j-1]
//     |    y0  y1     y[j-1]
//     v    z0  z1 ... z[j-1]
//
// (ii) mat[k]
//      index is calculated by k = j*lDim + i
//       0  1  2  3  4  5 ... 
//      x0 y0 z0 x1 y1 z1 ... 


// Constructor 1: initialize unit cell and lattice from input file
Lattice::Lattice(const char* inputFile)
{
  
  // Initialize unit cell
  unitCell.lattice_vectors.resize(3, 3);
  readUnitCellInfo(inputFile);
  numberOfUnitCells = unitCellDimensions[0] * unitCellDimensions[1] * unitCellDimensions[2];
  constructUnitCellVectors();

  // Initialize crystal
  totalNumberOfAtoms = numberOfUnitCells * unitCell.number_of_atoms;
  //constructGlobalCoordinates();
  // Check:
  //printAllPairwiseDistances();


  // Initialize nearest neighbor lists for each unit cell
  constructRelativeUnitCellVectors();
  nearestNeighborUnitCellList.resize(numberOfUnitCells);
  for (unsigned int i=0; i<numberOfUnitCells; i++)
    nearestNeighborUnitCellList[i] = constructNearestNeighborUnitCellList(i);

  constructRelativeCoordinates();
  // Check:
  printPairwiseDistancesInUnitCellList(13);

}


// TODO: Constructor for initializing unit cell in derived classes where crystal structures are known and standard 



// Member functions:

// Note: unit cell layout in lattice_vectors(i,j):
//       j ->
//     i    ax  bx  cx
//     |    ay  by  cy
//     v    az  bz  cz
void Lattice::readUnitCellInfo(const char* mainInputFile)
  {

    //if (GlobalComm.thisMPIrank == 0)
      if (std::filesystem::exists(mainInputFile))
        std::cout << "Crystal class reading input file: " << mainInputFile << "\n";

    std::ifstream inputFile(mainInputFile);
    std::string line, key;
    unsigned int atom_counter {0};

    if (inputFile.is_open()) {

      while (std::getline(inputFile, line)) {

        //std::cout << "Debug: " << line << "\n"; 

        if (!line.empty()) {
          std::istringstream lineStream(line);
          lineStream >> key;

          //std::cout << "Debug: " << key << "\n"; 

          if (key.compare(0, 1, "#") != 0) {

            if (key == "NumberOfAtomsPerUnitCell") {
              lineStream >> unitCell.number_of_atoms;
              std::cout << "Crystal: number of atoms per unit cell = " << unitCell.number_of_atoms << "\n";

              // Allocate memory for atomic_positions
              unitCell.atomic_positions.resize(3, unitCell.number_of_atoms);
              continue;
            }
            else if (key == "LatticeVectors") {
              lineStream >> unitCell.lattice_vectors(0,0) >> unitCell.lattice_vectors(1,0) >> unitCell.lattice_vectors(2,0);
              std::cout << "Crystal: Lattice vectors (" 
                        << unitCell.lattice_vectors(0,0) << ", " 
                        << unitCell.lattice_vectors(1,0) << ", " 
                        << unitCell.lattice_vectors(2,0) << ") \n";

              // Read the other two lattice vectors
              for (unsigned int i=1; i<3; i++) {
                lineStream.clear();
                std::getline(inputFile, line);               
                if (!line.empty()) lineStream.str(line);
                lineStream >> unitCell.lattice_vectors(0,i) >> unitCell.lattice_vectors(1,i) >> unitCell.lattice_vectors(2,i);
                std::cout << "Crystal: Lattice vectors (" 
                          << unitCell.lattice_vectors(0,i) << ", " 
                          << unitCell.lattice_vectors(1,i) << ", " 
                          << unitCell.lattice_vectors(2,i) << ") \n";
              }

              continue;
            }
            else if (key == "atom") {
              std::string element;
              lineStream >> element
                         >> unitCell.atomic_positions(0,atom_counter) 
                         >> unitCell.atomic_positions(1,atom_counter) 
                         >> unitCell.atomic_positions(2,atom_counter);
              //std::cout << "element = " << element << "\n";
              unitCell.atomic_species.push_back(element);
              atom_counter++;
              //std::cout << "atom_counter = " << atom_counter << "\n";
              continue;
            }
            else if (key == "UnitCellDimensions") {
              lineStream >> unitCellDimensions[0] >> unitCellDimensions[1] >> unitCellDimensions[2];
              std::cout << "Crystal: unit cell dimensions = " << unitCellDimensions[0] << " x " 
                                                              << unitCellDimensions[1] << " x " 
                                                              << unitCellDimensions[2] << " \n";
              continue;
            }

          }

        }

      }

      inputFile.close();

    }

    // Sanity checks:
    assert(atom_counter == unitCell.number_of_atoms);

    for (unsigned int i=0; i<unitCell.number_of_atoms; i++) {
      std::cout << unitCell.atomic_species[i] << " " 
                << unitCell.atomic_positions(0,i) << " " 
                << unitCell.atomic_positions(1,i) << " " 
                << unitCell.atomic_positions(2,i) << "\n";
    }

  }


  void Lattice::constructUnitCellVectors()
  {
 
    unitCellVectors.resize(3, numberOfUnitCells);

    unsigned int counter = 0;
    
    for (unsigned int k=0; k<unitCellDimensions[2]; k++) {
      for (unsigned int j=0; j<unitCellDimensions[1]; j++) {
        for (unsigned int i=0; i<unitCellDimensions[0]; i++) {
          unsigned int unitCellIndex = getUnitCellIndex(i, j, k);
          assert (unitCellIndex == counter);
          unitCellVectors(0,unitCellIndex) = i;
          unitCellVectors(1,unitCellIndex) = j;
          unitCellVectors(2,unitCellIndex) = k;
          counter++;
        }
      }
    }

  }


  void Lattice::constructGlobalCoordinates()
  {
   
    globalAtomicPositions.resize(3, totalNumberOfAtoms);
    
    for (unsigned int uc=0; uc<numberOfUnitCells; uc++) {
      for (unsigned int atomID=0; atomID<unitCell.number_of_atoms; atomID++) {
        unsigned int atomIndex = getAtomIndex(uc, atomID);
        //std::cout << "Global coordinates " << uc << " " << atomID << " = ( ";
        std::cout << "Global coordinates " << atomIndex << " = ( ";
        for (unsigned int i=0; i<3; i++) {
          globalAtomicPositions(i, atomIndex) = double(unitCellVectors(i,uc)) + unitCell.atomic_positions(i,atomID);
          std::cout << globalAtomicPositions(i, atomIndex) << " ";
        }
        std::cout << ") \n";
      }
    }
  
  }


  void Lattice::constructRelativeCoordinates()
  {

    totalNumberOfNeighboringAtoms = nearestNeighborUnitCellList.size() * unitCell.number_of_atoms;
    relativeAtomicPositions.resize(3, totalNumberOfNeighboringAtoms);

    unsigned int counter {0};
    for (unsigned int uc=0; uc<nearestNeighborUnitCellList.size(); uc++) {
      for (unsigned int atomID=0; atomID<unitCell.number_of_atoms; atomID++) {
        unsigned int atomIndex = uc * unitCell.number_of_atoms + atomID;
        std::cout << "Relative coordinates " << atomIndex << " = ( ";
        for (unsigned int i=0; i<3; i++) {
          assert (atomIndex == counter);
          relativeAtomicPositions(i, atomIndex) = double(relativeUnitCellVectors(i,uc)) + unitCell.atomic_positions(i,atomID);
          std::cout << relativeAtomicPositions(i, atomIndex) << " ";
        }
        std::cout << ") \n";
        counter++;
      }
    }

  }


  double Lattice::getPairwiseDistance(unsigned int atom1, unsigned int atom2)
  {  

    double distance = 0.0;
    for (unsigned int i=0; i<3; i++)
      distance += (globalAtomicPositions(i, atom1) - globalAtomicPositions(i, atom2)) * (globalAtomicPositions(i, atom1) - globalAtomicPositions(i, atom2));

    return sqrt(distance);  

  }  
  

  double Lattice::getRelativePairwiseDistance(unsigned int atom1, unsigned int atom2)
  {  

    double distance = 0.0;
    for (unsigned int i=0; i<3; i++)
      distance += (relativeAtomicPositions(i, atom1) - relativeAtomicPositions(i, atom2)) * (relativeAtomicPositions(i, atom1) - relativeAtomicPositions(i, atom2));

    return sqrt(distance);

  } 


  void Lattice::printAllPairwiseDistances()
  {  

    for (unsigned int atom1=0; atom1<totalNumberOfAtoms; atom1++) {
      for (unsigned int atom2=atom1; atom2<totalNumberOfAtoms; atom2++) {
        std::cout << "atom " << atom1 << ", atom " << atom2 << " : " 
                  << globalAtomicPositions(0, atom1) - globalAtomicPositions(0, atom2) << " , " 
                  << globalAtomicPositions(1, atom1) - globalAtomicPositions(1, atom2) << " , " 
                  << globalAtomicPositions(2, atom1) - globalAtomicPositions(2, atom2) << " . " 
                  << getPairwiseDistance(atom1,atom2) << "\n";
      }
    }

  }


  void Lattice::printPairwiseDistancesInUnitCellList(unsigned int atom1_global)
  {

    // Current unit cell is always (0,0,0) relatively.
    unsigned int localUnitCellIndex = getRelativeUnitCellIndex(0, 0, 0);
    unsigned int globalUnitCellIndex = atom1_global / unitCell.number_of_atoms;
    
    unsigned int atom1 = getAtomIndex(localUnitCellIndex, atom1_global % unitCell.number_of_atoms);

    for (unsigned int j=0; j<nearestNeighborUnitCellList[globalUnitCellIndex].size(); j++) {
      for (unsigned int k=0; k<unitCell.number_of_atoms; k++) {
        unsigned int atom2 = getAtomIndex(j, k);
        unsigned int atom2_global = getAtomIndex(nearestNeighborUnitCellList[globalUnitCellIndex][j], k);
        std::cout << "atom " << atom1_global << ", atom " << atom2_global << " : " 
                  << relativeAtomicPositions(0, atom1) - relativeAtomicPositions(0, atom2) << " , " 
                  << relativeAtomicPositions(1, atom1) - relativeAtomicPositions(1, atom2) << " , " 
                  << relativeAtomicPositions(2, atom1) - relativeAtomicPositions(2, atom2) << " . " 
                  << getRelativePairwiseDistance(atom1, atom2) << "\n";
      }
    }

  }


  // Note: this function is ad hoc for now. It only checks immediate nearest neighbors (-1, 0, 1) in all (x, y, z) diections
  void Lattice::constructRelativeUnitCellVectors()
  {

    // 27 elements because there are 3 possibilities (-1, 0, 1) for all (x, y, z) directions considering only nearest neighbors
    relativeUnitCellVectors.resize(3,27);

    unsigned int counter {0};
    for (int k=-1; k<=1; k++) {
      for (int j=-1; j<=1; j++) {
        for (int i=-1; i<=1; i++) {
          relativeUnitCellVectors(0, counter) = i;
          relativeUnitCellVectors(1, counter) = j;
          relativeUnitCellVectors(2, counter) = k;
          counter++;
        }
      }
    }

    assert(counter==27);

  }


  std::vector<unsigned int> Lattice::constructNearestNeighborUnitCellList(unsigned int currentUnitCell)
  {

    // A neighbor list where pairwise interactions with current unit cell will be checked
    std::vector<unsigned int> unitCellList;

    int nx_new, ny_new, nz_new;
    int nx = unitCellVectors(0, currentUnitCell);
    int ny = unitCellVectors(1, currentUnitCell);
    int nz = unitCellVectors(2, currentUnitCell);

    // A lambda to calculate neighboring unit cell components considering p.b.c. shift
    auto getUnitCellComponentPBC = [=](int x_new, unsigned int dimension) { 
      return x_new >= 0 ? (x_new % unitCellDimensions[dimension]) : (x_new + unitCellDimensions[dimension]);
    };

    for (int k=-1; k<=1; k++) {
      for (int j=-1; j<=1; j++) {
        for (int i=-1; i<=1; i++) {
          nx_new = getUnitCellComponentPBC(nx+i, 0);
          ny_new = getUnitCellComponentPBC(ny+j, 1);
          nz_new = getUnitCellComponentPBC(nz+k, 2);
          //std::cout << i << " " << j << " " << k << " : " << getUnitCellIndex(nx_new, ny_new, nz_new) << "\n";
          unitCellList.push_back(getUnitCellIndex(nx_new, ny_new, nz_new));
        }
      }
    }
    
    //std::sort(unitCellNeighborList.begin(), unitCellNeighborList.end(), 
    //          [](unsigned int a, unsigned int b) { return (a < b); }
    //);

    //std::cout << "Unit Cell " << currentUnitCell << " , NeighborList = ";
    //for (auto i : unitCellList)
    //  std::cout << i << ' ';
    //std::cout << "\n";
    
    return unitCellList;

  }