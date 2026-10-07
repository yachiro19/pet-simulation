#include "TofSteppingAction.hh"
#include "TofDetectorConstruction.hh"
#include "TofEventAction.hh"
#include "TofSettings.hh"
#include "G4Gamma.hh"
#include "G4OpticalPhoton.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"
#include "Randomize.hh"

TofSteppingAction::TofSteppingAction(const TofDetectorConstruction* detector, TofEventAction* eventAction)
    : fDetector(detector), fEventAction(eventAction)
{
}

void TofSteppingAction::UserSteppingAction(const G4Step* step)
{
    G4Track* track = step->GetTrack();
    const G4StepPoint* pre = step->GetPreStepPoint();
    const G4VPhysicalVolume* volume = pre->GetPhysicalVolume();
    if (!volume)
        return;

    // --------------------------------------------------------
    // Оптический фотон регистрируется на первом шаге внутри окна SiPM, то есть
    // только если он действительно прошёл границу кристалл -> SiPM
    // (отражённые на границе фотоны в окно не попадают)
    // --------------------------------------------------------
    if (track->GetDefinition() == G4OpticalPhoton::Definition())
    {
        if (volume->GetLogicalVolume()->GetName() != "SiPM")
            return;
        track->SetTrackStatus(fStopAndKill); // поглощён в SiPM
        if (pre->GetStepStatus() != fGeomBoundary)
            return; // фотон родился в самом окне, а не пришёл из кристалла

        const TofSettings& settings = TofSettings::Instance();
        if (G4UniformRand() >= settings.DetectionProbability(track->GetKineticEnergy()))
            return;

        PhotonOrigin origin = PhotonOrigin::Other;
        if (const G4VProcess* creator = track->GetCreatorProcess())
        {
            if (creator->GetProcessName() == "Scintillation")
                origin = PhotonOrigin::Scintillation;
            else if (creator->GetProcessName() == "Cerenkov")
                origin = PhotonOrigin::Cherenkov;
        }

        // Время входа в SiPM + разброс времени одного фотона (SPTR)
        G4double time = pre->GetGlobalTime() + G4RandGauss::shoot(0., settings.GetSptrSigma());
        fEventAction->AddPhoton(volume->GetCopyNo(), time, origin);
        return;
    }

    // --------------------------------------------------------
    // Энерговыделение гамма-квантов, электронов и позитронов в кристаллах
    // --------------------------------------------------------
    G4double edep = step->GetTotalEnergyDeposit();
    if (edep <= 0. || volume->GetLogicalVolume()->GetName() != "Crystal")
        return;

    // Гамма-квант отдаёт энергию в точке взаимодействия — в конце шага;
    // заряженная частица — вдоль шага, берём его начало
    const G4StepPoint* point = (track->GetDefinition() == G4Gamma::Definition())
                                   ? step->GetPostStepPoint() : pre;
    G4int id = volume->GetCopyNo();
    G4int module = id / TofDetectorConstruction::kCrystalsPerModule;
    fEventAction->AddEnergyDeposit(id, edep, point->GetGlobalTime(),
                                   fDetector->DepthOfInteraction(module, point->GetPosition()));
}
