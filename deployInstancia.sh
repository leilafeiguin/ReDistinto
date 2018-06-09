clear
echo "     __             __                        "
echo " .--|  .-----.-----|  .-----.--.--.-----.----."
echo " |  _  |  -__|  _  |  |  _  |  |  |  -__|   _|"
echo " |_____|_____|   __|__|_____|___  |_____|__|  "
echo "             |__|           |_____|           "
echo "               Desplegando Instancia"
echo " "


INSTANCIAPATH="Instancia/Debug"
INSTANCIACONFIG="configInstancia.cfg"


if [ "$1" = "-d" ]
then
	INSTANCIAPATH="Instancia/Debug"	
fi

mkdir -p "$INSTANCIAPATH"

# Descargar biblioteca

compilar(){
	make -C "$INSTANCIAPATH"
}

# Escribir el archivo de configuracion
generar_configuracion(){
	echo "Elija el algoritmo de reemplazo:"
	select ALG in "BSU" "CIRC" "LRU"; do
	    case $ALG in
	        BSU ) echo "ALGORITMO_REEMPLAZO=BSU" >> "$INSTANCIAPATH/$INSTANCIACONFIG"; break;;
	        CIRC ) echo "ALGORITMO_REEMPLAZO=CIRC" >> "$INSTANCIAPATH/$INSTANCIACONFIG";break;;
			LRU ) echo "ALGORITMO_REEMPLAZO=LRU" >> "$INSTANCIAPATH/$INSTANCIACONFIG";break;;
	    esac
	done

	echo "Ingrese IP Coordinador"
	echo -n "> "
	read IP_COORDINADOR
	echo "Ingrese puerto Coordinador"
	echo -n "> "
	read PUERTO_COORDINADOR
	echo "Ingrese nombre de Instancia"
	echo -n "> "
	read NOMBRE_INSTANCIA
	echo "Ingrese la ruta del punto de montaje"
	echo -n "> "
	read PUNTO_MONTAJE
	echo "Ingrese intervalo dump"
	echo -n "> "
	read INTERVALO_DUMP

	
	echo "IP_COORDINADOR=$IP_COORDINADOR" >> "$INSTANCIAPATH/$INSTANCIACONFIG"
	echo "PUERTO_COORDINADOR=$PUERTO_COORDINADOR" >> "$INSTANCIAPATH/$INSTANCIACONFIG"
	echo "NOMBRE_INSTANCIA=$NOMBRE_INSTANCIA" >> "$INSTANCIAPATH/$INSTANCIACONFIG"
	echo "PUNTO_MONTAJE=$PUNTO_MONTAJE" >> "$INSTANCIAPATH/$INSTANCIACONFIG"
	echo "INTERVALO_DUMP=$INTERVALO_DUMP" >> "$INSTANCIAPATH/$INSTANCIACONFIG"
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