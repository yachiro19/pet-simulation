#include "TofActionInitialization.hh"
#include "TofEventAction.hh"
#include "TofPrimaryGenerator.hh"
#include "TofRunAction.hh"
#include "TofSettings.hh"
#include "TofStackingAction.hh"
#include "TofSteppingAction.hh"

TofActionInitialization::TofActionInitialization(const TofDetectorConstruction* detector)
    : fDetector(detector)
{
}

void TofActionInitialization::Build() const
{
    TofSettings::Instance(); // команды /tof/sipm/, /tof/output/, /tof/opt/

    auto* generator = new TofPrimaryGenerator(fDetector);
    auto* runAction = new TofRunAction(fDetector, generator);
    auto* eventAction = new TofEventAction(runAction);

    SetUserAction(generator);
    SetUserAction(runAction);
    SetUserAction(eventAction);
    SetUserAction(new TofSteppingAction(fDetector, eventAction));
    SetUserAction(new TofStackingAction());
}
