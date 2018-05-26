#ifndef ESI_H_
#define ESI_H_

#include <Libraries.h>
#include <commons/log.h>
#include <parsi/parser.h>

void* archivo;
t_log* logger;
un_socket Coordinador;

typedef struct ESI_configuracion {
	char* IP_COORDINADOR;
	char* PUERTO_COORDINADOR;
	char* IP_PLANIFICADOR;
	char* PUERTO_PLANIFICADOR;

} ESI_configuracion;

typedef struct paqueteSentencias {
	int cantidadInstrucciones;
	int idESI;
} paqueteSentencias;

char* pathESIConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/ESI/configESI.cfg";

ESI_configuracion get_configuracion();

void leerScript(char* path);

void ejecutar_get(char* clave);

#endif /* ESI_H_ */
