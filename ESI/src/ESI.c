#include <stdio.h>
#include <stdlib.h>
#include "ESI.h"

void* archivo;
t_log* logger;

int main(void) {
	char* fileLog;
	fileLog = "ESI_logs.txt";

	logger = log_create(fileLog, "ESI Logs", 1, 1);
	log_info(logger, "Inicializando proceso ESI. \n");

	ESI_configuracion configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");


	return EXIT_SUCCESS;
}

ESI_configuracion get_configuracion() {
	printf("Levantando archivo de configuracion del proceso ESI\n");
	ESI_configuracion configuracion;
	t_config* archivo_configuracion = config_create(pathESIConfig);
	configuracion.IP_COORDINADOR = get_campo_config_string(archivo_configuracion, "IP_FILESYSTEM");
	configuracion.PUERTO_COORDINADOR = get_campo_config_int(archivo_configuracion, "PUERTO_FILESYSTEM");
	configuracion.IP_PLANIFICADOR = get_campo_config_string(archivo_configuracion, "NOMBRE_NODO");
	configuracion.PUERTO_PLANIFICADOR = get_campo_config_int(archivo_configuracion, "IP_NODO");
	return configuracion;
}
