#
# COMMONS LIBRARY - SISOPUTNFRBA
#

# Clonar repo
git clone https://github.com/sisoputnfrba/so-commons-library.git ../so-commons-library

# Acceder a la carpeta
cd ../so-commons-library

# Compilar
make

# Instalar
echo "Ingrese password para instalar las commons..."
sudo make install

#
# PARSI - SISOPUTNFRBA
#

# Clonar repo
git clone https://github.com/sisoputnfrba/parsi.git ../parsi

# Acceder a la carpeta
cd ../parsi

# Instalar
echo "Ingrese password para instalar el parsi..."
sudo make install

cd ../tp-2018-1c-PuntoZip/Libraries/Debug
make

echo "export LD_LIBRARY_PATH$=LD_LIBRARY_PATH:/home/utnso/tp-2018-1c-PuntoZip/Libraries/Debug" >> /home/utnso/.bashrc

