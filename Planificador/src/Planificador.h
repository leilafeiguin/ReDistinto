#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <Libraries.h>
#include <commons/log.h>

typedef struct planificador_configuracion {
	int PUERTO_ESCUCHA;
	char* ALGORITMO_PLANIFICACION;
	int ESTIMACION_INICIAL;
	char* IP_COORDINADOR;
	int PUERTO_COORDINADOR;
	char* CLAVES_BLOQUEADAS;

} planificador_configuracion;

const char* pathPlanificadorConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/ESI/configPlanificador.cfg";

planificador_configuracion get_configuracion();

#endif /* PLANIFICADOR_H_ */
