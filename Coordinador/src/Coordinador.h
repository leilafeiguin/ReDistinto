#ifndef COORDINADOR_H_
#define COORDINADOR_H_

#include <Libraries.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/string.h>

bool esEstadoInvalido;

enum estados_instancia {
	desconectada = 0,
	conectada = 1
};

typedef struct coordinador_configuracion {
	char* PUERTO_ESCUCHA;
	char* ALGORITMO_DISTRIBUCION;
	int CANTIDAD_ENTRADAS;
	int TAMANIO_ENTRADA;
	int RETARDO;

} coordinador_configuracion;

char* pathCoordinadorConfig = "/home/utnso/workspace/tp-2018-1c-PuntoZip/Coordinador/configCoordinador.cfg";

t_list* lista_instancias;

coordinador_configuracion get_configuracion();

void salir(int motivo);

void instancia_conectada(un_socket socket_instancia, char* nombre_instancia);

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
