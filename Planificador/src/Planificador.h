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

enum motivos_de_bloqueo {
	clave_en_uso = 0,
	bloqueado_por_consola = 1
};

typedef struct {
	t_ESI* ESI;
	int accion_a_tomar;
	char* clave_de_bloqueo;
	int motivo; //como fue bloqueado
} t_accion_a_tomar;

enum acciones_a_tomar {
	matar = 0,
	bloquear = 1,
	desbloquear = 2,
};

int idESI = 1;

sem_t sem_planificar; // Semaforo binario para saber si hay que planificar
sem_t sem_ESIs; // Semaforo contador que contiene el numero de ESIs actuales (incluyendo los listos y los bloqueados)

planificador_configuracion configuracion;

pthread_mutex_t mutex_pausa_por_consola;


pthread_mutex_t mutex_lista_de_ESIs;
t_list* lista_de_ESIs;

pthread_mutex_t mutex_cola_de_listos;
t_list* cola_de_listos;

pthread_mutex_t mutex_cola_de_bloqueados;
t_list* cola_de_bloqueados;

pthread_mutex_t mutex_cola_de_finalizados;
t_list* cola_de_finalizados;

pthread_mutex_t mutex_ESI_ejecutando;
t_ESI* ESI_ejecutando; //Es un unico esi a la vez

t_ESI* Ultimo_ESI_Ejecutado;

pthread_mutex_t mutex_accion_a_tomar;
t_list* accion_a_tomar;

void* archivo;
t_log* logger;

un_socket Coordinador; //Socket del coordinador

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

void ejecutarBloquear(char** parametros);

void ejecutarDesbloquear(char** parametros);

void ejecutarListar(char** parametros);

void * planificar(void* unused);

void ordenar_por_sjf_sd();

void ordenar_por_sjf_cd();

void ordenar_por_hrrn();

void list_swap_elems(t_list,void*,void*);

float estimarRafaga();

t_ESI* esi_por_id(int );

void actualizarRafaga();



void conectar_con_coordinador();

void * escuchar_coordinador(void * argumentos);

void ESI_conectado(un_socket socket, t_paquete* paqueteRecibido);

int nuevo_ESI(un_socket socket, int cantidad_instrucciones);

void ordenar_cola_listos();

void aumentar_espera_ESIs_listos()
;
void remover_ESI_listo(t_ESI* ESI);

void remover_ESI_bloqueado(t_ESI* ESI);

void ESI_ejecutado_exitosamente(t_ESI * ESI);

void nuevo_bloqueo(t_ESI* ESI, char* clave, int motivo);


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
