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

	// Inicializo los semaforos
	pthread_mutex_init(&mutex_lista_de_ESIs, NULL);
	pthread_mutex_init(&mutex_cola_de_listos, NULL);
	pthread_mutex_init(&mutex_cola_de_bloqueados, NULL);
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
							char str[12];
							sprintf(str, "%d", ESI->id_ESI);
							log_and_free(logger, string_concat(3, "ESI ", str, " desconectado. \n"));
							validar_si_procesador_liberado(ESI);
							eliminar_ESI_cola_actual(ESI);
						break;
					}
				}
			}
		}
	}
	return EXIT_SUCCESS;
}

planificador_configuracion get_configuracion() {
	log_info(logger, "Levantando archivo de configuracion del proceso Planificador \n");
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
	log_info(logger, "Abortando Planificador..");
	list_destroy_and_destroy_elements(lista_de_ESIs, free);
	list_destroy_and_destroy_elements(cola_de_bloqueados, free_t_bloqueado);
	list_destroy(cola_de_listos);
	free(configuracion.PUERTO_ESCUCHA);
	free(configuracion.ALGORITMO_PLANIFICACION);
	free(configuracion.IP_COORDINADOR);
	free(configuracion.PUERTO_COORDINADOR);
	free(configuracion.CLAVES_BLOQUEADAS);
	exit(motivo);
}

void * planificar(void* unused){
	while(1) {
		sem_wait(&sem_planificar); // Espero a a ver si tengo que planificar
		t_ESI* ESI_a_ejecutar = get_ESI_a_ejecutar();
		sem_wait(&sem_sistema_ejecucion);
		sem_post(&sem_sistema_ejecucion);

		pasar_ESI_a_ejecutando(ESI_a_ejecutar);
		char str[12];
		sprintf(str, "%d", ESI_a_ejecutar->id_ESI);
		log_and_free(logger, string_concat(3, "Ejecutando ESI ", str, " \n"));
		enviar(ESI_a_ejecutar->socket,cop_Planificador_Ejecutar_Sentencia, size_of_string(""),"");

		actualizarRafaga(ESI_a_ejecutar);
		aumentar_espera_ESIs_listos();
		Ultimo_ESI_Ejecutado = ESI_ejecutando;
	}
}

t_ESI * get_ESI_a_ejecutar() {
	if (ESI_ejecutando != NULL) {
		return ESI_ejecutando;
	}
	sem_wait(&sem_ESIs_listos); // Espero a que haya ESIs listos
	t_ESI* ESI_a_ejecutar;
	pthread_mutex_lock(&mutex_cola_de_listos);
	ESI_a_ejecutar = list_get(cola_de_listos,0);
	pthread_mutex_unlock(&mutex_cola_de_listos);
	return ESI_a_ejecutar;
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
			} else if (strcmp(primeraPalabra, "desbloquear") == 0) {
				log_info(logger, "Eligio la opcion Desloquear\n");
				parametros = validaCantParametrosComando(linea,1);
				if(parametros != NULL){
					ejecutar_desbloquear(parametros[1]);
				}
			} else if (strcmp(primeraPalabra, "bloquear") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				parametros = validaCantParametrosComando(linea,2);
				if(parametros != NULL){
					ejecutar_bloquear(atoi(parametros[1]), parametros[2]);
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
				enviar_mensaje_coordinador(cop_Planificador_Analizar_Deadlocks, size_of_string(""), "");
			} else {
				log_error(logger, "Opcion no valida.\n");
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
	log_and_free(logger, string_concat(3, "Consultando status de la clave '", clave, "'... \n"));
	enviar_mensaje_coordinador(cop_Planificador_Consultar_Clave, size_of_string(clave), clave);
}

void ejecutar_bloquear(int id_ESI, char* clave) {
	t_ESI * ESI = esi_por_id(id_ESI);
	if (ESI == NULL) {
		log_error(logger, "ESI %d no encontrado \n", id_ESI);
	} else {
		pasar_ESI_a_bloqueado(ESI, clave, bloqueado_por_consola);
	}
}

void ejecutar_desbloquear(char* clave) {
	log_info(logger,string_concat(3, "desbloquear ", clave," \n"));
	enviar_mensaje_coordinador(cop_Coordinador_Liberar_Clave, size_of_string(clave), clave);
	desbloquear_ESIs(clave_en_uso, clave);
	desbloquear_ESIs(bloqueado_por_consola, clave);
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

void validar_si_procesador_liberado(t_ESI * ESI) {
	if (ESI == ESI_ejecutando) {
		ESI_ejecutando = NULL;
	}
}

void pasar_ESI_a_bloqueado(t_ESI* ESI, char* clave_de_bloqueo, int motivo){
	validar_si_procesador_liberado(ESI);
	char str[12];
	sprintf(str, "%d", ESI->id_ESI);
	log_and_free(logger, string_concat(3, "ESI ", str," BLOQUEADO .\n"));
	eliminar_ESI_cola_actual(ESI);
	nuevo_bloqueo(ESI, clave_de_bloqueo, motivo);
	if (ESI->estado == listo) {
		sem_wait(&sem_ESIs_listos);
	}
	ESI->estado = bloqueado;
}

void pasar_ESI_a_finalizado(t_ESI* ESI, char* descripcion_estado){
	validar_si_procesador_liberado(ESI);
	char str[12];
	sprintf(str, "%d", ESI->id_ESI);
	log_and_free(logger, string_concat(5, "ESI ", str, " finalizado, estado: ", descripcion_estado, " \n"));

	// Les comunico al ESI y al Coordinador sobre la finalizacion
	int tamanio_buffer = sizeof(int);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_int(buffer, &desplazamiento, ESI->id_ESI);
	sem_trywait(&sem_planificar);
	enviar_mensaje_coordinador(cop_ESI_finalizado, tamanio_buffer, buffer);
	free(buffer);
	enviar(ESI->socket, cop_ESI_finalizado, size_of_string(""), "");
	eliminar_ESI_cola_actual(ESI);

	if (ESI->estado == listo) {
		sem_wait(&sem_ESIs_listos);
	}
	ESI->descripcion_estado = copy_string(descripcion_estado);
	ESI->estado = finalizado;
}

void enviar_mensaje_coordinador(int cop, int tamanio_buffer, void * buffer) {
	pthread_mutex_lock(&mutex_Coordinador);
	enviar(Coordinador, cop, tamanio_buffer, buffer);
	pthread_mutex_unlock(&mutex_Coordinador);
}

void pasar_ESI_a_listo(t_ESI* ESI) {
	validar_si_procesador_liberado(ESI);
	agregar_ESI_a_cola_listos(ESI);
	validar_desalojo();
	ordenar_cola_listos();
}

void agregar_ESI_a_cola_listos(t_ESI* ESI) {
	pthread_mutex_lock(&mutex_cola_de_listos);
	list_add(cola_de_listos, ESI);
	pthread_mutex_unlock(&mutex_cola_de_listos);
	ESI->estado = listo;
	char str[12];
	sprintf(str, "%d", ESI->id_ESI);
	log_and_free(logger, string_concat(3, "ESI ", str, " LISTO .\n"));
	sem_post(&sem_ESIs_listos);
}

void validar_desalojo() {
	if (ESI_ejecutando != NULL && strings_equal(configuracion.ALGORITMO_PLANIFICACION,"SJF-CD")) {
		t_ESI * ESI_desalojado = ESI_ejecutando;
		ESI_ejecutando = NULL;

		ESI_desalojado->estimacionUltimaRafaga -= ESI_desalojado->duracionRafaga; // Calculo el remamente
		ESI_desalojado->ejecutado_desde_estimacion = false; // Seteo en false para que no vuelva a estimarse

		// Agrego a listos el ESI desalojado
		agregar_ESI_a_cola_listos(ESI_desalojado);
	}
}

void pasar_ESI_a_ejecutando(t_ESI* ESI){
	eliminar_ESI_cola_actual(ESI);
	ESI_ejecutando = ESI;
	ESI->w = 0;
	ESI->estado = ejecutando;
	ESI->ejecutado_desde_estimacion = true;
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
	if (ESI1->estimacionUltimaRafaga == ESI2->estimacionUltimaRafaga) {
		return funcion_FIFO(ESI1, ESI2);
	}
	return ESI1->estimacionUltimaRafaga < ESI2->estimacionUltimaRafaga;
}

bool funcion_FIFO(t_ESI * ESI1, t_ESI * ESI2) {
	return ESI1->id_ESI < ESI2->id_ESI;
}

/* void ordenar_por_sjf_sd(){
	bool SJF_SD(void* item_ESI1, void* item_ESI2) {
		return funcion_SJF(item_ESI1, item_ESI2) && Ultimo_ESI_Ejecutado->id_ESI != ((t_ESI *) item_ESI2)->id_ESI;
	}
	list_sort(cola_de_listos, SJF_SD);
} */

void ordenar_por_sjf(){
	list_sort(cola_de_listos, funcion_SJF);
}

float response_ratio(t_ESI * ESI) {
	return (ESI->estimacionUltimaRafaga + ESI->w) / ESI->estimacionUltimaRafaga;
}

void ordenar_por_hrrn(){
	bool hrrn(void* item_ESI1, void* item_ESI2) {
		t_ESI * ESI1 = (t_ESI *) item_ESI1;
		t_ESI * ESI2 = (t_ESI *) item_ESI2;
		float response_ratio1 = response_ratio(ESI1);
		float response_ratio2 = response_ratio(ESI2);
		if (response_ratio1 == response_ratio2) {
			return funcion_FIFO(ESI1, ESI2);
		}
		return response_ratio1 > response_ratio2;
	}
	list_sort(cola_de_listos, hrrn);
}


void estimarRafaga(t_ESI * ESI){
	int tn = ESI->duracionRafaga; //Duracion de la rafaga anterior
	float Tn = ESI->estimacionUltimaRafaga; // Estimacion anterior
	float porcentaje_alfa = ((float) configuracion.ALFA_PLANIFICACION) / 100;
	float estimacion = porcentaje_alfa * tn + (1 - porcentaje_alfa) * Tn;
	ESI->estimacionUltimaRafaga = estimacion;
}

void estimar_ESIs_listos() {
	void estimar(void * item_ESI) {
		t_ESI * ESI = (t_ESI *) item_ESI;
		if (ESI->ejecutado_desde_estimacion) {
			ESI->ejecutado_desde_estimacion = false;
			estimarRafaga(ESI);
		}
	}
	pthread_mutex_lock(&mutex_cola_de_listos);
	list_iterate(cola_de_listos, estimar);
	pthread_mutex_unlock(&mutex_cola_de_listos);
}

void actualizarRafaga(t_ESI * ESI) {
	if(ESI == Ultimo_ESI_Ejecutado){
		ESI->duracionRafaga++;
	}else{
		ESI->duracionRafaga = 1;
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
	if (!strings_equal("-", configuracion.CLAVES_BLOQUEADAS)) {
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
		enviar_mensaje_coordinador(cop_Coordinador_Bloquear_Claves_Iniciales, tamanio_buffer, buffer);
		free(buffer);
		free(claves);
		list_destroy_and_destroy_elements(lista_claves, free);
	}
}

void * escuchar_coordinador(void * argumentos) {
	log_info(logger, "Escuchando al coordinador... \n");
	bool escuchar = true;
	while(escuchar) {
		t_paquete* paqueteRecibido = recibir(Coordinador); // Recibe el feedback de la instruccion ejecutada por el ESI
		int desplazamiento = 0;
		int id_ESI;
		char str[12];
		t_ESI * ESI;
		char* nombre_clave;

		switch(paqueteRecibido->codigo_operacion) {
			case cop_Coordinador_Sentencia_Exito_Clave_Sin_Valor:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				sprintf(str, "%d", id_ESI);
				log_and_free(logger, string_concat(3, "ESI ", str, ": Instruccion ejecutada con exito. Clave sin valor. \n"));
				ESI_ejecutado_exitosamente(ESI);
				sem_post(&sem_planificar);
			break;

			case cop_Coordinador_Sentencia_Exito:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				sprintf(str, "%d", id_ESI);
				log_and_free(logger, string_concat(3, "ESI ", str, ": Instruccion ejecutada con exito. \n"));
				ESI_ejecutado_exitosamente(ESI);
				sem_post(&sem_planificar);
			break;

			case cop_Planificador_Analizar_Deadlocks: ;
				t_list * claves_tomadas = recibir_claves_tomadas(paqueteRecibido->data);
				t_list * claves_pedidas = get_ESIs_bloqueados_por_motivo(clave_en_uso);
				detectar_deadlock(claves_tomadas, claves_pedidas);
				list_destroy_and_destroy_elements(claves_tomadas, free_claves_tomadas);
				list_destroy(claves_pedidas);
			break;

			case cop_Coordinador_Sentencia_Fallo_No_Instancias:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				sprintf(str, "%d", id_ESI);
				log_error_and_free(logger, string_concat(3,"ESI ", str, ": La instruccion fallo. No hay instancias dispobibles. \n"));
				pasar_ESI_a_bloqueado(ESI, "", no_instancias_disponiles);
				sem_post(&sem_planificar);
			break;

			case cop_Coordinador_Sentencia_Fallo_Instancia_No_Disponibe:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				char* nombre_instancia = deserializar_string(paqueteRecibido->data, &desplazamiento);
				sprintf(str, "%d", id_ESI);
				log_error_and_free(logger, string_concat(3, "ESI ", str, ": La instruccion fallo. La instancia con la clave no se encuentra disponible. \n"));
				pasar_ESI_a_bloqueado(ESI, nombre_instancia, instancia_no_disponible);
				sem_post(&sem_planificar);
			break;

			case cop_Coordinador_Sentencia_Fallo_Clave_Tomada:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				nombre_clave = deserializar_string(paqueteRecibido->data, &desplazamiento);
				sprintf(str, "%d", id_ESI);
				log_error_and_free(logger, string_concat(3, "ESI ", str, ": La instruccion fallo. La clave se encuentra tomada. \n"));
				pasar_ESI_a_bloqueado(ESI, nombre_clave, clave_en_uso);
				free(nombre_clave);
				sem_post(&sem_planificar);
			break;

			case cop_Coordinador_Sentencia_Fallo_Valor_Mayor_Anterior:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				nombre_clave = deserializar_string(paqueteRecibido->data, &desplazamiento);
				sprintf(str, "%d", id_ESI);
				log_error_and_free(logger, string_concat(3, "SET rechazado. El valor de  la clave ", nombre_clave," ocupa mas entradas que el valor anterior.. \n"));
				kill_ESI(ESI, "Solicita clave con valor mayor");
				free(nombre_clave);
				sem_post(&sem_planificar);
			break;

			case cop_Coordinador_Sentencia_Fallo_Clave_No_Pedida:
				id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				ESI = esi_por_id(id_ESI);
				sprintf(str, "%d", id_ESI);
				log_error_and_free(logger, string_concat(3, "ESI ", str, ": La instruccion fallo. No solicito el GET para la clave pedida. \n"));
				kill_ESI(ESI, "No solicito el GET para la clave pedida");
				sem_post(&sem_planificar);
			break;

			case cop_Instancia_Nueva:
				log_and_free(logger, string_concat(3, "Instancia ", (char*) paqueteRecibido->data," conectada. \n"));
				// Desbloqueo los ESIs que se bloquearon porque no habia instancias disponibles
				 desbloquear_ESIs(no_instancias_disponiles, paqueteRecibido->data);
			break;

			case cop_Instancia_Vieja:
				log_and_free(logger, string_concat(3, "Instancia ", (char*) paqueteRecibido->data," reconectada. \n"));
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
		// Posible error
		liberar_paquete(paqueteRecibido);
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
	newESI->ejecutado_desde_estimacion = false;
	newESI->w = 0;
	pthread_mutex_lock(&mutex_lista_de_ESIs);
	list_add(lista_de_ESIs, newESI);
	pthread_mutex_unlock(&mutex_lista_de_ESIs);

	idESI++;
	return newESI;
}

void ordenar_cola_listos() {
	// Aplicamos las estimaciones
	estimar_ESIs_listos();

	//Ordenamos la cola de listos segun el algoritmo.
	if(strings_equal(configuracion.ALGORITMO_PLANIFICACION,"HRRN")){
		ordenar_por_hrrn();
	} else {
		ordenar_por_sjf();
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
		return bloqueo->motivo == motivo && (strings_equal(parametro, bloqueo->clave_de_bloqueo) || motivo == no_instancias_disponiles);
	}
	pthread_mutex_lock(&mutex_cola_de_bloqueados);
	t_list * lista_liberados = list_remove_all_by_condition(cola_de_bloqueados, ESI_bloqueado_con_motivo);
	pthread_mutex_unlock(&mutex_cola_de_bloqueados);

	void pasar_a_listo(void * blocked){
		t_bloqueado* bloqueo = (t_bloqueado*) blocked;
		char str[12];
		sprintf(str, "%d", bloqueo->ESI->id_ESI);
		log_and_free(logger, string_concat(3, "ESI ", str," desbloqueado. \n"));
		pasar_ESI_a_listo(bloqueo->ESI);
	}
	list_iterate(lista_liberados, pasar_a_listo);
	list_destroy_and_destroy_elements(lista_liberados, free);
}

void kill_ESI(t_ESI * ESI, char* motivo) {
	eliminar_ESI_cola_actual(ESI);
	char str[12];
	sprintf(str, "%d", ESI->id_ESI);
	log_and_free(logger, string_concat(5, "ESI ", str, " abortado. Motivo: ", motivo,". \n"));
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

t_list * get_ESIs_bloqueados_por_motivo(int motivo) {
	bool ESI_bloqueados_por_motivo(void* item_bloqueo){
		t_bloqueado* bloqueo = (t_bloqueado*) item_bloqueo;
		return (motivo == -1 || bloqueo->motivo == motivo);
	}
	return list_filter(cola_de_bloqueados, ESI_bloqueados_por_motivo);
}

void mostrar_resultado_consulta(void * buffer_resultado) {
	int desplazamiento = 0;
	char* clave = deserializar_string(buffer_resultado, &desplazamiento);
	char* valor = deserializar_string(buffer_resultado, &desplazamiento);
	char* nombre_instancia_actual = deserializar_string(buffer_resultado, &desplazamiento);
	char* nombre_instancia_a_guardar = deserializar_string(buffer_resultado, &desplazamiento);

	log_and_free(logger, string_concat(3, "\nStatus de la clave '", valor, "': \n"));
	log_and_free(logger, string_concat(3, "Valor: ", valor, " \n"));
	log_and_free(logger, string_concat(3, "Instancia actual: ", nombre_instancia_actual, " \n"));
	log_and_free(logger, string_concat(3, "Proxima instancia a guardar: ", nombre_instancia_a_guardar, " \n"));
	mostrar_ESIs_bloqueados(clave, -1);

	free(clave);
	free(valor);
	free(nombre_instancia_actual);
	free(nombre_instancia_a_guardar);
}

void mostrar_ESIs_bloqueados(char* clave, int motivo) {
	t_list * ESIs_bloqueados = get_ESIs_bloqueados_por_clave(clave, motivo);
	if (list_size(ESIs_bloqueados)) {
		log_info(logger, "ESIs bloqueados:");
		void mostrar_ESI_bloqueado(void* item_bloqueo){
			t_bloqueado* bloqueo = (t_bloqueado*) item_bloqueo;
			char* descripcion_motivo = bloqueo->motivo == bloqueado_por_consola ? "(por consola)" : "";
			char str[12];
			sprintf(str, "%d", bloqueo->ESI->id_ESI);
			log_and_free(logger, string_concat(5, "- ESI ", str, " ", descripcion_motivo," \n"));
		}
		list_iterate(ESIs_bloqueados, mostrar_ESI_bloqueado);
	} else {
		log_info(logger, "No hay ESIs bloqueados.\n");
	}
	list_destroy(ESIs_bloqueados);
}

t_list * recibir_claves_tomadas(void * buffer) {
	int desplazamiento = 0;
	int cant_claves_tomadas = deserializar_int(buffer, &desplazamiento);
	t_list * list_claves_tomadas = list_create();

	for(int i = 0;i < cant_claves_tomadas; i++) {
		t_clave_tomada * clave_tomada = malloc(sizeof(t_clave_tomada));
		clave_tomada->id_ESI = deserializar_int(buffer, &desplazamiento);
		clave_tomada->clave = deserializar_string(buffer, &desplazamiento);
		list_add(list_claves_tomadas, clave_tomada);
	}
	return list_claves_tomadas;
}

/*
 * -----------------------------------------------
 * FUNCIONES DEADLOCK
 * -----------------------------------------------
 */

bool ESI_en_lista(t_list * lista, int id_ESI) {
	bool ESI_presente(void* item){
		return ((int) item) == id_ESI;
	}
	return list_find(lista, ESI_presente) != NULL;
}

t_list * deadlock_get_ids_ESIs(t_list * claves_tomadas, t_list * claves_pedidas) {
	t_list * lista_ids_ESIs = list_create();
	void agregar_ESI_claves_tomadas(void * item_clave_tomada) {
		t_clave_tomada * clave_tomada = (t_clave_tomada *) item_clave_tomada;
		bool ESI_ya_existente(void * id_ESI){
			return ((int) id_ESI) == clave_tomada->id_ESI;
		}
		if (list_find(lista_ids_ESIs, ESI_ya_existente) == NULL) {
			list_add(lista_ids_ESIs, (void *) clave_tomada->id_ESI);
		}

	}
	list_iterate(claves_tomadas, agregar_ESI_claves_tomadas);

	void agregar_ESI_claves_pedidas(void * item_clave_pedida) {
		t_bloqueado * clave_pedida = (t_bloqueado *) item_clave_pedida;
		bool ESI_ya_existente(void * id_ESI){
			return (int) id_ESI == clave_pedida->ESI->id_ESI;
		}
		if (list_find(lista_ids_ESIs, ESI_ya_existente) == NULL) {
			list_add(lista_ids_ESIs, (void *)clave_pedida->ESI->id_ESI);
		}

	}
	list_iterate(claves_pedidas, agregar_ESI_claves_pedidas);
	return lista_ids_ESIs;
}

t_list * deadlock_get_ESIs_contienen_claves_pedidas(t_list * claves_tomadas, t_list * claves_pedidas, int id_ESI) {
	bool clave_pedida_x_ESI(void* item_bloqueo){
		t_bloqueado* bloqueo = (t_bloqueado*) item_bloqueo;
		return bloqueo->ESI->id_ESI == id_ESI;
	}
	void * transformer(void* item_bloqueo){
		t_bloqueado* bloqueo = (t_bloqueado*) item_bloqueo;
		bool clave_tomada_x_ESI(void* item_clave_tomada){
			t_clave_tomada* clave_tomada = (t_clave_tomada*) item_clave_tomada;
			return strings_equal(clave_tomada->clave, bloqueo->clave_de_bloqueo);
		}
		t_clave_tomada* clave_tomada = list_find(claves_tomadas, clave_tomada_x_ESI);
		return (void*) clave_tomada->id_ESI;
	}
	t_list * pedidos = list_filter(claves_pedidas, clave_pedida_x_ESI);
	t_list * result = list_map(pedidos, transformer);
	list_destroy(pedidos);
	return result;
}

void detectar_deadlock(t_list * claves_tomadas, t_list * claves_pedidas) {
	t_list * ids_ESIs = deadlock_get_ids_ESIs(claves_tomadas, claves_pedidas);
	t_list * lista_deadlocks = list_create();
	t_list * lista_inicial = list_create();
	t_list * listas_usadas = list_create(); // Guardo todas las listas usadas para despues liberarlas

	void iterar_ESI(int id_ESI, t_list * lista_actual) {
		int id_primer_ESI_lista = (int) list_get(lista_actual, 0);
		if (id_primer_ESI_lista == id_ESI) { // Deadlock encontrado
			list_add(lista_deadlocks, lista_actual);
			return;
		}

		// Si no encontro un deadlock, sigo
		if (ESI_en_lista(lista_actual, id_ESI)) { // Si ya esta en la lista, no hay deadlock por este camino
			return;
		}

		 // Si no esta en la lista, lo agrego para seguir la cascada
		t_list * list_proximo_nivel = copy_list(lista_actual);
		list_add(listas_usadas, list_proximo_nivel);
		list_add(list_proximo_nivel, (void*) id_ESI);

		// Traigo los ESIs que contienen las claves pedidas por este ESI
		t_list * ESIs_contienen_claves_pedidas = deadlock_get_ESIs_contienen_claves_pedidas(claves_tomadas, claves_pedidas, id_ESI);
		void iterar_proximo_nivel(void * item_id_ESI2){
			int id_ESI2 = (int) item_id_ESI2;
			iterar_ESI(id_ESI2, list_proximo_nivel);
		}
		list_iterate(ESIs_contienen_claves_pedidas, iterar_proximo_nivel);
		list_destroy(ESIs_contienen_claves_pedidas);
	}
	void check_deadlock(void * item_id_ESI){
		int id_ESI = (int) item_id_ESI;
		iterar_ESI(id_ESI, lista_inicial);
	}
	list_iterate(ids_ESIs, check_deadlock);
	list_destroy(ids_ESIs);

	//limpiar_deadlocks_repetidos(lista_deadlocks);
	mostrar_deadlocks(lista_deadlocks);


	list_destroy(lista_deadlocks);
	list_destroy_and_destroy_elements(listas_usadas, list_destroy);
	list_destroy(lista_inicial);
}

void mostrar_deadlocks(t_list * listado_deadlocks) {
	int num_deadlock = 1;
	void mostrar_deadlock(void* item_deadlock){
		t_list * deadlock = (t_list *) item_deadlock;
		char* mensaje = string_new();
		string_append(&mensaje, "Deadlock ");
		char str_num_deadlock[12];
		sprintf(str_num_deadlock, "%d", num_deadlock);
		string_append(&mensaje, str_num_deadlock);
		string_append(&mensaje, ": ");
		void concatenar_valor(void* item_id_ESI){
			int id_ESI = (int) item_id_ESI;
			char str[12];
			sprintf(str, "%d", id_ESI);
			string_append(&mensaje, "ESI:");
			string_append(&mensaje, str);
			string_append(&mensaje, " -");
		}
		list_iterate(deadlock, concatenar_valor);
		puts(mensaje);
		free(mensaje);
		num_deadlock++;
	}
	list_iterate(listado_deadlocks, mostrar_deadlock);
}

void limpiar_deadlocks_repetidos(t_list * listado_deadlocks) {
	bool deadlocks_son_equivalentes(t_list * deadlock1, t_list * deadlock2) {
		bool ESI_en_deadlock2(void* id_ESI) {
			return ESI_en_lista(deadlock2, (int) id_ESI);
		}
		return list_all_satisfy(deadlock1, ESI_en_deadlock2);
	}

	t_list * get_deadlock_equivalente(t_list * deadlock, t_list * lista) {
		bool deadlock_gemelo(void* item_deadlock2) {
			t_list * deadlock2 = (t_list*) item_deadlock2;
			return deadlocks_son_equivalentes(deadlock, deadlock2);
		}
		return list_find(lista, deadlock_gemelo);
	}

	t_list * nueva_lista = list_create();
	void agregar_si_no_esta(void* item_deadlock) {
		t_list * deadlock = (t_list*) item_deadlock;
		if (get_deadlock_equivalente(deadlock, nueva_lista) == NULL) {
			list_add(nueva_lista, deadlock);
		}
	}
	list_iterate(listado_deadlocks, agregar_si_no_esta);
	// list_destroy(listado_deadlocks);
	listado_deadlocks = nueva_lista;
}

void free_claves_tomadas(void * item_clave_tomada) {
	t_clave_tomada * clave_tomada = (t_clave_tomada*) item_clave_tomada;
	free(clave_tomada->clave);
	free(clave_tomada);
}

void free_t_bloqueado(void * item_bloqueo) {
	t_bloqueado * bloqueo = (t_bloqueado*) bloqueo;
	free(bloqueo->clave_de_bloqueo);
	free(bloqueo);
}

