#include "TofActionInitialization.hh"
#include "TofPrimaryGenerator.hh"

TofActionInitialization::TofActionInitialization(const TofDetectorConstruction* detector)
    : fDetector(detector)
{
}

void TofActionInitialization::Build() const
{
    SetUserAction(new TofPrimaryGenerator(fDetector));
}
