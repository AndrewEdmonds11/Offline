//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
//
// $Id: Mu2eEmStandardPhysics_option4.hh 105735 2017-08-16 12:59:43Z gcosmo $
//
//---------------------------------------------------------------------------
//
// ClassName:   Mu2eEmStandardPhysics_option4 based on 
//                G4EmStandardPhysics_option4
//
// Author:      V.Ivanchenko 28.09.2012
//
// Modified:    K.L.Genser for Mu2e: changed highEnergyLimit to 115MeV
//
//----------------------------------------------------------------------------
//
// This class provides construction of EM physics using the best models
// of standard and low-energy packages and set of 
// the most adavced options allowing precise simulation at low
// and intermediate energies
//

#ifndef Mu2eEmStandardPhysics_option4_h
#define Mu2eEmStandardPhysics_option4_h 1

#include "G4VPhysicsConstructor.hh"
#include "G4EmParticleList.hh"
#include "globals.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

class Mu2eEmStandardPhysics_option4 : public G4VPhysicsConstructor
{
public:

  explicit Mu2eEmStandardPhysics_option4(G4int ver=1, const G4String& name="");

  virtual ~Mu2eEmStandardPhysics_option4();

  virtual void ConstructParticle();
  virtual void ConstructProcess();

private:
  G4int  verbose;
  G4EmParticleList partList;
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif






