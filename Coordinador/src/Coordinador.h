#ifndef COORDINADOR_H_
#define COORDINADOR_H_

#include <Libraries.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/string.h>

bool esEstadoInvalido;

typedef struct coordinador_configuracion {
	char* PUERTO_ESCUCHA;
	char* ALGORITMO_DISTRIBUCION;
	int CANTIDAD_ENTRADAS;
	int TAMANIO_ENTRADA;
	int RETARDO;

} coordinador_configuracion;

coordinador_configuracion configuracion;

char* pathCoordinadorConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/Coordinador/configCoordinador.cfg";

t_list* lista_instancias;

t_list* new_list_instancias_organized;

t_list* lista_claves_tomadas;

typedef struct {
	int id_ESI; // ID del ESI que tiene la clave
	char* clave; // Nombre de la clave tomada
} t_clave_tomada;

coordinador_configuracion get_configuracion();

void salir(int motivo);

void instancia_conectada(un_socket socket_instancia, char* nombre_instancia);

bool health_check(t_instancia * instancia);

t_list * instancias_activas();

int set(char* clave, char* valor);

t_clave_tomada * nueva_clave_tomada(int id_ESI, char* clave);

int ejecutar_get(int id_ESI, char* clave);

int get(char* clave);

int store(char* clave);

int dump(); // Ejecuta un dump en todas las instancias

int dump_instancia(t_instancia * instancia); // Ejecuta un dump en una instancia especificada

t_instancia * instancia_a_guardar(); // Devuelve la instancia en la que se ejecutara un SET de acuerdo al algoritmo correspondiente

t_instancia * get_instancia_con_clave(char * clave); // Devuelve la instancia que contiene una clave especificada

t_instancia * crear_instancia(un_socket socket, char* nombre);

int enviar_informacion_tabla_entradas(t_instancia * instancia); // Envia la informacion de la tabla de entradas a la instancia

void mensaje_instancia_conectada(char* nombre_instancia, int estado); // 0: Instancia nueva, 1: Instancia reconectandose

void * equitative_load();

void * crear_instancias_prueba_alan();

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

#endif /* COORDINADOR_H_ */
