#ifndef CAT_H
#define CAT_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "sesion.h"
#include "structures.h"

class ComandoCat{
public:
    static std::string execute(const std::vector<std::string>& files){
        if(!Sesion::activo){
            return "Error: se require tener sesion iniciada para la ejecucion de este comando";
        }

        if(files.empty()){
            return "Error: no hay archivos a leer";
        }

        std::string rutaDisco = Sesion::rutaDisco;
        int inicioParticion = Sesion::inicioParticion;

        std::fstream file(rutaDisco, std::ios::in | std::ios::binary);
        if (!file.is_open()) return "Error: no se pudo abrir el disco";

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
        
        std::string resultadoTotal = "";

        for(const std::string& path : files){
            resultadoTotal += "\n Contenido de: " + path + "\n";

            std::vector<std::string> carpetas;
            std::stringstream ss(path);
            std::string token;
            while (std::getline(ss, token, '/')){
                if(!token.empty()) carpetas.push_back(token);
            }

            if(carpetas.empty()){
                resultadoTotal += "Error: ruta invalida \n";
                continue;
            }

            std::string nombreArchivo = carpetas.back();
            carpetas.pop_back();

            int inodoPadreId = 0;
            bool rutaValida = true;
            
            for(const std::string& nombreCarpeta : carpetas){
                int siguienteInodo = buscarEnCarpeta(file, sb, inodoPadreId, nombreCarpeta);
                if(siguienteInodo == -1){
                    resultadoTotal += "Error: la ruta hacia el archivo no existe \n";
                    rutaValida = false;
                    break;
                }
                inodoPadreId = siguienteInodo;
            }
            if(!rutaValida) continue;

            int archivoInodoId = buscarEnCarpeta(file, sb, inodoPadreId, nombreArchivo);
            if (archivoInodoId == -1){
                resultadoTotal += "Error: el archivo '" + nombreArchivo + "' no existe \n";
                continue;
            }

            Inode inodoArchivo;
            file.seekg(sb.s_inode_start + (archivoInodoId * sizeof(Inode)), std::ios::beg);
            file.read(reinterpret_cast<char*>(&inodoArchivo), sizeof(Inode));

            if(inodoArchivo.i_type != '1'){
                resultadoTotal += "Error: la ruta apunta a una carpeta, no a un archivo \n";
                continue;
            }

            if(!tienePermiso(inodoArchivo, 4)){
                resultadoTotal += "Error: el usuario '" + Sesion::usuario + "' no tiene permiso de lectura sobre este archivo";
                continue;
            }

            std::string contenidoArchivo = "";
            for(int i = 0; i < 12; i++){
                if(inodoArchivo.i_block[i] != -1){
                    FileBlock fb;
                    file.seekg(sb.s_block_start + (inodoArchivo.i_block[i] * sizeof(FileBlock)), std::ios::beg);
                    file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));

                    size_t len = 0;
                    while (len < 64 && fb.b_content[len] != '\0'){
                        len++;
                    }
                    contenidoArchivo.append(fb.b_content, len);
                }
            }

            if(contenidoArchivo.empty()){
                resultadoTotal += "Archivo vacio \n";
            } else {
                resultadoTotal += contenidoArchivo + "\n";
            }
        }

        file.close();
        return resultadoTotal;
    }

private:
    static bool tienePermiso(const Inode& inodo, int permisoRequerido){
        if (Sesion::usuario == "root") return true;
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
                    if (fb.b_content[j].b_inodo != -1 && std::string(fb.b_content[j].b_name) == nombreBuscado){
                        return fb.b_content[j].b_inodo;
                    }
                }
            }
        }
        return -1;
    }
};

#endif