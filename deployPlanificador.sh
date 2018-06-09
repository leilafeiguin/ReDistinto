clear
echo "     __             __                        "
echo " .--|  .-----.-----|  .-----.--.--.-----.----."
echo " |  _  |  -__|  _  |  |  _  |  |  |  -__|   _|"
echo " |_____|_____|   __|__|_____|___  |_____|__|  "
echo "             |__|           |_____|           "
echo "               Desplegando Planificador"
echo " "


PLANIFICADORPATH="Planificador/Debug"
PLANIFICADORCONFIG="configPlanificador.cfg"


if [ "$1" = "-d" ]
then
	PLANIFICADORPATH="Planificador/Debug"	
fi

mkdir -p "$PLANIFICADORPATH"

# Descargar biblioteca

compilar(){
	make -C "$PLANIFICADORPATH"
}

# Escribir el archivo de configuracion
generar_configuracion(){
	echo "Elija el algoritmo de planificacion:"
	select ALG in "SJF-CD" "SJF-SD" "HRRN"; do
	    case $ALG in
	        SJF-CD ) echo "ALGORITMO_PLANIFICACION=SJF-CD" >> "$PLANIFICADORPATH/$PLANIFICADORCONFIG"; break;;
	        SJF-SD ) echo "ALGORITMO_PLANIFICACION=SJF-SD" >> "$PLANIFICADORPATH/$PLANIFICADORCONFIG";break;;
			HRRN ) echo "ALGORITMO_PLANIFICACION=HRRN" >> "$PLANIFICADORPATH/$PLANIFICADORCONFIG";break;;
	    esac
	done

	echo "Ingrese IP Coordinador"
	echo -n "> "
	read IP_COORDINADOR
	echo "Ingrese puerto Coordinador"
	echo -n "> "
	read PUERTO_COORDINADOR
	echo "Ingrese puerto escucha"
	echo -n "> "
	read PUERTO_ESCUCHA
	echo "Ingrese alfa de planificacion"
	echo -n "> "
	read ALFA_PLANIFICACION
	echo "Ingrese la estimacion inicial"
	echo -n "> "
	read ESTIMACION_INICIAL
	echo "Ingrese las claves bloqueadas"
	echo -n "> "
	read CLAVES_BLOQUEADAS

	
	echo "IP_COORDINADOR=$IP_COORDINADOR" >> "$PLANIFICADORPATH/$PLANIFICADORCONFIG"
	echo "PUERTO_COORDINADOR=$PUERTO_COORDINADOR" >> "$PLANIFICADORPATH/$PLANIFICADORCONFIG"
	echo "PUERTO_ESCUCHA=$PUERTO_ESCUCHA" >> "$PLANIFICADORPATH/$PLANIFICADORCONFIG"
	echo "ALFA_PLANIFICACION=$ALFA_PLANIFICACION" >> "$PLANIFICADORPATH/$PLANIFICADORCONFIG"
	echo "ESTIMACION_INICIAL=$ESTIMACION_INICIAL" >> "$PLANIFICADORPATH/$PLANIFICADORCONFIG"
	echo "CLAVES_BLOQUEADAS=$CLAVES_BLOQUEADAS" >> "$PLANIFICADORPATH/$PLANIFICADORCONFIG"
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