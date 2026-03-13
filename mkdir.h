#ifndef MKDIR_H
#define MKDIR_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include "sesion.h"
#include "structures.h"

class ComandoMkdir{
public:
    static std::string execute(const std::string& path, bool recursivo){
        if(!Sesion::activo){
            return "Error: se require tener sesion iniciada para la ejecucion de este comando";
        }

        std::string rutaDisco = Sesion::rutaDisco;
        int inicioParticion = Sesion::inicioParticion;

        std::vector<std::string> carpetas;
        std::stringstream ss(path);
        std::string token;
        while (std::getline(ss, token, '/')){
            if(!token.empty()) carpetas.push_back(token);
        }

        if(carpetas.empty()) return "Error: ruta invalida o intento de recrear la raiz";

        std::string nombreCarpetaDestino = carpetas.back();
        carpetas.pop_back();

        if (nombreCarpetaDestino.length() > 12) return "Error: el nombre de la carpeta excede los 12 caracteres";
        
        std::fstream file(rutaDisco, std::ios::in | std::ios::out | std::ios::binary);
        if(!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        int inodoPadreId = 0;

        for(const std::string& nombreCarpeta : carpetas){
            int siguienteInodo = buscarEnCarpeta(file, sb, inodoPadreId, nombreCarpeta);

            if(siguienteInodo == -1){
                if (recursivo){
                    Inode inodoPadreAux;
                    file.seekg(sb.s_inode_start + (inodoPadreId * sizeof(Inode)), std::ios::beg);
                    file.read(reinterpret_cast<char*>(&inodoPadreAux), sizeof(Inode));

                    if(!tienePermiso(inodoPadreAux, 2)){
                        file.close();
                        return "Error: sin permisos de escritura para crear la carpeta";
                    }

                    siguienteInodo = crearCarpetaFisica(file, sb, inodoPadreId, nombreCarpeta, inicioParticion);
                    if(siguienteInodo == -1){
                        file.close();
                        return "Error: disco lleno no se puedo crear la carpeta";
                    } 
                } else {
                    file.close();
                    return "Error: La ruta no existe utilize el comando -p para crerla recursivamente";
                }
            }
            inodoPadreId = siguienteInodo;
        }

        Inode inodoPadre;

        file.seekg(sb.s_inode_start + (inodoPadreId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoPadre), sizeof(Inode));

        if(!tienePermiso(inodoPadre, 2)){
            file.close();
            return "Error: el usuario no tiene permisos en la carpeta";
        }

        if(buscarEnCarpeta(file, sb, inodoPadreId, nombreCarpetaDestino) != -1){
            file.close();
            return "Error: la carpeta ya existe";
        }

        int nuevoInodoId = crearCarpetaFisica(file, sb, inodoPadreId, nombreCarpetaDestino, inicioParticion);
        file.close();

        if(nuevoInodoId == -1){
            return "Error: No hay espacio para crear la carpeta";
        }

        return "Carpeta '" + nombreCarpetaDestino + "' creada correctamente";
    }

private:
    static bool tienePermiso(const Inode& inodo, int permisoRequerido){
        if(Sesion::usuario == "root") return true;
        int permPropietario = inodo.i_perm / 100;
        int permGrupo = (inodo.i_perm / 10) % 10;
        int permOtros = inodo.i_perm % 10;
        if(inodo.i_uid == Sesion::uid) return (permPropietario & permisoRequerido) == permisoRequerido;
        if(inodo.i_gid == Sesion::gid) return (permGrupo & permisoRequerido) == permisoRequerido;
        return (permOtros & permisoRequerido) == permisoRequerido;
    }

    static int buscarEnCarpeta(std::fstream& file, const Superblock& sb, int inodoId, const std::string& nombreBuscado){
        Inode inodoCarpeta;
        file.seekg(sb.s_inode_start + (inodoId * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoCarpeta), sizeof(Inode));

        for(int i = 0; i < 12; i++){
            if (inodoCarpeta.i_block[i] != -1){
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoCarpeta.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                for(int j = 0; j < 4; j++){
                    if(fb.b_content[j].b_inodo != -1 && std::string(fb.b_content[j].b_name) == nombreBuscado){
                        return fb.b_content[j].b_inodo;
                    }
                }
            }
        }
        return -1;
    }

    static int obtenerInodoLibre(std::fstream& file, Superblock& sb, int inicioParticion){
        if (sb.s_free_inodes_count <= 0) return -1;
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

    static int obtenerBloqueLibre(std::fstream& file, Superblock& sb, int inicioParticion){
        if(sb.s_free_blocks_count <= 0) return -1;
        file.seekg(sb.s_bm_block_start, std::ios::beg);
        char bit;
        for(int i = 0; i < sb.s_blocks_count; i++){
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
            if (inodoPadre.i_block[i] != -1){
                FolderBlock fb;
                file.seekg(sb.s_block_start + (inodoPadre.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

                for(int j = 0; j < 4; j ++){
                    if(fb.b_content[j].b_inodo == -1){
                        std::strncpy(fb.b_content[j].b_name, nombre.c_str(), 12);
                        fb.b_content[j].b_inodo = nuevoInodoId;
                        file.seekp(sb.s_block_start + (inodoPadre.i_block[i] * sizeof(FolderBlock)), std::ios::beg);
                        file.write(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
                        return true;
                    }
                }
            } else {
                int nuevoBloque = obtenerBloqueLibre(file, sb, inicioParticion);
                if (nuevoBloque == -1) return false;

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
        int nuevoBloque = obtenerBloqueLibre(file, sb, inicioParticion);

        if(nuevoInodo == -1 || nuevoBloque == -1) return -1;

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
};

#endif