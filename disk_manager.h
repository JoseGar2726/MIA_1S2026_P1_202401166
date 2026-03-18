#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include <string>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/stat.h>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include "structures.h"

class DiskManager {
private:

    static std::string expandPath(const std::string& path) {
        if (path.empty() || path[0] != '~') {
            return path;
        }
        
        const char* home = std::getenv("HOME");
        if (!home) {
            std::cerr << "Error: No se pudo obtener el directorio HOME" << std::endl;
            return path;
        }
        
        return std::string(home) + path.substr(1);
    }
    
    static bool createDirectories(const std::string& path) {
        try {
            std::filesystem::path filePath(path);
            std::filesystem::path parentPath = filePath.parent_path();
            
            if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
                std::filesystem::create_directories(parentPath);
                std::cout << "Creadas carpetas: " << parentPath << std::endl;
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error creando carpetas: " << e.what() << std::endl;
            return false;
        }
    }

    static std::string toLowerCase(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

public:
    static std::string mkdisk(int size, const std::string& unit, const std::string& path, const std::string& fit = "FF") {
        try {
            std::string expandedPath = expandPath(path);
            
            if (size <= 0) {
                return "Error: El tamano debe ser mayor a 0";
            }

            int sizeInBytes = size;
            std::string unitLower = toLowerCase(unit);
            
            if (unitLower == "k") {
                sizeInBytes = size * 1024;
            } else if (unitLower == "m" || unitLower.empty()) {
                sizeInBytes = size * 1024 * 1024;
            } else {
                return "Error: Unidad no valida. Use 'k' para KB o 'm' para MB";
            }

            char diskFit = 'F';
            std::string fitLower = toLowerCase(fit);
            if (fitLower == "bf") diskFit = 'B';
            else if (fitLower == "ff" || fitLower.empty()) diskFit = 'F';
            else if (fitLower == "wf") diskFit = 'W';
            else return "Error: Parametro -fit no valido (use BF, FF, o WF).";

            if (!createDirectories(expandedPath)) {
                return "Error: No se pudieron crear las carpetas necesarias";
            }

            std::ifstream checkFile(expandedPath);
            if (checkFile.good()) {
                checkFile.close();
                return "Error: El disco ya existe en la ruta especificada";
            }

            std::ofstream diskFile(expandedPath, std::ios::binary);
            if (!diskFile.is_open()) {
                return "Error: No se pudo crear el archivo del disco";
            }

            char buffer[1024] = {0};
            int iterations = sizeInBytes / 1024;
            for (int i = 0; i < iterations; i++) {
                diskFile.write(buffer, 1024);
            }

            MBR mbr;
            mbr.mbr_size = sizeInBytes;
            mbr.mbr_creation_date = time(nullptr);
            mbr.mbr_disk_signature = rand() % 10000;
            mbr.disk_fit = diskFit;
            
            for (int i = 0; i < 4; i++) {
                mbr.mbr_partitions[i].part_status = '0';
                mbr.mbr_partitions[i].part_type = '\0';
                mbr.mbr_partitions[i].part_fit = '\0';
                mbr.mbr_partitions[i].part_start = -1;
                mbr.mbr_partitions[i].part_size = 0;
                memset(mbr.mbr_partitions[i].part_name, 0, 16);
            }

            diskFile.seekp(0, std::ios::beg);
            diskFile.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
            diskFile.close();

            char timeStr[26];
            struct tm* timeinfo = localtime(&mbr.mbr_creation_date);
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

            return "Disco creado exitosamente\n"
                   "  Ruta: " + expandedPath + "\n"
                   "  Tamano: " + std::to_string(sizeInBytes) + " bytes\n"
                   "  Ajuste: " + std::string(1, diskFit) + "\n"
                   "  Fecha: " + timeStr;

        } catch (const std::exception& e) {
            return std::string("Error al crear disco: ") + e.what();
        }
    }

    static std::string rmdisk(const std::string& path) {
        try {
            std::string expandedPath = expandPath(path);
            
            std::ifstream checkFile(expandedPath);
            if (!checkFile.good()) {
                checkFile.close();
                return "Error: El disco no existe en la ruta especificada";
            }
            checkFile.close();

            std::ifstream diskFile(expandedPath, std::ios::binary);
            MBR mbr;
            diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
            diskFile.close();

            char timeStr[26];
            struct tm* timeinfo = localtime(&mbr.mbr_creation_date);
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

            std::string info = "Disco eliminado:\n" +
                              std::string("  Ruta: ") + expandedPath + "\n" +
                              std::string("  Tamaño: ") + std::to_string(mbr.mbr_size) + " bytes\n" +
                              std::string("  Fecha creación: ") + timeStr + "\n" +
                              std::string("  Firma: ") + std::to_string(mbr.mbr_disk_signature);

            if (remove(expandedPath.c_str()) != 0) {
                return "Error: No se pudo eliminar el archivo del disco";
            }

            return info;

        } catch (const std::exception& e) {
            return std::string("Error al eliminar disco: ") + e.what();
        }
    }

    static std::string getDiskInfo(const std::string& path) {
        try {
            std::string expandedPath = expandPath(path);
            
            std::ifstream diskFile(expandedPath, std::ios::binary);
            if (!diskFile.is_open()) {
                return "Error: No se pudo abrir el disco";
            }

            MBR mbr;
            diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
            diskFile.close();

            char timeStr[26];
            struct tm* timeinfo = localtime(&mbr.mbr_creation_date);
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

            std::string info = "=== INFORMACIÓN DEL DISCO MBR ===\n";
            info += std::string("  Ruta: ") + expandedPath + "\n";
            info += std::string("  Tamaño: ") + std::to_string(mbr.mbr_size) + " bytes\n";
            info += std::string("  Fecha creación: ") + timeStr + "\n";
            info += std::string("  Firma: ") + std::to_string(mbr.mbr_disk_signature) + "\n";
            info += std::string("  Ajuste: ") + mbr.disk_fit + "\n";
            info += std::string("  \n=== PARTICIONES ===\n");

            for (int i = 0; i < 4; i++) {
                info += "  [" + std::to_string(i+1) + "] ";
                if (mbr.mbr_partitions[i].part_status == '1') {
                    info += "ACTIVA - " + std::string(mbr.mbr_partitions[i].part_name) + 
                           " (" + std::to_string(mbr.mbr_partitions[i].part_size) + " bytes)\n";
                } else {
                    info += "INACTIVA\n";
                }
            }
            
            info += "\n=== ===\n";
            diskFile.open(expandedPath, std::ios::binary);
            char buffer[100];
            diskFile.read(buffer, 100);
            diskFile.close();
            
            for (int i = 0; i < 100; i++) {
                if (i % 16 == 0) info += std::string("\n") + (i < 16 ? "0" : "") + std::to_string(i/16) + "0: ";
                char hex[4];
                sprintf(hex, "%02X ", (unsigned char)buffer[i]);
                info += hex;
            }

            return info;

        } catch (const std::exception& e) {
            return std::string("Error al obtener información: ") + e.what();
        }
    }
};

#endif // DISK_MANAGER_H