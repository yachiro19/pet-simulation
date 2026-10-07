#include "TofMaterials.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4NistManager.hh"
#include "G4SystemOfUnits.hh"
#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================
//  Источники значений
// ============================================================
// [SG-LYSO] Saint-Gobain, PreLude 420 (LYSO:Ce) data sheet.
// [SG-BGO]  Saint-Gobain, BGO data sheet.
// [Mao08]   R. Mao, L. Zhang, R.-Y. Zhu, "Optical and scintillation properties of
//           inorganic scintillators in high energy physics", IEEE TNS 55 (2008) 2425.
// [Gun18]   S. Gundacker et al., "Measurement of intrinsic rise times for various
//           L(Y)SO and LuAG scintillators...", NIM A 891 (2018) 42.
// [Gun20]   S. Gundacker et al., "Experimental time resolution limits of modern SiPMs
//           and TOF-PET detectors exploring different scintillators and Cherenkov
//           emission", Phys. Med. Biol. 65 (2020) 025001.
// [Wil96]   P.A. Williams et al., "Optical, thermo-optic, electro-optic, and
//           photoelastic properties of bismuth germanate (Bi4Ge3O12)",
//           Appl. Opt. 35 (1996) 3562.
// [Ach98]   P. Achenbach et al., "Radiation resistance and optical properties of lead
//           fluoride Cherenkov crystals", NIM A 416 (1998) 357.
//
// Кривые n(λ) построены однотермовой формулой Селлмейера по двум опорным точкам,
// края пропускания — логистической ступенькой. Это аппроксимации по опубликованным
// значениям и графикам, а не оцифрованные измерения; опорные числа указаны ниже.
// ============================================================

namespace
{
const G4double kHcEvNm = 1239.84193; // h*c, эВ·нм

// Сетка длин волн 250–700 нм (шаг 10 нм) в порядке возрастания энергии фотона
std::vector<G4double> WavelengthsNm()
{
    std::vector<G4double> nm;
    for (G4double l = 700.; l >= 250.; l -= 10.)
        nm.push_back(l);
    return nm;
}

G4double EnergyOf(G4double nm) { return kHcEvNm / nm * eV; }

// Однотермовая формула Селлмейера n^2 = 1 + B*λ^2 / (λ^2 - C), λ в мкм.
// B и C подбираются так, чтобы кривая прошла через две опорные точки.
struct Sellmeier
{
    G4double B, C;

    Sellmeier(G4double nm1, G4double n1, G4double nm2, G4double n2)
    {
        G4double x1 = std::pow(nm1 / 1000., 2), x2 = std::pow(nm2 / 1000., 2);
        G4double k1 = n1 * n1 - 1., k2 = n2 * n2 - 1.;
        C = (k1 - k2) / (k1 / x1 - k2 / x2);
        B = k1 * (x1 - C) / x1;
    }
    G4double operator()(G4double nm) const
    {
        G4double x = std::pow(nm / 1000., 2);
        return std::sqrt(1. + B * x / (x - C));
    }
};

// Длина поглощения: bulk выше края пропускания edgeNm, резкий спад ниже него
G4double AbsLength(G4double nm, G4double edgeNm, G4double bulk)
{
    G4double f = 1. / (1. + std::exp(-(nm - edgeNm) / 3.));
    return std::max(bulk * f, 1. * um);
}

// Спектр сцинтилляции — гауссиана по длине волны (нормировка не нужна)
G4double Emission(G4double nm, G4double peakNm, G4double sigmaNm)
{
    return std::exp(-0.5 * std::pow((nm - peakNm) / sigmaNm, 2));
}

struct OpticalSpec
{
    Sellmeier rindex;
    G4double edgeNm, bulkAbs;
};

// Таблицы RINDEX и ABSLENGTH по общей сетке
G4MaterialPropertiesTable* MakeBulkTable(const OpticalSpec& spec)
{
    std::vector<G4double> energy, rindex, absl;
    for (G4double nm : WavelengthsNm())
    {
        energy.push_back(EnergyOf(nm));
        rindex.push_back(spec.rindex(nm));
        absl.push_back(AbsLength(nm, spec.edgeNm, spec.bulkAbs));
    }
    auto* mpt = new G4MaterialPropertiesTable();
    mpt->AddProperty("RINDEX", energy, rindex);
    mpt->AddProperty("ABSLENGTH", energy, absl);
    return mpt;
}

void AddScintillation(G4MaterialPropertiesTable* mpt, G4double peakNm, G4double sigmaNm,
                      G4double yieldPerMeV, G4double riseTime, G4double decayTime)
{
    std::vector<G4double> energy, spectrum;
    for (G4double nm : WavelengthsNm())
    {
        energy.push_back(EnergyOf(nm));
        spectrum.push_back(Emission(nm, peakNm, sigmaNm));
    }
    mpt->AddProperty("SCINTILLATIONCOMPONENT1", energy, spectrum);
    mpt->AddConstProperty("SCINTILLATIONYIELD", yieldPerMeV / MeV);
    mpt->AddConstProperty("RESOLUTIONSCALE", 1.0);
    mpt->AddConstProperty("SCINTILLATIONTIMECONSTANT1", decayTime);
    mpt->AddConstProperty("SCINTILLATIONRISETIME1", riseTime);
    mpt->AddConstProperty("SCINTILLATIONYIELD1", 1.0);
}

// Постоянный показатель преломления для окна SiPM и воздуха
G4MaterialPropertiesTable* MakeConstTable(G4double n, G4double absLength)
{
    std::vector<G4double> energy = {1.0 * eV, 6.0 * eV};
    auto* mpt = new G4MaterialPropertiesTable();
    mpt->AddProperty("RINDEX", energy, std::vector<G4double>{n, n});
    mpt->AddProperty("ABSLENGTH", energy, std::vector<G4double>{absLength, absLength});
    return mpt;
}
} // namespace

TofMaterials& TofMaterials::Instance()
{
    static TofMaterials instance;
    return instance;
}

TofMaterials::TofMaterials()
{
    G4NistManager* nist = G4NistManager::Instance();

    fAir = nist->FindOrBuildMaterial("G4_AIR");
    fAir->SetMaterialPropertiesTable(MakeConstTable(1.0, 100. * m));

    // Вода — только материал куба-источника; без оптических свойств,
    // поэтому оптические фотоны в нём не рождаются
    fWater = nist->FindOrBuildMaterial("G4_WATER");

    // Окно SiPM: эпоксид/оптическая смазка, n ~ 1.5 (меняется командой /tof/det/windowIndex)
    fWindow = new G4Material("TOF_Window", 1.16 * g / cm3, 3);
    fWindow->AddElement(nist->FindOrBuildElement("C"), 11);
    fWindow->AddElement(nist->FindOrBuildElement("H"), 12);
    fWindow->AddElement(nist->FindOrBuildElement("O"), 3);
    SetWindowIndex(1.5);

    fCrystals["LYSO"] = BuildLYSO();
    fCrystals["BGO"] = BuildBGO();
    fCrystals["PbF2"] = BuildPbF2();
}

G4Material* TofMaterials::Crystal(const G4String& name) const
{
    auto it = fCrystals.find(name);
    return it == fCrystals.end() ? nullptr : it->second;
}

void TofMaterials::SetWindowIndex(G4double n)
{
    fWindow->SetMaterialPropertiesTable(MakeConstTable(n, 1. * m));
}

// ------------------------------------------------------------
//  LYSO:Ce (Lu1.8Y0.2SiO5), сцинтиллятор + черенковский излучатель
// ------------------------------------------------------------
G4Material* TofMaterials::BuildLYSO()
{
    G4NistManager* nist = G4NistManager::Instance();
    // Плотность 7.1 г/см3 [SG-LYSO]; Ce (~0.1 %) в составе не учитывается
    auto* lyso = new G4Material("TOF_LYSO", 7.1 * g / cm3, 4);
    lyso->AddElement(nist->FindOrBuildElement("Lu"), 18);
    lyso->AddElement(nist->FindOrBuildElement("Y"), 2);
    lyso->AddElement(nist->FindOrBuildElement("Si"), 10);
    lyso->AddElement(nist->FindOrBuildElement("O"), 50);

    // n(420 нм) = 1.82 [SG-LYSO]; вторая точка n(600 нм) = 1.80 — оценка типичной
    // для оксиортосиликатов дисперсии ([Mao08]), уточнить по измеренной кривой
    // Край пропускания ~390 нм (собственное поглощение, [Mao08]); длина поглощения
    // в прозрачной области принята 40 см
    OpticalSpec spec{Sellmeier(420., 1.82, 600., 1.80), 390., 40. * cm};
    G4MaterialPropertiesTable* mpt = MakeBulkTable(spec);

    // Эмиссия: пик 420 нм [SG-LYSO], ширина σ ≈ 30 нм (оценка по спектру [SG-LYSO])
    // Световыход 32 фотона/кэВ, время высвечивания 40 нс [SG-LYSO]
    // Время нарастания ~70 пс [Gun18]
    AddScintillation(mpt, 420., 30., 32000., 70. * ps, 40. * ns);

    lyso->SetMaterialPropertiesTable(mpt);
    return lyso;
}

// ------------------------------------------------------------
//  BGO (Bi4Ge3O12): медленный сцинтиллятор, но с заметным черенковским вкладом
// ------------------------------------------------------------
G4Material* TofMaterials::BuildBGO()
{
    G4NistManager* nist = G4NistManager::Instance();
    // Плотность 7.13 г/см3 [SG-BGO]
    auto* bgo = new G4Material("TOF_BGO", 7.13 * g / cm3, 3);
    bgo->AddElement(nist->FindOrBuildElement("Bi"), 4);
    bgo->AddElement(nist->FindOrBuildElement("Ge"), 3);
    bgo->AddElement(nist->FindOrBuildElement("O"), 12);

    // n(480 нм) = 2.15 [SG-BGO]; n(633 нм) ≈ 2.10 — по дисперсии из [Wil96] (приближённо)
    // Край пропускания ~310 нм [Mao08], [Gun20]; длина поглощения 40 см
    OpticalSpec spec{Sellmeier(480., 2.15, 633., 2.10), 310., 40. * cm};
    G4MaterialPropertiesTable* mpt = MakeBulkTable(spec);

    // Эмиссия: пик 480 нм [SG-BGO], σ ≈ 40 нм (оценка)
    // Световыход 8.5 фотона/кэВ, время высвечивания 300 нс [SG-BGO]
    // Нарастание принято мгновенным (упрощение): временную отметку BGO
    // в основном задают черенковские фотоны [Gun20]
    AddScintillation(mpt, 480., 40., 8500., 0., 300. * ns);

    bgo->SetMaterialPropertiesTable(mpt);
    return bgo;
}

// ------------------------------------------------------------
//  PbF2: чистый черенковский излучатель (не сцинтиллирует)
// ------------------------------------------------------------
G4Material* TofMaterials::BuildPbF2()
{
    G4NistManager* nist = G4NistManager::Instance();
    // Плотность 7.77 г/см3 [Ach98]
    auto* pbf2 = new G4Material("TOF_PbF2", 7.77 * g / cm3, 2);
    pbf2->AddElement(nist->FindOrBuildElement("Pb"), 1);
    pbf2->AddElement(nist->FindOrBuildElement("F"), 2);

    // n(400 нм) = 1.82 и n(589 нм) = 1.77 [Ach98]
    // Край пропускания ~250 нм [Ach98]: сетка начинается с 250 нм, поэтому UV-край
    // фактически задаётся границей сетки; длина поглощения 40 см
    OpticalSpec spec{Sellmeier(400., 1.82, 589., 1.77), 255., 40. * cm};
    G4MaterialPropertiesTable* mpt = MakeBulkTable(spec);
    // Без SCINTILLATIONYIELD — G4Scintillation в этом материале не работает

    pbf2->SetMaterialPropertiesTable(mpt);
    return pbf2;
}
