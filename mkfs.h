#ifndef MKFS_H
#define MKFS_H

#include <iostream>   
#include <string>     
#include <sstream>   
#include <fstream> 
#include <cmath>
#include <algorithm>  
#include "structures.h"
#include "mount.h"    

namespace CommandMkfs {
    
    inline std::string toLowerCase(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    
    inline std::string execute(const std::string& id, const std::string& type) {
        
        if (id.empty()) {
            return "Error: mkfs requiere el parámetro -id";
        }
        
        std::string formatType = toLowerCase(type);
        if (formatType.empty()) {
            formatType = "full";
        }
        
        if (formatType != "full") {
            return "Error: type debe ser 'full'";
        }
        
        std::string fsType = "ext2";
        
        ComandoMount::ParticionMontada partition;
        if (!ComandoMount::getMountedPartition(id, partition)) {
            return "Error: la partición con ID '" + id + "' no está montada";
        }
        
        std::fstream file(partition.ruta, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + partition.ruta + "'";
        }
        
        int partitionSize = partition.tamano;
        
        int numerator = partitionSize - sizeof(Superblock);
        int denominator = 4 + sizeof(Inode) + 3 * 64;
        int n = numerator / denominator;
        
        if (n <= 0) {
            file.close();
            return "Error: la partición es muy pequeña para crear un sistema de archivos";
        }
        
        Superblock sb;
        sb.s_filesystem_type = 2;
        sb.s_inodes_count = n;
        sb.s_blocks_count = 3 * n;
        sb.s_free_blocks_count = 3 * n - 2;
        sb.s_free_inodes_count = n - 2;
        sb.s_mtime = time(nullptr);
        sb.s_umtime = time(nullptr);
        sb.s_mnt_count = 1;
        sb.s_magic = 0xEF53;
        sb.s_inode_size = sizeof(Inode);
        sb.s_block_size = 64;
        sb.s_first_ino = 2;
        sb.s_first_blo = 2;
        sb.s_bm_inode_start = partition.inicio+ sizeof(Superblock);
        sb.s_bm_block_start = sb.s_bm_inode_start + n;
        sb.s_inode_start = sb.s_bm_block_start + 3 * n;
        sb.s_block_start = sb.s_inode_start + n * sizeof(Inode);
        
        file.seekp(partition.inicio, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));
        
        file.seekp(sb.s_bm_inode_start, std::ios::beg);
        for (int i = 0; i < n; i++) {
            char bit = (i == 0 || i == 1) ? '1' : '0';
            file.write(&bit, 1);
        }
        
        file.seekp(sb.s_bm_block_start, std::ios::beg);
        for (int i = 0; i < 3 * n; i++) {
            char bit = (i == 0 || i == 1) ? '1' : '0';
            file.write(&bit, 1);
        }
        
        Inode rootInode;
        rootInode.i_uid = 1;
        rootInode.i_gid = 1;
        rootInode.i_size = 0;
        rootInode.i_atime = time(nullptr);
        rootInode.i_ctime = time(nullptr);
        rootInode.i_mtime = time(nullptr);
        rootInode.i_type = '0';
        rootInode.i_perm = 664;
        rootInode.i_block[0] = 0; 
        
        file.seekp(sb.s_inode_start, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&rootInode), sizeof(Inode));
        
        Inode usersInode;
        usersInode.i_uid = 1;
        usersInode.i_gid = 1;
        usersInode.i_size = 27;
        usersInode.i_atime = time(nullptr);
        usersInode.i_ctime = time(nullptr);
        usersInode.i_mtime = time(nullptr);
        usersInode.i_type = '1';
        usersInode.i_perm = 664;
        usersInode.i_block[0] = 1;
        
        file.seekp(sb.s_inode_start + sizeof(Inode), std::ios::beg);
        file.write(reinterpret_cast<const char*>(&usersInode), sizeof(Inode));
        
        FolderBlock rootBlock;
        
        std::strncpy(rootBlock.b_content[0].b_name, ".", 12);
        rootBlock.b_content[0].b_inodo = 0;
        
        std::strncpy(rootBlock.b_content[1].b_name, "..", 12);
        rootBlock.b_content[1].b_inodo = 0;
        
        std::strncpy(rootBlock.b_content[2].b_name, "users.txt", 12);
        rootBlock.b_content[2].b_inodo = 1;
        
        rootBlock.b_content[3].b_inodo = -1;
        
        file.seekp(sb.s_block_start, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&rootBlock), sizeof(FolderBlock));
        
        FileBlock usersBlock;
        std::string usersContent = "1,G,root\n1,U,root,root,123\n";
        std::strncpy(usersBlock.b_content, usersContent.c_str(), 64);
        
        file.seekp(sb.s_block_start + 64, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&usersBlock), sizeof(FileBlock));
        
        file.close();
        
        std::ostringstream result;
        result << "\n=== MKFS ===\n";
        result << "Sistema de archivos EXT2 creado exitosamente\n";
        result << "  ID: " << id << "\n";
        result << "  Disco: " << partition.ruta << "\n";
        result << "  Partición: " << partition.nombre << "\n";
        result << "  Tamaño: " << partitionSize << " bytes\n";
        result << "  Inodos: " << n << "\n";
        result << "  Bloques: " << (3 * n) << "\n";
        result << "  Archivo users.txt creado en la raíz";
        
        return result.str();
    }
    
}

#endif