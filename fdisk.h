#ifndef FDISK_H  
#define FDISK_H   

#include <string>    
#include <iostream>  
#include <fstream>   
#include <cstring>   
#include <cstdlib>   
#include "structures.h"


namespace ComandoFdisk {
    
    inline std::string expandirRuta(const std::string& ruta) {
        if (ruta.empty() || ruta[0] != '~') {
            return ruta;
        }
        
        const char* home = std::getenv("HOME");
        if (!home) {
            std::cerr << "Error: No se pudo obtener el directorio HOME" << std::endl;
            return ruta;
        }
        
        return std::string(home) + ruta.substr(1);
    }

    // Crear partición primaria o extendida
    inline std::string crearParticionPoE(const std::string& ruta, int tamano, 
                                                       char tipo, char fit, const std::string& nombre) {
        std::fstream diskFile(ruta, std::ios::binary | std::ios::in | std::ios::out);
        if (!diskFile.is_open()) {
            return "Error: No se pudo abrir el disco";
        }

        MBR mbr;
        diskFile.seekg(0, std::ios::beg);
        diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1' && 
                strcmp(mbr.mbr_partitions[i].part_name, nombre.c_str()) == 0) {
                diskFile.close();
                return "Error: Ya existe una partición con ese nombre";
            }
        }

        // Contar particiones y buscar extendida
        int contadorParticiones = 0;
        bool condicion = false;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                contadorParticiones++;
                if (mbr.mbr_partitions[i].part_type == 'E') {
                    condicion = true;
                }
            }
        }

        if (contadorParticiones >= 4) {
            diskFile.close();
            return "Error: Ya existen 4 particiones (máximo permitido)";
        }

        if (tipo == 'E' && condicion) {
            diskFile.close();
            return "Error: Ya existe una partición extendida";
        }

        // Encontrar espacio disponible según el ajuste
        int espacioSeleccionado = -1;
        int mejorAjuste = -1;

        if (fit == 'F') {
            for (int i = 0; i < 4; i++) {
                if (mbr.mbr_partitions[i].part_status == '0') {
                    int posicionActual = sizeof(MBR);
                    
                    for (int j = 0; j < 4; j++) {
                        if (mbr.mbr_partitions[j].part_status == '1' && 
                            mbr.mbr_partitions[j].part_start == posicionActual) {
                            posicionActual = mbr.mbr_partitions[j].part_start + mbr.mbr_partitions[j].part_size;
                        }
                    }
                    
                    int availableSpace = mbr.mbr_size - posicionActual;
                    if (availableSpace >= tamano) {
                        espacioSeleccionado = i;
                        mejorAjuste = posicionActual;
                        break;
                    }
                }
            }
        } else if (fit == 'B') {
            int minimo = mbr.mbr_size;
            for (int i = 0; i < 4; i++) {
                if (mbr.mbr_partitions[i].part_status == '0') {
                    int posicionActual = sizeof(MBR);
                    
                    for (int j = 0; j < 4; j++) {
                        if (mbr.mbr_partitions[j].part_status == '1' && 
                            mbr.mbr_partitions[j].part_start == posicionActual) {
                            posicionActual = mbr.mbr_partitions[j].part_start + mbr.mbr_partitions[j].part_size;
                        }
                    }
                    
                    int espacioDisponible = mbr.mbr_size - posicionActual;
                    int desperdicio = espacioDisponible - tamano;
                    if (espacioDisponible >= tamano && desperdicio < minimo) {
                        espacioSeleccionado = i;
                        mejorAjuste = posicionActual;
                        minimo = desperdicio;
                    }
                }
            }
        } else {
            int maximoESpacio = 0;
            for (int i = 0; i < 4; i++) {
                if (mbr.mbr_partitions[i].part_status == '0') {
                    int posicionActual = sizeof(MBR);
                    
                    for (int j = 0; j < 4; j++) {
                        if (mbr.mbr_partitions[j].part_status == '1' && 
                            mbr.mbr_partitions[j].part_start == posicionActual) {
                            posicionActual = mbr.mbr_partitions[j].part_start + mbr.mbr_partitions[j].part_size;
                        }
                    }
                    
                    int espacioDisponible = mbr.mbr_size - posicionActual;
                    if (espacioDisponible >= tamano && espacioDisponible > maximoESpacio) {
                        espacioSeleccionado = i;
                        mejorAjuste = posicionActual;
                        maximoESpacio = espacioDisponible;
                    }
                }
            }
        }

        if (espacioSeleccionado == -1 || mejorAjuste == -1) {
            diskFile.close();
            return "Error: No hay espacio suficiente en el disco";
        }

        // Crear la partición
        mbr.mbr_partitions[espacioSeleccionado].part_status = '1';
        mbr.mbr_partitions[espacioSeleccionado].part_type = tipo;
        mbr.mbr_partitions[espacioSeleccionado].part_fit = fit;
        mbr.mbr_partitions[espacioSeleccionado].part_start = mejorAjuste;
        mbr.mbr_partitions[espacioSeleccionado].part_size = tamano;
        strncpy(mbr.mbr_partitions[espacioSeleccionado].part_name, nombre.c_str(), 16);

        // Si es extendida, inicializar con un EBR
        if (tipo == 'E') {
            EBR ebr;
            ebr.part_status = '0';
            ebr.part_fit = fit;
            ebr.part_start = -1;
            ebr.part_size = 0;
            ebr.part_next = -1;
            memset(ebr.part_name, 0, 16);

            diskFile.seekp(mejorAjuste, std::ios::beg);
            diskFile.write(reinterpret_cast<char*>(&ebr), sizeof(EBR));
        }

        // Escribir MBR actualizado
        diskFile.seekp(0, std::ios::beg);
        diskFile.write(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        diskFile.close();

        return "Partición " + std::string(1, tipo) + " '" + nombre + "' creada exitosamente\n" +
               "  Inicio: " + std::to_string(mejorAjuste) + "\n" +
               "  Tamaño: " + std::to_string(tamano) + " bytes\n" +
               "  Ajuste: " + std::string(1, fit);
    }

    // Crear partición lógica
    inline std::string crearParticionL(const std::string& ruta, int tamano, 
                                             char fit, const std::string& nombre) {
        std::fstream diskFile(ruta, std::ios::binary | std::ios::in | std::ios::out);
        if (!diskFile.is_open()) {
            return "Error: No se pudo abrir el disco";
        }

        MBR mbr;
        diskFile.seekg(0, std::ios::beg);
        diskFile.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        // Buscar partición extendida
        int indiceExtendido = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1' && 
                mbr.mbr_partitions[i].part_type == 'E') {
                indiceExtendido = i;
                break;
            }
        }

        if (indiceExtendido == -1) {
            diskFile.close();
            return "Error: No existe una partición extendida para crear particiones lógicas";
        }

        Partition& extended = mbr.mbr_partitions[indiceExtendido];
        int extInicio = extended.part_start;
        int extFin = extended.part_start + extended.part_size;

        // Leer primer EBR
        EBR currentEBR;
        diskFile.seekg(extInicio, std::ios::beg);
        diskFile.read(reinterpret_cast<char*>(&currentEBR), sizeof(EBR));

        // Si el primer EBR está vacío
        if (currentEBR.part_status == '0') {
            currentEBR.part_status = '1';
            currentEBR.part_fit = fit;
            currentEBR.part_start = extInicio + sizeof(EBR);
            currentEBR.part_size = tamano;
            currentEBR.part_next = -1;
            strncpy(currentEBR.part_name, nombre.c_str(), 16);

            diskFile.seekp(extInicio, std::ios::beg);
            diskFile.write(reinterpret_cast<char*>(&currentEBR), sizeof(EBR));
            diskFile.close();

            return "Partición lógica '" + nombre + "' creada exitosamente\n" +
                   "  Inicio: " + std::to_string(currentEBR.part_start) + "\n" +
                   "  Tamaño: " + std::to_string(tamano) + " bytes";
        }

        // Buscar el último EBR
        int currentEBRPos = extInicio;
        while (true) {
            diskFile.seekg(currentEBRPos, std::ios::beg);
            diskFile.read(reinterpret_cast<char*>(&currentEBR), sizeof(EBR));

            if (currentEBR.part_status == '1' && 
                strcmp(currentEBR.part_name, nombre.c_str()) == 0) {
                diskFile.close();
                return "Error: Ya existe una partición lógica con ese nombre";
            }

            if (currentEBR.part_next == -1) {
                int nextEBRPos = currentEBR.part_start + currentEBR.part_size;
                int espacioDisponible = extFin - nextEBRPos - sizeof(EBR);

                if (espacioDisponible < tamano) {
                    diskFile.close();
                    return "Error: No hay espacio suficiente en la partición extendida";
                }

                // Crear nuevo EBR
                EBR newEBR;
                newEBR.part_status = '1';
                newEBR.part_fit = fit;
                newEBR.part_start = nextEBRPos + sizeof(EBR);
                newEBR.part_size = tamano;
                newEBR.part_next = -1;
                strncpy(newEBR.part_name, nombre.c_str(), 16);

                // Actualizar EBR anterior
                currentEBR.part_next = nextEBRPos;
                diskFile.seekp(currentEBRPos, std::ios::beg);
                diskFile.write(reinterpret_cast<char*>(&currentEBR), sizeof(EBR));

                // Escribir nuevo EBR
                diskFile.seekp(nextEBRPos, std::ios::beg);
                diskFile.write(reinterpret_cast<char*>(&newEBR), sizeof(EBR));
                diskFile.close();

                return "Partición lógica '" + nombre + "' creada exitosamente\n" +
                       "  Inicio: " + std::to_string(newEBR.part_start) + "\n" +
                       "  Tamaño: " + std::to_string(tamano) + " bytes";
            }

            currentEBRPos = currentEBR.part_next;
        }
    }

    inline std::string execute(int tamano, const std::string& unidad, const std::string& ruta, 
                               const std::string& tipo, const std::string& fit, 
                               const std::string& nombre) {
        try {
            std::string expandedPath = expandirRuta(ruta);

            std::ifstream checkFile(expandedPath);
            if (!checkFile.good()) {
                checkFile.close();
                return "Error: El disco no existe en la ruta indicada";
            }
            checkFile.close();

            int sizeInBytes = tamano;
            if (unidad == "k" || unidad == "K") {
                sizeInBytes = tamano * 1024;
            } else if (unidad == "m" || unidad == "M") {
                sizeInBytes = tamano * 1024 * 1024;
            } else if (unidad == "b" || unidad == "B") {
                sizeInBytes = tamano;
            } else {
                return "Error: Unidad no válida. Use 'b', 'k' o 'm'";
            }

            char tipoParticion = 'P'; 
            if (!tipo.empty()) {
                if (tipo == "p" || tipo == "P") {
                    tipoParticion = 'P';
                } else if (tipo == "e" || tipo == "E") {
                    tipoParticion = 'E';
                } else if (tipo == "l" || tipo == "L") {
                    tipoParticion = 'L';
                } else {
                    return "Error: Tipo inválido. Use P (primaria), E (extendida) o L (lógica)";
                }
            }

            char partFit = 'W'; 
            if (!fit.empty()) {
                if (fit == "bf" || fit == "BF") {
                    partFit = 'B';
                } else if (fit == "ff" || fit == "FF") {
                    partFit = 'F';
                } else if (fit == "wf" || fit == "WF") {
                    partFit = 'W';
                } else {
                    return "Error: Fit inválido. Use BF, FF o WF";
                }
            }

            if (tipoParticion == 'L') {
                return crearParticionL(expandedPath, sizeInBytes, partFit, nombre);
            } else {
                return crearParticionPoE(expandedPath, sizeInBytes, tipoParticion, partFit, nombre);
            }

        } catch (const std::exception& e) {
            return std::string("Error en fdisk: ") + e.what();
        }
    }

}

#endif