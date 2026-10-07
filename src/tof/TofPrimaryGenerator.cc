#include "TofPrimaryGenerator.hh"
#include "TofDetectorConstruction.hh"
#include "G4Event.hh"
#include "G4Gamma.hh"
#include "G4GenericMessenger.hh"
#include "G4ParticleGun.hh"
#include "G4PhysicalConstants.hh"
#include "G4Positron.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <cmath>

TofPrimaryGenerator::TofPrimaryGenerator(const TofDetectorConstruction* detector)
    : fDetector(detector), fGun(new G4ParticleGun(1))
{
    fGun->SetParticleTime(0.);

    fMessenger = new G4GenericMessenger(this, "/tof/gun/", "TOF primary source");
    fMessenger->DeclareProperty("type", fSourceType, "gammaPair or positron")
        .SetCandidates("gammaPair positron");
    fMessenger->DeclarePropertyWithUnit("coneAngle", "deg", fConeAngle,
                                        "gammaPair: cone half-angle around the axis (<= 0: auto, covers modules)");
    fMessenger->DeclarePropertyWithUnit("positronEnergy", "keV", fPositronEnergy,
                                        "positron: kinetic energy (0 = annihilation at rest)");
}

TofPrimaryGenerator::~TofPrimaryGenerator()
{
    delete fMessenger;
    delete fGun;
}

void TofPrimaryGenerator::GeneratePrimaries(G4Event* event)
{
    // Точка вылета — равномерно в кубе-источнике
    G4double size = fDetector->GetSourceSize();
    G4ThreeVector position = fDetector->GetSourcePosition() +
                             G4ThreeVector(size * (G4UniformRand() - 0.5),
                                           size * (G4UniformRand() - 0.5),
                                           size * (G4UniformRand() - 0.5));

    if (fSourceType == "positron")
        GeneratePositron(event, position);
    else
        GenerateGammaPair(event, position);
}

G4double TofPrimaryGenerator::ConeHalfAngle() const
{
    if (fConeAngle > 0.)
        return fConeAngle;

    // Конус из самой неудобной точки источника должен накрыть торец ближайшего модуля
    G4ThreeVector source = fDetector->GetSourcePosition();
    G4double halfSize = fDetector->GetSourceSize() / 2.;
    G4double reach = fDetector->GetModuleSize() / std::sqrt(2.) + source.perp() + halfSize * std::sqrt(2.);
    G4double depth = fDetector->GetModuleDistance() / 2. - std::abs(source.z()) - halfSize;
    return std::min(1.1 * std::atan2(reach, depth), 90. * deg);
}

// ------------------------------------------------------------
//  Два гамма-кванта 511 кэВ в противоположных направлениях.
//  Ограничение направлений конусом — это выборка по важности: события, где кванты
//  не попадают в модули, ничего не дают, а в отсутствие рассеивающей среды между
//  источником и детекторами конус не искажает регистрируемые события.
//  Неколлинеарность аннигиляционных квантов (~0.5° FWHM) не учитывается.
// ------------------------------------------------------------
void TofPrimaryGenerator::GenerateGammaPair(G4Event* event, const G4ThreeVector& position)
{
    G4double cosMax = std::cos(ConeHalfAngle());
    G4double cosTheta = 1. - G4UniformRand() * (1. - cosMax);
    G4double sinTheta = std::sqrt(1. - cosTheta * cosTheta);
    G4double phi = twopi * G4UniformRand();
    G4ThreeVector direction(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);

    // Поляризации аннигиляционных квантов взаимно перпендикулярны
    G4ThreeVector e1 = direction.orthogonal().unit();
    G4ThreeVector e2 = direction.cross(e1);
    G4double psi = twopi * G4UniformRand();
    G4ThreeVector polarization1 = std::cos(psi) * e1 + std::sin(psi) * e2;
    G4ThreeVector polarization2 = direction.cross(polarization1).unit();

    fGun->SetParticleDefinition(G4Gamma::Definition());
    fGun->SetParticleEnergy(electron_mass_c2);
    fGun->SetParticlePosition(position);

    fGun->SetParticleMomentumDirection(direction);
    fGun->SetParticlePolarization(polarization1);
    fGun->GeneratePrimaryVertex(event);

    fGun->SetParticleMomentumDirection(-direction);
    fGun->SetParticlePolarization(polarization2);
    fGun->GeneratePrimaryVertex(event);
}

// ------------------------------------------------------------
//  Позитрон: изотропно, аннигиляция в воде куба-источника моделируется Geant4.
//  Конус не применяется (направление квантов определяется аннигиляцией), поэтому
//  в модули попадает лишь малая доля событий (~1e-3 при расстоянии 100 мм).
// ------------------------------------------------------------
void TofPrimaryGenerator::GeneratePositron(G4Event* event, const G4ThreeVector& position)
{
    G4double cosTheta = 2. * G4UniformRand() - 1.;
    G4double sinTheta = std::sqrt(1. - cosTheta * cosTheta);
    G4double phi = twopi * G4UniformRand();

    fGun->SetParticleDefinition(G4Positron::Definition());
    fGun->SetParticleEnergy(fPositronEnergy);
    fGun->SetParticlePosition(position);
    fGun->SetParticleMomentumDirection(G4ThreeVector(sinTheta * std::cos(phi),
                                                     sinTheta * std::sin(phi), cosTheta));
    fGun->SetParticlePolarization(G4ThreeVector());
    fGun->GeneratePrimaryVertex(event);
}
