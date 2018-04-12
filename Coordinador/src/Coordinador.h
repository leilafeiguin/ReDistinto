#ifndef COORDINADOR_H_
#define COORDINADOR_H_

#include <Libraries.h>
#include <commons/log.h>

typedef struct coordinador_configuracion {
	int PUERTO_ESCUCHA;
	char* ALGORITMO_DISTRIBUCION;
	int CANTIDAD_ENTRADAS;
	int TAMANIO_ENTRADA;
	int RETARDO;

} coordinador_configuracion;

const char* pathCoordinadorConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/Coordinador/configCoordinador.cfg";

coordinador_configuracion get_configuracion();

#endif /* COORDINADOR_H_ */
