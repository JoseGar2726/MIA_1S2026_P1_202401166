#ifndef MKFILE_H
#define MKFILE_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include "sesion.h"
#include "structures.h"

class ComandoMkfile{
public:
    static std::string execute (const std::string& path, bool recursivo, int size, const std::string& cont){
        if(!Sesion::activo){
            return "Error: se require tener sesion iniciada para la ejecucion de este comando";
        }

        if(size < 0){
            return "Error: el parametro -size no puede ser negativo";
        }

        std::string rutaDisco = Sesion::rutaDisco;
        int inicioParticion = Sesion::inicioParticion;

        std::vector<std::string> carpetas;
        std::stringstream ss(path);
        std::string token;
        while (std::getline(ss, token, '/')){
            if(!token.empty()){
                carpetas.push_back(token);
            }
        }

        if(carpetas.empty()) return "Error: ruta invalida";
        
        std::string nombreArchivo = carpetas.back();
        carpetas.pop_back();

        if(nombreArchivo.length() > 12){
            return "Error: el nombre del archivo excede los 12 caracteres permitidos";
        }

        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if(!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        int inodoPadreId = 0;

        for(const std::string& nombreCarpeta : carpetas){
            int siguienteInodo = buscarEnCarpeta(file, sb, inodoPadreId, nombreCarpeta);

            if(siguienteInodo == -1){
                if(recursivo){
                    siguienteInodo = crearCarpetaFisica(file, sb, inodoPadreId, nombreCarpeta, inicioParticion);
                    if(siguienteInodo == -1){
                        file.close();
                        return "Error: no hay espacio para crear la carpeta";
                    }
                } else {
                    file.close();
                    return "Error: la ruta destino no existe, utilize el parametro -r para crearlas recursivamente";
                }
            }
            inodoPadreId = siguienteInodo;
        }

        Inode inodoPadre;
        file.seekg(sb.s_inode_start + (inodoPadreId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoPadre), sizeof(Inode));

        if(!tienePermiso(inodoPadre, 2)){
            file.close();
            return "Error: el usuario '" + Sesion::usuario + "' no tiene permisos de escritura en la carpeta destino.";
        }

        if(buscarEnCarpeta(file, sb, inodoPadreId, nombreArchivo) != -1){
            file.close();
            return "Error: el archivo '" + nombreArchivo + "' ya existe en esta ruta";
        }

        std::string resultado = crearArchivoFisico(file, sb, inodoPadre, inodoPadreId, nombreArchivo, size, cont, inicioParticion);

        file.close();
        return resultado;
    }
private:
    static bool tienePermiso(const Inode& inodo, int permisoRequerido){
        if(Sesion::usuario == "root") return true;

        int permPropietario = inodo.i_perm / 100;
        int permGrupo = (inodo.i_perm / 10) % 10;
        int permOtros = inodo.i_perm % 10;

        if(inodo.i_uid == Sesion::uid){
            return (permPropietario & permisoRequerido) == permisoRequerido;
        } else if (inodo.i_gid == Sesion::gid){
            return (permGrupo & permisoRequerido) == permisoRequerido;
        } else {
            return (permOtros & permisoRequerido) == permisoRequerido;
        }
    }

    static int buscarEnCarpeta(std::fstream& file, const Superblock& sb, int inodoId, const std::string& nombreBuscado){
        Inode inodoCarpeta;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoCarpeta), sizeof(Inode));

        for(int i=0; i<12; i++){
            if(inodoCarpeta.i_block[i] != -1){
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoCarpeta.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                for(int j=0; j<4; j++){
                    if(fb.b_content[j].b_inodo != -1 ){
                        std::string nombreActual = fb.b_content[j].b_name;
                        if(nombreActual == nombreBuscado){
                            return fb.b_content[j].b_inodo;
                        }
                    }
                }
            }
        }
        return -1;
    }

    static int obtenerInodoLibre(std::fstream& file, Superblock& sb, int inicioParticion){
        if(sb.s_free_inodes_count <= 0) return -1;
        file.seekg(sb.s_bm_inode_start, std::ios::beg);
        char bit;
        for(int i = 0; i < sb.s_inodes_count; i++){
            file.read(&bit, 1);
            if(bit == '0'){
                bit = '1';
                file.seekp(sb.s_bm_inode_start + i, std::ios::beg);
                file.write(&bit, 1);
                sb.s_free_inodes_count--;
                sb.s_first_ino++;
                file.seekp(inicioParticion, std::ios::beg);
                file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));
                return i;
            }
        }
        return -1;
    }

    static int obtenerBloqueLIbre(std::fstream& file, Superblock& sb, int inicioParticion){
        if(sb.s_free_blocks_count <= 0) return -1;
        file.seekg(sb.s_bm_block_start, std::ios::beg);
        char bit;
        for (int i = 0; i < sb.s_blocks_count; i++){
            file.read(&bit, 1);
            if(bit == '0'){
                bit = '1';
                file.seekp(sb.s_bm_block_start + i, std::ios::beg);
                file.write(&bit, 1);
                sb.s_free_blocks_count--;
                sb.s_first_blo++;
                file.seekp(inicioParticion, std::ios::beg);
                file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));
                return i;
            }
        }
        return -1;
    }

    static bool agregarEntradaCarpeta(std::fstream& file, Superblock& sb, int inodoPadreId, const std::string& nombre, int nuevoInodoId, int inicioParticion){
        Inode inodoPadre;
        file.seekg(sb.s_inode_start + (inodoPadreId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoPadre), sizeof(Inode));

        for(int i = 0; i < 12; i++){
            if(inodoPadre.i_block[i] != -1){
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoPadre.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                for(int j = 0; j<4; j++){
                    if(fb.b_content[j].b_inodo == -1){
                        std::strncpy(fb.b_content[j].b_name, nombre.c_str(), 12);
                        fb.b_content[j].b_inodo = nuevoInodoId;
                        file.seekp(sb.s_block_start + (inodoPadre.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                        file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
                        return true;
                    }
                }
            } else {
                int nuevoBloque = obtenerBloqueLIbre(file, sb, inicioParticion);
                if(nuevoBloque == -1) return false;

                inodoPadre.i_block[i] = nuevoBloque;
                FolderBlock fb;
                std::strncpy(fb.b_content[0].b_name, nombre.c_str(), 12);
                fb.b_content[0].b_inodo = nuevoInodoId;

                file.seekp(sb.s_block_start + (nuevoBloque * sizeof(FolderBlock)), std::ios::beg);
                file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                file.seekp(sb.s_inode_start + (inodoPadreId * sizeof(Inode)), std::ios::beg);
                file.write(reinterpret_cast<char*>(&inodoPadre), sizeof(Inode));
                return true;
            }
        }
        return false;
    }

    static int crearCarpetaFisica(std::fstream& file, Superblock& sb, int inodoPadreId, const std::string& nombre, int inicioParticion){
        int nuevoInodo = obtenerInodoLibre(file, sb, inicioParticion);
        int nuevoBloque = obtenerBloqueLIbre(file, sb, inicioParticion);
        if (nuevoInodo == -1 || nuevoBloque == -1) return -1;

        Inode inodo;
        inodo.i_uid = Sesion::uid;
        inodo.i_gid = Sesion::gid;
        inodo.i_size = 0;
        inodo.i_type = '0';
        inodo.i_perm = 664;
        inodo.i_block[0] = nuevoBloque;

        file.seekp(sb.s_inode_start + (nuevoInodo * sizeof(Inode)), std::ios::beg);
        file.write(reinterpret_cast<char*>(&inodo), sizeof(Inode));

        FolderBlock fb;
        std::strncpy(fb.b_content[0].b_name, ".", 12);
        fb.b_content[0].b_inodo = nuevoInodo;
        std::strncpy(fb.b_content[1].b_name, "..", 12);
        fb.b_content[1].b_inodo = inodoPadreId;

        file.seekp(sb.s_block_start + (nuevoBloque * sizeof(FolderBlock)), std::ios::beg);
        file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

        if(!agregarEntradaCarpeta(file, sb, inodoPadreId, nombre, nuevoInodo, inicioParticion)) return -1;

        return nuevoInodo;
        
    }

    static std::string crearArchivoFisico(std::fstream& file, Superblock& sb, Inode& inodoPadre, int inodoPadreId,
                                          const std::string& nombreArchivo, int size, const std::string& cont, int inicioParticion){
        std::string contenidoFinal = "";
        if(!cont.empty()){
            std::ifstream externalFile(cont);
            if(!externalFile.is_open()) return "Error: no se puede leer el archivo externo '" + cont + "'";
            std::ostringstream ss;
            ss << externalFile.rdbuf();
            contenidoFinal = ss.str();
            externalFile.close();
        } else if(size > 0){
            for(int i = 0; i < size; i++){
                contenidoFinal += std::to_string(i % 10);
            }
        }

        int bloquesRequeridos = (contenidoFinal.length() / 64) + ((contenidoFinal.length() % 64 != 0) ? 1 : 0);
        if(bloquesRequeridos == 0) bloquesRequeridos = 1;

        if(bloquesRequeridos > 12){
            return "Error: el archivo requiere " + std::to_string(bloquesRequeridos) + " bloques, solo soporta 12 bloques directos actualmente";
        }

        int nuevoInodo = obtenerInodoLibre(file, sb, inicioParticion);
        if(nuevoInodo == -1) return "Error: no hay inodos libres";

        Inode inodoArchivo;
        inodoArchivo.i_uid = Sesion::uid;
        inodoArchivo.i_gid = Sesion::gid;
        inodoArchivo.i_size = contenidoFinal.length();
        inodoArchivo.i_type = '1';
        inodoArchivo.i_perm = 664;

        for(int i = 0; i < bloquesRequeridos; i++){
            int nuevoBloque = obtenerBloqueLIbre(file, sb, inicioParticion);
            if (nuevoBloque == -1) return "Error: disco lleno, no hay bloques libres";

            inodoArchivo.i_block[i] = nuevoBloque;

            FileBlock fb;
            int inicioChunk = i * 64;
            int longitudChunk = std::min(64, (int)contenidoFinal.length() - inicioChunk);
            if(longitudChunk > 0){
                std::strncpy(fb.b_content, contenidoFinal.substr(inicioChunk, longitudChunk).c_str(), 64);
            }

            file.seekp(sb.s_block_start + (nuevoBloque * sizeof(FileBlock)), std::ios::beg);
            file.write(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
        }

        file.seekp(sb.s_inode_start + (nuevoInodo * sizeof(Inode)), std::ios::beg);
        file.write(reinterpret_cast<char*>(&inodoArchivo), sizeof(Inode));

        if(!agregarEntradaCarpeta(file, sb, inodoPadreId, nombreArchivo, nuevoInodo, inicioParticion)){
            return "Error: la carpeta destino alcanzo su limite de archivos";
        }

        return "Archivo '" + nombreArchivo + "' creado exitosamente. Tamanio: " + std::to_string(contenidoFinal.length()) + " bytes";                                
    }
};

#endif