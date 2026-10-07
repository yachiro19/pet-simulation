#ifndef DetectorConstruction_h
#define DetectorConstruction_h 1

#include "G4VUserDetectorConstruction.hh"
#include "G4Material.hh"
#include "G4LogicalVolume.hh"
#include <vector>

class DetectorConstruction : public G4VUserDetectorConstruction
{
public:
    DetectorConstruction();
    virtual ~DetectorConstruction();
    virtual G4VPhysicalVolume* Construct();

    const std::vector<G4LogicalVolume*>& GetCrystalLogVols() const { return fCrystalLogVols; }
    G4int GetNumberOfCrystals() const { return fNumberOfCrystals; }

private:
    G4Material* CreateScintillatorMaterial(G4String name);
    void CreatePETRing(G4LogicalVolume* worldLog, G4Material* scintMaterial);
    std::vector<G4LogicalVolume*> fCrystalLogVols;
    G4int fNumberOfCrystals;
};

#endif
