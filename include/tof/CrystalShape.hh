#ifndef CrystalShape_h
#define CrystalShape_h 1

#include "globals.hh"
#include <map>
#include <memory>

class G4VSolid;

// Параметры кристалла, общие для всех форм.
// В локальных координатах кристалла торец, обращённый к источнику, лежит при
// z = -halfLength, торец с SiPM — при z = +halfLength.
struct CrystalParams
{
    G4double wideHalf = 0.;     // половина стороны широкого (квадратного) торца
    G4double halfLength = 0.;   // половина длины кристалла
    G4double taper = 1.;        // отношение узкого торца к широкому (для сужающихся форм)
    G4bool narrowAtSiPM = true; // узкий торец со стороны SiPM (иначе — со стороны источника)
};

// Форма кристалла.
// Чтобы добавить новую форму: унаследоваться от CrystalShape, реализовать Build()
// и SiPMFaceHalf() и зарегистрировать класс в RegisterBuiltinShapes().
// После этого форма становится доступна в макросе: /tof/det/shape <имя>
class CrystalShape
{
public:
    virtual ~CrystalShape() = default;
    virtual G4VSolid* Build(const G4String& name, const CrystalParams& p) const = 0;
    // Половина стороны торца со стороны SiPM — по ней задаётся размер SiPM
    virtual G4double SiPMFaceHalf(const CrystalParams& p) const = 0;
};

// Прямоугольный параллелепипед (параметр taper игнорируется)
class BoxShape : public CrystalShape
{
public:
    G4VSolid* Build(const G4String& name, const CrystalParams& p) const override;
    G4double SiPMFaceHalf(const CrystalParams& p) const override;
};

// Усечённая пирамида (G4Trd): один торец wideHalf, другой wideHalf * taper
class TrdShape : public CrystalShape
{
public:
    G4VSolid* Build(const G4String& name, const CrystalParams& p) const override;
    G4double SiPMFaceHalf(const CrystalParams& p) const override;
};

class ShapeRegistry
{
public:
    static ShapeRegistry& Instance();
    void Register(const G4String& name, std::unique_ptr<CrystalShape> shape);
    const CrystalShape* Find(const G4String& name) const;
    G4String Names() const; // имена через пробел — для списка допустимых значений команды

private:
    ShapeRegistry() = default;
    std::map<G4String, std::unique_ptr<CrystalShape>> fShapes;
};

void RegisterBuiltinShapes();

#endif
