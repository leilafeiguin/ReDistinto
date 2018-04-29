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
char* pathInstanciaData = "/home/utnso/workspace/tp-2018-1c-PuntoZip/Instancia/data/";
int cantidad_entradas;
int tamanio_entradas;

instancia_configuracion get_configuracion();
t_instancia instancia;

void crear_tabla_entradas(un_socket coordinador);
void esperar_instrucciones(un_socket coordinador);
int ejecutar_set(un_socket coordinador, char* clave);
int set(t_entrada * entrada, char* clave, char* valor);
int ejecutar_get(un_socket coordinador, char* clave);
char* get(char* clave);
int ejecutar_store(un_socket coordinador, char* clave);
int espacio_total();
int espacio_ocupado();
int espacio_disponible();
int verificar_set(char* valor); // Verifica si se puede guardar un valor
t_entrada * get_entrada_a_guardar(char* valor);	// Devuelve la entrada donde se guardara el valor
t_list * get_entradas_con_clave(char* clave); // Devuelve las entradas que contiene una clave especificada
int dump_entrada(t_entrada * entrada); // Persiste a disco una entrada especificada
void mostrar_tabla_entradas();
t_entrada * get_next(t_entrada * entrada); // Devuelve la entrada consecutiva a una entrada especificada
char* get_file_path(t_entrada * entrada); // Devuelve el path del archivo .txt que almacena el contenido de la entrada

#endif /* INSTANCIA_H_ */
