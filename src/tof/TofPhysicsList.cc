#include "TofPhysicsList.hh"
#include "G4DecayPhysics.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4OpticalParameters.hh"
#include "G4OpticalPhysics.hh"
#include "G4SystemOfUnits.hh"

TofPhysicsList::TofPhysicsList()
{
    // Порог рождения вторичных частиц 0.1 мм: дельта-электроны с энергией выше
    // черенковского порога (~0.1 МэВ при n ≈ 1.8) рождаются явно и излучают свет
    SetDefaultCutValue(0.1 * mm);

    // option4 — наиболее точная модель при низких энергиях (фотоэффект, комптон с
    // учётом связи электронов, поляризация)
    RegisterPhysics(new G4EmStandardPhysics_option4());
    RegisterPhysics(new G4DecayPhysics());
    RegisterPhysics(new G4OpticalPhysics());

    auto* params = G4OpticalParameters::Instance();
    params->SetProcessActivation("Scintillation", true);
    params->SetProcessActivation("Cerenkov", true);
    params->SetScintByParticleType(false);
    // Конечное время нарастания сцинтилляции (SCINTILLATIONRISETIME1) — важно для LYSO
    params->SetScintFiniteRiseTime(true);
    params->SetScintTrackSecondariesFirst(true);
    params->SetCerenkovTrackSecondariesFirst(true);
    params->SetCerenkovMaxPhotonsPerStep(100);
    params->SetCerenkovMaxBetaChange(10.0);
}
