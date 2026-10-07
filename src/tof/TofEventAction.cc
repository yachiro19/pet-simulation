#include "TofEventAction.hh"
#include "TofRunAction.hh"
#include "G4Event.hh"
#include <algorithm>

TofEventAction::TofEventAction(TofRunAction* runAction)
    : fRunAction(runAction)
{
}

void TofEventAction::BeginOfEventAction(const G4Event*)
{
    for (auto& crystal : fCrystals)
        crystal = CrystalRecord();
}

void TofEventAction::AddEnergyDeposit(G4int crystalID, G4double edep, G4double time, G4double doi)
{
    CrystalRecord& crystal = fCrystals[crystalID];
    crystal.edep += edep;
    if (crystal.gammaTime < 0. || time < crystal.gammaTime)
    {
        crystal.gammaTime = time;
        crystal.doi = doi;
    }
}

void TofEventAction::AddPhoton(G4int crystalID, G4double time, PhotonOrigin origin)
{
    CrystalRecord& crystal = fCrystals[crystalID];
    crystal.photons.push_back({time, origin});
    if (origin == PhotonOrigin::Scintillation)
        crystal.nScint++;
    else if (origin == PhotonOrigin::Cherenkov)
        crystal.nCherenkov++;
}

void TofEventAction::EndOfEventAction(const G4Event* event)
{
    // В файл попадают кристаллы с энерговыделением или зарегистрированными фотонами
    for (G4int id = 0; id < TofDetectorConstruction::kCrystals; id++)
    {
        CrystalRecord& crystal = fCrystals[id];
        if (crystal.edep <= 0. && crystal.photons.empty())
            continue;
        std::sort(crystal.photons.begin(), crystal.photons.end(),
                  [](const DetectedPhoton& a, const DetectedPhoton& b) { return a.time < b.time; });
        fRunAction->WriteCrystal(event->GetEventID(), id, crystal);
    }
}
