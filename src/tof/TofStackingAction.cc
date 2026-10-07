#include "TofStackingAction.hh"
#include "TofSettings.hh"
#include "G4OpticalPhoton.hh"
#include "G4Track.hh"

G4ClassificationOfNewTrack TofStackingAction::ClassifyNewTrack(const G4Track* track)
{
    if (track->GetDefinition() == G4OpticalPhoton::Definition())
    {
        G4double timeCut = TofSettings::Instance().GetTimeCut();
        if (timeCut > 0. && track->GetGlobalTime() > timeCut)
            return fKill;
    }
    return fUrgent;
}
