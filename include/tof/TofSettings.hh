#ifndef TofSettings_h
#define TofSettings_h 1

#include "globals.hh"
#include <utility>
#include <vector>

class G4GenericMessenger;

// Параметры регистрации и вывода режима tof (команды /tof/sipm/, /tof/output/, /tof/opt/)
class TofSettings
{
public:
    static TofSettings& Instance();

    // Вероятность регистрации фотона SiPM: PDE(E) * pdeScale, в пределах [0, 1]
    G4double DetectionProbability(G4double photonEnergy) const;
    G4double GetSptrFwhm() const { return fSptrFwhm; }
    G4double GetSptrSigma() const { return fSptrFwhm / 2.354820; }
    const G4String& GetPdeModel() const { return fPdeModel; }
    G4double GetPdeScale() const { return fPdeScale; }

    G4double GetTimeCut() const { return fTimeCut; }
    G4int GetNFirst() const { return fNFirst; }
    const G4String& GetOutputDir() const { return fOutputDir; }
    const G4String& GetTag() const { return fTag; }
    G4bool GetAutoSeed() const { return fAutoSeed; }

private:
    TofSettings();
    ~TofSettings();

    G4GenericMessenger* fSipmMessenger;
    G4GenericMessenger* fOutputMessenger;
    G4GenericMessenger* fOptMessenger;

    G4String fPdeModel = "nuvhd"; // nuvhd | flat
    G4double fPdeScale = 1.;
    G4double fFlatPde = 0.5;
    G4double fSptrFwhm;
    G4double fTimeCut;
    G4int fNFirst = 10;
    G4String fOutputDir = "tof_output";
    G4String fTag;
    G4bool fAutoSeed = true;

    std::vector<std::pair<G4double, G4double>> fPdeCurve; // (длина волны, нм; PDE)
};

#endif
