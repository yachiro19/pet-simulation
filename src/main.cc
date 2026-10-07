#include "G4RunManager.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "DetectorConstruction.hh"
#include "PhysicsList.hh"
#include "ActionInitialization.hh"
#include "TofDetectorConstruction.hh"
#include "TofPhysicsList.hh"
#include "TofActionInitialization.hh"
#include <iostream>
#include <string>

static int PrintUsage(const char* exe)
{
    G4cerr << "Usage: " << exe << " [-m ring|tof] [macro_file]" << G4endl;
    return 1;
}

int main(int argc, char** argv)
{
    // Режим: ring — кольцевой ПЭТ (по умолчанию), tof — два модуля 2x2 для изучения CTR
    G4String mode = "ring";
    G4String macro;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-m" && i + 1 < argc)
            mode = argv[++i];
        else if (macro.empty())
            macro = arg;
        else
            return PrintUsage(argv[0]);
    }
    if (mode != "ring" && mode != "tof")
        return PrintUsage(argv[0]);

    G4RunManager* runManager = new G4RunManager();
    if (mode == "tof") {
        auto* detector = new TofDetectorConstruction();
        runManager->SetUserInitialization(detector);
        runManager->SetUserInitialization(new TofPhysicsList());
        runManager->SetUserInitialization(new TofActionInitialization(detector));
    } else {
        runManager->SetUserInitialization(new DetectorConstruction());
        runManager->SetUserInitialization(new PhysicsList());
        runManager->SetUserInitialization(new ActionInitialization());
    }

    G4UImanager* UI = G4UImanager::GetUIpointer();

    // Проверяем аргументы командной строки
    if (macro.empty()) {
        // Интерактивный режим с визуализацией
        G4VisManager* visManager = new G4VisExecutive("quiet");
        visManager->Initialize();

        G4UIExecutive* ui = new G4UIExecutive(argc, argv);

        // Загружаем макрос визуализации
        UI->ApplyCommand(mode == "tof" ? "/control/execute tof/vis.mac" : "/control/execute run.mac");

        // Запускаем интерактивную сессию
        ui->SessionStart();

        delete ui;
        delete visManager;
    }
    else {
        // Пакетный режим - запуск макроса
        G4String command = "/control/execute ";
        command += macro;
        UI->ApplyCommand(command);
    }

    delete runManager;
    return 0;
}
