#ifndef REP_H
#define REP_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include <algorithm>
#include "mount.h"
#include "structures.h"

class ComandoRep {
public:
    static std::string execute(const std::string& name, const std::string& path, const std::string& id, const std::string& rutaInterior = ""){
        ComandoMount::ParticionMontada particion;
        if(!ComandoMount::getMountedPartition(id, particion)){
            return "Error: la particion con id '" + id + "' no esta montada";
        }

        std::string rutaDisco = particion.ruta;
        std::string nombreReporte = toLowerCase(name);

        std::string dirPath = getDirectory(path);
        if(!dirPath.empty()){
            std::string cmdMkdir = "mkdir -p \"" + dirPath + "\"";
            system(cmdMkdir.c_str());
        }

        if(nombreReporte == "mbr"){
            return reporteMBR(rutaDisco, path);
        } else if(nombreReporte == "disk"){
            return reporteDISK(rutaDisco, path);
        } else if(nombreReporte == "inode"){
            return reporteINODE(rutaDisco, path, particion.inicio);
        } else if(nombreReporte == "block"){
            return reporteBLOCK(rutaDisco, path, particion.inicio);
        } else if(nombreReporte == "bm_inode"){
            return reporteBMINODE(rutaDisco, path, particion.inicio);
        } else if(nombreReporte == "bm_block"){
            return reporteBMBLOCK(rutaDisco, path, particion.inicio);
        } else if(nombreReporte == "tree"){
            return reporteTREE(rutaDisco, path, particion.inicio);
        } else if(nombreReporte == "sb"){
            return reporteSB(rutaDisco, path, particion.inicio);
        } else if(nombreReporte == "file"){
            if(rutaInterior.empty()) return "Error: el reporte 'file' requiere el parametro -path_file_ls";
            return reporteFILE(rutaDisco, path, particion.inicio, rutaInterior);
        } else if(nombreReporte == "ls"){
            std::string rutaDirectorio = rutaInterior.empty() ? "/" : rutaInterior;
            return reporteLS(rutaDisco, path, particion.inicio, rutaInterior);
        } else {
            return "Error: Reporte '" + name + "' no reconocido";
        }
    }

private:
    static std::string toLowerCase(const std::string& str){
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    static std::string getDirectory(const std::string& path){
        size_t pos = path.find_last_of('/');
        if(pos != std::string::npos){
            return path.substr(0, pos);
        }
        return "";
    }

    static std::string formatearFecha(time_t fecha){
        std::string date = std::ctime(&fecha);
        date.erase(std::remove(date.begin(), date.end(), '\n'), date.end());
        return date;
    }

    static std::string sanitizarHTML(const std::string& str) {
        std::string out;
        for (char c : str) {
            if (c == '<') out += "&lt;";
            else if (c == '>') out += "&gt;";
            else if (c == '&') out += "&amp;";
            else if (c == '\n') out += "<br/>";
            else if (c == '\0') break;
            else out += c;
        }
        return out;
    }

    static int buscarEnCarpeta(std::fstream& file, const Superblock& sb, int inodoId, const std::string& nombreBuscado) {
        Inode inodoCarpeta;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoCarpeta), sizeof(Inode));

        for (int i = 0; i < 12; i++) {
            if (inodoCarpeta.i_block[i] != -1) {
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoCarpeta.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                for (int j = 0; j < 4; j++) {
                    if (fb.b_content[j].b_inodo != -1 && std::string(fb.b_content[j].b_name) == nombreBuscado) {
                        return fb.b_content[j].b_inodo; 
                    }
                }
            }
        }
        return -1; 
    }

    static std::string generarImagen(const std::string& dotCode, const std::string& outputPath, const std::string& tipo) {
        std::string dotPath = outputPath + ".dot";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) return "Error: No se pudo crear el archivo .dot intermedio para el reporte " + tipo + ".";
        dotFile << dotCode;
        dotFile.close();

        std::string comandoComando = "dot -Tpng \"" + dotPath + "\" -o \"" + outputPath + "\"";
        int result = system(comandoComando.c_str());

        if (result != 0) {
            return "Error: Fallo la generacion de la imagen. Verifique que Graphviz ('dot') este instalado.";
        }

        return "Reporte " + tipo + " generado exitosamente en: " + outputPath;
    }

    static std::string reporteMBR(const std::string& rutaDisco, const std::string& outputPath) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para el reporte MBR.";

        MBR mbr;
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        std::string dotCode = "digraph G {\n";
        dotCode += "  node [shape=plaintext];\n";
        dotCode += "  tabla [label=<\n";
        dotCode += "    <table border=\"1\" cellborder=\"1\" cellspacing=\"0\">\n";
        
        dotCode += "      <tr><td colspan=\"2\" bgcolor=\"#4a148c\"><font color=\"white\"><b>REPORTE MBR</b></font></td></tr>\n";
        dotCode += "      <tr><td><b>mbr_tamano</b></td><td>" + std::to_string(mbr.mbr_size) + "</td></tr>\n";
        dotCode += "      <tr><td><b>mbr_fecha_creacion</b></td><td>" + formatearFecha(mbr.mbr_creation_date) + "</td></tr>\n";
        dotCode += "      <tr><td><b>mbr_disk_signature</b></td><td>" + std::to_string(mbr.mbr_disk_signature) + "</td></tr>\n";
        dotCode += "      <tr><td><b>mbr_disk_fit</b></td><td>" + std::string(1, mbr.disk_fit) + "</td></tr>\n";

        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') { 
                dotCode += "      <tr><td colspan=\"2\" bgcolor=\"#1565c0\"><font color=\"white\"><b>Particion " + std::to_string(i + 1) + "</b></font></td></tr>\n";
                dotCode += "      <tr><td><b>part_status</b></td><td>" + std::string(1, mbr.mbr_partitions[i].part_status) + "</td></tr>\n";
                dotCode += "      <tr><td><b>part_type</b></td><td>" + std::string(1, mbr.mbr_partitions[i].part_type) + "</td></tr>\n";
                dotCode += "      <tr><td><b>part_fit</b></td><td>" + std::string(1, mbr.mbr_partitions[i].part_fit) + "</td></tr>\n";
                dotCode += "      <tr><td><b>part_start</b></td><td>" + std::to_string(mbr.mbr_partitions[i].part_start) + "</td></tr>\n";
                dotCode += "      <tr><td><b>part_size</b></td><td>" + std::to_string(mbr.mbr_partitions[i].part_size) + "</td></tr>\n";
                dotCode += "      <tr><td><b>part_name</b></td><td>" + std::string(mbr.mbr_partitions[i].part_name) + "</td></tr>\n";

                if (mbr.mbr_partitions[i].part_type == 'E' || mbr.mbr_partitions[i].part_type == 'e') {
                    int nextEbr = mbr.mbr_partitions[i].part_start;
                    while (nextEbr != -1) {
                        EBR ebr;
                        file.seekg(nextEbr, std::ios::beg);
                        file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));

                        if (ebr.part_status == '1') {
                            dotCode += "      <tr><td colspan=\"2\" bgcolor=\"#2e7d32\"><font color=\"white\"><b>Particion Logica (EBR)</b></font></td></tr>\n";
                            dotCode += "      <tr><td><b>part_status</b></td><td>" + std::string(1, ebr.part_status) + "</td></tr>\n";
                            dotCode += "      <tr><td><b>part_fit</b></td><td>" + std::string(1, ebr.part_fit) + "</td></tr>\n";
                            dotCode += "      <tr><td><b>part_start</b></td><td>" + std::to_string(ebr.part_start) + "</td></tr>\n";
                            dotCode += "      <tr><td><b>part_size</b></td><td>" + std::to_string(ebr.part_size) + "</td></tr>\n";
                            dotCode += "      <tr><td><b>part_next</b></td><td>" + std::to_string(ebr.part_next) + "</td></tr>\n";
                            dotCode += "      <tr><td><b>part_name</b></td><td>" + std::string(ebr.part_name) + "</td></tr>\n";
                        }
                        nextEbr = ebr.part_next;
                    }
                }
            }
        }

        dotCode += "    </table>\n";
        dotCode += "  >];\n";
        dotCode += "}\n";
        file.close();

        return generarImagen(dotCode, outputPath, "MBR");
    }

    static std::string reporteDISK(const std::string& rutaDisco, const std::string& outputPath) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para el reporte DISK.";

        MBR mbr;
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        std::string dotCode = "digraph G {\n";
        dotCode += "  node [shape=plaintext];\n";
        dotCode += "  tabla [label=<\n";
        
        dotCode += "    <table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"10\">\n";
        dotCode += "      <tr>\n";
        
        dotCode += "        <td bgcolor=\"#d32f2f\"><font color=\"white\"><b>MBR</b></font></td>\n";

        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_status == '1') {
                
                double pct = ((double)mbr.mbr_partitions[i].part_size / (double)mbr.mbr_size) * 100.0;
                std::stringstream streamPct;
                streamPct << std::fixed << std::setprecision(2) << pct << "%";
                std::string porcentaje = streamPct.str();

                if (mbr.mbr_partitions[i].part_type == 'P' || mbr.mbr_partitions[i].part_type == 'p') {
                    
                    dotCode += "        <td bgcolor=\"#1976d2\"><font color=\"white\"><b>Primaria</b><br/>" + porcentaje + "</font></td>\n";
                } else if (mbr.mbr_partitions[i].part_type == 'E' || mbr.mbr_partitions[i].part_type == 'e') {
                    
                    dotCode += "        <td bgcolor=\"#ffb300\">\n";
                    dotCode += "          <table border=\"0\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\">\n";
                    
                    
                    std::string logicasStr = "";
                    int cols = 0;
                    int nextEbr = mbr.mbr_partitions[i].part_start;
                    
                    while (nextEbr != -1) {
                        EBR ebr;
                        file.seekg(nextEbr, std::ios::beg);
                        file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));

                        if (ebr.part_status == '1') {
                            double pctLog = ((double)ebr.part_size / (double)mbr.mbr_size) * 100.0;
                            std::stringstream streamPctLog;
                            streamPctLog << std::fixed << std::setprecision(2) << pctLog << "%";
                            
                            logicasStr += "              <td bgcolor=\"#388e3c\"><font color=\"white\"><b>EBR</b></font></td>\n";
                            logicasStr += "              <td bgcolor=\"#81c784\"><font color=\"black\"><b>Logica</b><br/>" + streamPctLog.str() + "</font></td>\n";
                            cols += 2;
                        }
                        nextEbr = ebr.part_next;
                    }

                    if (cols == 0) {
                        logicasStr = "              <td>Libre</td>\n";
                        cols = 1;
                    }

                    dotCode += "            <tr><td colspan=\"" + std::to_string(cols) + "\"><b>Extendida</b><br/>" + porcentaje + "</td></tr>\n";
                    dotCode += "            <tr>\n" + logicasStr + "            </tr>\n";
                    dotCode += "          </table>\n";
                    dotCode += "        </td>\n";
                }
            } else {
                
                dotCode += "        <td bgcolor=\"#9e9e9e\"><font color=\"white\"><b>Libre</b></font></td>\n";
            }
        }

        dotCode += "      </tr>\n";
        dotCode += "    </table>\n";
        dotCode += "  >];\n";
        dotCode += "}\n";
        file.close();

        return generarImagen(dotCode, outputPath, "DISK");
    }

    static std::string reporteINODE(const std::string& rutaDisco, const std::string& outputPath, int inicioParticion) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para el reporte INODE.";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            return "Error: La particion no tiene un sistema de archivos EXT2 valido (Falta formatear).";
        }

        std::string dotCode = "digraph G {\n";
        dotCode += "  rankdir=LR;\n";
        dotCode += "  node [shape=plaintext];\n";

        char bit;
        int prevInodo = -1;

        for (int i = 0; i < sb.s_inodes_count; i++) {
            file.seekg(sb.s_bm_inode_start + i, std::ios::beg);
            file.read(&bit, 1);

            if (bit == '1') {
                Inode inodo;
                file.seekg(sb.s_inode_start + (i * sizeof(Inode)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));

                dotCode += "  inodo_" + std::to_string(i) + " [label=<\n";
                dotCode += "    <table border=\"1\" cellborder=\"1\" cellspacing=\"0\">\n";
                dotCode += "      <tr><td colspan=\"2\" bgcolor=\"#00695c\"><font color=\"white\"><b>Inodo " + std::to_string(i) + "</b></font></td></tr>\n";
                dotCode += "      <tr><td><b>i_uid</b></td><td>" + std::to_string(inodo.i_uid) + "</td></tr>\n";
                dotCode += "      <tr><td><b>i_gid</b></td><td>" + std::to_string(inodo.i_gid) + "</td></tr>\n";
                dotCode += "      <tr><td><b>i_size</b></td><td>" + std::to_string(inodo.i_size) + "</td></tr>\n";
                dotCode += "      <tr><td><b>i_atime</b></td><td>" + formatearFecha(inodo.i_atime) + "</td></tr>\n";
                dotCode += "      <tr><td><b>i_ctime</b></td><td>" + formatearFecha(inodo.i_ctime) + "</td></tr>\n";
                dotCode += "      <tr><td><b>i_mtime</b></td><td>" + formatearFecha(inodo.i_mtime) + "</td></tr>\n";
                dotCode += "      <tr><td><b>i_type</b></td><td>" + std::string(1, inodo.i_type) + "</td></tr>\n";
                dotCode += "      <tr><td><b>i_perm</b></td><td>" + std::to_string(inodo.i_perm) + "</td></tr>\n";

                for (int j = 0; j < 15; j++) {
                    dotCode += "      <tr><td><b>i_block[" + std::to_string(j) + "]</b></td><td>" + std::to_string(inodo.i_block[j]) + "</td></tr>\n";
                }

                dotCode += "    </table>\n";
                dotCode += "  >];\n";

                if (prevInodo != -1) {
                    dotCode += "  inodo_" + std::to_string(prevInodo) + " -> inodo_" + std::to_string(i) + ";\n";
                }
                
                prevInodo = i;
            }
        }

        dotCode += "}\n";
        file.close();

        return generarImagen(dotCode, outputPath, "INODE");
    }

    static std::string reporteBLOCK(const std::string& rutaDisco, const std::string& outputPath, int inicioParticion) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para el reporte BLOCK.";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            return "Error: La particion no tiene un sistema de archivos EXT2 valido.";
        }

        std::vector<char> tipoBloque(sb.s_blocks_count, '-'); 
        char bit;
        
        for (int i = 0; i < sb.s_inodes_count; i++) {
            file.seekg(sb.s_bm_inode_start + i, std::ios::beg);
            file.read(&bit, 1);
            if (bit == '1') {
                Inode inodo;
                file.seekg(sb.s_inode_start + (i * sizeof(Inode)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));
                
                for (int j = 0; j < 12; j++) {
                    if (inodo.i_block[j] != -1) {
                        tipoBloque[inodo.i_block[j]] = inodo.i_type;
                    }
                }
            }
        }

        std::string dotCode = "digraph G {\n";
        dotCode += "  rankdir=LR;\n"; 
        dotCode += "  node [shape=plaintext];\n";

        int prevBloque = -1;

        for (int i = 0; i < sb.s_blocks_count; i++) {
            file.seekg(sb.s_bm_block_start + i, std::ios::beg);
            file.read(&bit, 1);

            if (bit == '1') {
                char tipo = tipoBloque[i];
                dotCode += "  bloque_" + std::to_string(i) + " [label=<\n";
                dotCode += "    <table border=\"1\" cellborder=\"1\" cellspacing=\"0\">\n";

                if (tipo == '0') {
                    FolderBlock fb;
                    file.seekg(sb.s_block_start + (i * sizeof(FolderBlock)), std::ios::beg);
                    file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                    dotCode += "      <tr><td colspan=\"2\" bgcolor=\"#1565c0\"><font color=\"white\"><b>Bloque Carpeta " + std::to_string(i) + "</b></font></td></tr>\n";
                    dotCode += "      <tr><td bgcolor=\"#bbdefb\"><b>b_name</b></td><td bgcolor=\"#bbdefb\"><b>b_inodo</b></td></tr>\n";
                    
                    for (int j = 0; j < 4; j++) {
                        std::string nombre = (fb.b_content[j].b_inodo != -1) ? sanitizarHTML(fb.b_content[j].b_name) : "---";
                        dotCode += "      <tr><td>" + nombre + "</td><td>" + std::to_string(fb.b_content[j].b_inodo) + "</td></tr>\n";
                    }

                } else if (tipo == '1') {
                    FileBlock fb;
                    file.seekg(sb.s_block_start + (i * sizeof(FileBlock)), std::ios::beg);
                    file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));

                    std::string contenido(fb.b_content, 64);
                    contenido = sanitizarHTML(contenido);

                    dotCode += "      <tr><td bgcolor=\"#e65100\"><font color=\"white\"><b>Bloque Archivo " + std::to_string(i) + "</b></font></td></tr>\n";
                    dotCode += "      <tr><td>" + contenido + "</td></tr>\n";
                } else {
                    
                    dotCode += "      <tr><td bgcolor=\"#757575\"><font color=\"white\"><b>Bloque Apuntador/Otro " + std::to_string(i) + "</b></font></td></tr>\n";
                    dotCode += "      <tr><td>Contenido no directo</td></tr>\n";
                }

                dotCode += "    </table>\n  >];\n";

                if (prevBloque != -1) {
                    dotCode += "  bloque_" + std::to_string(prevBloque) + " -> bloque_" + std::to_string(i) + ";\n";
                }
                prevBloque = i;
            }
        }

        dotCode += "}\n";
        file.close();

        return generarImagen(dotCode, outputPath, "BLOCK");
    }

    static std::string reporteBMINODE(const std::string& rutaDisco, const std::string& outputPath, int inicioParticion) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para el reporte BM_INODE.";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            return "Error: La particion no tiene un sistema de archivos EXT2 valido.";
        }

        std::ofstream txtFile(outputPath);
        if (!txtFile.is_open()) {
            file.close();
            return "Error: No se pudo crear el archivo de texto en la ruta: " + outputPath;
        }

        file.seekg(sb.s_bm_inode_start, std::ios::beg);
        char bit;

        for (int i = 0; i < sb.s_inodes_count; i++) {
            file.read(&bit, 1);
            
            txtFile << bit << " "; 

            if ((i + 1) % 20 == 0) {
                txtFile << "\n";
            }
        }

        txtFile.close();
        file.close();

        return "Reporte BM_INODE generado exitosamente en: " + outputPath;
    }

    static std::string reporteBMBLOCK(const std::string& rutaDisco, const std::string& outputPath, int inicioParticion) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para el reporte BM_BLOCK.";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            return "Error: La particion no tiene un sistema de archivos EXT2 valido.";
        }

        std::ofstream txtFile(outputPath);
        if (!txtFile.is_open()) {
            file.close();
            return "Error: No se pudo crear el archivo de texto en la ruta: " + outputPath;
        }

        file.seekg(sb.s_bm_block_start, std::ios::beg);
        char bit;

        for (int i = 0; i < sb.s_blocks_count; i++) {
            file.read(&bit, 1);
            txtFile << bit << " "; 

            if ((i + 1) % 20 == 0) {
                txtFile << "\n";
            }
        }

        txtFile.close();
        file.close();

        return "Reporte BM_BLOCK generado exitosamente en: " + outputPath;
    }

    static std::string reporteTREE(const std::string& rutaDisco, const std::string& outputPath, int inicioParticion) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para el reporte TREE.";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            return "Error: La particion no tiene un sistema de archivos EXT2 valido.";
        }

        std::string dotCode = "digraph G {\n";
        dotCode += "  rankdir=LR;\n";
        dotCode += "  node [shape=plaintext];\n";

        std::vector<char> tipoBloque(sb.s_blocks_count, '-'); 
        char bit;

        for (int i = 0; i < sb.s_inodes_count; i++) {
            file.seekg(sb.s_bm_inode_start + i, std::ios::beg);
            file.read(&bit, 1);
            
            if (bit == '1') {
                Inode inodo;
                file.seekg(sb.s_inode_start + (i * sizeof(Inode)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));

                dotCode += "  inodo_" + std::to_string(i) + " [label=<\n";
                dotCode += "    <table border=\"1\" cellborder=\"1\" cellspacing=\"0\">\n";
                dotCode += "      <tr><td colspan=\"2\" bgcolor=\"#00695c\"><font color=\"white\"><b>Inodo " + std::to_string(i) + "</b></font></td></tr>\n";
                dotCode += "      <tr><td><b>i_uid</b></td><td>" + std::to_string(inodo.i_uid) + "</td></tr>\n";
                dotCode += "      <tr><td><b>i_gid</b></td><td>" + std::to_string(inodo.i_gid) + "</td></tr>\n";
                dotCode += "      <tr><td><b>i_size</b></td><td>" + std::to_string(inodo.i_size) + "</td></tr>\n";
                dotCode += "      <tr><td><b>i_type</b></td><td>" + std::string(1, inodo.i_type) + "</td></tr>\n";

                for (int j = 0; j < 15; j++) {
                    dotCode += "      <tr><td><b>i_block[" + std::to_string(j) + "]</b></td><td port=\"f" + std::to_string(j) + "\">" + std::to_string(inodo.i_block[j]) + "</td></tr>\n";
                    if (inodo.i_block[j] != -1) {
                        tipoBloque[inodo.i_block[j]] = inodo.i_type;
                    }
                }
                dotCode += "    </table>\n  >];\n";
            }
        }

        for (int i = 0; i < sb.s_blocks_count; i++) {
            file.seekg(sb.s_bm_block_start + i, std::ios::beg);
            file.read(&bit, 1);
            
            if (bit == '1') {
                char tipo = tipoBloque[i];
                dotCode += "  bloque_" + std::to_string(i) + " [label=<\n";
                dotCode += "    <table border=\"1\" cellborder=\"1\" cellspacing=\"0\">\n";

                if (tipo == '0') {
                    FolderBlock fb;
                    file.seekg(sb.s_block_start + (i * sizeof(FolderBlock)), std::ios::beg);
                    file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                    dotCode += "      <tr><td colspan=\"2\" bgcolor=\"#1565c0\"><font color=\"white\"><b>Bloque Carpeta " + std::to_string(i) + "</b></font></td></tr>\n";
                    dotCode += "      <tr><td bgcolor=\"#bbdefb\"><b>b_name</b></td><td bgcolor=\"#bbdefb\"><b>b_inodo</b></td></tr>\n";
                    
                    for (int j = 0; j < 4; j++) {
                        std::string nombre = (fb.b_content[j].b_inodo != -1) ? sanitizarHTML(fb.b_content[j].b_name) : "---";
                        dotCode += "      <tr><td>" + nombre + "</td><td port=\"f" + std::to_string(j) + "\">" + std::to_string(fb.b_content[j].b_inodo) + "</td></tr>\n";
                    }

                } else if (tipo == '1') {
                    FileBlock fb;
                    file.seekg(sb.s_block_start + (i * sizeof(FileBlock)), std::ios::beg);
                    file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
                    std::string contenido(fb.b_content, 64);
                    contenido = sanitizarHTML(contenido);

                    dotCode += "      <tr><td bgcolor=\"#e65100\"><font color=\"white\"><b>Bloque Archivo " + std::to_string(i) + "</b></font></td></tr>\n";
                    dotCode += "      <tr><td>" + contenido + "</td></tr>\n";
                }

                dotCode += "    </table>\n  >];\n";
            }
        }

        for (int i = 0; i < sb.s_inodes_count; i++) {
            file.seekg(sb.s_bm_inode_start + i, std::ios::beg);
            file.read(&bit, 1);
            
            if (bit == '1') {
                Inode inodo;
                file.seekg(sb.s_inode_start + (i * sizeof(Inode)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));

                for (int j = 0; j < 15; j++) {
                    if (inodo.i_block[j] != -1) {
                        dotCode += "  inodo_" + std::to_string(i) + ":f" + std::to_string(j) + " -> bloque_" + std::to_string(inodo.i_block[j]) + ";\n";
                    }
                }

                if (inodo.i_type == '0') {
                    for (int j = 0; j < 15; j++) {
                        if (inodo.i_block[j] != -1) {
                            FolderBlock fb;
                            file.seekg(sb.s_block_start + (inodo.i_block[j] * sizeof(FolderBlock)), std::ios::beg);
                            file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
                            
                            for (int k = 0; k < 4; k++) {
                                if (fb.b_content[k].b_inodo != -1) {
                                    std::string nombreDir = fb.b_content[k].b_name;
                                    if (nombreDir != "." && nombreDir != "..") {
                                        dotCode += "  bloque_" + std::to_string(inodo.i_block[j]) + ":f" + std::to_string(k) + " -> inodo_" + std::to_string(fb.b_content[k].b_inodo) + ";\n";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        dotCode += "}\n";
        file.close();

        return generarImagen(dotCode, outputPath, "TREE");
    }

    static std::string reporteSB(const std::string& rutaDisco, const std::string& outputPath, int inicioParticion) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para el reporte SB.";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            return "Error: La particion no tiene un sistema de archivos EXT2 valido.";
        }

        std::string dotCode = "digraph G {\n";
        dotCode += "  node [shape=plaintext];\n";
        dotCode += "  tabla [label=<\n";
        dotCode += "    <table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\">\n";
        
        dotCode += "      <tr><td colspan=\"2\" bgcolor=\"#0277bd\"><font color=\"white\"><b>REPORTE SUPERBLOQUE</b></font></td></tr>\n";
        
        dotCode += "      <tr><td><b>s_inodes_count</b></td><td>" + std::to_string(sb.s_inodes_count) + "</td></tr>\n";
        dotCode += "      <tr><td><b>s_blocks_count</b></td><td>" + std::to_string(sb.s_blocks_count) + "</td></tr>\n";
        dotCode += "      <tr><td><b>s_free_blocks_count</b></td><td>" + std::to_string(sb.s_free_blocks_count) + "</td></tr>\n";
        dotCode += "      <tr><td><b>s_free_inodes_count</b></td><td>" + std::to_string(sb.s_free_inodes_count) + "</td></tr>\n";
        
        dotCode += "      <tr><td><b>s_mtime</b></td><td>" + formatearFecha(sb.s_mtime) + "</td></tr>\n";
        dotCode += "      <tr><td><b>s_umtime</b></td><td>" + formatearFecha(sb.s_umtime) + "</td></tr>\n";
        
        dotCode += "      <tr><td><b>s_mnt_count</b></td><td>" + std::to_string(sb.s_mnt_count) + "</td></tr>\n";
        
        std::stringstream ssMagic;
        ssMagic << "0x" << std::hex << std::uppercase << sb.s_magic;
        dotCode += "      <tr><td><b>s_magic</b></td><td>" + ssMagic.str() + "</td></tr>\n";
        
        dotCode += "      <tr><td><b>s_inode_size</b></td><td>" + std::to_string(sb.s_inode_size) + "</td></tr>\n";
        dotCode += "      <tr><td><b>s_block_size</b></td><td>" + std::to_string(sb.s_block_size) + "</td></tr>\n";
        
        dotCode += "      <tr><td><b>s_first_ino</b></td><td>" + std::to_string(sb.s_first_ino) + "</td></tr>\n";
        dotCode += "      <tr><td><b>s_first_blo</b></td><td>" + std::to_string(sb.s_first_blo) + "</td></tr>\n";
        
        dotCode += "      <tr><td><b>s_bm_inode_start</b></td><td>" + std::to_string(sb.s_bm_inode_start) + "</td></tr>\n";
        dotCode += "      <tr><td><b>s_bm_block_start</b></td><td>" + std::to_string(sb.s_bm_block_start) + "</td></tr>\n";
        dotCode += "      <tr><td><b>s_inode_start</b></td><td>" + std::to_string(sb.s_inode_start) + "</td></tr>\n";
        dotCode += "      <tr><td><b>s_block_start</b></td><td>" + std::to_string(sb.s_block_start) + "</td></tr>\n";

        dotCode += "    </table>\n";
        dotCode += "  >];\n";
        dotCode += "}\n";
        
        file.close();

        return generarImagen(dotCode, outputPath, "SB");
    }

    static std::string reporteFILE(const std::string& rutaDisco, const std::string& outputPath, int inicioParticion, const std::string& rutaInterior) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para el reporte FILE.";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            return "Error: La particion no tiene un sistema de archivos EXT2 valido.";
        }

        std::vector<std::string> carpetas;
        std::stringstream ss(rutaInterior);
        std::string token;
        while (std::getline(ss, token, '/')) {
            if (!token.empty()) carpetas.push_back(token);
        }

        if (carpetas.empty()) return "Error: La -ruta proporcionada no es valida.";

        std::string nombreArchivo = carpetas.back();
        carpetas.pop_back();

        int inodoIdActual = 0;
        for (const std::string& nombreCarpeta : carpetas) {
            inodoIdActual = buscarEnCarpeta(file, sb, inodoIdActual, nombreCarpeta);
            if (inodoIdActual == -1) {
                file.close();
                return "Error: La ruta '" + rutaInterior + "' no existe en el sistema de archivos.";
            }
        }

        int archivoInodoId = buscarEnCarpeta(file, sb, inodoIdActual, nombreArchivo);
        if (archivoInodoId == -1) {
            file.close();
            return "Error: El archivo '" + nombreArchivo + "' no existe.";
        }

        Inode inodoArchivo;
        file.seekg(sb.s_inode_start + (archivoInodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoArchivo), sizeof(Inode));

        if (inodoArchivo.i_type != '1') {
            file.close();
            return "Error: La -ruta proporcionada apunta a una carpeta, no a un archivo.";
        }

        std::string contenido = "";
        for (int i = 0; i < 12; i++) {
            if (inodoArchivo.i_block[i] != -1) {
                FileBlock fb;
                file.seekg(sb.s_block_start + (inodoArchivo.i_block[i] * sizeof(FileBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
                
                size_t len = 0;
                while (len < 64 && fb.b_content[len] != '\0') {
                    len++;
                }
                contenido.append(fb.b_content, len);
            }
        }
        file.close();

        std::ofstream txtFile(outputPath);
        if (!txtFile.is_open()) {
            return "Error: No se pudo crear el archivo de destino en la ruta real: " + outputPath;
        }

        txtFile << contenido;
        txtFile.close();

        return "Reporte FILE exportado exitosamente en: " + outputPath;
    }

    static std::string reporteLS(const std::string& rutaDisco, const std::string& outputPath, int inicioParticion, const std::string& rutaInterior) {
        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: No se pudo abrir el disco para el reporte LS.";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            return "Error: La particion no tiene un sistema de archivos EXT2 valido.";
        }

        int inodoIdActual = 0;
        
        if (rutaInterior != "/" && !rutaInterior.empty()) {
            std::vector<std::string> carpetas;
            std::stringstream ss(rutaInterior);
            std::string token;
            while (std::getline(ss, token, '/')) {
                if (!token.empty()) carpetas.push_back(token);
            }

            for (const std::string& nombreCarpeta : carpetas) {
                inodoIdActual = buscarEnCarpeta(file, sb, inodoIdActual, nombreCarpeta);
                if (inodoIdActual == -1) {
                    file.close();
                    return "Error: La ruta '" + rutaInterior + "' no existe.";
                }
            }
        }

        Inode inodoCarpeta;
        file.seekg(sb.s_inode_start + (inodoIdActual * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoCarpeta), sizeof(Inode));

        if (inodoCarpeta.i_type != '0') {
            file.close();
            return "Error: La ruta '" + rutaInterior + "' apunta a un archivo, no a una carpeta.";
        }

        std::string dotCode = "digraph G {\n";
        dotCode += "  node [shape=plaintext];\n";
        dotCode += "  tabla [label=<\n";
        dotCode += "    <table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\">\n";
        
        dotCode += "      <tr><td colspan=\"7\" bgcolor=\"#1565c0\"><font color=\"white\"><b>Reporte LS : " + rutaInterior + "</b></font></td></tr>\n";
        dotCode += "      <tr bgcolor=\"#bbdefb\">\n";
        dotCode += "        <td><b>Permisos</b></td><td><b>Owner</b></td><td><b>Grupo</b></td><td><b>Size (Bytes)</b></td><td><b>Fecha</b></td><td><b>Tipo</b></td><td><b>Nombre</b></td>\n";
        dotCode += "      </tr>\n";

        for (int i = 0; i < 12; i++) {
            if (inodoCarpeta.i_block[i] != -1) {
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoCarpeta.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                for (int j = 0; j < 4; j++) {
                    if (fb.b_content[j].b_inodo != -1) {
                        Inode hijo;
                        file.seekg(sb.s_inode_start + (fb.b_content[j].b_inodo * sizeof(Inode)), std::ios::beg);
                        file.read(reinterpret_cast<char*>(&hijo), sizeof(Inode));

                        std::string nombre = sanitizarHTML(fb.b_content[j].b_name);
                        std::string tipo = (hijo.i_type == '0') ? "Carpeta" : "Archivo";
                        std::string fecha = formatearFecha(hijo.i_mtime);

                        dotCode += "      <tr>\n";
                        dotCode += "        <td>" + std::to_string(hijo.i_perm) + "</td>\n";
                        dotCode += "        <td>" + std::to_string(hijo.i_uid) + "</td>\n";
                        dotCode += "        <td>" + std::to_string(hijo.i_gid) + "</td>\n";
                        dotCode += "        <td>" + std::to_string(hijo.i_size) + "</td>\n";
                        dotCode += "        <td>" + fecha + "</td>\n";
                        dotCode += "        <td>" + tipo + "</td>\n";
                        dotCode += "        <td>" + nombre + "</td>\n";
                        dotCode += "      </tr>\n";
                    }
                }
            }
        }

        dotCode += "    </table>\n  >];\n}\n";
        file.close();

        return generarImagen(dotCode, outputPath, "LS");
    }
};

#endif