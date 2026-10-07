#include "TofRunAction.hh"
#include "TofDetectorConstruction.hh"
#include "TofEventAction.hh"
#include "TofPrimaryGenerator.hh"
#include "TofSettings.hh"
#include "G4Run.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace
{
// FNV-1a: стабильный хеш строки (std::hash зависит от компилятора)
std::uint64_t StableHash(const std::string& text)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : text)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}
} // namespace

TofRunAction::TofRunAction(const TofDetectorConstruction* detector, const TofPrimaryGenerator* generator)
    : fDetector(detector), fGenerator(generator)
{
}

G4String TofRunAction::OutputFileName() const
{
    const TofSettings& settings = TofSettings::Instance();
    std::ostringstream name;
    name << "tof_" << fDetector->ConfigTag();
    if (!settings.GetTag().empty())
        name << "_" << settings.GetTag();
    name << "_z" << std::showpos << std::fixed << std::setprecision(2)
         << fDetector->GetSourcePosition().z() / mm << "mm.csv";
    return (std::filesystem::path(std::string(settings.GetOutputDir())) / name.str()).string();
}

void TofRunAction::BeginOfRunAction(const G4Run* run)
{
    const TofSettings& settings = TofSettings::Instance();
    fRunID = run->GetRunID();
    fNFirst = settings.GetNFirst();
    fRows = 0;
    fFileName = OutputFileName();

    // Воспроизводимое и разное для разных конфигураций зерно генератора
    if (settings.GetAutoSeed())
    {
        long seed = static_cast<long>(StableHash(fFileName) % 2147483647ULL) + 1;
        G4Random::setTheSeed(seed);
        G4cout << "[TOF] Random seed " << seed << G4endl;
    }

    std::filesystem::create_directories(std::string(settings.GetOutputDir()));
    fFile.open(fFileName, std::ios::out | std::ios::trunc);
    if (!fFile)
    {
        G4Exception("TofRunAction::BeginOfRunAction", "Tof101", FatalException,
                    ("Cannot open output file " + fFileName).c_str());
    }

    fFile << "run,event,material,shape,taper,narrow_end,wrapping,crystal_mm,length_mm,"
             "distance_mm,sptr_ps,pde_model,pde_scale,time_cut_ns,source,src_z_mm,"
             "module,crystal,edep_keV,t_gamma_ps,doi_mm,n_scint,n_cher,"
             "t_first_ps,t_first_scint_ps,t_first_cher_ps";
    for (G4int i = 1; i <= fNFirst; i++)
        fFile << ",t" << std::setw(2) << std::setfill('0') << i << "_ps";
    fFile << std::setfill(' ') << ",types\n";

    std::ostringstream columns;
    columns << fDetector->GetMaterialName() << "," << fDetector->GetShapeName() << ","
            << fDetector->GetTaper() << "," << (fDetector->IsNarrowAtSiPM() ? "sipm" : "source") << ","
            << fDetector->GetWrapping() << "," << fDetector->GetCrystalWidth() / mm << ","
            << fDetector->GetCrystalLength() / mm << "," << fDetector->GetModuleDistance() / mm << ","
            << settings.GetSptrFwhm() / ps << "," << settings.GetPdeModel() << ","
            << settings.GetPdeScale() << "," << settings.GetTimeCut() / ns << ","
            << fGenerator->GetSourceType() << "," << fDetector->GetSourcePosition().z() / mm;
    fRunColumns = columns.str();

    fStart = std::chrono::steady_clock::now();
}

void TofRunAction::WriteCrystal(G4int eventID, G4int crystalID, const CrystalRecord& crystal)
{
    auto timeOrEmpty = [](G4double t) {
        std::ostringstream s;
        if (t >= 0.)
            s << std::fixed << std::setprecision(2) << t / ps;
        return s.str();
    };

    G4double firstScint = -1., firstCherenkov = -1.;
    for (const auto& photon : crystal.photons)
    {
        if (photon.origin == PhotonOrigin::Scintillation && firstScint < 0.)
            firstScint = photon.time;
        if (photon.origin == PhotonOrigin::Cherenkov && firstCherenkov < 0.)
            firstCherenkov = photon.time;
    }

    G4int module = crystalID / TofDetectorConstruction::kCrystalsPerModule;
    G4int inModule = crystalID % TofDetectorConstruction::kCrystalsPerModule;

    fFile << fRunID << "," << eventID << "," << fRunColumns << "," << module << "," << inModule << ","
          << std::fixed << std::setprecision(3) << crystal.edep / keV << ","
          << timeOrEmpty(crystal.gammaTime) << ","
          << std::setprecision(3) << (crystal.gammaTime >= 0. ? crystal.doi / mm : 0.) << ","
          << crystal.nScint << "," << crystal.nCherenkov << ","
          << timeOrEmpty(crystal.photons.empty() ? -1. : crystal.photons.front().time) << ","
          << timeOrEmpty(firstScint) << "," << timeOrEmpty(firstCherenkov);
    fFile.unsetf(std::ios::fixed);

    std::string types;
    for (G4int i = 0; i < fNFirst; i++)
    {
        fFile << ",";
        if (i < static_cast<G4int>(crystal.photons.size()))
        {
            fFile << timeOrEmpty(crystal.photons[i].time);
            types += static_cast<char>(crystal.photons[i].origin);
        }
    }
    fFile << "," << types << "\n";
    fRows++;
}

void TofRunAction::EndOfRunAction(const G4Run* run)
{
    fFile.close();
    G4double seconds = std::chrono::duration<G4double>(std::chrono::steady_clock::now() - fStart).count();
    G4int events = run->GetNumberOfEvent();
    G4cout << "[TOF] Run " << fRunID << ": " << events << " events, " << fRows << " rows -> "
           << fFileName << "\n[TOF] Wall time " << std::fixed << std::setprecision(1) << seconds
           << " s (" << std::setprecision(2) << (events > 0 ? 1000. * seconds / events : 0.)
           << " ms/event)" << G4endl;
    G4cout.unsetf(std::ios::fixed);
}
