#ifndef EventAction_h
#define EventAction_h 1

#include "G4UserEventAction.hh"
#include "G4Event.hh"
#include "globals.hh"
#include <vector>

class EventAction : public G4UserEventAction
{
public:
    EventAction();
    virtual ~EventAction();
    virtual void BeginOfEventAction(const G4Event *);
    virtual void EndOfEventAction(const G4Event *);

    void AddHit(G4int crystalID, G4double energy, G4double time);
    void AddPhotonHit(G4int sipmID, G4double energy, G4double time);

    G4double GetTotalEnergy() const { return fTotalEnergyDeposit; }
    const std::vector<G4double> &GetEnergies() const { return fEnergies; }
    const std::vector<G4int> &GetCrystalIDs() const { return fCrystalIDs; }
    const std::vector<G4double> &GetTimes() const { return fTimes; }
    G4int GetPhotonCount() const { return fPhotonCount; }
    G4int GetNumGammaHits() const { return fNumGammaHits; }

private:
    G4double fTotalEnergyDeposit;
    std::vector<G4double> fEnergies;
    std::vector<G4int> fCrystalIDs;
    std::vector<G4double> fTimes;
    G4int fPhotonCount;
    G4double fPhotonTotalEnergy;
    G4int fNumGammaHits;
};
#endif
