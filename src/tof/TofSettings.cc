#include "TofSettings.hh"
#include "G4GenericMessenger.hh"
#include "G4SystemOfUnits.hh"
#include <algorithm>

TofSettings& TofSettings::Instance()
{
    static TofSettings instance;
    return instance;
}

TofSettings::TofSettings()
    : fSptrFwhm(70. * ps), fTimeCut(5. * ns)
{
    // ------------------------------------------------------------
    // PDE NUV-SiPM (FBK NUV-HD, перенапряжение ~6 В) в зависимости от длины волны.
    // Аппроксимация по графику из A. Gola et al., "NUV-Sensitive Silicon
    // Photomultiplier Technologies Developed at Fondazione Bruno Kessler",
    // Sensors 19 (2019) 308. Значения приблизительные; ниже ~270 нм свет
    // поглощается защитным покрытием.
    // ------------------------------------------------------------
    fPdeCurve = {{250., 0.00}, {270., 0.10}, {300., 0.25}, {330., 0.38}, {360., 0.48},
                 {390., 0.54}, {420., 0.55}, {450., 0.52}, {480., 0.47}, {520., 0.40},
                 {560., 0.33}, {600., 0.27}, {650., 0.20}, {700., 0.14}};

    // SPTR по умолчанию 70 пс FWHM — порядок величины для современных NUV-SiPM
    // с быстрой электроникой (S. Gundacker et al., PMB 65 (2020) 025001)
    fSipmMessenger = new G4GenericMessenger(this, "/tof/sipm/", "SiPM response");
    fSipmMessenger->DeclareProperty("pdeModel", fPdeModel, "nuvhd (spectral curve) or flat")
        .SetCandidates("nuvhd flat");
    fSipmMessenger->DeclareProperty("pdeScale", fPdeScale, "Scale factor for the PDE curve");
    fSipmMessenger->DeclareProperty("pde", fFlatPde, "PDE for pdeModel flat");
    fSipmMessenger->DeclarePropertyWithUnit("sptr", "ps", fSptrFwhm,
                                            "Single photon time resolution, FWHM (Gaussian)");

    fOutputMessenger = new G4GenericMessenger(this, "/tof/output/", "TOF output");
    fOutputMessenger->DeclareProperty("dir", fOutputDir, "Output directory for CSV files");
    fOutputMessenger->DeclareProperty("tag", fTag, "Extra suffix for output file names");
    fOutputMessenger->DeclareProperty("nFirst", fNFirst, "Number of first photon times to store");
    fOutputMessenger->DeclareProperty("autoSeed", fAutoSeed,
                                      "Seed each run from its configuration (reproducible)");

    fOptMessenger = new G4GenericMessenger(this, "/tof/opt/", "Optical photon tracking");
    fOptMessenger->DeclarePropertyWithUnit("timeCut", "ns", fTimeCut,
                                           "Kill optical photons created later than this (0 = off)");
}

TofSettings::~TofSettings()
{
    delete fSipmMessenger;
    delete fOutputMessenger;
    delete fOptMessenger;
}

G4double TofSettings::DetectionProbability(G4double photonEnergy) const
{
    G4double pde;
    if (fPdeModel == "flat")
    {
        pde = fFlatPde;
    }
    else
    {
        G4double nm = 1239.84193 / (photonEnergy / eV);
        if (nm <= fPdeCurve.front().first)
            pde = fPdeCurve.front().second;
        else if (nm >= fPdeCurve.back().first)
            pde = fPdeCurve.back().second;
        else
        {
            auto hi = std::lower_bound(fPdeCurve.begin(), fPdeCurve.end(), nm,
                                       [](const auto& point, G4double x) { return point.first < x; });
            auto lo = hi - 1;
            G4double f = (nm - lo->first) / (hi->first - lo->first);
            pde = lo->second + f * (hi->second - lo->second);
        }
    }
    return std::clamp(pde * fPdeScale, 0., 1.);
}
