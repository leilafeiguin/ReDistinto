#ifndef ESI_H_
#define ESI_H_

#include <Libraries.h>
#include <commons/log.h>
#include <parsi/parser.h>

typedef struct ESI_configuracion {
	char* IP_COORDINADOR;
	char* PUERTO_COORDINADOR;
	char* IP_PLANIFICADOR;
	char* PUERTO_PLANIFICADOR;

} ESI_configuracion;

void* archivo;
t_log* logger;
un_socket Coordinador;
ESI_configuracion configuracion;
un_socket Planificador;
t_list* instrucciones;
t_config* archivo_configuracion;

int ID;
int index_proxima_instruccion = 0;

typedef struct paqueteSentencias {
	int cantidadInstrucciones;
	int idESI;
} paqueteSentencias;

char* pathESIConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/ESI/configESI.cfg";

ESI_configuracion get_configuracion();

void leerScript(char* path);

void ejecutar(t_esi_operacion*);

void ejecutar_get(char* clave);

void leer_archivo(char* path);

void conectar_con_planificador();

void conectar_con_coordinador();

void liberar_memoria();

#endif /* ESI_H_ */
