#include <stdio.h>
#include <stdlib.h>
#include "Coordinador.h"

void* archivo;
t_log* logger;

int main(void) {
	char* fileLog;
	fileLog = "coordinador_logs.txt";

	logger = log_create(fileLog, "Coordinador Logs", 1, 1);
	log_info(logger, "Inicializando proceso Coordinador. \n");

	esEstadoInvalido = true;
	lista_instancias = list_create();

	coordinador_configuracion configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

/*
--------------------------------------------------------
----------------- Implementacion Select ----------------
--------------------------------------------------------
*/

	FD_ZERO(&master);    // clear the master and temp sets
	FD_ZERO(&read_fds);

	// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, configuracion.PUERTO_ESCUCHA, &hints, &ai)) != 0) {
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		salir(1);
	}

	for(p = ai; p != NULL; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) {
			continue;
		}
		// lose the pesky "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}
		break;
	}
	// if we got here, it means we didn't get bound
	if (p == NULL) {
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}
	freeaddrinfo(ai); // all done with this
	// listen
	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(3);
	}
	// add the listener to the master set
	FD_SET(listener, &master);
	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one

/*
--------------------------------------------------------
--------------------- Conexiones -----------------------
--------------------------------------------------------
*/

	while(1){
		read_fds = master;
		select(fdmax+1, &read_fds, NULL, NULL, NULL);
		int socketActual;
		for(socketActual = 0; socketActual <= fdmax; socketActual++) {
			if (FD_ISSET(socketActual, &read_fds)) {
				if (socketActual == listener) { //es una conexion nueva
					newfd = aceptar_conexion(socketActual);
					t_paquete* handshake = recibir(socketActual);
					FD_SET(newfd, &master); //Agregar al master SET
					if (newfd > fdmax) {    //Update el Maximo
						fdmax = newfd;
					}
					log_info(logger, "Recibi una nueva conexion. \n");
					free(handshake);
				} else { //No es una nueva conexion -> Recibo el paquete
					t_paquete* paqueteRecibido = recibir(socketActual);
					switch(paqueteRecibido->codigo_operacion){
						case cop_handshake_ESI_Coordinador:
							if( !esEstadoInvalido ){ // Hay un planificador conectado

								esperar_handshake(socketActual,paqueteRecibido,cop_handshake_ESI_Coordinador);
								log_info(logger, "Realice handshake con ESI \n");
								paqueteRecibido = recibir(socketActual); // Info sobre el ESI

								//Todo actualizar estructuras necesarias con datos del ESI
								//Todo abrir hilo para el ESI y pasar por parametro todos los datos necesarios

							}else{ //No hay planificador, se debe rechazar la conexion
								log_info(logger, "Se conecto un ESI estando en estado invalido. \n");
								close(socketActual); //Se cierra el socket
								FD_CLR(socketActual, &master); //se elimina el socket de la lista master
							}
						break;
						case cop_handshake_Planificador_Coordinador:
							esEstadoInvalido = false;
							esperar_handshake(socketActual,paqueteRecibido,cop_handshake_Planificador_Coordinador);
							log_info(logger, "Realice handshake con Planificador \n");
							paqueteRecibido = recibir(socketActual); // Info sobre el Planificador

							//Todo handle informacion que se requiera intercambiar y actualizar estructuras

						break;
						case cop_handshake_Instancia_Coordinador:
							esperar_handshake(socketActual,paqueteRecibido,cop_handshake_Instancia_Coordinador);
							paqueteRecibido = recibir(socketActual); // Info sobre la Instancia
							instancia_conectada(socketActual, paqueteRecibido->data);

						break;
					}
					free(paqueteRecibido);
				}
			}
		}
	}
	return EXIT_SUCCESS;
}

coordinador_configuracion get_configuracion() {
	printf("Levantando archivo de configuracion del proceso Coordinador\n");
	coordinador_configuracion configuracion;
	t_config* archivo_configuracion = config_create(pathCoordinadorConfig);
	configuracion.PUERTO_ESCUCHA = get_campo_config_string(archivo_configuracion, "PUERTO_ESCUCHA");
	configuracion.ALGORITMO_DISTRIBUCION = get_campo_config_string(archivo_configuracion, "ALGORITMO_DISTRIBUCION");
	configuracion.CANTIDAD_ENTRADAS = get_campo_config_int(archivo_configuracion, "CANTIDAD_ENTRADAS");
	configuracion.TAMANIO_ENTRADA = get_campo_config_int(archivo_configuracion, "TAMANIO_ENTRADA");
	configuracion.RETARDO = get_campo_config_int(archivo_configuracion, "RETARDO");
	return configuracion;
}

void salir(int motivo){
	//Todo mejorar funcion de salida
	exit(motivo);
}

void instancia_conectada(un_socket socket_instancia, char* nombre_instancia) {
	char *mensaje = string_new();
	string_append(&mensaje, "Instancia conectada: ");
	string_append(&mensaje, nombre_instancia);
	string_append(&mensaje, " \n");
	log_info(logger, mensaje);

	instancia * instancia_conectada = malloc(sizeof(instancia));
	instancia_conectada->socket = socket_instancia;
	instancia_conectada->estado = conectada;
	list_add(lista_instancias, instancia_conectada);
}

