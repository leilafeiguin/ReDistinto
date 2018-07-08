#include <stdio.h>
#include <stdlib.h>
#include "Planificador.h"

int main(void) {
	imprimir("/home/utnso/workspace/tp-2018-1c-PuntoZip/Planificador/planif_image.txt");
	char* fileLog;
	fileLog = "planificador_logs.txt";

	logger = log_create(fileLog, "Planificador Logs", 1, 1);
	log_info(logger, "Inicializando proceso Planificador. \n");

	configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

	lista_de_ESIs = list_create();
	cola_de_listos = list_create();
	cola_de_bloqueados = list_create();
	cola_de_finalizados = list_create();

	// Inicializo los semaforos
	pthread_mutex_init(&mutex_lista_de_ESIs, NULL);
	pthread_mutex_init(&mutex_cola_de_listos, NULL);
	pthread_mutex_init(&mutex_cola_de_bloqueados, NULL);
	pthread_mutex_init(&mutex_cola_de_finalizados, NULL);
	pthread_mutex_init(&mutex_Coordinador, NULL);

	sem_init(&sem_planificar, 0, 1);
	sem_init(&sem_sistema_ejecucion, 0, 1);
	sem_init(&sem_ESIs_listos, 0, 0);

	// Ejecutar consola
	pthread_t hilo_consola;
	pthread_create(&hilo_consola, NULL, ejecutar_consola, NULL);

	// Ejecutar hilo de planificacion
	pthread_t hilo_de_planificacion;
	pthread_create(&hilo_de_planificacion, NULL, planificar, NULL);

	ESI_ejecutando = malloc(sizeof(t_ESI));
	Ultimo_ESI_Ejecutado = malloc(sizeof(t_ESI));

	conectar_con_coordinador();

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
		salir(2);
	}
	freeaddrinfo(ai); // all done with this
	// listen
	if (listen(listener, 10) == -1) {
		perror("listen");
		salir(3);
	}
	// add the listener to the master set
	FD_SET(listener, &master);
	// add the Coordinador to the master set
	// FD_SET(Coordinador, &master);
	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one
	if (Coordinador > fdmax) {    //Update el Maximo
		fdmax = Coordinador;
	}


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
						case cop_handshake_ESI_Planificador:
							ESI_conectado(socketActual, paqueteRecibido);
							break;
						case -1:
							//Hubo una desconexion
							FD_CLR(socketActual, &master); // Elimina del master SET
							t_ESI * ESI = esi_por_socket(socketActual);
							printf("ESI %d desconectado. \n", ESI->id_ESI);
							eliminar_ESI_cola_actual(ESI);
							free(ESI);
						break;
					}
				}
			}
		}
	}
	return EXIT_SUCCESS;
}

planificador_configuracion get_configuracion() {
	printf("Levantando archivo de configuracion del proceso Planificador\n");
	planificador_configuracion configuracion;
	t_config* archivo_configuracion = config_create(pathPlanificadorConfig);
	configuracion.PUERTO_ESCUCHA = copy_string(get_campo_config_string(archivo_configuracion, "PUERTO_ESCUCHA"));
	configuracion.ALGORITMO_PLANIFICACION = copy_string(get_campo_config_string(archivo_configuracion, "ALGORITMO_PLANIFICACION"));
	configuracion.ALFA_PLANIFICACION = get_campo_config_int(archivo_configuracion, "ALFA_PLANIFICACION");
	configuracion.ESTIMACION_INICIAL = get_campo_config_int(archivo_configuracion, "ESTIMACION_INICIAL");
	configuracion.IP_COORDINADOR = copy_string(get_campo_config_string(archivo_configuracion, "IP_COORDINADOR"));
	configuracion.PUERTO_COORDINADOR = copy_string(get_campo_config_string(archivo_configuracion, "PUERTO_COORDINADOR"));
	configuracion.CLAVES_BLOQUEADAS = copy_string(get_campo_config_string(archivo_configuracion, "CLAVES_BLOQUEADAS"));
	config_destroy(archivo_configuracion);
	return configuracion;
}

void salir(int motivo){
	log_info(logger, "Aborando Planificador..");
	list_destroy(lista_de_ESIs);
	list_destroy(cola_de_finalizados);
	list_destroy(cola_de_bloqueados);
	list_destroy(cola_de_listos);
	free(ESI_ejecutando);
	free(configuracion.PUERTO_ESCUCHA);
	free(configuracion.ALGORITMO_PLANIFICACION);
	free(configuracion.IP_COORDINADOR);
	free(configuracion.PUERTO_COORDINADOR);
	free(configuracion.CLAVES_BLOQUEADAS);
	exit(motivo);
}

void * planificar(void* unused){
	while(1) {
		log_info(logger, "Aguardando para planificar... \n");
		sem_wait(&sem_ESIs_listos); // Espero a que haya ESIs
		sem_wait(&sem_planificar); // Espero a a ver si tengo que planificar
		sem_wait(&sem_sistema_ejecucion);
		sem_post(&sem_sistema_ejecucion);
		log_info(logger, "Planificando \n");

		ordenar_cola_listos();
		pthread_mutex_lock(&mutex_cola_de_listos);
		t_ESI* ESI_a_ejecutar = list_get(cola_de_listos,0);
		pthread_mutex_unlock(&mutex_cola_de_listos);
		pasar_ESI_a_ejecutando(ESI_a_ejecutar);
		printf("Ejecutando ESI %d \n", ESI_a_ejecutar->id_ESI);
		enviar(ESI_ejecutando->socket,cop_Planificador_Ejecutar_Sentencia, size_of_string(""),"");

		ESI_ejecutando->duracionRafaga++;
		// actualizarRafaga();
		Ultimo_ESI_Ejecutado = ESI_ejecutando;
	}
}

void* ejecutar_consola(void * unused){
	log_info(logger, "Consola Iniciada. Ingrese una opcion: \n");
	char * linea;
	char* primeraPalabra;
	char* context;
	while (1) {
		linea = readline(">");
		if (!linea || string_equals_ignore_case(linea, "")) {
			continue;
		} else {
			add_history(linea);
			char** parametros=NULL;
			int lineaLength = strlen(linea);
			char *lineaCopia = (char*) calloc(lineaLength + 1,
					sizeof(char));
			strncpy(lineaCopia, linea, lineaLength);
			primeraPalabra = strtok_r(lineaCopia, " ", &context);

			if (strcmp(linea, "pausar") == 0) {
				log_info(logger, "Eligio la opcion Pausar\n");
				ejecutar_pausar();
			}else if (strcmp(linea, "continuar") == 0) {
				log_info(logger, "Eligio la opcion Continuar\n");
				ejecutar_continuar();
			}else if (strcmp(linea, "exit") == 0) {
				salir(3);
			} else if (strcmp(primeraPalabra, "bloquear") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				parametros = validaCantParametrosComando(linea,2);
				if(parametros != NULL){
					ejecutar_bloquear(atoi(parametros[1]), parametros[2]);
				}
			} else if (strcmp(primeraPalabra, "desbloquear") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				parametros = validaCantParametrosComando(linea,1);
				if(parametros != NULL){
					ejecutar_desbloquear(atoi(parametros[1]));
				}
			} else if (strcmp(primeraPalabra, "listar") == 0) {
				log_info(logger, "Eligio la opcion Listar\n");
				parametros = validaCantParametrosComando(linea,1);
				if(parametros != NULL){
					ejecutar_listar(parametros[1]);
				}
			} else if (strcmp(primeraPalabra, "kill") == 0) {
				log_info(logger, "Eligio la opcion Kill\n");
				parametros = validaCantParametrosComando(linea,1);
				if(parametros != NULL){
					ejecutar_kill(atoi(parametros[1]));
				}
			} else if (strcmp(primeraPalabra, "status") == 0) {
				log_info(logger, "Eligio la opcion Status\n");
				parametros = validaCantParametrosComando(linea,1);
				if(parametros != NULL){
					ejecutar_status(parametros[1]);
				}
			} else if (strcmp(linea, "deadlock") == 0) {
				log_info(logger, "Analizando Deadlocks\n");
				void* buffer = "";
				enviar(Coordinador,cop_Planificador_Deadlock,sizeof(int),buffer);
			} else {
				log_error(logger, "Opcion no valida.\n");
				printf("Opcion no valida.\n");
			}
			free(linea);
			free(lineaCopia);
			if (parametros != NULL)
				free(parametros);
		}
	}
}

bool sistema_pausado() {
	int valor_semaforo;
	sem_getvalue(&sem_sistema_ejecucion, &valor_semaforo);
	return valor_semaforo > 0 ? false : true;
}

void ejecutar_pausar(){
	if (sistema_pausado()) {
		log_info(logger, "El sistema ya estaba pausado. \n");
	} else {
		log_info(logger, "Sistema pausado. \n");
		sem_wait(&sem_sistema_ejecucion);
	}
}

void ejecutar_continuar(){
	if (sistema_pausado()) {
		log_info(logger, "Sistema reanudado. \n");
		sem_post(&sem_sistema_ejecucion);
	} else {
		log_info(logger, "El sistema ya estaba ejecutando. \n");
	}
}

void ejecutar_kill(int id_ESI) {
	t_ESI * ESI = esi_por_id(id_ESI);
	if (ESI == NULL) {
		log_error(logger, "ESI %d no encontrado \n", id_ESI);
	} else {
		kill_ESI(ESI, "Abortado por consola");
	}
}

void ejecutar_status(char* clave) {
	printf("Consultando status de la clave '%s'... \n", clave);
	enviar(Coordinador, cop_Planificador_Consultar_Clave, size_of_string(clave), clave);
}

void ejecutar_bloquear(int id_ESI, char* clave) {
	t_ESI * ESI = esi_por_id(id_ESI);
	if (ESI == NULL) {
		log_error(logger, "ESI %d no encontrado \n", id_ESI);
	} else {
		pasar_ESI_a_bloqueado(ESI, clave, bloqueado_por_consola);
	}
}

void ejecutar_desbloquear(int id_ESI) {
	t_ESI * ESI = esi_por_id(id_ESI);
	if (ESI == NULL) {
		log_error(logger, "ESI %d no encontrado \n", id_ESI);
	} else {
		if (ESI->estado == bloqueado) {
			remover_ESI_bloqueado(ESI);
			pasar_ESI_a_listo(ESI);
		} else {
			log_error(logger, "El ESI %d no esta bloqueado \n", id_ESI);
		}
	}
}

void ejecutar_listar(char* clave){
	mostrar_ESIs_bloqueados(clave, clave_en_uso);
}

char** validaCantParametrosComando(char* comando, int cantParametros) {
	int i = 0;
	char** parametros;
	parametros = str_split(comando, ' ');
	for (i = 1; *(parametros + i); i++) {}
	if (i != cantParametros + 1) {
		log_error(logger, "%s necesita %i parametro/s. \n", comando, cantParametros);
		return NULL;
	} else {
		log_info(logger, "Cantidad de parametros correcta. \n");
		return parametros;
	}
	return NULL;
}

void pasar_ESI_a_bloqueado(t_ESI* ESI, char* clave_de_bloqueo, int motivo){
	printf("ESI %d BLOQUEADO .\n", ESI->id_ESI);
	eliminar_ESI_cola_actual(ESI);
	nuevo_bloqueo(ESI, clave_de_bloqueo, motivo);
	if (ESI->estado == listo) {
		sem_wait(&sem_ESIs_listos);
	}
	ESI->estado = bloqueado;
}

void pasar_ESI_a_finalizado(t_ESI* ESI, char* descripcion_estado){
	printf("ESI %d finalizado, estado: %s \n", ESI->id_ESI, descripcion_estado);

	// Les comunico al ESI y al Coordinador sobre la finalizacion
	int tamanio_buffer = sizeof(int);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_int(buffer, &desplazamiento, ESI->id_ESI);
	sem_trywait(&sem_planificar);
	pthread_mutex_lock(&mutex_Coordinador);
	enviar(Coordinador, cop_ESI_finalizado, tamanio_buffer, buffer);
	pthread_mutex_unlock(&mutex_Coordinador);
	free(buffer);
	enviar(ESI->socket, cop_ESI_finalizado, size_of_string(""), "");
	eliminar_ESI_cola_actual(ESI);

	if (ESI->estado == listo) {
		sem_wait(&sem_ESIs_listos);
	}
	ESI->descripcion_estado = copy_string(descripcion_estado);
	ESI->estado = finalizado;
	pthread_mutex_lock(&mutex_cola_de_finalizados);
	list_add(cola_de_finalizados, ESI);
	pthread_mutex_unlock(&mutex_cola_de_finalizados);
}

void pasar_ESI_a_listo(t_ESI* ESI){
	printf("ESI %d LISTO .\n", ESI->id_ESI);
	ESI->estado = listo;
	pthread_mutex_lock(&mutex_cola_de_listos);
	list_add(cola_de_listos, ESI);
	pthread_mutex_unlock(&mutex_cola_de_listos);
	sem_post(&sem_ESIs_listos);
}

void pasar_ESI_a_ejecutando(t_ESI* ESI){
	eliminar_ESI_cola_actual(ESI);
	ESI->w = 0;
	ESI->estado = ejecutando;
	ESI_ejecutando = ESI;
	aumentar_espera_ESIs_listos();
}

bool validar_ESI_id(int id_ESI){

	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == id_ESI;
	}

	t_ESI* esi = list_find(lista_de_ESIs, encontrar_esi);

	if(esi != NULL){
		return true;
	}
	return false;
}

bool funcion_SJF(void* item_ESI1, void* item_ESI2) {
	t_ESI * ESI1 = (t_ESI *) item_ESI1;
	t_ESI * ESI2 = (t_ESI *) item_ESI2;
	return estimarRafaga(ESI1) < estimarRafaga(ESI2);
}

void ordenar_por_sjf_sd(){
	bool SJF_SD(void* item_ESI1, void* item_ESI2) {
		return funcion_SJF(item_ESI1, item_ESI2) && Ultimo_ESI_Ejecutado->id_ESI != ((t_ESI *) item_ESI2)->id_ESI;
	}
	list_sort(cola_de_listos, SJF_SD);
}

void ordenar_por_sjf_cd(){
	list_sort(cola_de_listos, funcion_SJF);
}

float response_ratio(t_ESI * ESI) {
	return (estimarRafaga(ESI) + ESI->w) / estimarRafaga(ESI);
}

void ordenar_por_hrrn(){
	bool hrrn(void* item_ESI1, void* item_ESI2) {
		t_ESI * ESI1 = (t_ESI *) item_ESI1;
		t_ESI * ESI2 = (t_ESI *) item_ESI2;
		return response_ratio(ESI1) > response_ratio(ESI2);
	}
	list_sort(cola_de_listos, hrrn);
}


float estimarRafaga(t_ESI * ESI){
	int tn = ESI->duracionRafaga; //Duracion de la rafaga anterior
	float Tn = ESI->estimacionUltimaRafaga; // Estimacion anterior
	float estimacion = (configuracion.ALFA_PLANIFICACION / 100)* tn + (1 - (configuracion.ALFA_PLANIFICACION / 100))* Tn;
	ESI->estimacionUltimaRafaga = estimacion;
	return estimacion;
}

void actualizarRafaga() {
	if(Ultimo_ESI_Ejecutado == ESI_ejecutando){
		ESI_ejecutando->duracionRafaga++;
	}else{
		Ultimo_ESI_Ejecutado->duracionRafaga = 0;
		ESI_ejecutando->duracionRafaga = 1;
	}
}

t_ESI* esi_por_id(int id_ESI){
	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == id_ESI;
	}
	pthread_mutex_lock(&mutex_lista_de_ESIs);
	t_ESI* result = list_find(lista_de_ESIs, encontrar_esi);
	pthread_mutex_unlock(&mutex_lista_de_ESIs);
	return result;
}

t_ESI* esi_por_socket(un_socket socket) {
	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->socket == socket;
	}
	pthread_mutex_lock(&mutex_lista_de_ESIs);
	t_ESI* result = list_find(lista_de_ESIs, encontrar_esi);
	pthread_mutex_unlock(&mutex_lista_de_ESIs);
	return result;
}

void liberarESI(){

}

void conectar_con_coordinador() {
	Coordinador = conectar_a(configuracion.IP_COORDINADOR,configuracion.PUERTO_COORDINADOR);
	realizar_handshake(Coordinador, cop_handshake_Planificador_Coordinador);
	enviar(Coordinador,cop_generico,size_of_string(""),"");
	log_info(logger, "Me conecte con el Coordinador. \n");
	pthread_t hilo_coordinador;
	pthread_create(&hilo_coordinador, NULL, escuchar_coordinador, NULL);
	bloquear_claves_iniciales();
}

void bloquear_claves_iniciales() {
	t_list * lista_claves = list_create();
	char** claves = str_split(configuracion.CLAVES_BLOQUEADAS, ',');
	int i = 0;
	while(claves[i] != NULL) {
		list_add(lista_claves, claves[i]);
		i++;
	}
	int tamanio_buffer = size_of_list_of_strings_to_serialize(lista_claves);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_lista_strings(buffer, &desplazamiento, lista_claves);
	pthread_mutex_lock(&mutex_Coordinador);
	enviar(Coordinador, cop_Coordinador_Bloquear_Claves_Iniciales, tamanio_buffer, buffer);
	pthread_mutex_unlock(&mutex_Coordinador);
	free(buffer);
	list_destroy(lista_claves);
}

void * escuchar_coordinador(void * argumentos) {
	log_info(logger, "Escuchando al coordinador... \n");
	bool escuchar = true;
	while(escuchar) {
		t_paquete* paqueteRecibido = recibir(Coordinador); // Recibe el feedback de la instruccion ejecutada por el ESI
		printf("Mensaje recibido del Coordinador, codigo de operacion: %d \n", paqueteRecibido->codigo_operacion);

		int desplazamiento = 0;
		int id_ESI;
		t_ESI * ESI;

		switch(paqueteRecibido->codigo_operacion) {
			case cop_Coordinador_Sentencia_Exito_Clave_Sin_Valor:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				printf("ESI %d: Instruccion ejecutada con exito. Clave sin valor. \n", ESI->id_ESI);
				ESI_ejecutado_exitosamente(ESI);
				sem_post(&sem_planificar);
			break;

			case cop_Coordinador_Sentencia_Exito:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				printf("ESI %d: Instruccion ejecutada con exito. \n", ESI->id_ESI);
				ESI_ejecutado_exitosamente(ESI);
				sem_post(&sem_planificar);
			break;

			case cop_Planificador_Deadlock:
				detectar_deadlock(paqueteRecibido->data);
			break;

			case cop_Coordinador_Sentencia_Fallo_No_Instancias:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				printf("ESI %d: La instruccion fallo. No hay instancias dispobibles. \n", ESI->id_ESI);
				pasar_ESI_a_bloqueado(ESI, "", no_instancias_disponiles);
				sem_post(&sem_planificar);
			break;

			case cop_Coordinador_Sentencia_Fallo_Instancia_No_Disponibe:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				char* nombre_instancia = deserializar_string(paqueteRecibido->data, &desplazamiento);
				printf("ESI %d: La instruccion fallo. La instancia con la clave no se encuentra disponible. \n", ESI->id_ESI);
				pasar_ESI_a_bloqueado(ESI, nombre_instancia, instancia_no_disponible);
				sem_post(&sem_planificar);
			break;

			case cop_Coordinador_Sentencia_Fallo_Clave_Tomada:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				char* nombre_clave = deserializar_string(paqueteRecibido->data, &desplazamiento);
				printf("ESI %d: La instruccion fallo. La clave se encuentra tomada. \n", ESI->id_ESI);
				pasar_ESI_a_bloqueado(ESI, nombre_clave, clave_en_uso);
				sem_post(&sem_planificar);
			break;

			case cop_Coordinador_Sentencia_Fallo_Clave_No_Pedida:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				printf("ESI %d: La instruccion fallo. No solicito el GET para la clave pedida. \n", ESI->id_ESI);
				kill_ESI(ESI, "No solicito el GET para la clave pedida");
				sem_post(&sem_planificar);
			break;

			case cop_Instancia_Nueva:
				 printf("Instancia %s conectada. \n", (char*) paqueteRecibido->data);
				 // Desbloqueo los ESIs que se bloquearon porque no habia instancias disponibles
				 desbloquear_ESIs(no_instancias_disponiles, paqueteRecibido->data);
			break;

			case cop_Instancia_Vieja:
				printf("Instancia %s reconectada. \n", (char*) paqueteRecibido->data);
				// Desbloqueo los ESIs que se bloquearon porque determinada instancia no estaba disponible o no habia instancias
				desbloquear_ESIs(no_instancias_disponiles, paqueteRecibido->data);
				desbloquear_ESIs(instancia_no_disponible, paqueteRecibido->data);
			break;

			case cop_Coordinador_Clave_Liberada:
				// Desbloqueo los ESIs que se bloquearon por esta clave
				desbloquear_ESIs(clave_en_uso, paqueteRecibido->data);
				desbloquear_ESIs(bloqueado_por_consola, paqueteRecibido->data);
			break;

			case cop_Planificador_Consultar_Clave:
				// Recibo el resultado de la consulta realizada por la consola
				mostrar_resultado_consulta(paqueteRecibido->data);
			break;

			case codigo_error:
				log_info(logger, "Error en el Coordinador. Abortando. \n");
				escuchar = false;
			break;

			case cop_Coordinador_Sentencia_Fallo_Clave_Larga:
				enviar(ESI->socket,cop_Planificador_kill_ESI,sizeof(int),paqueteRecibido->data);
				//Matar al ESI
			break;

			case cop_Coordinador_Claves_ESI_finalizado_Liberadas: ;
				t_list * lista_claves_liberadas = deserializar_lista_strings(paqueteRecibido->data, &desplazamiento);
				void desbloquear_ESIs_clave(void* item_clave){
					char* clave = (char*) item_clave;
					desbloquear_ESIs(clave_en_uso, clave);
					desbloquear_ESIs(bloqueado_por_consola, clave);
				}
				list_iterate(lista_claves_liberadas,desbloquear_ESIs_clave);
			break;
		}
	}
	pthread_detach(pthread_self());
	return NULL;
}

void ESI_conectado(un_socket socketESI, t_paquete* paqueteRecibido) {
	esperar_handshake(socketESI,paqueteRecibido,cop_handshake_ESI_Planificador);
	log_info(logger, "Realice handshake con ESI \n");
	paqueteRecibido = recibir(socketESI); // Info sobre el ESI

	// Recibo la cantidad de instrucciones del ESI
	int desp = 0;
	int cantidad_instrucciones = deserializar_int(paqueteRecibido->data, &desp);
	liberar_paquete(paqueteRecibido);

	// Envio al ESI su ID
	int tamanio_buffer = sizeof(int);
	void * buffer = malloc(tamanio_buffer);
	desp = 0;
	t_ESI * ESI_nuevo = nuevo_ESI(socketESI, cantidad_instrucciones); // Genera la estructura y la agrega a la lista
	serializar_int(buffer, &desp, ESI_nuevo->id_ESI);
	enviar(socketESI, cop_handshake_Planificador_ESI, tamanio_buffer, buffer);
	free(buffer);

	pasar_ESI_a_listo(ESI_nuevo);
}

t_ESI * nuevo_ESI(un_socket socket, int cantidad_instrucciones) {
	//Todo actualizar estructuras necesarias con datos del ESI
	t_ESI* newESI = malloc(sizeof(t_ESI));
	newESI->estimacionUltimaRafaga = configuracion.ESTIMACION_INICIAL;
	newESI->estado = 0;
	newESI->socket = socket;
	newESI->id_ESI = idESI;
	newESI->cantidad_instrucciones = cantidad_instrucciones;
	newESI->duracionRafaga = 0;
	pthread_mutex_lock(&mutex_lista_de_ESIs);
	list_add(lista_de_ESIs, newESI);
	pthread_mutex_unlock(&mutex_lista_de_ESIs);

	idESI++;
	return newESI;
}

void ordenar_cola_listos() {
	//Ordenamos la cola de listos segun el algoritmo.
	if( strcmp(configuracion.ALGORITMO_PLANIFICACION,"SJF-SD") == 0 ){
		ordenar_por_sjf_sd();
	}else if( strcmp(configuracion.ALGORITMO_PLANIFICACION,"SJF-CD") == 0 ){
		ordenar_por_sjf_cd();
	}else if( strcmp(configuracion.ALGORITMO_PLANIFICACION,"HRRN") == 0 ){
		ordenar_por_hrrn();
	}
}

void aumentar_espera_ESIs_listos() {
	void aumentarW(void* elem){
		((t_ESI*)elem)->w ++;
	}
	pthread_mutex_lock(&mutex_cola_de_listos);
	list_iterate(cola_de_listos, aumentarW);
	pthread_mutex_unlock(&mutex_cola_de_listos);
}

void remover_ESI_listo(t_ESI* ESI) {
	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == ESI->id_ESI;
	}
	pthread_mutex_lock(&mutex_cola_de_listos);
	list_remove_by_condition(cola_de_listos, encontrar_esi);
	pthread_mutex_unlock(&mutex_cola_de_listos);
}

void remover_ESI_bloqueado(t_ESI* ESI) {
	bool encontrar_esi_bloqueado(void* esi){
		return ((t_bloqueado*)esi)->ESI->id_ESI == ESI->id_ESI;
	}
	pthread_mutex_lock(&mutex_cola_de_bloqueados);
	list_remove_by_condition(cola_de_bloqueados, encontrar_esi_bloqueado);
	pthread_mutex_unlock(&mutex_cola_de_bloqueados);
}

void ESI_ejecutado_exitosamente(t_ESI * ESI) {
	ESI->cantidad_instrucciones --;
	if(ESI->cantidad_instrucciones == 0){
		pasar_ESI_a_finalizado(ESI, "Finalizo correctamente");
	}else{
		pasar_ESI_a_listo(ESI);
	}
}

void nuevo_bloqueo(t_ESI* ESI, char* clave, int motivo) { // Crea la estructura y la agrega a la lista
	t_bloqueado* esi_bloqueado = malloc(sizeof(t_bloqueado));
	esi_bloqueado->ESI = ESI;
	esi_bloqueado->clave_de_bloqueo = copy_string(clave);
	esi_bloqueado->motivo = motivo;
	pthread_mutex_lock(&mutex_cola_de_bloqueados);
	list_add(cola_de_bloqueados,esi_bloqueado);
	pthread_mutex_unlock(&mutex_cola_de_bloqueados);
}


// Desbloquea los ESIs bloqueados por un determinado motivo
void desbloquear_ESIs(int motivo, char* parametro) {
	bool ESI_bloqueado_con_motivo(void * blocked){
		t_bloqueado* bloqueo = (t_bloqueado*) blocked;
		if (bloqueo->motivo == motivo && (bloqueo->motivo != instancia_no_disponible || strcmp(parametro, bloqueo->clave_de_bloqueo) == 0)) {
			printf("ESI %d desbloqueado. \n", bloqueo->ESI->id_ESI);
			pasar_ESI_a_listo(bloqueo->ESI);
			return true;
		}
		return false;
	}
	pthread_mutex_lock(&mutex_cola_de_bloqueados);
	list_remove_by_condition(cola_de_bloqueados, ESI_bloqueado_con_motivo);
	pthread_mutex_unlock(&mutex_cola_de_bloqueados);
}

void kill_ESI(t_ESI * ESI, char* motivo) {
	eliminar_ESI_cola_actual(ESI);
	printf("ESI %d abortado. Motivo: %s. \n", ESI->id_ESI, motivo);
	enviar(ESI->socket, cop_Planificador_kill_ESI, size_of_string(motivo),motivo);
}

void eliminar_ESI_cola_actual(t_ESI * ESI) {
	// Lo saco de la cola actual en la que se encuentra
	if (ESI->estado == listo) {
		remover_ESI_listo(ESI);
	} else {
		remover_ESI_bloqueado(ESI);
	}
}

t_list * get_ESIs_bloqueados_por_clave(char* clave, int motivo) {
	bool ESI_bloqueados_por_clave(void* item_bloqueo){
		t_bloqueado* bloqueo = (t_bloqueado*) item_bloqueo;
		return strcmp(bloqueo->clave_de_bloqueo, clave) == 0 && (motivo == -1 || bloqueo->motivo == motivo);
	}
	return list_filter(cola_de_bloqueados, ESI_bloqueados_por_clave);
}

void mostrar_resultado_consulta(void * buffer_resultado) {
	int desplazamiento = 0;
	char* clave = deserializar_string(buffer_resultado, &desplazamiento);
	char* valor = deserializar_string(buffer_resultado, &desplazamiento);
	char* nombre_instancia_actual = deserializar_string(buffer_resultado, &desplazamiento);
	char* nombre_instancia_a_guardar = deserializar_string(buffer_resultado, &desplazamiento);

	printf("\nStatus de la clave '%s': \n", valor);
	printf("Valor: %s \n", valor);
	printf("Instancia actual: %s \n", nombre_instancia_actual);
	printf("Proxima instancia a guardar: %s \n", nombre_instancia_a_guardar);
	mostrar_ESIs_bloqueados(clave, -1);

	free(clave);
	free(valor);
	free(nombre_instancia_actual);
	free(nombre_instancia_a_guardar);
}

void mostrar_ESIs_bloqueados(char* clave, int motivo) {
	t_list * ESIs_bloqueados = get_ESIs_bloqueados_por_clave(clave, motivo);
	if (list_size(ESIs_bloqueados)) {
		puts("ESIs bloqueados:");
		void mostrar_ESI_bloqueado(void* item_bloqueo){
			t_bloqueado* bloqueo = (t_bloqueado*) item_bloqueo;
			char* descripcion_motivo = bloqueo->motivo == bloqueado_por_consola ? "(por consola)" : "";
			printf("- ESI %d %s \n", bloqueo->ESI->id_ESI, descripcion_motivo);
		}
		list_iterate(ESIs_bloqueados, mostrar_ESI_bloqueado);
	} else {
		puts("No hay ESIs bloqueados.\n");
	}
	list_destroy(ESIs_bloqueados);
}

void detectar_deadlock(void* datos_coordinador){
	t_list* claves_ESI = list_create();

	//Deserealizo el paquete
	int desplazamiento = 0;
	int tam_lista;
	memcpy(&tam_lista,datos_coordinador,sizeof(int));
	desplazamiento += sizeof(int);

	for(int i = 0; i < tam_lista; i++){
		t_claves_por_esi* clave_por_esi = malloc(sizeof(t_claves_por_esi));
		memcpy(&clave_por_esi->id_ESI,desplazamiento + datos_coordinador,sizeof(int));
		desplazamiento += sizeof(int);

		int tam_clave;
		memcpy(&tam_clave,desplazamiento + datos_coordinador,sizeof(int));
		desplazamiento += sizeof(int);

		clave_por_esi->clave_tomada = malloc(tam_clave);
		memcpy(clave_por_esi->clave_tomada,desplazamiento + datos_coordinador,tam_clave);
		desplazamiento += tam_clave;

		bool encontrar_esi_bloqueado_por_id(void* esi_bloquado){
			return ((t_bloqueado*) esi_bloquado)->ESI->id_ESI == clave_por_esi->id_ESI ;
		}

		t_bloqueado* esi_bloqueado = list_find(cola_de_bloqueados,encontrar_esi_bloqueado_por_id);
		char* clave_de_bloqueo; //Todo revisar
		if(esi_bloqueado != NULL){
			strcpy(clave_de_bloqueo,esi_bloqueado->clave_de_bloqueo);
			clave_por_esi->clave_de_bloqueo = malloc(strlen(clave_de_bloqueo));
			strcpy(clave_por_esi->clave_de_bloqueo,clave_de_bloqueo);
		}else{
			clave_por_esi->clave_de_bloqueo = malloc(strlen(""));
			strcpy(clave_por_esi->clave_de_bloqueo,"");
		}

		list_add(claves_ESI,clave_por_esi);
	}

	//Lista preparada para analizar

	t_list* ESIs_en_deadlock = list_create();
	void encontrar_deadlock(void* elem){
		t_claves_por_esi* clave_por_esi = (t_claves_por_esi*) elem;
		if(strcmp(clave_por_esi->clave_de_bloqueo, "") != 0){

			bool encontrar_esi_que_tomo_clave(void* elem1){
				t_claves_por_esi* clave_por_esi1 = (t_claves_por_esi*) elem1;
				if(clave_por_esi1->id_ESI != clave_por_esi->id_ESI){
					if(strcmp(clave_por_esi->clave_de_bloqueo,clave_por_esi1->clave_tomada) == 0){
						return true;
					}
				}
				return false;
			}
			t_claves_por_esi* cadena1 = list_find(claves_ESI, encontrar_esi_que_tomo_clave);
			if(cadena1 != NULL){
				if(strcmp(cadena1->clave_de_bloqueo,clave_por_esi->clave_tomada) == 0){
					list_add(ESIs_en_deadlock,cadena1);
					list_add(ESIs_en_deadlock,clave_por_esi);
				}else{
					encontrar_deadlock(cadena1);
				}
			}
		}
	}
	list_iterate(claves_ESI,encontrar_deadlock);

	if(list_size(ESIs_en_deadlock) > 0){
		log_info(logger, "Se encuentran en deadlock los ESIs:\n");
		void imprimir_IDs(void* elem){
			log_info(logger, "%i\n", ((t_claves_por_esi*) elem)->id_ESI);
		}
		list_iterate(ESIs_en_deadlock,imprimir_IDs);
	}else{
		log_info(logger, "No hay ESIs en Deadlock:\n");
	}

	//limpieza
	list_destroy(claves_ESI);
	list_destroy(ESIs_en_deadlock);
	return;
}

void funcion_exit(int sig) {
	puts("Abortando Planificador..");

	(void) signal(SIGINT, SIG_DFL);
}

