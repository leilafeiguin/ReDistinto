#ifndef COORDINADOR_H_
#define COORDINADOR_H_

#include <Libraries.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>


// Variables globales
pthread_mutex_t sem_instancias;

pthread_mutex_t sem_planificador;

pthread_mutex_t sem_claves_tomadas;

un_socket Planificador = codigo_error;

sem_t operaciones_habilitadas; // Semaforo que indica que se pueden realizar operaciones, utilizado para compactar

int MAX_TAMANIO_CLAVE = 40;

t_list* lista_instancias;

t_list* lista_claves_tomadas;

t_list* new_list_instancias_organized;

t_list* lista_ESIs;

int siguiente_equitative_load = 0; //Equitative Load

// !Variables globales

typedef struct coordinador_configuracion {
	char* PUERTO_ESCUCHA;
	char* ALGORITMO_DISTRIBUCION;
	int CANTIDAD_ENTRADAS;
	int TAMANIO_ENTRADA;
	int RETARDO;

} coordinador_configuracion;

coordinador_configuracion configuracion;

char* pathCoordinadorConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/Coordinador/configCoordinador.cfg";

coordinador_configuracion get_configuracion();

void serializar_claves_tomadas(void* buffer);

void salir(int motivo);

void instancia_conectada(un_socket socket_instancia, char* nombre_instancia);

bool health_check(t_instancia * instancia);

t_list * instancias_activas();

int ejecutar_set(t_ESI * ESI, char* clave, char* valor);

void ejecucion_set_caso_exito(t_instancia * instancia, t_ESI * ESI, char* clave, char* valor);

int setear(t_instancia * instancia, char* clave, char* valor);

int validar_necesidad_compactacion(t_instancia * instancia, char* clave, char* valor);

int actualizar_keys_contenidas(t_instancia * instancia);

int actualizar_cantidad_entradas_ocupadas(t_instancia * instancia);

t_clave_tomada * nueva_clave_tomada(t_ESI * id_ESI, char* clave);

int ejecutar_get(t_ESI * ESI, char* clave);

char* get(char* clave);

int ejecutar_store(t_ESI * ESI, char* clave);

int dump(); // Ejecuta un dump en todas las instancias

int dump_instancia(t_instancia * instancia); // Ejecuta un dump en una instancia especificada

t_instancia * instancia_a_guardar(char* clave); // Devuelve la instancia en la que se ejecutara un SET de acuerdo al algoritmo correspondiente

t_instancia * get_instancia_con_clave(char * clave); // Devuelve la instancia que contiene una clave especificada

t_instancia * crear_instancia(un_socket socket, char* nombre);

int enviar_informacion_tabla_entradas(t_instancia * instancia); // Envia la informacion de la tabla de entradas a la instancia

int enviar_keys_contenidas(t_instancia * instancia); // Envia las keys contenidas para la instancia que se esta restaurando

void mensaje_instancia_conectada(char* nombre_instancia, int estado); // 0: Instancia nueva, 1: Instancia reconectandose

void ejecutar_compactacion();

void handle_planificador(un_socket planificador, t_paquete* paquetePlanificador);

void handle_instancia(un_socket instancia, t_paquete* paqueteInstancia);

void handle_ESI(un_socket socket_ESI, t_paquete* paqueteESI);

int get_id_ESI_con_clave(char* clave);

bool validar_clave_ingresada(char* clave);

bool validar_clave_tomada(char* clave);

void liberar_clave_tomada(char* clave);

void liberar_claves_ESI(t_ESI * ESI);

void escuchar_ESI(t_ESI * ESI);

void escuchar_planificador();

t_ESI * generar_ESI(un_socket socket, int ID);

bool validar_tamanio_clave(char* clave);

void error_clave_larga(t_ESI * ESI, char* operacion, char* clave);

void iniciar_loggers();

void kill_ESI(t_ESI * ESI);

void notificar_resultado_instruccion(t_ESI * ESI, int cop, char* parametro);

void funcion_exit(int sig);

void liberar_instancia(t_instancia * instancia);

void retardo();

t_ESI * get_ESI_por_id(int id_ESI);

void enviar_mensaje_planificador(int cop, int tamanio_buffer, void * buffer);

void handle_consulta_clave(char* clave);

void handle_ESI_finalizado(t_ESI *  ESI);

void bloquear_claves_iniciales(t_list * lista_claves);

void enviar_claves_informacion_tomadas();

// ALGORITMOS DE DISTRIBUCION

void * crear_instancias_prueba_alan();

t_instancia * equitative_load();

t_instancia * least_space_used();

t_instancia * key_explicit(char* clave);

// !ALGORITMOS DE DISTRIBUCION

// Funciones de hilos

int i_thread = 0;
pthread_t threads[10];

pthread_t nuevo_hilo(void *(* funcion ) (void *), t_list * parametros);

void* instancia_conectada_funcion_thread(void* argumentos);

void* ESI_conectado_funcion_thread(void* argumentos);

void* planificador_conectado_funcion_thread(void* argumentos);

void clave_tomada_destroyer(t_clave_tomada * clave_tomada);

// !Funciones de hilos

/*
--------------------------------------------------------
----------------- Variables para el SV -----------------
--------------------------------------------------------
*/
void iniciar_servidor();

void handle_coneccion(int socketActual);

int listener;     // listening socket descriptors
struct sockaddr_storage remoteaddr; // client address
socklen_t addrlen;
char buf[256];    // buffer for client data
int nbytes;
char remoteIP[INET6_ADDRSTRLEN];
int yes=1;        // for setsockopt() SO_REUSEADDR, below
int i, j, rv;
struct addrinfo hints, *ai, *p;
/*
--------------------------------------------------------
----------------- FIN Variables para el SV -------------
--------------------------------------------------------
*/

#endif /* COORDINADOR_H_ */
