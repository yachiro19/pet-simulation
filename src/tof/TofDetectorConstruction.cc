#include "TofDetectorConstruction.hh"
#include "CrystalShape.hh"
#include "TofMaterials.hh"
#include "G4Box.hh"
#include "G4Colour.hh"
#include "G4GenericMessenger.hh"
#include "G4GeometryManager.hh"
#include "G4LogicalBorderSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4OpticalSurface.hh"
#include "G4PVPlacement.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4RotationMatrix.hh"
#include "G4RunManager.hh"
#include "G4SolidStore.hh"
#include "G4SystemOfUnits.hh"
#include "G4VisAttributes.hh"
#include <cmath>
#include <sstream>
#include <iomanip>

TofDetectorConstruction::TofDetectorConstruction()
    : fModuleSize(6.1 * mm),
      fGap(0.1 * mm),
      fLength(20. * mm),
      fDistance(100. * mm),
      fSiPMThickness(0.5 * mm),
      fSourceX(0.),
      fSourceY(0.),
      fSourceZ(0.),
      fSourceSize(1. * mm)
{
    RegisterBuiltinShapes();
    TofMaterials::Instance(); // все материалы создаются до инициализации физики
    DefineCommands();
}

TofDetectorConstruction::~TofDetectorConstruction()
{
    delete fDetMessenger;
    delete fSourceMessenger;
}

G4VPhysicalVolume* TofDetectorConstruction::Construct()
{
    // При повторной сборке (параметры изменены из макроса) удаляем старую геометрию
    G4GeometryManager::GetInstance()->OpenGeometry();
    G4PhysicalVolumeStore::GetInstance()->Clean();
    G4LogicalVolumeStore::GetInstance()->Clean();
    G4SolidStore::GetInstance()->Clean();
    G4LogicalSkinSurface::CleanSurfaceTable();
    G4LogicalBorderSurface::CleanSurfaceTable();

    TofMaterials& materials = TofMaterials::Instance();
    materials.SetWindowIndex(fWindowIndex);
    G4Material* crystalMaterial = materials.Crystal(fMaterialName);
    const CrystalShape* shape = ShapeRegistry::Instance().Find(fShapeName);

    CrystalParams params;
    params.wideHalf = GetCrystalWidth() / 2.;
    params.halfLength = fLength / 2.;
    params.taper = fTaper;
    params.narrowAtSiPM = fNarrowAtSiPM;
    G4double pitch = GetCrystalWidth() + fGap;
    G4double sipmHalf = shape->SiPMFaceHalf(params);

    // --- Мировой объём (воздух) ---
    G4double worldHalfZ = fDistance / 2. + fLength + fSiPMThickness + 10. * mm;
    G4double worldHalfXY = fModuleSize + 10. * mm;
    auto* worldSolid = new G4Box("World", worldHalfXY, worldHalfXY, worldHalfZ);
    auto* worldLV = new G4LogicalVolume(worldSolid, materials.Air(), "World");
    worldLV->SetVisAttributes(G4VisAttributes::GetInvisible());
    auto* worldPV = new G4PVPlacement(nullptr, G4ThreeVector(), worldLV, "World", nullptr, false, 0);

    // --- Источник: водный куб между модулями ---
    if (std::abs(fSourceZ) + fSourceSize / 2. >= fDistance / 2.)
    {
        G4ExceptionDescription ed;
        ed << "Source at z = " << fSourceZ / mm << " mm does not fit between modules "
           << "(distance " << fDistance / mm << " mm)";
        G4Exception("TofDetectorConstruction::Construct", "Tof001", FatalErrorInArgument, ed);
    }
    auto* sourceSolid = new G4Box("Source", fSourceSize / 2., fSourceSize / 2., fSourceSize / 2.);
    auto* sourceLV = new G4LogicalVolume(sourceSolid, materials.Water(), "Source");
    sourceLV->SetVisAttributes(G4VisAttributes(G4Colour(0.0, 0.6, 1.0, 0.6)));
    new G4PVPlacement(nullptr, GetSourcePosition(), sourceLV, "Source", worldLV, false, 0, true);

    // --- Оптические поверхности ---
    G4OpticalSurface* wrapping = MakeWrappingSurface(); // nullptr для "none"
    // Кристалл -> окно SiPM: полированная граница, только френелевское преломление
    auto* coupling = new G4OpticalSurface("CrystalToSiPM", unified, polished, dielectric_dielectric);

    G4VisAttributes crystalVis(G4Colour(1.0, 0.5, 0.0, 0.5));
    crystalVis.SetForceSolid(true);
    G4VisAttributes sipmVis(G4Colour(0.2, 0.2, 1.0, 0.9));
    sipmVis.SetForceSolid(true);

    // --- Модули ---
    // Локальная ось z кристалла направлена от источника к SiPM. Для модуля 0 (z < 0)
    // кристалл и SiPM поворачиваются на 180° вокруг оси y.
    auto* flip = new G4RotationMatrix();
    flip->rotateY(180. * deg);

    for (G4int module = 0; module < kModules; module++)
    {
        G4double sign = (module == 0) ? -1. : 1.;
        G4RotationMatrix* rotation = (module == 0) ? flip : nullptr;

        for (G4int crystal = 0; crystal < kCrystalsPerModule; crystal++)
        {
            G4int id = module * kCrystalsPerModule + crystal;
            G4double x = ((crystal % 2) == 0 ? -0.5 : 0.5) * pitch;
            G4double y = ((crystal / 2) == 0 ? -0.5 : 0.5) * pitch;

            // У каждого кристалла свой логический объём — и своя обёртка
            auto* crystalLV = new G4LogicalVolume(shape->Build("Crystal", params), crystalMaterial, "Crystal");
            crystalLV->SetVisAttributes(crystalVis);
            G4ThreeVector crystalPos(x, y, sign * (fDistance / 2. + fLength / 2.));
            auto* crystalPV = new G4PVPlacement(rotation, crystalPos, crystalLV, "Crystal",
                                                worldLV, false, id, true);
            if (wrapping)
                new G4LogicalSkinSurface("Wrapping_" + std::to_string(id), crystalLV, wrapping);

            // SiPM (оптическое окно) на внешнем торце, размер равен торцу кристалла
            auto* sipmSolid = new G4Box("SiPM", sipmHalf, sipmHalf, fSiPMThickness / 2.);
            auto* sipmLV = new G4LogicalVolume(sipmSolid, materials.Window(), "SiPM");
            sipmLV->SetVisAttributes(sipmVis);
            G4ThreeVector sipmPos(x, y, sign * (fDistance / 2. + fLength + fSiPMThickness / 2.));
            auto* sipmPV = new G4PVPlacement(rotation, sipmPos, sipmLV, "SiPM", worldLV, false, id, true);

            // Граничная поверхность имеет приоритет над обёрткой
            new G4LogicalBorderSurface("Coupling_" + std::to_string(id), crystalPV, sipmPV, coupling);
        }
    }

    G4cout << "[TOF] " << fMaterialName << ", shape " << fShapeName;
    if (fShapeName != "box")
        G4cout << " (taper " << fTaper << ", narrow end at " << (fNarrowAtSiPM ? "SiPM" : "source") << ")";
    G4cout << ", crystal " << GetCrystalWidth() / mm << "x" << GetCrystalWidth() / mm << "x"
           << fLength / mm << " mm, wrapping " << fWrapping << ", modules " << fDistance / mm
           << " mm apart, source at z = " << fSourceZ / mm << " mm" << G4endl;

    return worldPV;
}

// ------------------------------------------------------------
//  Обёртка кристалла (модель unified)
//   esr    — зеркальный отражатель через воздушный зазор (polishedbackpainted):
//            сначала френелевское отражение/ПВО на грани кристалла, затем отражатель
//   teflon — диффузный (ламбертовский) отражатель через воздушный зазор (groundbackpainted)
//   black  — поглощающая краска прямо на грани (polishedfrontpainted, R = 0)
//   mirror — идеальное зеркало прямо на грани (dielectric_metal): без ПВО, каждое
//            отражение с вероятностью R; эталон для проверки модели
//   none   — голый кристалл в воздухе
// ------------------------------------------------------------
G4OpticalSurface* TofDetectorConstruction::MakeWrappingSurface() const
{
    if (fWrapping == "none")
        return nullptr;

    auto* surface = new G4OpticalSurface("Wrapping");
    surface->SetModel(unified);
    surface->SetType(dielectric_dielectric);

    G4double reflectivity = fReflectivity;
    if (fWrapping == "mirror")
    {
        surface->SetType(dielectric_metal);
        surface->SetFinish(polished);
    }
    else if (fWrapping == "esr")
        surface->SetFinish(polishedbackpainted);
    else if (fWrapping == "teflon")
        surface->SetFinish(groundbackpainted);
    else // black
    {
        surface->SetFinish(polishedfrontpainted);
        reflectivity = 0.;
    }
    surface->SetSigmaAlpha(0.); // грани кристалла полированные

    std::vector<G4double> energy = {1.0 * eV, 6.0 * eV};
    auto* mpt = new G4MaterialPropertiesTable();
    mpt->AddProperty("REFLECTIVITY", energy, std::vector<G4double>{reflectivity, reflectivity});
    // Показатель преломления зазора между кристаллом и отражателем (воздух)
    mpt->AddProperty("RINDEX", energy, std::vector<G4double>{1.0, 1.0});
    // Грань кристалла полированная: френелевское и полное внутреннее отражение на ней
    // зеркальные. Без этих констант модель unified при отделке *backpainted выбирает
    // тип отражения с нулевыми вероятностями и делает любое такое отражение
    // ламбертовским (G4OpBoundaryProcess::ChooseReflection) — кристалл вёл бы себя
    // как матовый. Диффузность тефлона задаётся отдельно, на отражении от «краски».
    mpt->AddProperty("SPECULARSPIKECONSTANT", energy, std::vector<G4double>{1.0, 1.0});
    mpt->AddProperty("SPECULARLOBECONSTANT", energy, std::vector<G4double>{0.0, 0.0});
    mpt->AddProperty("BACKSCATTERCONSTANT", energy, std::vector<G4double>{0.0, 0.0});
    surface->SetMaterialPropertiesTable(mpt);
    return surface;
}

G4String TofDetectorConstruction::ConfigTag() const
{
    std::ostringstream tag;
    tag << fMaterialName << "_" << fShapeName;
    if (fShapeName != "box")
        tag << std::fixed << std::setprecision(2) << fTaper << (fNarrowAtSiPM ? "s" : "f");
    return tag.str();
}

G4double TofDetectorConstruction::DepthOfInteraction(G4int module, const G4ThreeVector& globalPos) const
{
    return (module == 0) ? -globalPos.z() - fDistance / 2. : globalPos.z() - fDistance / 2.;
}

// ------------------------------------------------------------
//  Команды макроса
// ------------------------------------------------------------
void TofDetectorConstruction::DefineCommands()
{
    fDetMessenger = new G4GenericMessenger(this, "/tof/det/", "TOF detector geometry");

    fDetMessenger->DeclareMethod("material", &TofDetectorConstruction::SetMaterial,
                                 "Crystal material")
        .SetCandidates(TofMaterials::CrystalNames());
    fDetMessenger->DeclareMethod("shape", &TofDetectorConstruction::SetShape, "Crystal shape")
        .SetCandidates(ShapeRegistry::Instance().Names());
    fDetMessenger->DeclareMethod("narrowEnd", &TofDetectorConstruction::SetNarrowEnd,
                                 "Which end of a tapered crystal is narrow")
        .SetCandidates("sipm source");
    fDetMessenger->DeclareMethod("wrapping", &TofDetectorConstruction::SetWrapping,
                                 "Crystal wrapping")
        .SetCandidates("esr teflon black none mirror");
    fDetMessenger->DeclareMethodWithUnit("moduleSize", "mm", &TofDetectorConstruction::SetModuleSize,
                                         "Side of the square module face (2x2 crystals + gap)");
    fDetMessenger->DeclareMethodWithUnit("gap", "mm", &TofDetectorConstruction::SetGap,
                                         "Gap between crystals in a module");
    fDetMessenger->DeclareMethodWithUnit("length", "mm", &TofDetectorConstruction::SetLength,
                                         "Crystal length");
    fDetMessenger->DeclareMethodWithUnit("distance", "mm", &TofDetectorConstruction::SetDistance,
                                         "Distance between the source-facing faces of the modules");
    fDetMessenger->DeclareMethod("taper", &TofDetectorConstruction::SetTaper,
                                 "Tapered shapes: narrow/wide face ratio (0..1]");
    fDetMessenger->DeclareMethod("reflectivity", &TofDetectorConstruction::SetReflectivity,
                                 "Wrapping reflectivity (esr, teflon)");
    fDetMessenger->DeclareMethod("windowIndex", &TofDetectorConstruction::SetWindowIndex,
                                 "Refractive index of the SiPM optical window / coupling");

    fSourceMessenger = new G4GenericMessenger(this, "/tof/source/", "TOF source position");
    fSourceMessenger->DeclareMethodWithUnit("z", "mm", &TofDetectorConstruction::SetSourceZ,
                                            "Source position along the module axis");
    fSourceMessenger->DeclareMethodWithUnit("x", "mm", &TofDetectorConstruction::SetSourceX,
                                            "Source offset across the axis (x)");
    fSourceMessenger->DeclareMethodWithUnit("y", "mm", &TofDetectorConstruction::SetSourceY,
                                            "Source offset across the axis (y)");
    fSourceMessenger->DeclareMethodWithUnit("size", "mm", &TofDetectorConstruction::SetSourceSize,
                                            "Side of the source cube");
}

void TofDetectorConstruction::GeometryChanged()
{
    G4RunManager::GetRunManager()->ReinitializeGeometry();
}

void TofDetectorConstruction::SetMaterial(const G4String& name)
{
    if (!TofMaterials::Instance().Crystal(name))
    {
        G4Exception("TofDetectorConstruction::SetMaterial", "Tof002", JustWarning,
                    ("Unknown material " + name).c_str());
        return;
    }
    fMaterialName = name;
    GeometryChanged();
}

void TofDetectorConstruction::SetShape(const G4String& name)
{
    if (!ShapeRegistry::Instance().Find(name))
    {
        G4Exception("TofDetectorConstruction::SetShape", "Tof003", JustWarning,
                    ("Unknown shape " + name).c_str());
        return;
    }
    fShapeName = name;
    GeometryChanged();
}

void TofDetectorConstruction::SetNarrowEnd(const G4String& end)
{
    fNarrowAtSiPM = (end == "sipm");
    GeometryChanged();
}

void TofDetectorConstruction::SetWrapping(const G4String& type)
{
    fWrapping = type;
    GeometryChanged();
}

void TofDetectorConstruction::SetModuleSize(G4double value) { fModuleSize = value; GeometryChanged(); }
void TofDetectorConstruction::SetGap(G4double value) { fGap = value; GeometryChanged(); }
void TofDetectorConstruction::SetLength(G4double value) { fLength = value; GeometryChanged(); }
void TofDetectorConstruction::SetDistance(G4double value) { fDistance = value; GeometryChanged(); }
void TofDetectorConstruction::SetReflectivity(G4double value) { fReflectivity = value; GeometryChanged(); }
void TofDetectorConstruction::SetWindowIndex(G4double value) { fWindowIndex = value; GeometryChanged(); }
void TofDetectorConstruction::SetSourceX(G4double value) { fSourceX = value; GeometryChanged(); }
void TofDetectorConstruction::SetSourceY(G4double value) { fSourceY = value; GeometryChanged(); }
void TofDetectorConstruction::SetSourceZ(G4double value) { fSourceZ = value; GeometryChanged(); }
void TofDetectorConstruction::SetSourceSize(G4double value) { fSourceSize = value; GeometryChanged(); }

void TofDetectorConstruction::SetTaper(G4double value)
{
    if (value <= 0. || value > 1.)
    {
        G4Exception("TofDetectorConstruction::SetTaper", "Tof004", JustWarning,
                    "Taper must be in (0, 1]");
        return;
    }
    fTaper = value;
    GeometryChanged();
}
