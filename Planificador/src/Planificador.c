#include <stdio.h>
#include <stdlib.h>
#include "Planificador.h"

void* archivo;
t_log* logger;

int main(void) {
	char* fileLog;
	fileLog = "planificador_logs.txt";

	logger = log_create(fileLog, "Planificador Logs", 1, 1);
	log_info(logger, "Inicializando proceso Planificador. \n");

	planificador_configuracion configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

	iniciarConsolaPlanificador();

	return EXIT_SUCCESS;
}

planificador_configuracion get_configuracion() {
	printf("Levantando archivo de configuracion del proceso Planificador\n");
	planificador_configuracion configuracion;
	t_config* archivo_configuracion = config_create(pathPlanificadorConfig);
	configuracion.PUERTO_ESCUCHA = get_campo_config_int(archivo_configuracion, "PUERTO_ESCUCHA");
	configuracion.ALGORITMO_PLANIFICACION = get_campo_config_string(archivo_configuracion, "ALGORITMO_PLANIFICACION");
	configuracion.ESTIMACION_INICIAL = get_campo_config_int(archivo_configuracion, "ESTIMACION_INICIAL");
	configuracion.IP_COORDINADOR = get_campo_config_string(archivo_configuracion, "IP_COORDINADOR");
	configuracion.PUERTO_COORDINADOR = get_campo_config_int(archivo_configuracion, "PUERTO_COORDINADOR");
	configuracion.CLAVES_BLOQUEADAS = get_campo_config_string(archivo_configuracion, "CLAVES_BLOQUEADAS");
	return configuracion;
}

void iniciarConsolaPlanificador(){
	log_info(logger, "Consola Iniciada. Ingrese una opcion: \n");
	char * linea;
	char* primeraPalabra;
	char* context;
	while (1) {
		linea = readline(">");
		if (!linea || string_equals_ignore_case(linea, "")) {
			continue;
		} else {
			add_history(linea);
			char** parametros=NULL;
			int lineaLength = strlen(linea);
			char *lineaCopia = (char*) calloc(lineaLength + 1,
					sizeof(char));
			strncpy(lineaCopia, linea, lineaLength);
			primeraPalabra = strtok_r(lineaCopia, " ", &context);

			if (strcmp(linea, "Pausar") == 0) {
				log_info(logger, "Eligio la opcion Pausar\n");
				free(linea);
			}else if (strcmp(linea, "Continuar") == 0) {
				log_info(logger, "Eligio la opcion Continuar\n");
				free(linea);
			} else if (strcmp(primeraPalabra, "bloquear") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				parametros = validaCantParametrosComando(linea,2);
				free(linea);
			} else if (strcmp(primeraPalabra, "desbloquear") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				parametros = validaCantParametrosComando(linea,1);
				free(linea);
			} else if (strcmp(primeraPalabra, "listar") == 0) {
				log_info(logger, "Eligio la opcion Listar\n");
				parametros = validaCantParametrosComando(linea,1);
				free(linea);
			} else if (strcmp(primeraPalabra, "kill") == 0) {
				log_info(logger, "Eligio la opcion Kill\n");
				parametros = validaCantParametrosComando(linea,1);
				free(linea);
			} else if (strcmp(primeraPalabra, "status") == 0) {
				log_info(logger, "Eligio la opcion Status\n");
				parametros = validaCantParametrosComando(linea,1);
				free(linea);
			} else if (strcmp(linea, "deadlock") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				free(linea);
			} else {
				log_error(logger, "Opcion no valida.\n");
				printf("Opcion no valida.\n");
				free(linea);
			}
			free(lineaCopia);
			if (parametros != NULL)
				free(parametros);
		}
	}
}

char** validaCantParametrosComando(char* comando, int cantParametros) {
	int i = 0;
	char** parametros;
	parametros = str_split(comando, ' ');
	for (i = 1; *(parametros + i); i++) {}
	if (i != cantParametros + 1) {
		log_error(logger, "%s necesita %i parametro/s. \n", comando, cantParametros);
		return NULL;
	} else {
		log_info(logger, "Cantidad de parametros correcta. \n");
		return parametros;
	}
	return NULL;
}
