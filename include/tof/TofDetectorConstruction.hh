#ifndef TofDetectorConstruction_h
#define TofDetectorConstruction_h 1

#include "G4VUserDetectorConstruction.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

class G4GenericMessenger;
class G4OpticalSurface;

// Установка режима tof: два модуля 2x2 кристаллов друг напротив друга на оси z.
// Модуль 0 — при z < 0, модуль 1 — при z > 0. Номер кристалла (copy number кристалла
// и его SiPM) = 4 * module + crystal. Все параметры задаются командами /tof/det/ и
// /tof/source/; их изменение пересобирает геометрию перед следующим /run/beamOn.
class TofDetectorConstruction : public G4VUserDetectorConstruction
{
public:
    TofDetectorConstruction();
    ~TofDetectorConstruction() override;

    G4VPhysicalVolume* Construct() override;

    static constexpr G4int kModules = 2;
    static constexpr G4int kCrystalsPerModule = 4;
    static constexpr G4int kCrystals = kModules * kCrystalsPerModule;

    G4ThreeVector GetSourcePosition() const { return G4ThreeVector(fSourceX, fSourceY, fSourceZ); }
    G4double GetSourceSize() const { return fSourceSize; }
    G4double GetModuleDistance() const { return fDistance; }
    G4double GetModuleSize() const { return fModuleSize; }
    G4double GetCrystalWidth() const { return (fModuleSize - fGap) / 2.; } // широкий торец
    G4double GetCrystalLength() const { return fLength; }
    G4double GetTaper() const { return fTaper; }
    G4bool IsNarrowAtSiPM() const { return fNarrowAtSiPM; }
    const G4String& GetMaterialName() const { return fMaterialName; }
    const G4String& GetShapeName() const { return fShapeName; }
    const G4String& GetWrapping() const { return fWrapping; }

    // Краткое имя конфигурации для имён файлов: LYSO_box, BGO_trd0.50s и т.п.
    G4String ConfigTag() const;

    // Глубина точки от торца кристалла, обращённого к источнику
    G4double DepthOfInteraction(G4int module, const G4ThreeVector& globalPos) const;

private:
    void DefineCommands();
    void GeometryChanged();
    G4OpticalSurface* MakeWrappingSurface() const;

    void SetMaterial(const G4String& name);
    void SetShape(const G4String& name);
    void SetNarrowEnd(const G4String& end);
    void SetWrapping(const G4String& type);
    void SetModuleSize(G4double value);
    void SetGap(G4double value);
    void SetLength(G4double value);
    void SetDistance(G4double value);
    void SetTaper(G4double value);
    void SetReflectivity(G4double value);
    void SetWindowIndex(G4double value);
    void SetSourceX(G4double value);
    void SetSourceY(G4double value);
    void SetSourceZ(G4double value);
    void SetSourceSize(G4double value);

    G4GenericMessenger* fDetMessenger = nullptr;
    G4GenericMessenger* fSourceMessenger = nullptr;

    G4String fMaterialName = "LYSO";
    G4String fShapeName = "box";
    G4String fWrapping = "esr";
    G4double fModuleSize;   // сторона квадратного торца модуля (2x2 кристалла + зазор)
    G4double fGap;          // зазор между кристаллами модуля
    G4double fLength;       // длина кристалла
    G4double fDistance;     // расстояние между торцами модулей, обращёнными к источнику
    G4double fTaper = 0.5;  // для trd: отношение узкого торца к широкому
    G4bool fNarrowAtSiPM = true;
    G4double fReflectivity = 0.98;
    G4double fWindowIndex = 1.5;
    G4double fSiPMThickness;
    G4double fSourceX, fSourceY, fSourceZ, fSourceSize;
};

#endif
