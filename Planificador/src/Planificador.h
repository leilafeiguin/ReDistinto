#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <Libraries.h>
#include <commons/log.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <commons/collections/list.h>

typedef struct planificador_configuracion {
	char* PUERTO_ESCUCHA;
	char* ALGORITMO_PLANIFICACION;
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

t_list* lista_de_ESIs;
t_list* cola_de_listos;
t_list* cola_de_bloqueados;
t_list* cola_de_finalizados;
t_ESI* ESI_ejecutando; //Es un unico esi a la vez
t_list* accion_a_tomar;

char* pathPlanificadorConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/Planificador/configPlanificador.cfg";

planificador_configuracion get_configuracion();

void salir(int motivo);

void iniciarConsolaPlanificador();

char** validaCantParametrosComando(char* comando, int cantParametros);

void pasar_ESI_a_bloqueado(int id_ESI, char* clave_de_bloqueo, int motivo);

void pasar_ESI_a_finalizado(int id_ESI);
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
