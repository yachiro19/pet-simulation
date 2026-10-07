#include "SteppingAction.hh"
#include "EventAction.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4SystemOfUnits.hh"
#include "G4VPhysicalVolume.hh"
#include "G4OpticalPhoton.hh"

SteppingAction::SteppingAction(EventAction *eventAction)
    : G4UserSteppingAction(), fEventAction(eventAction) {}

SteppingAction::~SteppingAction() {}

void SteppingAction::UserSteppingAction(const G4Step *step)
{
    G4Track *track = step->GetTrack();
    G4VPhysicalVolume *preVolume = step->GetPreStepPoint()->GetTouchable()->GetVolume();
    G4VPhysicalVolume *postVolume = step->GetPostStepPoint()->GetTouchable()->GetVolume();

    if (!preVolume)
        return;

    G4String preVolName = preVolume->GetName();

    // --------------------------------------------------------
    // 1. Только НЕ-оптические частицы (гамма, электроны, e+)
    //    регистрируем энерговыделение в кристалле
    // --------------------------------------------------------
    if (track->GetDefinition() != G4OpticalPhoton::OpticalPhotonDefinition())
    {
        G4double edep = step->GetTotalEnergyDeposit();
        if (edep > 0. && preVolName.find("Crystal") != std::string::npos)
        {
            G4int crystalID = preVolume->GetCopyNo();
            G4double time = track->GetGlobalTime();
            fEventAction->AddHit(crystalID, edep, time); // гамма-хит
        }
    }

    // --------------------------------------------------------
    // 2. Оптические фотоны — только считаем на SiPM
    // --------------------------------------------------------
    if (track->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition())
    {
        if (postVolume && postVolume->GetName().find("SiPM") != std::string::npos)
        {
            G4int sipmID = postVolume->GetCopyNo();
            G4double time = track->GetGlobalTime();
            G4double ePh = track->GetKineticEnergy();
            fEventAction->AddPhotonHit(sipmID, ePh, time);
            track->SetTrackStatus(fStopAndKill);
        }
    }
}