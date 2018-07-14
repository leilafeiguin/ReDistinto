clear
echo "     __             __                        "
echo " .--|  .-----.-----|  .-----.--.--.-----.----."
echo " |  _  |  -__|  _  |  |  _  |  |  |  -__|   _|"
echo " |_____|_____|   __|__|_____|___  |_____|__|  "
echo "             |__|           |_____|           "
echo "               Desplegando ESI"
echo " "


ESIPATH="ESI/Debug"
ESICONFIG="configESI.cfg"


if [ "$1" = "-d" ]
then
	ESIPATH="ESI/Debug"	
fi

mkdir -p "$ESIPATH"

# Descargar biblioteca

compilar(){
	make -C "$ESIPATH"
}

# Clonar repo ESI

git clone https://github.com/sisoputnfrba/Pruebas-ESI


# Escribir el archivo de configuracion
generar_configuracion(){

	echo "Ingrese IP Coordinador"
	echo -n "> "
	read IP_COORDINADOR
	echo "Ingrese puerto Coordinador"
	echo -n "> "
	read PUERTO_COORDINADOR
	echo "Ingrese IP Planificador"
	echo -n "> "
	read IP_PLANIFICADOR
	echo "Ingrese puerto Planificador"
	echo -n "> "
	read PUERTO_PLANIFICADOR

	
	echo "IP_COORDINADOR=$IP_COORDINADOR" >> "$ESIPATH/$ESICONFIG"
	echo "PUERTO_COORDINADOR=$PUERTO_COORDINADOR" >> "$ESIPATH/$ESICONFIG"
	echo "IP_PLANIFICADOR=$IP_PLANIFICADOR" >> "$ESIPATH/$ESICONFIG"
	echo "PUERTO_PLANIFICADOR=$PUERTO_PLANIFICADOR" >> "$ESIPATH/$ESICONFIG"
	
}

#
# Script principal
#

# Opcion: descargar el ejecutable
if [ "$1" != "-d" ]
then

	echo "Desea compilar?"
	select SN in "Si" "No"; do
	    case $SN in
	        Si ) compilar; break;;
	        No ) break;;
	    esac
	done

fi

# Opcion: generar el archivo de configuracion
echo "Desea generar el archivo de configuracion?"
select SN in "Si" "No"; do
    case $SN in
        Si ) generar_configuracion; break;;
        No ) break;;
    esac
done
