#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <Libraries.h>
#include <commons/log.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <commons/collections/list.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct planificador_configuracion {
	char* PUERTO_ESCUCHA;
	char* ALGORITMO_PLANIFICACION;
	int ALFA_PLANIFICACION;
	int ESTIMACION_INICIAL;
	char* IP_COORDINADOR;
	char* PUERTO_COORDINADOR;
	char* CLAVES_BLOQUEADAS;

} planificador_configuracion;

typedef struct {
	t_ESI* ESI;
	char* clave_de_bloqueo;
	int motivo; //como fue bloqueado
} t_bloqueado;

typedef struct {
	int id_ESI;
	char* clave_tomada;
	char* clave_de_bloqueo;
}t_claves_por_esi;

enum motivos_de_bloqueo {
	clave_en_uso = 0,
	bloqueado_por_consola = 1,
	no_instancias_disponiles = 2,
	instancia_no_disponible = 3
};

int idESI = 1;

sem_t sem_planificar; // Semaforo binario para saber si hay que planificar
sem_t sem_sistema_ejecucion; // Semaforo para establecer la pausa por consola
sem_t sem_ESIs_listos; // Semaforo contador que contiene el numero de ESIs listos

planificador_configuracion configuracion;

pthread_mutex_t mutex_lista_de_ESIs;
t_list* lista_de_ESIs;

pthread_mutex_t mutex_cola_de_listos;
t_list* cola_de_listos;

pthread_mutex_t mutex_cola_de_bloqueados;
t_list* cola_de_bloqueados;

t_ESI* ESI_ejecutando = NULL; // Es un unico esi a la vez

t_ESI* Ultimo_ESI_Ejecutado;

t_list* ESIs_muertos;

pthread_mutex_t mutex_Coordinador;
un_socket Coordinador; //Socket del coordinador

void* archivo;
t_log* logger;

char* pathPlanificadorConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/Planificador/configPlanificador.cfg";

planificador_configuracion get_configuracion();

void salir(int motivo);

void* ejecutar_consola(void * unused);

char** validaCantParametrosComando(char* comando, int cantParametros);

void pasar_ESI_a_bloqueado(t_ESI* ESI, char* clave_de_bloqueo, int motivo);

void pasar_ESI_a_finalizado(t_ESI* ESI, char* descripcion_estado);

void pasar_ESI_a_listo(t_ESI* ESI);

void pasar_ESI_a_ejecutando(t_ESI* ESI);

bool validar_ESI_id(int id);

bool sistema_pausado();

void ejecutar_pausar();

void ejecutar_continuar();

void ejecutar_kill(int id_ESI);

void ejecutar_status(char* clave);

void ejecutar_listar(char* clave);

void ejecutar_bloquear(int id_ESI, char* clave);

void ejecutar_desbloquear(char* clave);

void * planificar(void* unused);

t_ESI * get_ESI_a_ejecutar();

bool funcion_SJF(void* item_ESI1, void* item_ESI2);

bool funcion_FIFO(t_ESI * ESI1, t_ESI * ESI2);

void ordenar_por_sjf();

float response_ratio(t_ESI * ESI);

void ordenar_por_hrrn();

void list_swap_elems(t_list,void*,void*);

void estimarRafaga(t_ESI * ESI);

t_ESI* esi_por_id(int id_ESI);

t_ESI* esi_por_socket(un_socket socket);

void actualizarRafaga(t_ESI * ESI);

void desbloquear_ESI(t_ESI * ESI, int motivo);

void desbloquear_ESIs(int motivo, char* parametro);

void conectar_con_coordinador();

void * escuchar_coordinador(void * argumentos);

void ESI_conectado(un_socket socket, t_paquete* paqueteRecibido);

t_ESI * nuevo_ESI(un_socket socket, int cantidad_instrucciones);

void ordenar_cola_listos();

void aumentar_espera_ESIs_listos();

void remover_ESI_listo(t_ESI* ESI);

void remover_ESI_bloqueado(t_ESI* ESI);

void ESI_ejecutado_exitosamente(t_ESI * ESI);

void nuevo_bloqueo(t_ESI* ESI, char* clave, int motivo);

void kill_ESI(t_ESI * ESI, char* motivo);

void eliminar_ESI_cola_actual(t_ESI * ESI);

void mostrar_resultado_consulta(void * buffer_resultado);

t_list * get_ESIs_bloqueados_por_clave(char* clave, int motivo); // Si el motivo es -1 trae todos

t_list * get_ESIs_bloqueados_por_motivo(int motivo); // Si el motivo es -1 trae todos

void mostrar_ESIs_bloqueados(char* clave, int motivo);

void bloquear_claves_iniciales();

void enviar_mensaje_coordinador(int cop, int tamanio_buffer, void * buffer);

t_list * recibir_claves_tomadas(void * buffer);

void free_claves_tomadas(void * item_clave_tomada);

void free_t_bloqueado(void * item_bloqueo);

void estimar_ESIs();

void validar_si_procesador_liberado(t_ESI * ESI);

void validar_desalojo();

void agregar_ESI_a_cola_listos(t_ESI* ESI);

/*
 * -----------------------------------------------
 * FUNCIONES DEADLOCK
 * -----------------------------------------------
 */

bool ESI_en_lista( t_list * lista, int id_ESI);

t_list * deadlock_get_ids_ESIs(t_list * claves_tomadas, t_list * claves_pedidas);

t_list * deadlock_get_ESIs_contienen_claves_pedidas(t_list * claves_tomadas, t_list * claves_pedidas, int id_ESI);

void detectar_deadlock(t_list * claves_tomadas, t_list * claves_pedidas);

void mostrar_deadlocks(t_list * listado_deadlocks);

void limpiar_deadlocks_repetidos(t_list * listado_deadlocks);

/*
 * -----------------------------------------------
 * !FUNCIONES DEADLOCK
 * -----------------------------------------------
 */


/*
--------------------------------------------------------
----------------- Variables para el SV -----------------
--------------------------------------------------------
*/
fd_set master;    // master file descriptor list
fd_set read_fds;  // temp file descriptor list for select()
int fdmax;        // maximum file descriptor number
int listener;     // listening socket descriptor
int newfd;        // newly accept()ed socket descriptor
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

#endif /* PLANIFICADOR_H_ */
