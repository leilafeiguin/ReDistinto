#ifndef INSTANCIA_H_
#define INSTANCIA_H_

#include <Libraries.h>

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
char* nombre_instancia;
int cantidad_entradas;
int tamanio_entradas;

instancia_configuracion get_configuracion();
t_instancia instancia;

void inicializar_instancia(un_socket coordinador);

int esperar_instrucciones(un_socket coordinador);

int ejecutar_set(un_socket coordinador, void * clave);

int set(char* clave, char* valor, bool log_mensaje);

int ejecutar_get(un_socket coordinador, char* clave);

char* get(char* clave);

int ejecutar_store(un_socket coordinador, char* clave);

int ejecutar_dump(un_socket coordinador);

int validar_necesidad_compactacion(un_socket coordinador, char* clave, char* valor); // Valida si es necesario compactar la tabla de entradas para guardar un determinado valor

t_entrada * get_entrada_a_guardar_algoritmo_reemplazo(char* clave, char* valor);	// Detecta si no hay espacio disponible a causa de fragmentacion (y especifica el tipo) y aplica el algoritmo de reemplazo

t_entrada * get_entrada_a_guardar(char* clave, char* valor);	// Devuelve la entrada donde se guardara el valor

t_entrada * get_entrada_x_index(int id); // El ID de la intrada es igual al index de la misma en la lista

t_list * get_entradas_con_clave(char* clave); // Devuelve las entradas que contiene una clave especificada

int dump_clave(char* clave); // Persiste a disco una clave especificada

void crear_tabla_entradas(int cantidad_entradas, int tamanio_entrada);

void restaurar_clave(char* clave);

void mostrar_tabla_entradas();

t_entrada * get_next(t_entrada * entrada); // Devuelve la entrada consecutiva a una entrada especificada

char* get_file_path(char* clave); // Devuelve el path del archivo .txt que almacena el contenido de la clave

int remover_clave(char* clave); // Borra el contenido de todas las entradas que guardan una determinada clave

void compactar_tabla_entradas();

int cantidad_entradas_ocupadas();

int enviar_cantidad_entradas_ocupadas(un_socket coordinador);

void validar_directorio_data();

// ALGORITMOS DE REEMPLAZO

void * algoritmo_circular();

void * least_recently_used();

void * biggest_space_used();

// !ALGORITMOS DE REEMPLAZO

#endif /* INSTANCIA_H_ */
