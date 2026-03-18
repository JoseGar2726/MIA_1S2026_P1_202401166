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
#include "cat.h"
#include "mkfs.h"    
#include "login.h"  
#include "mkgrp.h" 
#include "rmgrp.h"
#include "mkusr.h"
#include "rmusr.h"
#include "chgrp.h"
#include "mkfile.h"
#include "mkdir.h"
#include "rep.h"

// Funcion para convertir string a minusculas
std::string toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// Funcion para remover comillas de una cadena
std::string removeQuotes(const std::string& str) {
    if (str.length() >= 2 && 
        ((str.front() == '"' && str.back() == '"') || 
         (str.front() == '\'' && str.back() == '\''))) {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

// Funcion para parsear parametros con soporte para comillas y cualquier orden
std::string parseParameter(const std::string& commandLine, const std::string& paramName) {
    // Buscar el parametro
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

std::string parametrosInvalidos(const std::string& commandLine, const std::vector<std::string>& parametrosValidos){
    std::string minCmd = toLowerCase(commandLine);
    size_t pos = 0;

    while ((pos = minCmd.find("-", pos)) != std::string::npos){
        if(pos > 0 && minCmd[pos - 1] != ' '){
            pos++;
            continue;
        }
        
        size_t eqPos = minCmd.find('=', pos);
        size_t spacePos = minCmd.find(' ', pos);

        size_t endPos = std::min(eqPos, spacePos);
        if(endPos == std::string::npos) {
            endPos = minCmd.length();
        }

        std::string nombreParametro = minCmd.substr(pos, endPos - pos);

        bool permitido = false;
        for(const std::string& valido: parametrosValidos){
            if(nombreParametro == toLowerCase(valido)){
                permitido = true;
                break;
            }
        }

        if(!permitido){
            return nombreParametro;
        }

        pos = endPos;
    }
    return "";
}

bool validarExtension(const std::string& path){
    size_t pos = path.find_last_of('.');

    if(pos == std::string::npos){
        return false;
    }

    std::string ext = toLowerCase(path.substr(pos));

    return ext == ".mia";
}

// Funcion para parsear y ejecutar comandos
std::string executeCommand(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string cmd;
    iss >> cmd;

    if(cmd.empty()){
        return "";
    }
    
    cmd = toLowerCase(cmd);

    if(cmd[0] == '#'){
        return commandLine;
    }

    //COMANDO MKDISK
    if (cmd == "mkdisk") { 
        std::vector<std::string> permitidos = {"-size", "-fit", "-unit", "-path"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkdisk";
        }

        std::string sizeStr = parseParameter(commandLine, "-size");
        std::string unit = parseParameter(commandLine, "-unit");
        std::string path = parseParameter(commandLine, "-path");
        std::string fit = parseParameter(commandLine, "-fit");
        
        if (sizeStr.empty() || path.empty()) {
            return "Error: mkdisk requiere parámetros -size y -path\n"
                   "Uso: mkdisk -size=N -unit=[k|m] -path=ruta\n -fit=[BF|FF|WF]"
                   "Los parámetros pueden estar en cualquier orden";
        }

        path = removeQuotes("path");

        if(!validarExtension(path)){
            return "Error: el disco debe tener la extension .mia";
        }
        
        int size;
        try {
            size = std::stoi(sizeStr);
        } catch (const std::exception& e) {
            return "Error: el valor de size debe ser un número entero positivo";
        }
        
        if (size <= 0) {
            return "Error: el tamaño debe ser un número positivo";
        }
        
        if (unit.empty()) {
            unit = "m";
        } else {
            unit = toLowerCase(unit);
        }
        
        if (unit != "k" && unit != "m") {
            return "Error: unit debe ser 'k' (kilobytes) o 'm' (megabytes)";
        }

        return DiskManager::mkdisk(size, unit, path, fit);

    //COMANDO RMDISK
    } else if (cmd == "rmdisk") {
        std::vector<std::string> permitidos = {"-path"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando rmdisk";
        }

        std::string path = parseParameter(commandLine, "-path");

        if (path.empty()) {
            return "Error: rmdisk requiere parámetro -path\n"
                   "Uso: rmdisk -path=ruta";
        }

        path = removeQuotes("path");

        if(!validarExtension(path)){
            return "Error: el disco debe tener la extension .mia";
        }

        return DiskManager::rmdisk(path);

    //COMANDO FDISK
    } else if (cmd == "fdisk"){
        std::vector<std::string> permitidos = {"-path", "-size", "-unit", "-type", "-fit", "-name"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando fdisk";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        std::string nombre = parseParameter(commandLine, "-name");

        if (ruta.empty()){
            return "Error: fdisk requiere parametro -path\n"
            "Uso: fdisk -size=N -unit=[k|m] -path=ruta -type=[P|E|L] -fit=[BF|FF|WF] -name=nombre\n"
            "fdisk -delete=nombre -path=ruta";
        }

        ruta = removeQuotes(ruta);

        if(!validarExtension(ruta)){
            return "Error: el disco debe tener la extension .mia";
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

        if (unidad != "b" && unidad != "k" && unidad != "m") {
            return "Error: unit debe ser 'b' (bytes) o 'k' (kilobytes) o 'm' (megabytes)";
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

        return ComandoFdisk::execute(tamano, unidad, ruta, tipo, fit , nombre);

    //COMANDO MOUNT
    } else if (cmd == "mount"){
        std::vector<std::string> permitidos = {"-path", "-name"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mount";
        }

        std::string ruta = parseParameter(commandLine, "-path");
        std::string nombre = parseParameter(commandLine, "-name");

        ruta = removeQuotes(ruta);
        nombre = removeQuotes(nombre);

        if(!validarExtension(nombre)){
            return "Error: el disco debe tener la extension .mia";
        }
        
        if (ruta.empty() || nombre.empty()) {
            return "Error: mount requiere parámetros -path y -name\n"
                   "Uso: mount -path=ruta -name=nombre";
        }
        
        return ComandoMount::execute(ruta, nombre);

    //COMANDO MKFS
    } else if (cmd == "mkfs") {
        std::vector<std::string> permitidos = {"-id", "-type"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkfs";
        }

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

    //COMANDO CAT
    } else if (cmd == "cat"){
        std::vector<std::string> archivos;

        int i = 1;
        while (true){
            std::string path = parseParameter(commandLine, "-file" + std::to_string(i));
            if(path.empty()) break;
            archivos.push_back(removeQuotes(path));
            i++;
        }

        if(archivos.empty()){
            std::string path = parseParameter(commandLine, "-file");
            if(!path.empty()){
                archivos.push_back(removeQuotes(path));
            } else {
                return "Error: cat requiere almenos un parametro -file1 \n"
                       "Uso: cat -file1=ruta1 ... -filen=rutan";
            }
        }
        
        return ComandoCat::execute(archivos);

    //COMANDO LOGIN
    } else if (cmd == "login"){
        std::vector<std::string> permitidos = {"-user", "-pass", "-id"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando login";
        }

        std::string usuario = parseParameter(commandLine, "-user");
        std::string password = parseParameter(commandLine, "-pass");
        std::string id = parseParameter(commandLine, "-id");

        if(usuario.empty() || password.empty() || id.empty()){
            return "Error: login requiere los parametros -user, -pass e -id \n"
                   "Uso login -user=usuario -pass=password -id=id_particion";
        }

        usuario = removeQuotes(usuario);
        password = removeQuotes(password);

        return ComandoLogin::execute(usuario, password, id);
    
    //COMANDO LOGOUT
    } else if(cmd == "logout"){
        if(!Sesion::activo){
            return "Error: no hay sesion iniciada";
        }

        std::string usuarioActual = Sesion::usuario;

        Sesion::logout();

        return "Sesion cerrada correctamente - " + usuarioActual + "";

    //COMANDO MKGRP
    } else if (cmd == "mkgrp"){
        std::vector<std::string> permitidos = {"-name"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkgrp";
        }

        std::string nombre = parseParameter(commandLine, "-name");

        if(nombre.empty()){
            return "Error: mkgrp requiere el parametro -name\n"
                   "Uso: mkgrp -name=nombre";
        }

        nombre = removeQuotes(nombre);
        return ComandoMkgrp::execute(nombre);
    
    //COMANDO RMGRP
    } else if (cmd == "rmgrp"){
        std::vector<std::string> permitidos = {"-name"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando rmgrp";
        }

        std::string nombre = parseParameter(commandLine, "-name");

        if(nombre.empty()){
            return "Error: rmgrp requiere el parametro -name\n"
                   "Uso: rmgrp -name=nombre";
        }

        nombre = removeQuotes(nombre);
        return ComandoRmgrp::execute(nombre);

    //COMANDO MKUSR
    } else if (cmd == "mkusr"){
        std::vector<std::string> permitidos = {"-user", "-pass", "-grp"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkuser";
        }
        
        std::string nombre = parseParameter(commandLine, "-user");
        std::string pass = parseParameter(commandLine, "-pass");
        std::string grupo = parseParameter(commandLine, "-grp");

        if(nombre.empty() || pass.empty() || grupo.empty()){
            return "Error: mkusr requiere los parametros -user, -pass y -grp\n"
                   "Uso: mkusr -user=nombre -pass=contrasena -grp=grupo";
        }

        nombre = removeQuotes(nombre);
        pass = removeQuotes(pass);
        grupo = removeQuotes(grupo);

        return ComandoMkusr::execute(nombre, pass, grupo);
    
    //COMANDO RMUSR
    } else if (cmd == "rmusr"){
        std::vector<std::string> permitidos = {"-user"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando rmuser";
        }

        std::string nombre = parseParameter(commandLine, "-user");

        if(nombre.empty()){
            return "Error: rmusr requiere el parametro -user\n"
                   "Uso: mkusr -user=nombre";
        }

        nombre = removeQuotes(nombre);

        return ComandoRmusr::execute(nombre);

    //COMANDO CHGRP
    } else if (cmd == "chgrp"){
        std::vector<std::string> permitidos = {"-user", "-grp"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando chgrp";
        }

        std::string nombre = parseParameter(commandLine, "-user");
        std::string grupo = parseParameter(commandLine, "-grp");

        if(nombre.empty() || grupo.empty()){
            return "Error: chgrp requiere el parametro -user y -grp\n"
                   "Uso: chgrp -user=nombre -grp=grupo";
        }

        nombre = removeQuotes(nombre);
        grupo = removeQuotes(grupo);

        return ComandoChgrp::execute(nombre, grupo);

    //COMANDO MKFILE
    } else if (cmd == "mkfile"){
        std::vector<std::string> permitidos = {"-path", "-r", "-size", "-cont"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkfile";
        }

        std::string path = parseParameter(commandLine, "-path");
        std::string strR = parseParameter(commandLine, "-r");
        std::string strSize = parseParameter(commandLine, "-size");
        std::string cont = parseParameter(commandLine, "-cont");

        if(path.empty()){
            return "Error: mkfile requiere el parametro -path\n"
                   "Uso: mkfile -path=ruta -r -size=tamano -cont=ruta";
        }

        path = removeQuotes(path);
        cont = removeQuotes(cont);

        std::string comandoMinusculas = toLowerCase(commandLine);
        bool r = (comandoMinusculas.find("-r") != std::string::npos);

        int size = strSize.empty() ? 0 : std::stoi(strSize);

        return ComandoMkfile::execute(path, r, size, cont);

    //COMANDO MKDIR
    } else if (cmd == "mkdir"){
        std::vector<std::string> permitidos = {"-path", "-p"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando mkdir";
        }

        std::string path = parseParameter(commandLine, "-path");

        if(path.empty()){
            return "Error: mkdir requiere el parametro -path\n"
                   "Uso: mkdir -path=ruta -p";
        }

        path = removeQuotes(path);

        std::string comandoMinusculas = toLowerCase(commandLine);
        bool p = false;
        std::stringstream ssCmd(comandoMinusculas);
        std::string palabra;

        while (ssCmd >> palabra){
            if (palabra == "-p"){
                p = true;
                break;
            }
        }
        
        return ComandoMkdir::execute(path, p);

    //COMANDO REP
    } else if(cmd == "rep"){
        std::vector<std::string> permitidos = {"-name", "-path", "-id", "-path_file_ls"};
        std::string parametroInvalido = parametrosInvalidos(commandLine, permitidos);

        if(!parametroInvalido.empty()){
            return "Error: parametro no reconocido '" + parametroInvalido + "' en el comando rep";
        }

        std::string name = parseParameter(commandLine, "-name");
        std::string path = parseParameter(commandLine, "-path");
        std::string id = parseParameter(commandLine, "-id");
        std::string ruta = parseParameter(commandLine, "-path_file_ls");
        

        if(name.empty() || path.empty() || id.empty()){
            return "Error: rep requiere los parametros -name, -path y -id\n"
                   "Uso: rep -name=nombre -path=ruta -id=idParticion";
        }

        name = removeQuotes(name);
        path = removeQuotes(path);
        id = removeQuotes(id);
        ruta = removeQuotes(ruta);

        return ComandoRep::execute(name, path, id, ruta);

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