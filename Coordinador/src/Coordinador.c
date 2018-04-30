#include <stdio.h>
#include <stdlib.h>
#include "Coordinador.h"

void* archivo;
t_log* logger;

int main(void) {
	imprimir("/home/utnso/workspace/tp-2018-1c-PuntoZip/Coordinador/coord_image.txt");
	char* fileLog;
	fileLog = "coordinador_logs.txt";

	logger = log_create(fileLog, "Coordinador Logs", 1, 1);
	log_info(logger, "Inicializando proceso Coordinador. \n");

	esEstadoInvalido = true;
	lista_instancias = list_create();
	lista_cant_entradas_x_instancia = list_create();

	configuracion = get_configuracion();
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
	puts("Saliendo wachooo");
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

t_list * instancias_activas() {
	bool instancia_activa(t_instancia * i) {
		 if (i->estado == conectada) {
			 return true;
		 }
		return false;
	}
	return list_filter(lista_instancias, instancia_activa);
}

void instancia_conectada(un_socket socket_instancia, char* nombre_instancia) {
	bool instancia_ya_existente(t_instancia * ins){
		if (strcmp(ins->nombre, nombre_instancia) == 0) {
			return true;
		}
		return false;
	}
	t_instancia * instancia = list_find(lista_instancias, instancia_ya_existente);
	int codigo_respuesta = cop_Instancia_Nueva;
	if (instancia == NULL) { // Si es una instancia nueva
		mensaje_instancia_conectada(nombre_instancia, 0);
		instancia = crear_instancia(socket_instancia, nombre_instancia);
	} else { // Si es una instancia ya creada reconectandose
		codigo_respuesta = cop_Instancia_Vieja;
		mensaje_instancia_conectada(nombre_instancia, 1);
		instancia->socket = socket_instancia;
		instancia->estado = conectada;
	}
	enviar(instancia->socket, codigo_respuesta, sizeof(int), "0");
	enviar_informacion_tabla_entradas(instancia);

	// BORRAR PROXIMAMENTE: Para probar las funciones
	set("nombre", "tomas uriel chejanovich");
	get("nombre");
	store("nombre");
	dump();
}

int set(char* clave, char* valor) {
	t_instancia * instancia = instancia_a_guardar();
	if (health_check(instancia)) {
		list_add(instancia->keys_contenidas, clave); // Registro que esta instancia contendra la clave especificada
		enviar(instancia->socket, cop_Instancia_Ejecutar_Set, size_of_string(clave), clave); // Envio la clave en la que se guardara
		enviar(instancia->socket, cop_Instancia_Ejecutar_Set, size_of_string(valor), valor); // Envio el valor a guardar
		printf("SET %s '%s' \n", clave, valor);
		return 1;
	}
	printf("ERROR: No pudo ejecutarse el SET. %s no disponible. \n", instancia->nombre);
	return 0;
}

int get(char* clave) {
	t_instancia * instancia = get_instancia_con_clave(clave);
	if (health_check(instancia)) {
		enviar(instancia->socket, cop_Instancia_Ejecutar_Get, size_of_string(clave), clave); // Envia a la instancia la clave
		t_paquete* paqueteValor = recibir(instancia->socket); // Recibe el valor solicitado
		printf("GET %s: '%s' \n", clave, paqueteValor->data);
		return 1;
	}
	printf("ERROR: No pudo ejecutarse el GET. %s no disponible. \n", instancia->nombre);
	return 0;
}

int store(char* clave) {
	t_instancia * instancia = get_instancia_con_clave(clave);
	if (health_check(instancia)) {
		enviar(instancia->socket, cop_Instancia_Ejecutar_Store, size_of_string(clave), clave); // Envia a la instancia la clave
		t_paquete* paqueteEstadoOperacion = recibir(instancia->socket); // Aguarda a que la instancia le comunique que el STORE se ejectuo de forma exitosa
		if (paqueteEstadoOperacion->codigo_operacion == cop_Instancia_Ejecucion_Exito) {
			printf("Clave '%s' liberada \n", clave);
		}
		return 1;
	}
	printf("ERROR: No pudo ejecutarse el STORE. %s no disponible. \n", instancia->nombre);
	return 0;
}

int dump() {
	list_iterate(instancias_activas(), dump_instancia);
}

int dump_instancia(t_instancia * instancia) {
	if (health_check(instancia)) {
		enviar(instancia->socket, cop_Instancia_Ejecutar_Dump, size_of_string(""), "");
		return 1;
	}
	printf("ERROR: No pudo ejecutarse el DUMP. %s no disponible. \n", instancia->nombre);
	return 0;
}

bool health_check(t_instancia * instancia) {
	if (instancia->estado == desconectada) {
		return false;
	}
	enviar(instancia->socket, codigo_healthcheck, size_of_string(""), ""); // Envio el request de healthcheck
	t_paquete* paqueteRecibido = recibir(instancia->socket); // Aguardo a recbir el OK de la instancia
	if (paqueteRecibido->codigo_operacion == codigo_healthcheck) {
		return true;
	}
	instancia->estado = desconectada;
	printf("%s no disponible \n", instancia->nombre);
	return false;
}

t_instancia * get_instancia_con_clave(char * clave) {
	bool instancia_tiene_clave(t_instancia * instancia){
		bool clave_match(char * clave_comparar){
			if (strcmp(clave, clave_comparar) == 0) {
				return true;
			}
			return false;
		}
		if (list_find(instancia->keys_contenidas, clave_match) != NULL) {
			return true;
		}
		return false;
	}
	return list_find(lista_instancias, instancia_tiene_clave);
}

t_instancia * instancia_a_guardar() {
	return list_get(lista_instancias, 0); // TODO: Utilizar algoritmo correspondiente
}

t_instancia * crear_instancia(un_socket socket, char* nombre) {
	// Creo la estructura de la instancia y la agrego a la lista
	t_instancia * instancia_nueva = malloc(sizeof(t_instancia));
	instancia_nueva->socket = socket;
	instancia_nueva->nombre = nombre;
	instancia_nueva->estado = conectada;
	instancia_nueva->cant_entradas_ocupadas = 0;
	instancia_nueva->keys_contenidas = list_create();
	list_add(lista_instancias, instancia_nueva);
	return instancia_nueva;
}

int enviar_informacion_tabla_entradas(t_instancia * instancia) {
	// Envio la cantidad de entradas que va a tener esa instancia
	char* cant_entradas = string_itoa(configuracion.CANTIDAD_ENTRADAS);
	enviar(instancia->socket, cop_generico, size_of_string(cant_entradas), cant_entradas);

	// Envio el tamaño que va a tener cada entrada
	char* tamanio_entrada = string_itoa(configuracion.TAMANIO_ENTRADA);
	enviar(instancia->socket, cop_generico, size_of_string(tamanio_entrada), tamanio_entrada);
}

void mensaje_instancia_conectada(char* nombre_instancia, int estado) { // 0: Instancia nueva, 1: Instancia reconectandose
	char* mensaje = string_new();
	if (estado == 0) {
		string_append(&mensaje, "Instancia conectada: ");
	} else {
		string_append(&mensaje, "Instancia reconectada: ");
	}
	string_append(&mensaje, nombre_instancia);
	string_append(&mensaje, " \n");
	log_info(logger, mensaje);
}


void * equitative_load() {
	return list_find(lista_instancias, cantidad_entradas_x_instancia);
}

int cantidad_entradas_x_instancia(t_instancia * i) {
	return i->cant_entradas_ocupadas;
}

