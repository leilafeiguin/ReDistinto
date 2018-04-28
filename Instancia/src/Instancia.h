#ifndef INSTANCIA_H_
#define INSTANCIA_H_

#include <stdlib.h>
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

char* pathInstanciaConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/Instancia/configInstancia.cfg";
int cantidad_entradas;
int tamanio_entradas;

instancia_configuracion get_configuracion();
t_instancia instancia;

void crear_tabla_entradas(un_socket coordinador);
void esperar_instrucciones(un_socket coordinador);
int ejecutar_set(un_socket coordinador, char* clave);
int espacio_total();
int espacio_ocupado();
int espacio_disponible();
int verificar_set(char* valor);
int get_entrada_a_guardar(char* valor);	// Devuelve la posicion de la entrada donde se guardara el valor

#endif /* INSTANCIA_H_ */
