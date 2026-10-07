#include "PhysicsList.hh"
#include "G4EmStandardPhysics.hh"
#include "G4DecayPhysics.hh"
#include "G4RadioactiveDecayPhysics.hh"
#include "G4EmExtraPhysics.hh"
#include "G4HadronElasticPhysics.hh"
#include "G4HadronPhysicsFTFP_BERT.hh"
#include "G4StoppingPhysics.hh"
#include "G4IonPhysics.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalParameters.hh"

PhysicsList::PhysicsList()
{
    RegisterPhysics(new G4EmStandardPhysics());
    RegisterPhysics(new G4DecayPhysics());
    RegisterPhysics(new G4RadioactiveDecayPhysics());
    RegisterPhysics(new G4EmExtraPhysics());
    RegisterPhysics(new G4HadronElasticPhysics());
    RegisterPhysics(new G4HadronPhysicsFTFP_BERT());
    RegisterPhysics(new G4StoppingPhysics());
    RegisterPhysics(new G4IonPhysics());

    // --- Оптическая физика ---
    G4OpticalPhysics *opticalPhysics = new G4OpticalPhysics();
    RegisterPhysics(opticalPhysics);

    auto optParams = G4OpticalParameters::Instance();
    optParams->SetScintTrackSecondariesFirst(true);
    optParams->SetScintByParticleType(false);
    optParams->SetCerenkovMaxPhotonsPerStep(100);
    optParams->SetCerenkovMaxBetaChange(10.0);
    optParams->SetCerenkovTrackSecondariesFirst(true);
}

PhysicsList::~PhysicsList() {}

void PhysicsList::ConstructParticle()
{
    G4VModularPhysicsList::ConstructParticle();
}

void PhysicsList::ConstructProcess()
{
    G4VModularPhysicsList::ConstructProcess();
}
