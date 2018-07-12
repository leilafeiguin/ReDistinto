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

instancia_configuracion configuracion;
char* pathInstanciaConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/Instancia/configInstancia.cfg";
char* pathInstanciaData;
char* nombre_instancia;
int cantidad_entradas;
int tamanio_entradas;

instancia_configuracion get_configuracion();
t_instancia instancia;
un_socket Coordinador;
void* archivo;
t_log* logger;
int punteroInstancia = 0; //Algoritmo circular
pthread_t hilo_dump;

pthread_mutex_t mutex_keys_contenidas;

void inicializar_instancia();

int esperar_instrucciones();

int ejecutar_set(void * clave);

int set(char* clave, char* valor, bool log_mensaje);

int ejecutar_get(char* clave);

char* get(char* clave);

int ejecutar_store(char* clave);

int ejecutar_dump();

int validar_necesidad_compactacion(char* clave, char* valor); // Valida si es necesario compactar la tabla de entradas para guardar un determinado valor

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

void enviar_cantidad_entradas_ocupadas();

void validar_directorio_data();

bool clave_ingresada(char* clave);

void sumar_a_entradas_no_modificadas(char* clave);

void funcion_exit(int sig);

void free_t_entrada(void * item_entrada);

void* funcion_hilo_dump(void * unused);

bool entrada_disponible(t_entrada * entrada, char* clave); // Indica si una entrada esta disponible para guardar una determinada clave

// ALGORITMOS DE REEMPLAZO

t_entrada * algoritmo_circular(char* valor);

t_entrada * least_recently_used();

t_entrada * biggest_space_used();

// !ALGORITMOS DE REEMPLAZO

#endif /* INSTANCIA_H_ */
