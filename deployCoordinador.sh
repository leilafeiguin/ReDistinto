clear
echo "     __             __                        "
echo " .--|  .-----.-----|  .-----.--.--.-----.----."
echo " |  _  |  -__|  _  |  |  _  |  |  |  -__|   _|"
echo " |_____|_____|   __|__|_____|___  |_____|__|  "
echo "             |__|           |_____|           "
echo "               Desplegando Coordinador"
echo " "


COORDINADORPATH="Coordinador/Debug"
COORDINADORCONFIG="configCoordinador.cfg"


if [ "$1" = "-d" ]
then
	COORDINADORPATH="Coordinador/Debug"	
fi

mkdir -p "$COORDINADORPATH"

# Descargar biblioteca

compilar(){
	make -C "$COORDINADORPATH"
}

# Escribir el archivo de configuracion
generar_configuracion(){
	echo "Elija el algoritmo de distribucion:"
	select ALG in "EL" "LSU" "KE"; do
	    case $ALG in
	        EL ) echo "ALGORITMO_BALANCEO=EL" >> "$COORDINADORPATH/$COORDINADORCONFIG"; break;;
	        LSU ) echo "ALGORITMO_BALANCEO=LSU" >> "$COORDINADORPATH/$COORDINADORCONFIG";break;;
			KE ) echo "ALGORITMO_BALANCEO=KE" >> "$COORDINADORPATH/$COORDINADORCONFIG";break;;
	    esac
	done

	echo "Ingrese la cantidad de entradas"
	echo -n "> "
	read CANTIDAD_ENTRADAS
	echo "Ingrese el tamaño de entrada"
	echo -n "> "
	read TAMANIO_ENTRADA
	echo "Ingrese el retardo"
	echo -n "> "
	read RETARDO
	
	echo "ALGORITMO_DISTRIBUCION=$ALGORITMO_DISTRIBUCION" >> "$COORDINADORPATH/$COORDINADORCONFIG"
	echo "CANTIDAD_ENTRADAS=$CANTIDAD_ENTRADAS" >> "$COORDINADORPATH/$COORDINADORCONFIG"
	echo "TAMANIO_ENTRADA=$TAMANIO_ENTRADA" >> "$COORDINADORPATH/$COORDINADORCONFIG"
	echo "RETARDO=$RETARDO" >> "$COORDINADORPATH/$COORDINADORCONFIG"
	echo "PUERTO_ESCUCHA=8000" >> "$COORDINADORPATH/$COORDINADORCONFIG"
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