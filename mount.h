#ifndef MOUNT_H
#define MOUNT_H

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <cstring>
#include <algorithm>
#include "structures.h"

namespace ComandoMount {
    
    struct ParticionMontada {
        std::string ruta;          
        std::string nombre;          
        std::string id;            
        char tipo;                 
        int inicio;                 
        int tamano;                  
    };
    
    
    static std::map<std::string, ParticionMontada> particionesMontadas;
    
    static std::map<std::string, char> letrasDiscos;
    
    static char siguienteLetra = 'a';
    
    inline std::string expandirRuta(const std::string& ruta) {
        if (ruta.empty() || ruta[0] != '~') {
            return ruta;
        }
        
        const char* home = std::getenv("HOME");
        if (!home) {
            home = std::getenv("USERPROFILE");
        }
        
        if (home) {
            return std::string(home) + ruta.substr(1);
        }
        return ruta;
    }
    
    inline bool buscarParticionMBR(const std::string& ruta, const std::string& nombre, 
                                    char& tipo, int& inicio, int& tamano) {
        std::ifstream file(ruta, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        MBR mbr;
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                std::string partName(mbr.mbr_partitions[i].part_name);
                partName.erase(std::remove_if(partName.begin(), partName.end(), ::isspace), partName.end());
                partName.erase(std::find(partName.begin(), partName.end(), '\0'), partName.end());
                
                if (partName == nombre) {
                    tipo = mbr.mbr_partitions[i].part_type;
                    inicio = mbr.mbr_partitions[i].part_start;
                    tamano = mbr.mbr_partitions[i].part_size;
                    file.close();
                    return true;
                }
                
                if (mbr.mbr_partitions[i].part_type == 'E') {
                    int ebrPos = mbr.mbr_partitions[i].part_start;
                    while (ebrPos != -1) {
                        EBR ebr;
                        file.seekg(ebrPos, std::ios::beg);
                        file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                        
                        if (ebr.part_status == '1') {
                            std::string ebrName(ebr.part_name);
                            ebrName.erase(std::remove_if(ebrName.begin(), ebrName.end(), ::isspace), ebrName.end());
                            ebrName.erase(std::find(ebrName.begin(), ebrName.end(), '\0'), ebrName.end());
                            
                            if (ebrName == nombre) {
                                tipo = 'L';
                                inicio = ebr.part_start;
                                tamano = ebr.part_size;
                                file.close();
                                return true;
                            }
                        }
                        
                        ebrPos = ebr.part_next;
                    }
                }
            }
        }
        
        file.close();
        return false;
    }
    
    inline std::string generarIdMontaje(const std::string& ruta) {

        std::string carnet = "66";
        char letraDisco;
        
        auto it = letrasDiscos.find(ruta);
        if (it != letrasDiscos.end()) {
            letraDisco = it->second;
        } else {
            letraDisco = siguienteLetra++;
            letrasDiscos[ruta] = letraDisco;
        }
        
        int numeroParticiones = 1;
        for (const auto& [id, partition] : particionesMontadas) {
            if (partition.ruta == ruta) {
                numeroParticiones++;
            }
        }
        
        char letraMayus = std::toupper(letraDisco);
        std::string idMontaje = carnet;
        idMontaje += std::to_string(numeroParticiones);
        idMontaje += letraMayus;
        
        return idMontaje;
    }
    
    inline bool esParticionMontada(const std::string& ruta, const std::string& nombre) {
        for (const auto& [id, partition] : particionesMontadas) {
            if (partition.ruta == ruta && partition.nombre == nombre) {
                return true;
            }
        }
        return false;
    }

    inline void ejecutarMount(const std::map<std::string, std::string>& params) {
        std::cout << "\n=== MOUNT ===" << std::endl;
        
        auto pathIt = params.find("-path");
        auto nameIt = params.find("-name");
        
        if (pathIt == params.end() || nameIt == params.end()) {
            std::cerr << "Error: faltan parámetros obligatorios (-path y -name)" << std::endl;
            return;
        }
        
        std::string path = expandirRuta(pathIt->second);
        std::string name = nameIt->second;
        
        std::ifstream file(path);
        if (!file.good()) {
            std::cerr << "Error: el disco '" << path << "' no existe" << std::endl;
            return;
        }
        file.close();
        
        if (esParticionMontada(path, name)) {
            std::cerr << "Error: la partición '" << name << "' en '" << path 
                      << "' ya está montada" << std::endl;
            return;
        }
        
        char type;
        int start, size;
        if (!buscarParticionMBR(path, name, type, start, size)) {
            std::cerr << "Error: no se encontró la partición '" << name 
                      << "' en el disco '" << path << "'" << std::endl;
            return;
        }
        
        std::string mountID = generarIdMontaje(path);
        
        ParticionMontada mounted;
        mounted.ruta = path;
        mounted.nombre = name;
        mounted.id = mountID;
        mounted.tipo = type;
        mounted.inicio = start;
        mounted.tamano = size;
        
        particionesMontadas[mountID] = mounted;
        
        std::cout << "Partición montada exitosamente" << std::endl;
        std::cout << "  ID: " << mountID << std::endl;
        std::cout << "  Disco: " << path << std::endl;
        std::cout << "  Partición: " << name << std::endl;
        std::cout << "  Tipo: " << type << std::endl;
        std::cout << "  Inicio: " << start << " bytes" << std::endl;
        std::cout << "  Tamaño: " << size << " bytes" << std::endl;
    }
    
    inline std::string execute(const std::string& pathParam, const std::string& nameParam) {
        std::string path = expandirRuta(pathParam);
        std::string name = nameParam;
        
        std::ifstream file(path);
        if (!file.good()) {
            return "Error: el disco '" + path + "' no existe";
        }
        file.close();
        
        if (esParticionMontada(path, name)) {
            return "Error: la partición '" + name + "' en '" + path + "' ya está montada";
        }
        
        char type;
        int start, size;
        if (!buscarParticionMBR(path, name, type, start, size)) {
            return "Error: no se encontró la partición '" + name + "' en el disco '" + path + "'";
        }
        
        std::string mountID = generarIdMontaje(path);
        
        ParticionMontada mounted;
        mounted.ruta = path;
        mounted.nombre = name;
        mounted.id = mountID;
        mounted.tipo = type;
        mounted.inicio = start;
        mounted.tamano = size;
        
        particionesMontadas[mountID] = mounted;
        
        std::ostringstream result;
        result << "\n=== MOUNT ===\n";
        result << "Partición montada exitosamente\n";
        result << "  ID: " << mountID << "\n";
        result << "  Disco: " << path << "\n";
        result << "  Partición: " << name << "\n";
        result << "  Tipo: " << type << "\n";
        result << "  Inicio: " << start << " bytes\n";
        result << "  Tamaño: " << size << " bytes";
        
        return result.str();
    }
    
    inline std::string listMountedPartitions() {
        if (particionesMontadas.empty()) {
            return "No hay particiones montadas";
        }
        
        std::ostringstream result;
        result << "\n=== PARTICIONES MONTADAS ===\n";
        for (const auto& [id, partition] : particionesMontadas) {
            result << "ID: " << id << "\n";
            result << "  Disco: " << partition.ruta << "\n";
            result << "  Partición: " << partition.nombre << "\n";
            result << "  Tipo: " << partition.tipo << "\n";
            result << "  Inicio: " << partition.inicio << " bytes\n";
            result << "  Tamaño: " << partition.tamano << " bytes\n";
            result << "---\n";
        }
        return result.str();
    }
    
    inline void showMountedPartitions() {
        std::cout << listMountedPartitions() << std::endl;
    }
    
    inline bool getMountedPartition(const std::string& id, ParticionMontada& partition) {
        auto it = particionesMontadas.find(id);
        if (it != particionesMontadas.end()) {
            partition = it->second;
            return true;
        }
        return false;
    }

}

#endif