#define _USE_MATH_DEFINES
#include <cmath>
#include "PrimaryGeneratorAction.hh"
#include "G4ParticleTable.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include "G4Positron.hh"
#include "G4Event.hh"
#include "G4ios.hh"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PrimaryGeneratorAction::PrimaryGeneratorAction()
{
    fParticleGun = new G4ParticleGun(1);
}

PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
    delete fParticleGun;
}

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{
    G4double positronEnergy = 250*keV + 250*keV * G4UniformRand();
    fParticleGun->SetParticleDefinition(G4Positron::Definition());
    fParticleGun->SetParticleEnergy(positronEnergy);

    G4double theta = 2.0 * M_PI * G4UniformRand();
    G4double phi = std::acos(2.0 * G4UniformRand() - 1.0);
    G4ThreeVector direction(std::sin(phi)*std::cos(theta),
                            std::sin(phi)*std::sin(theta),
                            std::cos(phi));
    fParticleGun->SetParticleMomentumDirection(direction);

    G4double offset = 2.5*mm * (G4UniformRand() - 0.5);
    fParticleGun->SetParticlePosition(G4ThreeVector(offset, offset, offset));

    fParticleGun->GeneratePrimaryVertex(anEvent);
}