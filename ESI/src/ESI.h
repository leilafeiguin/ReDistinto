#ifndef ESI_H_
#define ESI_H_

#include <Libraries.h>
#include <commons/log.h>

void* archivo;
t_log* logger;

typedef struct ESI_configuracion {
	char* IP_COORDINADOR;
	int PUERTO_COORDINADOR;
	char* IP_PLANIFICADOR;
	int PUERTO_PLANIFICADOR;

} ESI_configuracion;

const char* pathESIConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/ESI/configESI.cfg";

ESI_configuracion get_configuracion();

#endif /* ESI_H_ */
