#ifndef TofSteppingAction_h
#define TofSteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "globals.hh"

class TofDetectorConstruction;
class TofEventAction;

// Энерговыделение в кристаллах и регистрация оптических фотонов в SiPM
class TofSteppingAction : public G4UserSteppingAction
{
public:
    TofSteppingAction(const TofDetectorConstruction* detector, TofEventAction* eventAction);
    ~TofSteppingAction() override = default;

    void UserSteppingAction(const G4Step* step) override;

private:
    const TofDetectorConstruction* fDetector;
    TofEventAction* fEventAction;
};

#endif
