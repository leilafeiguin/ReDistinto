#include <stdio.h>
#include <stdlib.h>
#include "ESI.h"

int main(void) {
	char* fileLog;
	fileLog = "ESI_logs.txt";

	logger = log_create(fileLog, "ESI Logs", 1, 1);
	log_info(logger, "Inicializando proceso ESI. \n");

	ESI_configuracion configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

	un_socket Coordinador = conectar_a(configuracion.IP_COORDINADOR,configuracion.PUERTO_COORDINADOR);
	realizar_handshake(Coordinador, cop_handshake_ESI_Coordinador);
	int tamanio = 0; //Calcular el tamanio del paquete
	void* buffer = malloc(tamanio); //Info que necesita enviar al coordinador.
	enviar(Coordinador,cop_generico,tamanio,buffer);
	log_info(logger, "Me conecte con el Coordinador. \n");

	un_socket Planificador = conectar_a(configuracion.IP_PLANIFICADOR,configuracion.PUERTO_PLANIFICADOR);
	realizar_handshake(Planificador, cop_handshake_ESI_Planificador);
	int tamanio1 = 0; //Calcular el tamanio del paquete
	void* buffer1 = malloc(tamanio1); //Info que necesita enviar al Planificador.
	enviar(Planificador,cop_generico,tamanio1,buffer);
	log_info(logger, "Me conecte con el Planificador. \n");


	return EXIT_SUCCESS;
}

ESI_configuracion get_configuracion() {
	printf("Levantando archivo de configuracion del proceso ESI\n");
	ESI_configuracion configuracion;
	t_config* archivo_configuracion = config_create(pathESIConfig);
	configuracion.IP_COORDINADOR = get_campo_config_string(archivo_configuracion, "IP_COORDINADOR");
	configuracion.PUERTO_COORDINADOR = get_campo_config_string(archivo_configuracion, "PUERTO_COORDINADOR");
	configuracion.IP_PLANIFICADOR = get_campo_config_string(archivo_configuracion, "IP_PLANIFICADOR");
	configuracion.PUERTO_PLANIFICADOR = get_campo_config_string(archivo_configuracion, "PUERTO_PLANIFICADOR");
	return configuracion;
}
