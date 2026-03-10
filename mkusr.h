#ifndef MKUSR_H
#define MKUSR_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "sesion.h"
#include "structures.h"

class ComandoMkusr{
public:
    static std::string execute(const std::string& usuario, const std::string& password, const std::string& grupo){
        if(!Sesion::activo){
            return "Error: se require tener sesion iniciada para la ejecucion de este comando";
        }

        if(Sesion::usuario != "root"){
            return "Error: Solo el usuario 'root' tiene permisos para realizar este comando";
        }

        if(usuario.length() > 10){
            return "Error: el nombre del usuario no puede tener mas de 10 caracteres";
        }

        if(password.length() > 10){
            return "Error: la contrasena del usuario no puede tener mas de 10 caracteres";
        }

        if(grupo.length() > 10){
            return "Error: el grupo al que pertenece el usuario no puede tener mas de 10 caracteres";
        }

        std::string ruta = Sesion::rutaDisco;
        int inicio = Sesion::inicioParticion;
  
        std::string contenidoUsuarios = leerTxtUsuarios(ruta, inicio);
        if(contenidoUsuarios.empty()){
            return "Error: no se pudo leer users.txt";
        }

        std::stringstream iss(contenidoUsuarios);
        std::string linea;

        int maxUsrId = 0;
        bool grupoExiste = false;

        while(std::getline(iss, linea, '\n')){
            if(linea.empty()) continue;

            std::stringstream ss(linea);
            std::string item;
            std::vector<std::string> tokens;

            while(std::getline(ss, item, ',')){
                size_t primero = item.find_first_not_of(' ');
                size_t ultimo = item.find_last_not_of(' ');
                if(primero != std::string::npos && ultimo != std::string::npos){
                    tokens.push_back(item.substr(primero, ultimo - primero + 1));
                }
            }

            if(tokens.size() >= 3 && tokens[1] == "G" && tokens[2] == grupo){
                if(tokens[0] != "0"){
                    grupoExiste = true;
                }
            }

            if(tokens.size() == 5 && tokens[1] == "U"){
                int idActual = std::stoi(tokens[0]);

                if(tokens[3] == usuario && idActual != 0){
                    return "Error: el usuario '" + usuario + "' ya existe";
                }

                if(idActual > maxUsrId){
                    maxUsrId = idActual;
                }
            }
        }

        if(!grupoExiste){
            return "Error: el grupo '" + grupo + "' no existe";
        }

        std::string nuevaLinea = std::to_string(maxUsrId + 1) + ", U, " + grupo + ", " + usuario + ", " + password + "\n";
        contenidoUsuarios += nuevaLinea;

        if(!escribirTxtUsuarios(ruta, inicio, contenidoUsuarios)){
            return "Error: error al guardar el nuevo usuario";
        }

        return "Usuario '" + usuario + "' creado exitosamente con ID " + std::to_string(maxUsrId + 1);
    }
private:
    static std::string leerTxtUsuarios(const std::string& ruta, int inicioParticion){
        std::string content = "";
        std::ifstream file(ruta, std::ios::binary);

        if(!file.is_open()) return content;

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if(sb.s_magic != 0xEF53){
            file.close();
            return content;
        }

        Inode inodoUsuarios;
        int posInodo = sb.s_inode_start + (1 * sizeof(Inode));
        file.seekg(posInodo, std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoUsuarios), sizeof(Inode));
        if(inodoUsuarios.i_type != '1'){
            file.close();
            return "";
        }

        for (int i = 0; i < 12; i++){
            if(inodoUsuarios.i_block[i] != -1){
                FileBlock fb;
                int posBloque = sb.s_block_start + (inodoUsuarios.i_block[i] * sizeof(FileBlock));
                file.seekg(posBloque, std::ios::beg);
                file.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));

                size_t len = 0;
                while (len < 64 && fb.b_content[len] != '\0'){
                    len++;
                }
                content.append(fb.b_content, len);
            }
        }

        file.close();
        return content;
    }

    static bool escribirTxtUsuarios(const std::string& ruta, int inicioParticion, const std::string& contenido){
        std::fstream file(ruta, std::ios::in | std::ios::out | std::ios::binary);
        if(!file.is_open()) return false;

        Superblock sb;
        file.seekg(inicioParticion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        Inode inodoUsuarios;
        int posInodo = sb.s_inode_start + (1 * sizeof(Inode));
        file.seekg(posInodo, std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodoUsuarios), sizeof(Inode));

        int bloquesRequeridos = (contenido.length() / 64) + ((contenido.length() % 64 != 0) ? 1 : 0);
        
        for(int i = 0; i < bloquesRequeridos && i < 12; i++){
            if(inodoUsuarios.i_block[i] == -1){
                file.seekg(sb.s_bm_block_start, std::ios::beg);
                char bit;
                int bloqueLibre = -1;

                for(int j = 0; j < sb.s_blocks_count; j++){
                    file.read(&bit, 1);
                    if(bit == '0'){
                        bloqueLibre = j;
                        break;
                    }
                }

                if(bloqueLibre == -1){
                    file.close();
                    return false;
                }

                bit = '1';
                file.seekp(sb.s_bm_block_start + bloqueLibre, std::ios::beg);
                file.write(&bit, 1);

                sb.s_free_blocks_count--;
                inodoUsuarios.i_block[i] = bloqueLibre;
            }

            FileBlock fb;
            int inicioChunk = i * 64;
            int longitudChunk = std::min(64, (int)contenido.length() - inicioChunk);
            std::strncpy(fb.b_content, contenido.substr(inicioChunk, longitudChunk).c_str(), 64);

            int posBloque = sb.s_block_start + (inodoUsuarios.i_block[i] * sizeof(FileBlock));
            file.seekp(posBloque, std::ios::beg);
            file.write(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
        
        }

        inodoUsuarios.i_size = contenido.length();
        inodoUsuarios.i_mtime = time(nullptr);

        file.seekp(posInodo, std::ios::beg);
        file.write(reinterpret_cast<char*>(&inodoUsuarios), sizeof(Inode));

        file.seekp(inicioParticion, std::ios::beg);
        file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        file.close();
        return true;
    }
};

#endif