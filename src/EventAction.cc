#include "EventAction.hh"
#include "G4Event.hh"
#include "G4SystemOfUnits.hh"
#include <fstream>

EventAction::EventAction()
    : G4UserEventAction(),
      fTotalEnergyDeposit(0.),
      fPhotonCount(0),
      fPhotonTotalEnergy(0.),
      fNumGammaHits(0)
{
}

EventAction::~EventAction() {}

void EventAction::BeginOfEventAction(const G4Event *)
{
    fTotalEnergyDeposit = 0.;
    fEnergies.clear();
    fCrystalIDs.clear();
    fTimes.clear();
    fPhotonCount = 0;
    fPhotonTotalEnergy = 0.;
    fNumGammaHits = 0;
}

void EventAction::AddHit(G4int crystalID, G4double energy, G4double time)
{
    fTotalEnergyDeposit += energy;
    fEnergies.push_back(energy);
    fCrystalIDs.push_back(crystalID);
    fTimes.push_back(time);
    fNumGammaHits++; // считаем только гамма-хиты
}

void EventAction::AddPhotonHit(G4int /*sipmID*/, G4double energy, G4double /*time*/)
{
    fPhotonCount++;
    fPhotonTotalEnergy += energy;
}

void EventAction::EndOfEventAction(const G4Event *event)
{
    if (fTotalEnergyDeposit <= 0.)
        return;

    static std::ofstream dataFile("pet_data.csv");
    static bool headerWritten = false;
    if (!headerWritten)
    {
        dataFile << "EventID,TotalEnergy_keV,NumGammaHits,CrystalIDs,"
                    "Energies_keV,Times_ns,PhotonCount,PhotonEnergy_eV\n";
        headerWritten = true;
    }

    dataFile << event->GetEventID() << ","
             << fTotalEnergyDeposit / keV << ","
             << fNumGammaHits << ","; // <-- теперь только гамма-хиты

    for (size_t i = 0; i < fCrystalIDs.size(); i++)
        dataFile << fCrystalIDs[i] << (i < fCrystalIDs.size() - 1 ? ";" : "");
    dataFile << ",";

    for (size_t i = 0; i < fEnergies.size(); i++)
        dataFile << fEnergies[i] / keV << (i < fEnergies.size() - 1 ? ";" : "");
    dataFile << ",";

    for (size_t i = 0; i < fTimes.size(); i++)
        dataFile << fTimes[i] / ns << (i < fTimes.size() - 1 ? ";" : "");
    dataFile << ",";

    dataFile << fPhotonCount << ","
             << fPhotonTotalEnergy / eV << "\n";
    dataFile.flush();
}
