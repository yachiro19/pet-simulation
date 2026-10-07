#ifndef TofPhysicsList_h
#define TofPhysicsList_h 1

#include "G4VModularPhysicsList.hh"

// Физика режима tof: электромагнитная (option4) + оптическая (сцинтилляция и Черенков).
// Адронная физика не нужна для гамма-квантов 511 кэВ и не подключается.
class TofPhysicsList : public G4VModularPhysicsList
{
public:
    TofPhysicsList();
    ~TofPhysicsList() override = default;
};

#endif
