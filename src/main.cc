#include "G4RunManager.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "G4UIExecutive.hh"
#include "DetectorConstruction.hh"
#include "PhysicsList.hh"
#include "ActionInitialization.hh"
#include <iostream>

int main(int argc, char** argv)
{
    G4RunManager* runManager = new G4RunManager();
    runManager->SetUserInitialization(new DetectorConstruction());
    runManager->SetUserInitialization(new PhysicsList());
    runManager->SetUserInitialization(new ActionInitialization());

    G4UImanager* UI = G4UImanager::GetUIpointer();
    
    // Проверяем аргументы командной строки
    if (argc == 1) {
        // Интерактивный режим с визуализацией
        G4VisManager* visManager = new G4VisExecutive("quiet");
        visManager->Initialize();
        
        G4UIExecutive* ui = new G4UIExecutive(argc, argv);
        
        // Загружаем макрос визуализации
        UI->ApplyCommand("/control/execute run.mac");
        
        // Запускаем интерактивную сессию
        ui->SessionStart();
        
        delete ui;
        delete visManager;
    } 
    else if (argc == 2) {
        // Пакетный режим - запуск макроса
        G4String command = "/control/execute ";
        command += argv[1];
        UI->ApplyCommand(command);
    }
    else {
        G4cerr << "Usage: " << argv[0] << " [macro_file]" << G4endl;
        return 1;
    }

    delete runManager;
    return 0;
}