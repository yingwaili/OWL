#include <cstdio>
#include <cmath>
#include "HistogramFreeMUCA.hpp"
#include "RandomNumberGenerator.hpp"

// Constructor
DiscreteHistogramFreeMUCA::DiscreteHistogramFreeMUCA(int restart, const char* inputFile) : h(restart, inputFile)    // call Histgoram h's constructor here
{
  printf("Simulation method: Histogram-free multicanonical sampling for discrete energy models\n");
  restartFlag = restart;
  
  numberOfDataPoints = 1000;                // TO DO: should be set from input file!
  DataSet.assign(numberOfDataPoints, 0);

}


DiscreteHistogramFreeMUCA::~DiscreteHistogramFreeMUCA()
{
 
  DataSet.clear();
  printf("Exiting DiscreteHistogramFreeMUCA class... \n");
//  delete physical_system;

}


void DiscreteHistogramFreeMUCA::run(PhysicalSystem* physical_system)
{

  //double currentTime, lastBackUpTime;
  currentTime = lastBackUpTime = MPI_Wtime();
  if (GlobalComm.thisMPIrank == 0)
    printf("Running DiscreteHistogramFreeMUCA...\n");

  char fileName[51];
 
  // Find the first energy that falls within the energy range    
  while (!acceptMove) {
    physical_system -> doMCMove();
    physical_system -> getObservables();
    physical_system -> acceptMCMove();    // always accept the move to push the state forward
    acceptMove = h.checkEnergyInRange(physical_system -> observables[0]);
  }

  // Always accept the first energy if it is within range
  


  h.updateHistogramDOS(physical_system -> observables[0]);
  //physical_system -> acceptMCMove();

  // Write out the energy
  if (GlobalComm.thisMPIrank == 0) 
    physical_system -> writeConfiguration(0, "energyLatticePos.dat");
    //writeSystemFile("energyLatticePos.dat", oldEnergy, oldPos, oldLatticeVec);

//-------------- End initialization --------------//

// WL procedure starts here
  while (h.modFactor > h.modFactorFinal) {
    h.histogramFlat = false;

    while (!(h.histogramFlat)) {
      for (unsigned int MCSteps=0; MCSteps<h.histogramCheckInterval; MCSteps++)
      {

        physical_system -> doMCMove();
        physical_system -> getObservables();
        //proposeMCmoves(trialPos, trialLatticeVec);
        //pass_pos_array(&trialPos(0,0));          // Update the atomic positions
        //pass_cell_array(&trialLatticeVec(0,0));  // Update the lattice cell vector

        //owl_do_pwscf(&exit_status);               // Run the subsequent PWscf calculation 
        //get_natom_ener(&natom, &trialEnergy);

        // check if the energy falls within the energy range
        if ( !h.checkEnergyInRange(physical_system -> observables[0]) )
          acceptMove = false;
        else {
          // determine WL acceptance
          if ( exp(h.getDOS(physical_system -> oldObservables[0]) - 
                   h.getDOS(physical_system -> observables[0])) > getRandomNumber2() )
            acceptMove = true;
          else
            acceptMove = false;
        }

        if (acceptMove) {
           // Update histogram and DOS with trialEnergy
           h.updateHistogramDOS(physical_system -> observables[0]);
           h.acceptedMoves++;        
 
           physical_system -> acceptMCMove();
           // Store trialPos, trialLatticeVec, trialEnergy
           // oldPos = trialPos;
           // oldLatticeVec = trialLatticeVec;
           // oldEnergy = trialEnergy;
/*
           if (GlobalComm.thisMPIrank == 0) {
             physical_system -> writeConfiguration(0, "energyLatticePos.dat");
             //writeSystemFile("energyLatticePos.dat", oldEnergy, oldPos, oldLatticeVec);
             std::cerr << "trial move accepted: Energy = " << physical_system -> observables[0] << "\n";
           }
*/
        }
        else {
/*
           if (GlobalComm.thisMPIrank == 0) {
             std::cerr << "trial move rejected: Energy = " << physical_system -> observables[0] << "\n";
             std::cerr << "Update with energy = " << physical_system -> oldObservables[0] << "\n";
           }
*/
           physical_system -> rejectMCMove();
           // Restore trialPos and trialLatticeVec
           //trialPos = oldPos;
           //trialLatticeVec = oldLatticeVec;

           // Update histogram and DOS with oldEnergy
           h.updateHistogramDOS(physical_system -> oldObservables[0]);
           h.rejectedMoves++;
        }
        h.totalMCsteps++;
     
        //owl_stop_run(&exit_status);              // Clean up the PWscf run

        // Write restart files at interval
        currentTime = MPI_Wtime();
        if (GlobalComm.thisMPIrank == 0) {
          if (currentTime - lastBackUpTime > 300) {
            h.writeHistogramDOSFile("hist_dos_checkpoint.dat");
            physical_system -> writeConfiguration(1, "OWL_QE_restart_input");
            //writeQErestartFile("OWL_QE_restart_input", trialPos, trialLatticeVec);
            lastBackUpTime = currentTime;
          }
        }
      }
      //h.writeHistogramDOSFile("hist_dos_checkpoint.dat");

      // Check histogram flatness
      h.histogramFlat = h.checkHistogramFlatness();
      if (h.numHistogramNotImproved >= h.histogramRefreshInterval)
        h.refreshHistogram();
    }

    if (GlobalComm.thisMPIrank == 0) {
      printf("Number of iterations performed = %d\n", h.iterations);
    
      // Also write restart file here 
      sprintf(fileName, "hist_dos_iteration%02d.dat", h.iterations);
      h.writeHistogramDOSFile(fileName);
      physical_system -> writeConfiguration(1, "OWL_QE_restart_input");
      //writeQErestartFile("OWL_QE_restart_input", trialPos, trialLatticeVec);
    }

    // Go to next iteration
    h.modFactor /= h.modFactorReducer;
    h.resetHistogram();
    h.iterations++;
  }

  // Write out data at the end of the simulation
  h.writeNormDOSFile("dos.dat");
  h.writeHistogramDOSFile("hist_dos_final.dat");
  //wl_qe_stop_(&exit_status);  // Finish the PWscf calculation

}



// Private member functions
void DiscreteHistogramFreeMUCA::resetDataSet()
{
  DataSet.assign(numberOfDataPoints, 0);  // To DO: check other functions!
}

