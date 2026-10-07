#include "CrystalShape.hh"
#include "G4Box.hh"
#include "G4Trd.hh"

G4VSolid* BoxShape::Build(const G4String& name, const CrystalParams& p) const
{
    return new G4Box(name, p.wideHalf, p.wideHalf, p.halfLength);
}

G4double BoxShape::SiPMFaceHalf(const CrystalParams& p) const
{
    return p.wideHalf;
}

G4VSolid* TrdShape::Build(const G4String& name, const CrystalParams& p) const
{
    G4double narrowHalf = p.wideHalf * p.taper;
    G4double atSource = p.narrowAtSiPM ? p.wideHalf : narrowHalf;
    G4double atSiPM = p.narrowAtSiPM ? narrowHalf : p.wideHalf;
    // G4Trd: (dx1, dy1) — торец при -dz (источник), (dx2, dy2) — при +dz (SiPM)
    return new G4Trd(name, atSource, atSiPM, atSource, atSiPM, p.halfLength);
}

G4double TrdShape::SiPMFaceHalf(const CrystalParams& p) const
{
    return p.narrowAtSiPM ? p.wideHalf * p.taper : p.wideHalf;
}

ShapeRegistry& ShapeRegistry::Instance()
{
    static ShapeRegistry instance;
    return instance;
}

void ShapeRegistry::Register(const G4String& name, std::unique_ptr<CrystalShape> shape)
{
    fShapes[name] = std::move(shape);
}

const CrystalShape* ShapeRegistry::Find(const G4String& name) const
{
    auto it = fShapes.find(name);
    return it == fShapes.end() ? nullptr : it->second.get();
}

G4String ShapeRegistry::Names() const
{
    G4String names;
    for (const auto& entry : fShapes)
    {
        if (!names.empty())
            names += " ";
        names += entry.first;
    }
    return names;
}

void RegisterBuiltinShapes()
{
    static G4bool registered = false;
    if (registered)
        return;
    registered = true;

    auto& registry = ShapeRegistry::Instance();
    registry.Register("box", std::make_unique<BoxShape>());
    registry.Register("trd", std::make_unique<TrdShape>());
}
