#ifndef TofPrimaryGenerator_h
#define TofPrimaryGenerator_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

class G4GenericMessenger;
class G4ParticleGun;
class TofDetectorConstruction;

// Источник режима tof: точка, равномерно разыгранная в кубе-источнике.
//   gammaPair (по умолчанию) — два гамма-кванта 511 кэВ в противоположных направлениях
//                              с перпендикулярными поляризациями; направление
//                              разыгрывается в конусе вокруг оси модулей
//   positron                 — позитрон (изотропно), аннигиляцию моделирует Geant4
class TofPrimaryGenerator : public G4VUserPrimaryGeneratorAction
{
public:
    explicit TofPrimaryGenerator(const TofDetectorConstruction* detector);
    ~TofPrimaryGenerator() override;

    void GeneratePrimaries(G4Event* event) override;

    const G4String& GetSourceType() const { return fSourceType; }
    // Фактический полуугол конуса (с учётом автоматического выбора)
    G4double ConeHalfAngle() const;

private:
    void GenerateGammaPair(G4Event* event, const G4ThreeVector& position);
    void GeneratePositron(G4Event* event, const G4ThreeVector& position);

    const TofDetectorConstruction* fDetector;
    G4ParticleGun* fGun;
    G4GenericMessenger* fMessenger;

    G4String fSourceType = "gammaPair";
    G4double fConeAngle = -1.;   // <= 0 — автоматически, чтобы конус накрыл модули
    G4double fPositronEnergy = 0.;
};

#endif
