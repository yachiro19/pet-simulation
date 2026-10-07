#ifndef TofRunAction_h
#define TofRunAction_h 1

#include "G4UserRunAction.hh"
#include "globals.hh"
#include <chrono>
#include <fstream>

class TofDetectorConstruction;
class TofPrimaryGenerator;
struct CrystalRecord;

// Один CSV-файл на прогон (/run/beamOn): tof_<конфигурация>_z<положение>mm.csv
// Строка — один кристалл в одном событии; параметры конфигурации повторяются в
// каждой строке, чтобы файлы разных прогонов можно было просто объединить.
class TofRunAction : public G4UserRunAction
{
public:
    TofRunAction(const TofDetectorConstruction* detector, const TofPrimaryGenerator* generator);
    ~TofRunAction() override = default;

    void BeginOfRunAction(const G4Run* run) override;
    void EndOfRunAction(const G4Run* run) override;

    void WriteCrystal(G4int eventID, G4int crystalID, const CrystalRecord& crystal);

private:
    G4String OutputFileName() const;

    const TofDetectorConstruction* fDetector;
    const TofPrimaryGenerator* fGenerator;
    std::ofstream fFile;
    G4String fFileName;
    G4String fRunColumns; // значения столбцов конфигурации для текущего прогона
    G4int fRunID = 0;
    G4int fNFirst = 0;
    G4long fRows = 0;
    std::chrono::steady_clock::time_point fStart;
};

#endif
