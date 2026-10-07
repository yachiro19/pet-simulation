#ifndef TofEventAction_h
#define TofEventAction_h 1

#include "G4UserEventAction.hh"
#include "TofDetectorConstruction.hh"
#include "globals.hh"
#include <array>
#include <vector>

class TofRunAction;

// Происхождение оптического фотона по процессу-создателю
enum class PhotonOrigin : char
{
    Scintillation = 'S',
    Cherenkov = 'C',
    Other = 'O'
};

struct DetectedPhoton
{
    G4double time;
    PhotonOrigin origin;
};

// Данные одного кристалла за событие
struct CrystalRecord
{
    G4double edep = 0.;
    G4double gammaTime = -1.; // время первого энерговыделения (< 0 — не было)
    G4double doi = 0.;        // глубина первого энерговыделения от торца со стороны источника
    G4int nScint = 0;
    G4int nCherenkov = 0;
    std::vector<DetectedPhoton> photons;
};

class TofEventAction : public G4UserEventAction
{
public:
    explicit TofEventAction(TofRunAction* runAction);
    ~TofEventAction() override = default;

    void BeginOfEventAction(const G4Event*) override;
    void EndOfEventAction(const G4Event* event) override;

    void AddEnergyDeposit(G4int crystalID, G4double edep, G4double time, G4double doi);
    void AddPhoton(G4int crystalID, G4double time, PhotonOrigin origin);

private:
    TofRunAction* fRunAction;
    std::array<CrystalRecord, TofDetectorConstruction::kCrystals> fCrystals;
};

#endif
