# MANUAL TÉCNICO - SISTEMA DE ARCHIVOS EXT2

## 1. Descripción de la Arquitectura del Sistema

* **Capa de Presentación (Frontend - Web):**
  * **Tecnología:** HTML, CSS y JavaScript 
  * **Función:** Proporciona una interfaz gráfica de usuario simulando una consola de comandos interactiva. Captura la entrada del usuario por medio de comandos manuales o carga de archivos .smia y envía peticiones HTTP estructuradas en formato JSON hacia el servidor.
* **Capa Intermedia (API REST):**
  * **Tecnología:** Node.js con el framework Express.
  * **Función:** Actúa como puente de comunicación. Recibe el JSON del frontend, extrae los comandos, los almacena temporalmente en un archivo de texto y ejecuta un proceso hijo para invocar el binario compilado de C++. Una vez que el binario finaliza, captura la salida estándar y la devuelve al frontend.
* **Capa Lógica y de Datos (Backend - C++):**
  * **Tecnología:** C++
  * **Función:** Es el núcleo del sistema. Cuenta con un analizador que procesa los comandos. Gestiona la memoria, lee y escribe bytes directamente en los archivos .mia simulando discos duros físicos.

---

## 2. Explicación de las Estructuras de Datos

El sistema simula el almacenamiento físico y lógico utilizando estructuras fundamentales programadas en C++ mediante estructuras, las cuales se escriben a nivel de bytes.

### Estructuras de Particionamiento
* **MBR (Master Boot Record):**
  * **Función:** Se ubica al inicio absoluto del archivo .mia. Contiene la información global del disco (tamaño total, fecha de creación, firma y algoritmo de ajuste).
  * **Organización:** Contiene un arreglo estático de 4 estructuras Partition. Permite un máximo de 4 particiones primarias, o hasta 3 primarias y 1 extendida.
* **EBR (Extended Boot Record):**
  * **Función:** Estructura que controla las particiones lógicas dentro de una partición extendida.
  * **Organización:** Funciona como una lista enlazada simple. Cada EBR se ubica al inicio de su partición lógica y contiene un puntero que indica el byte exacto donde comienza el siguiente EBR.

### Estructuras del Sistema de Archivos (EXT2)
* **Superbloque (Superblock):**
  * Almacena los metadatos globales de la partición. Lleva el conteo total y disponible de Inodos y Bloques, el tamaño de las estructuras, y los punteros de inicio de los Mapas de Bits. Se valida mediante el Magic Number 0xEF53.
* **Inodos (Inodes):**
  * Representan metadatos de un archivo o carpeta. No guardan el contenido, sino los permisos, propietario, tamaño, fechas, y un arreglo de 15 punteros que apuntan a los bloques de datos reales.
* **Bloques (Blocks):**
  * **Bloques de Carpeta (FolderBlock):** Contienen 4 registros. Cada registro almacena el nombre del hijo (12 caracteres) y el número del Inodo al que apunta.
  * **Bloques de Archivo (FileBlock):** Contienen un arreglo de 64 caracteres para almacenar el texto plano.

---

## 3. Descripción de los Comandos Implementados

### 3.1. Administración de Discos
* **MKDISK**: Crea un archivo binario .mia simulando un disco duro, llenándolo de ceros y escribiendo el MBR inicial.
  * **Parámetros:** -size -path, -unit, -fit.
* **RMDISK**: Elimina físicamente el archivo .mia del sistema operativo.
  * **Parámetros:** -path.
* **FDISK**: Administra las particiones dentro de un disco. Busca espacio disponible según el fit y registra particiones Primarias, Extendidas o Lógicas (creando EBRs).
  * **Parámetros:** -size, -path, -name, -unit, -type, -fit.
* **MOUNT**: Carga una partición formateada en la memoria RAM del programa. Genera un ID único para futuras referencias.
  * **Parámetros:** -path, -name.
* **MKFS**: Formatea una partición montada instalando el sistema de archivos EXT2. Calcula el número de estructuras mediante $n = \frac{tamano\_particion - sizeof(Superblock)}{4 + sizeof(Inode) + 3 \times 64}$, escribe ceros en toda la partición, inicializa los Bitmaps y crea el Inodo raíz y el archivo users.txt.
  * **Parámetros:** -id, -type.

### 3.2. Administración de Usuarios y Grupos
* **LOGIN**: Inicia una sesión de usuario. Lee el Inodo users.txt, extrae su contenido descifrando los bloques, y valida que las credenciales coincidan con un usuario activo en la partición indicada.
  * **Parámetros:** -user, -pass, -id.
* **LOGOUT**: Cierra la sesión activa actual en la memoria RAM del programa. No requiere parámetros.
* **MKGRP / RMGRP**: Crea o elimina un grupo. Requiere que el usuario root tenga una sesión activa. Extrae users.txt, añade o desactiva el registro y reescribe los bloques de archivo correspondientes.
  * **Parámetros:** -name.
* **MKUSR / RMUSR**: Crea o elimina un usuario, vinculándolo a un grupo existente. Valida reglas de negocio y reescribe users.txt.
  * **Parámetros mkusr:** -user, -pass, -grp.
* **CHGRP**: Cambia el grupo al que pertenece un usuario modificando su registro en users.txt.

### 3.3. Administración de Archivos y Carpetas
* **MKDIR**: Crea un nuevo directorio. Navega el árbol de inodos desde la raíz. Al encontrar el destino, solicita un Inodo libre y un FolderBlock libre, configurando los punteros base.
  * **Parámetros:** -path, -p
* **MKFILE**: Crea un archivo de texto. Al igual que mkdir, navega el árbol, reserva un Inodo y solicita la cantidad necesaria de FileBlocks para almacenar el texto proveniente de la consola o del archivo real referenciado.
  * **Parámetros:** -path, -size, -cont, -r.
* **CAT**: Imprime en consola el contenido de uno o más archivos. Extrae el Inodo, recorre sus i_block concatenando el contenido de cada FileBlock válido.
  * **Parámetros:** -file1, -file2, ..., -fileN.

### 3.4. Reportería e Información
* **REP**: Motor gráfico que utiliza la herramienta Graphviz. Lee las estructuras binarias y genera código DOT para renderizar imágenes
  * **Reportes soportados:** mbr, disk, inode, block, bm_inode, bm_block, tree, sb, file, ls.
  * **Parámetros:** -name, -path, -id, -ruta.
* **INFO**: Comando de auditoría que imprime en la salida estándar un resumen en texto plano del estado actual del MBR y la tabla de particiones de un disco específico, junto con un volcado hexadecimal de los primeros bytes del archivo.
  * **Parámetros:** -path.
  <br> <br>  <br>


## 2.USO DE LA APLICACION

![alt text](image.png)

### Consola de comandos
Aqui el usuario podra ingresar los comandos explicados anteriormente para su posible ejecucion

### Consola de salida
Aqui se mostrara el resultado de los comandos ejecutados

### Boton ejecutar comandos
Al presionar este boton se ejecutaron los comandos ingresados en la conssola de comandos

### Boton Cargar Script(.smia)
Al presionar este boton se abrira una ventana la cual nos permitira abrir archivos .smia, los cuales contienen comandos y estos se escribiran en la consola de comandos

### Boton limpiar
Al presionar este boton se limpiaraan tanto la consola de comandos como la consola de salida