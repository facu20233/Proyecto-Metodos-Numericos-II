# Compilar binarios
make run_all //comando para que corra todo. Previamente se necesita instalar la herramienta "make" uso en linux

# Ejecutar barrido de pruebas
chmod +x scripts/benchmark.sh  //necesario para los permisos //con el make se hace solo
./scripts/benchmark.sh

# Generar gráficos analíticos y de rendimiento
python scripts/generar_graficos.py  //con el make se hace solo