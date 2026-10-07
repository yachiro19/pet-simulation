#ifndef TofActionInitialization_h
#define TofActionInitialization_h 1

#include "G4VUserActionInitialization.hh"

class TofDetectorConstruction;

class TofActionInitialization : public G4VUserActionInitialization
{
public:
    explicit TofActionInitialization(const TofDetectorConstruction* detector);
    ~TofActionInitialization() override = default;
    void Build() const override;

private:
    const TofDetectorConstruction* fDetector;
};

#endif
