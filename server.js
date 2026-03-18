const express = require('express');
const cors = require('cors');
const { exec } = require('child_process');
const fs = require('fs');

const app = express();

app.use(cors());
app.use(express.json());

app.use(express.static(__dirname));

app.post('/ejecutar', (req, res) => {
    
    const comandos = req.body.consola; 

    fs.writeFileSync('comandos.txt', comandos + '\nexit\n');

    const comandoEjecucion = './proyecto1 < comandos.txt';

    exec(comandoEjecucion, (error, stdout, stderr) => {
        if (error) {
            console.error(`Error ejecutando: ${error.message}`);
            
            return res.json({ respuesta: stdout + "\nERROR DEL SISTEMA: " + stderr });
        }
        
        res.json({ respuesta: stdout });
    });
});

app.listen(3000, () => {
    console.log('Corriendo en http://localhost:3000');
});