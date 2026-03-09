#include "disk_manager.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cctype>  
#include "fdisk.h"
#include "mount.h"
#include "mkfs.h"       

// Función para convertir string a minúsculas
std::string toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// Función para remover comillas de una cadena
std::string removeQuotes(const std::string& str) {
    if (str.length() >= 2 && 
        ((str.front() == '"' && str.back() == '"') || 
         (str.front() == '\'' && str.back() == '\''))) {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

// Función para parsear parámetros con soporte para comillas y cualquier orden
std::string parseParameter(const std::string& commandLine, const std::string& paramName) {
    // Buscar el parámetro
    std::string lowerCommandLine = toLowerCase(commandLine);
    std::string lowerParamName = toLowerCase(paramName);
    
    size_t pos = lowerCommandLine.find(lowerParamName + "=");
    if (pos == std::string::npos) {
        return "";
    }
    
    // Encontrar el inicio del valor
    size_t valueStart = pos + paramName.length() + 1;
    if (valueStart >= commandLine.length()) {
        return "";
    }
    
    // Determinar el final del valor
    size_t valueEnd = commandLine.length();
    
    if (commandLine[valueStart] == '"' || commandLine[valueStart] == '\'') {
        char quote = commandLine[valueStart];
        size_t quoteEnd = commandLine.find(quote, valueStart + 1);
        if (quoteEnd != std::string::npos) {
            return commandLine.substr(valueStart + 1, quoteEnd - valueStart - 1);
        } else {
 
            size_t spacePos = commandLine.find(' ', valueStart + 1);
            if (spacePos != std::string::npos) {
                valueEnd = spacePos;
            }
            return commandLine.substr(valueStart + 1, valueEnd - valueStart - 1);
        }
    } else {
        // Si no hay comillas, buscar el siguiente espacio o final de línea
        size_t spacePos = commandLine.find(' ', valueStart);
        if (spacePos != std::string::npos) {
            valueEnd = spacePos;
        }
        return commandLine.substr(valueStart, valueEnd - valueStart);
    }
}

// Función para parsear y ejecutar comandos
std::string executeCommand(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string cmd;
    iss >> cmd;
    
    // Convertir comando a minúsculas para comparación case-insensitive
    cmd = toLowerCase(cmd);

    //COMANDO MKDISK
    if (cmd == "mkdisk") {
        // Parsear parámetros
        std::string sizeStr = parseParameter(commandLine, "-size");
        std::string unit = parseParameter(commandLine, "-unit");
        std::string path = parseParameter(commandLine, "-path");
        
        // Validar parámetros obligatorios
        if (sizeStr.empty() || path.empty()) {
            return "Error: mkdisk requiere parámetros -size y -path\n"
                   "Uso: mkdisk -size=N -unit=[k|m] -path=ruta\n"
                   "Los parámetros pueden estar en cualquier orden";
        }
        
        // Convertir size a entero
        int size;
        try {
            size = std::stoi(sizeStr);
        } catch (const std::exception& e) {
            return "Error: el valor de size debe ser un número entero positivo";
        }
        
        if (size <= 0) {
            return "Error: el tamaño debe ser un número positivo";
        }
        
        // Unit por defecto es megabytes si no se especifica
        if (unit.empty()) {
            unit = "m";
        } else {
            unit = toLowerCase(unit);
        }
        
        // Validar unidad
        if (unit != "k" && unit != "m") {
            return "Error: unit debe ser 'k' (kilobytes) o 'm' (megabytes)";
        }

        return DiskManager::mkdisk(size, unit, path);

    //COMANDO RMDISK
    } else if (cmd == "rmdisk") {
        std::string path = parseParameter(commandLine, "-path");

        if (path.empty()) {
            return "Error: rmdisk requiere parámetro -path\n"
                   "Uso: rmdisk -path=ruta";
        }

        return DiskManager::rmdisk(path);

    //COMANDO FDISK
    } else if (cmd == "fdisk"){
        std::string ruta = parseParameter(commandLine, "-path");
        std::string nombre = parseParameter(commandLine, "-name");
        std::string deleteName = parseParameter(commandLine, "-delete");

        //Validacion Parametros
        if (ruta.empty()){
            return "Error: fdisk requiere parametro -path\n"
            "Uso: fdisk -size=N -unit=[k|m] -path=ruta -type=[P|E|L] -fit=[BF|FF|WF] -name=nombre\n"
            "fdisk -delete=nombre -path=ruta";
        }

        //Eliminacion
        if (nombre.empty()) {
            return "Error: fdisk requiere parametro -name o -delete\n"
                   "Uso: fdisk -size=N -unit=[k|m] -path=ruta -type=[P|E|L] -fit=[BF|FF|WF] -name=nombre\n"
                   "fdisk -delete=nombre -path=ruta";
        }

        std::string tamanoS = parseParameter(commandLine, "-size");
        if (tamanoS.empty()) {
            return "Error: fdisk requiere parametro -size para crear particiones\n"
                   "Uso: fdisk -size=N -unit=[k|m] -path=ruta -type=[P|E|L] -fit=[BF|FF|WF] -name=nombre";
        }

        int tamano;
        try {
            tamano = std::stoi(tamanoS);
        } catch (const std::exception& e) {
            return "Error: el valor de size debe ser un número entero positivo";
        }

        if (tamano <= 0) {
            return "Error: el tamaño debe ser un número positivo";
        }

        std::string unidad = parseParameter(commandLine, "-unit");
        if (unidad.empty()) {
            unidad = "k";
        } else {
            unidad = toLowerCase(unidad);
        }

        if (unidad != "k" && unidad != "m") {
            return "Error: unit debe ser 'k' (kilobytes) o 'm' (megabytes)";
        }

        std::string tipo = parseParameter(commandLine, "-type");
        if (tipo.empty()) {
            tipo = "P";
        } else {
            tipo = toLowerCase(tipo);
        }

        std::string fit = parseParameter(commandLine, "-fit");
        if (fit.empty()) {
            fit = "WF";
        } else {
            fit = toLowerCase(fit);
        }

        return ComandoFdisk::execute(tamano, unidad, ruta, tipo, fit, "", nombre);

    //COMANDO MOUNT
    } else if (cmd == "mount"){
        std::string ruta = parseParameter(commandLine, "-path");
        std::string nombre = parseParameter(commandLine, "-name");
        
        if (ruta.empty() || nombre.empty()) {
            return "Error: mount requiere parámetros -path y -name\n"
                   "Uso: mount -path=ruta -name=nombre";
        }
        
        return ComandoMount::execute(ruta, nombre);

    //COMANDO MKFS
    } else if (cmd == "mkfs") {
        std::string id = parseParameter(commandLine, "-id");
        std::string type = parseParameter(commandLine, "-type");
        
        if (id.empty()) {
            return "Error: mkfs requiere el parámetro -id\n"
                   "Uso: mkfs -id=id [-type=full]";
        }
        
        return CommandMkfs::execute(id, type);
    
    //COMANDO MOUNTED
    } else if (cmd == "mounted"){
        return ComandoMount::listMountedPartitions();

    //COMANDO INFO
    } else if (cmd == "info") {
        std::string path = parseParameter(commandLine, "-path");

        if (path.empty()) {
            return "Error: info requiere parámetro -path\n"
                   "Uso: info -path=ruta";
        }

        return DiskManager::getDiskInfo(path);

    //COMANDO SALIR
    } else if (cmd == "exit" || cmd == "quit") {
        return "EXIT";
    } else if (cmd.empty()) {
        return "";
    } else {
        return "Error: Comando no reconocido";
    }
}

int main(int argc, char* argv[]) {

    srand(time(nullptr));

    std::cout << "C++ DISK\n";
    std::cout << "MIA Proyecto 1 - 2026\n";
    std::cout << "Escriba 'exit' para salir\n";

    std::string commandLine;
    
    while (true) {
        std::cout << "> ";
        std::cout.flush(); 
        
        if (!std::getline(std::cin, commandLine)) {

            std::cout << "\nSaliendo del programa...\n";
            break;
        }

        if (commandLine.empty()) {
            continue;
        }

        // Ejecutar comando
        std::string result = executeCommand(commandLine);

        if (result == "EXIT") {
            std::cout << "Saliendo del programa...\n";
            break;
        }

        // Mostrar resultado
        if (!result.empty()) {
            std::cout << result << "\n\n";
        }
    }

    return 0;
}