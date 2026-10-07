#ifndef TofMaterials_h
#define TofMaterials_h 1

#include "globals.hh"
#include <map>

class G4Material;

// Материалы режима tof с оптическими свойствами.
// Все кристаллы создаются сразу (до /run/initialize), чтобы оптические процессы
// построили таблицы для каждого из них и материал можно было менять между прогонами.
class TofMaterials
{
public:
    static TofMaterials& Instance();

    G4Material* Crystal(const G4String& name) const; // LYSO, BGO, PbF2; nullptr, если нет
    G4Material* Air() const { return fAir; }
    G4Material* Water() const { return fWater; }
    G4Material* Window() const { return fWindow; }

    // Показатель преломления оптического окна SiPM (смазка/эпоксид)
    void SetWindowIndex(G4double n);

    static G4String CrystalNames() { return "LYSO BGO PbF2"; }

private:
    TofMaterials();
    G4Material* BuildLYSO();
    G4Material* BuildBGO();
    G4Material* BuildPbF2();

    std::map<G4String, G4Material*> fCrystals;
    G4Material* fAir = nullptr;
    G4Material* fWater = nullptr;
    G4Material* fWindow = nullptr;
};

#endif
