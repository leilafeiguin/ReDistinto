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