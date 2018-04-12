#include <stdio.h>
#include <stdlib.h>
#include "Coordinador.h"

void* archivo;
t_log* logger;

int main(void) {
	char* fileLog;
	fileLog = "coordinador_logs.txt";

	logger = log_create(fileLog, "ESI Logs", 1, 1);
	log_info(logger, "Inicializando proceso Coordinador. \n");

	coordinador_configuracion configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

	return EXIT_SUCCESS;
}

coordinador_configuracion get_configuracion() {
	printf("Levantando archivo de configuracion del proceso Coordinador\n");
	coordinador_configuracion configuracion;
	t_config* archivo_configuracion = config_create(pathCoordinadorConfig);
	configuracion.PUERTO_ESCUCHA = get_campo_config_int(archivo_configuracion, "PUERTO_ESCUCHA");
	configuracion.ALGORITMO_DISTRIBUCION = get_campo_config_string(archivo_configuracion, "ALGORITMO_DISTRIBUCION");
	configuracion.CANTIDAD_ENTRADAS = get_campo_config_int(archivo_configuracion, "CANTIDAD_ENTRADAS");
	configuracion.TAMANIO_ENTRADA = get_campo_config_int(archivo_configuracion, "TAMANIO_ENTRADA");
	configuracion.RETARDO = get_campo_config_int(archivo_configuracion, "RETARDO");
	return configuracion;
}
