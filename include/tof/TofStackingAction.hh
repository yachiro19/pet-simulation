#ifndef TofStackingAction_h
#define TofStackingAction_h 1

#include "G4UserStackingAction.hh"

// Ускорение счёта: оптические фотоны, рождённые позже /tof/opt/timeCut
// (по глобальному времени события), сразу уничтожаются. Временную отметку задают
// первые фотоны, а поздние сцинтилляционные фотоны (LYSO 40 нс, BGO 300 нс)
// составляют большую часть времени счёта.
class TofStackingAction : public G4UserStackingAction
{
public:
    G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* track) override;
};

#endif
