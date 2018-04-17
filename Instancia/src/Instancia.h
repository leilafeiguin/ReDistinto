#ifndef INSTANCIA_H_
#define INSTANCIA_H_

#include <Libraries.h>
#include <commons/log.h>

typedef struct instancia_configuracion {
	char* IP_COORDINADOR;
	char* PUERTO_COORDINADOR;
	char* ALGORITMO_REEMPLAZO;
	char* PUNTO_MONTAJE;
	char* NOMBRE_INSTANCIA;
	int INTERVALO_DUMP;
} instancia_configuracion;

const char* pathInstanciaConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/Instancia/configInstancia.cfg";

instancia_configuracion get_configuracion();

#endif /* INSTANCIA_H_ */
