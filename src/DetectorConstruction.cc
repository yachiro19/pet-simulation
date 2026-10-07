#include "DetectorConstruction.hh"
#include "G4NistManager.hh"
#include "G4Material.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4RotationMatrix.hh"
#include "G4OpticalSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4LogicalBorderSurface.hh"
#include "G4MaterialPropertiesTable.hh"
#include "CLHEP/Units/PhysicalConstants.h"
#include <cmath>

DetectorConstruction::DetectorConstruction() : fNumberOfCrystals(24)
{
    fCrystalLogVols.reserve(1);
}

DetectorConstruction::~DetectorConstruction() {}

// ============================================================
//  Создание материала сцинтиллятора
// ============================================================
G4Material *DetectorConstruction::CreateScintillatorMaterial(G4String name)
{
    if (name == "LYSO")
    {
        G4Element *Lu = new G4Element("Lutetium", "Lu", 71, 174.97 * g / mole);
        G4Element *Y = new G4Element("Yttrium", "Y", 39, 88.91 * g / mole);
        G4Element *Si = new G4Element("Silicon", "Si", 14, 28.09 * g / mole);
        G4Element *O = new G4Element("Oxygen", "O", 8, 16.00 * g / mole);
        G4Element *Ce = new G4Element("Cerium", "Ce", 58, 140.12 * g / mole);

        G4Material *LYSO = new G4Material("LYSO", 7.1 * g / cm3, 5);
        LYSO->AddElement(Lu, 0.71);
        LYSO->AddElement(Y, 0.04);
        LYSO->AddElement(Si, 0.07);
        LYSO->AddElement(O, 0.18);
        LYSO->AddElement(Ce, 0.001);

        // --------------------------------------------------------
        // Оптические свойства LYSO
        // Энергии фотонов соответствуют диапазону ~300–600 нм
        // (основной пик сцинтилляции LYSO ~420 нм = 2.95 эВ)
        // --------------------------------------------------------
        const G4int nEntries = 12;

        G4double photonEnergy[nEntries] = {
            2.07 * eV, 2.25 * eV, 2.48 * eV, 2.58 * eV,
            2.70 * eV, 2.87 * eV, 2.95 * eV, 3.06 * eV,
            3.18 * eV, 3.31 * eV, 3.44 * eV, 3.54 * eV};
        // Соответствующие длины волн для справки:
        // 599, 551, 500, 481, 459, 432, 420, 405, 390, 375, 360, 350 нм

        // --- Показатель преломления LYSO (~1.82) ---
        G4double refractiveIndex[nEntries] = {
            1.81, 1.81, 1.82, 1.82,
            1.82, 1.82, 1.82, 1.83,
            1.83, 1.83, 1.84, 1.84};

        // --- Длина поглощения (LYSO очень прозрачен ~30–50 см) ---
        G4double absorptionLength[nEntries] = {
            380. * cm, 380. * cm, 370. * cm, 360. * cm,
            350. * cm, 330. * cm, 300. * cm, 250. * cm,
            200. * cm, 150. * cm, 100. * cm, 50. * cm};

        // --- Спектр сцинтилляции (нормированный, пик на 420 нм) ---
        G4double scintilFast[nEntries] = {
            0.01, 0.02, 0.10, 0.20,
            0.40, 0.70, 1.00, 0.70,
            0.40, 0.15, 0.05, 0.01};

        G4MaterialPropertiesTable *lysoMPT = new G4MaterialPropertiesTable();
        lysoMPT->AddProperty("RINDEX", photonEnergy, refractiveIndex, nEntries);
        lysoMPT->AddProperty("ABSLENGTH", photonEnergy, absorptionLength, nEntries);
        lysoMPT->AddProperty("SCINTILLATIONCOMPONENT1", photonEnergy, scintilFast, nEntries);

        // Световыход LYSO: ~32000 фотонов/МэВ
        // Для скорости симуляции можно уменьшить через /process/optical/scint/setYieldFactor
        lysoMPT->AddConstProperty("SCINTILLATIONYIELD", 320. / MeV); // 32000
        lysoMPT->AddConstProperty("RESOLUTIONSCALE", 1.0);
        // Время сцинтилляции LYSO: быстрая компонента ~40 нс
        lysoMPT->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 40. * ns);
        lysoMPT->AddConstProperty("SCINTILLATIONYIELD1", 1.0); // 100% быстрая

        LYSO->SetMaterialPropertiesTable(lysoMPT);
        return LYSO;
    }
    else if (name == "BGO")
    {
        return G4NistManager::Instance()->FindOrBuildMaterial("G4_BGO");
    }
    return nullptr;
}

// ============================================================
//  Оптические свойства воздуха (между кристаллами)
// ============================================================
static void SetAirOpticalProperties(G4Material *air)
{
    const G4int n = 2;
    G4double energy[n] = {2.0 * eV, 4.0 * eV};
    G4double rindex[n] = {1.0, 1.0};
    G4double absl[n] = {1000. * m, 1000. * m};

    G4MaterialPropertiesTable *mpt = new G4MaterialPropertiesTable();
    mpt->AddProperty("RINDEX", energy, rindex, n);
    mpt->AddProperty("ABSLENGTH", energy, absl, n);
    air->SetMaterialPropertiesTable(mpt);
}

// ============================================================
//  Оптические свойства воды (центральный регион)
// ============================================================
static void SetWaterOpticalProperties(G4Material *water)
{
    const G4int n = 4;
    G4double energy[n] = {2.0 * eV, 2.5 * eV, 3.0 * eV, 3.5 * eV};
    G4double rindex[n] = {1.333, 1.334, 1.336, 1.340};
    G4double absl[n] = {10. * m, 10. * m, 10. * m, 10. * m};

    G4MaterialPropertiesTable *mpt = new G4MaterialPropertiesTable();
    mpt->AddProperty("RINDEX", energy, rindex, n);
    mpt->AddProperty("ABSLENGTH", energy, absl, n);
    water->SetMaterialPropertiesTable(mpt);
}

// ============================================================
//  Основная функция построения геометрии
// ============================================================
void DetectorConstruction::CreatePETRing(G4LogicalVolume * /*worldLog*/, G4Material * /*scintMaterial*/)
{
    // Реализация вынесена в Construct()
}

G4VPhysicalVolume *DetectorConstruction::Construct()
{
    G4NistManager *nist = G4NistManager::Instance();
    G4Material *air = nist->FindOrBuildMaterial("G4_AIR");
    G4Material *water = nist->FindOrBuildMaterial("G4_WATER");

    // Оптические свойства воздуха и воды
    SetAirOpticalProperties(air);
    SetWaterOpticalProperties(water);

    // --- Мировой объём ---
    G4double worldSize = 500. * mm;
    G4Box *solidWorld = new G4Box("World", worldSize / 2, worldSize / 2, worldSize / 2);
    G4LogicalVolume *logicWorld = new G4LogicalVolume(solidWorld, air, "World");
    G4VPhysicalVolume *physWorld =
        new G4PVPlacement(0, G4ThreeVector(), logicWorld, "World", 0, false, 0);

    // --- Материал кристалла с оптическими свойствами ---
    G4Material *lyso = CreateScintillatorMaterial("LYSO");

    // --- Центральный регион (вода, место аннигиляции позитрона) ---
    G4double centerSize = 50. * mm;
    G4Box *solidCenter = new G4Box("CenterRegion", centerSize / 2, centerSize / 2, centerSize / 2);
    G4LogicalVolume *centerLog =
        new G4LogicalVolume(solidCenter, water, "CenterRegion");
    new G4PVPlacement(0, G4ThreeVector(), centerLog, "CenterRegion", logicWorld, false, 0);

    G4VisAttributes *centerVis = new G4VisAttributes(G4Colour(0.0, 0.5, 1.0, 0.2));
    centerVis->SetVisibility(true);
    centerLog->SetVisAttributes(centerVis);

    G4VisAttributes *worldVis = new G4VisAttributes(G4Colour(1.0, 1.0, 1.0, 0.05));
    worldVis->SetVisibility(true);
    logicWorld->SetVisAttributes(worldVis);

    // ============================================================
    //  Кристаллы LYSO
    // ============================================================
    G4double crystalX = 10. * mm;
    G4double crystalY = 10. * mm;
    G4double crystalZ = 30. * mm; // длинная ось — радиально
    G4double ringRadius = 150. * mm;

    G4Box *solidCrystal = new G4Box("Crystal", crystalX / 2, crystalY / 2, crystalZ / 2);
    G4LogicalVolume *crystalLog =
        new G4LogicalVolume(solidCrystal, lyso, "CrystalLog");

    G4VisAttributes *crystalVis = new G4VisAttributes(G4Colour(1.0, 0.4, 0.0, 0.7));
    crystalVis->SetVisibility(true);
    crystalVis->SetForceSolid(true);
    crystalLog->SetVisAttributes(crystalVis);

    fCrystalLogVols.push_back(crystalLog);

    // ============================================================
    //  Обёртка кристалла (отражающая поверхность — Teflon/ESR)
    //  Моделируется как G4OpticalSurface типа unified + lambertian
    //  Отражательная способность ~98% (типично для ESR плёнки)
    // ============================================================
    G4OpticalSurface *wrappingSurface = new G4OpticalSurface("WrappingSurface");
    wrappingSurface->SetType(dielectric_metal); // граница диэлектрик–металл
    wrappingSurface->SetFinish(polished);       // зеркальное отражение
    wrappingSurface->SetModel(glisur);

    const G4int nWrap = 2;
    G4double wrapEnergy[nWrap] = {2.0 * eV, 4.0 * eV};
    G4double wrapReflectivity[nWrap] = {0.98, 0.98}; // 98% отражение
    G4double wrapEfficiency[nWrap] = {0.0, 0.0};     // не поглощает

    G4MaterialPropertiesTable *wrapMPT = new G4MaterialPropertiesTable();
    wrapMPT->AddProperty("REFLECTIVITY", wrapEnergy, wrapReflectivity, nWrap);
    wrapMPT->AddProperty("EFFICIENCY", wrapEnergy, wrapEfficiency, nWrap);
    wrappingSurface->SetMaterialPropertiesTable(wrapMPT);

    // Применяем обёртку как skin surface — ко всей внешней поверхности кристалла
    new G4LogicalSkinSurface("CrystalWrapping", crystalLog, wrappingSurface);

    // ============================================================
    //  Фотодетектор SiPM
    //  Размещается вплотную к торцу кристалла (наружному)
    //  Материал — кремний, размер 10×10×1 мм
    // ============================================================
    G4Material *silicon = nist->FindOrBuildMaterial("G4_Si");

    // Оптические свойства кремния (SiPM): квантовая эффективность ~40%
    const G4int nSi = 4;
    G4double siEnergy[nSi] = {2.0 * eV, 2.5 * eV, 3.0 * eV, 3.5 * eV};
    G4double siRindex[nSi] = {3.5, 3.6, 3.7, 3.8};
    G4double siAbsl[nSi] = {0.1 * mm, 0.1 * mm, 0.1 * mm, 0.1 * mm};

    G4MaterialPropertiesTable *siMPT = new G4MaterialPropertiesTable();
    siMPT->AddProperty("RINDEX", siEnergy, siRindex, nSi);
    siMPT->AddProperty("ABSLENGTH", siEnergy, siAbsl, nSi);
    silicon->SetMaterialPropertiesTable(siMPT);

    G4double sipmZ = 1. * mm;
    G4Box *solidSiPM = new G4Box("SiPM", crystalX / 2, crystalY / 2, sipmZ / 2);
    G4LogicalVolume *sipmLog =
        new G4LogicalVolume(solidSiPM, silicon, "SiPMLog");

    G4VisAttributes *sipmVis = new G4VisAttributes(G4Colour(0.2, 0.2, 1.0, 0.9));
    sipmVis->SetVisibility(true);
    sipmVis->SetForceSolid(true);
    sipmLog->SetVisAttributes(sipmVis);

    // Поверхность между кристаллом и SiPM:
    // dielectric_metal, квантовая эффективность ~40%
    G4OpticalSurface *sipmSurface = new G4OpticalSurface("SiPMSurface");
    sipmSurface->SetType(dielectric_metal);
    sipmSurface->SetFinish(polished);
    sipmSurface->SetModel(glisur);

    const G4int nDet = 4;
    G4double detEnergy[nDet] = {2.0 * eV, 2.5 * eV, 3.0 * eV, 3.5 * eV};
    G4double detReflect[nDet] = {0.0, 0.0, 0.0, 0.0};        // не отражает
    G4double detEfficiency[nDet] = {0.40, 0.42, 0.40, 0.35}; // QE ~40%

    G4MaterialPropertiesTable *sipmMPT = new G4MaterialPropertiesTable();
    sipmMPT->AddProperty("REFLECTIVITY", detEnergy, detReflect, nDet);
    sipmMPT->AddProperty("EFFICIENCY", detEnergy, detEfficiency, nDet);
    sipmSurface->SetMaterialPropertiesTable(sipmMPT);

    // ============================================================
    //  Размещение кристаллов и SiPM по кольцу
    // ============================================================
    for (G4int i = 0; i < fNumberOfCrystals; i++)
    {
        G4double angle = i * 2.0 * CLHEP::pi / fNumberOfCrystals;

        // Позиция центра кристалла
        G4ThreeVector crystalPos(ringRadius * std::cos(angle),
                                 ringRadius * std::sin(angle),
                                 0.0);

        // Поворот: длинная ось Z_local → радиально
        G4ThreeVector tangAxis(-std::sin(angle), std::cos(angle), 0.);
        G4RotationMatrix *rotMatrix = new G4RotationMatrix();
        rotMatrix->rotate(-CLHEP::pi / 2.0, tangAxis);

        // Размещаем кристалл
        G4VPhysicalVolume *crystalPhys =
            new G4PVPlacement(rotMatrix, crystalPos, crystalLog,
                              "Crystal_" + std::to_string(i),
                              logicWorld, false, i);

        // SiPM размещается на наружном торце кристалла
        // Наружный торец: crystalPos + radialDir * (crystalZ/2 + sipmZ/2)
        G4ThreeVector radialDir(std::cos(angle), std::sin(angle), 0.);
        G4ThreeVector sipmPos = crystalPos + radialDir * (crystalZ / 2. + sipmZ / 2.);

        G4VPhysicalVolume *sipmPhys =
            new G4PVPlacement(rotMatrix, sipmPos, sipmLog,
                              "SiPM_" + std::to_string(i),
                              logicWorld, false, i);

        // Граница кристалл → SiPM (border surface, направленная)
        new G4LogicalBorderSurface(
            "Crystal_SiPM_" + std::to_string(i),
            crystalPhys, sipmPhys, sipmSurface);
    }

    G4cout << "[Detector] PET Ring: " << fNumberOfCrystals
           << " LYSO crystals + SiPM detectors, full optical simulation" << G4endl;
    return physWorld;
}
