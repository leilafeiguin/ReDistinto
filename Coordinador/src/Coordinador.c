#include <stdio.h>
#include <stdlib.h>
#include "Coordinador.h"

void* archivo;
t_log* logger;
t_log* log_operaciones;

int main(void) {
	pthread_mutex_init(&sem_instancias, NULL);
	pthread_mutex_init(&sem_planificador, NULL);
	pthread_mutex_init(&sem_claves_tomadas, NULL);

	sem_init(&operaciones_habilitadas, 0, 1);

	(void) signal(SIGINT, funcion_exit);
	imprimir("/home/utnso/workspace/tp-2018-1c-PuntoZip/Coordinador/coord_image.txt");
	iniciar_loggers();
	log_info(logger, "Inicializando proceso Coordinador. \n");

	lista_instancias = list_create();
	lista_claves_tomadas = list_create();
	new_list_instancias_organized = list_create();
	lista_ESIs = list_create();

	configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

	iniciar_servidor();
	return EXIT_SUCCESS;
}

coordinador_configuracion get_configuracion() {
	log_info(logger,"Levantando archivo de configuracion del proceso Coordinador \n");
	coordinador_configuracion configuracion;
	t_config*  archivo_configuracion = config_create(pathCoordinadorConfig);
	configuracion.PUERTO_ESCUCHA = copy_string(get_campo_config_string(archivo_configuracion, "PUERTO_ESCUCHA"));
	configuracion.ALGORITMO_DISTRIBUCION = copy_string(get_campo_config_string(archivo_configuracion, "ALGORITMO_DISTRIBUCION"));
	configuracion.CANTIDAD_ENTRADAS = get_campo_config_int(archivo_configuracion, "CANTIDAD_ENTRADAS");
	configuracion.TAMANIO_ENTRADA = get_campo_config_int(archivo_configuracion, "TAMANIO_ENTRADA");
	configuracion.RETARDO = get_campo_config_int(archivo_configuracion, "RETARDO");
	config_destroy(archivo_configuracion);
	return configuracion;
}

void salir(int motivo){
	//Todo mejorar funcion de salida
	exit(motivo);
}

void iniciar_loggers() {
	logger = log_create("coordinador_logs.txt", "Coordinador Logs", 1, 1);
	log_operaciones = log_create("log_operaciones.txt", "Log de operaciones", 0, 1);
}

void iniciar_servidor() {
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

/*
--------------------------------------------------------
----------------- Conexiones entrantes -----------------
--------------------------------------------------------
*/
	while(1){
		int nueva_coneccion = aceptar_conexion(listener);
		t_paquete* handshake = recibir(listener);
		log_info(logger, "Recibi una nueva conexion. \n");
		free(handshake);
		handle_coneccion(nueva_coneccion);
	}
}

void handle_coneccion(int socketActual) {
	t_paquete* paqueteRecibido = recibir(socketActual);
	switch(paqueteRecibido->codigo_operacion){
		case cop_handshake_ESI_Coordinador:
			handle_ESI(socketActual, paqueteRecibido);
		break;

		case cop_handshake_Planificador_Coordinador:
			handle_planificador(socketActual, paqueteRecibido);
		break;

		case cop_handshake_Instancia_Coordinador:
			handle_instancia(socketActual,paqueteRecibido);
		break;
	}
}

void handle_planificador(un_socket planificador, t_paquete* paquetePlanificador) {
	Planificador = planificador;
	esperar_handshake(Planificador, paquetePlanificador,cop_handshake_Planificador_Coordinador);
	log_info(logger, "Realice handshake con Planificador \n");
	t_paquete* paqueteRecibido = recibir(Planificador); // Info sobre el Planificador
	liberar_paquete(paqueteRecibido);
	t_list* thread_params;
	nuevo_hilo(planificador_conectado_funcion_thread, thread_params);
}

void handle_instancia(un_socket instancia, t_paquete* paqueteInstancia) {
	esperar_handshake(instancia, paqueteInstancia,cop_handshake_Instancia_Coordinador);
	t_paquete* paqueteRecibido = recibir(instancia); // Info sobre la Instancia
	char* nombre_instancia = copy_string(paqueteRecibido->data);
	t_list* thread_params = list_create();
	list_add(thread_params, (void*) instancia);
	list_add(thread_params, (void*) nombre_instancia);
	pthread_t thread = nuevo_hilo(instancia_conectada_funcion_thread, thread_params);
	pthread_join(thread, NULL);
	liberar_paquete(paqueteRecibido);
}

void handle_ESI(un_socket socket_ESI, t_paquete* paqueteESI) {
	if(Planificador != codigo_error){
		esperar_handshake(socket_ESI,paqueteESI,cop_handshake_ESI_Coordinador);
		log_info(logger, "Realice handshake con ESI \n");
		t_paquete* paqueteRecibido = recibir(socket_ESI); // Info sobre el ESI
		int desplazamiento = 0;
		int id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
		t_ESI * ESI = generar_ESI(socket_ESI, id_ESI);
		list_add(lista_ESIs, ESI);
		liberar_paquete(paqueteRecibido);
		t_list* thread_params = list_create();
		list_add(thread_params, ESI);
		nuevo_hilo(ESI_conectado_funcion_thread, thread_params);
	}else{ //No hay planificador, se debe rechazar la conexion
		log_info(logger, "Se conecto un ESI estando en estado invalido. \n");
		close(socket_ESI); //Se cierra el socket
	}
}

t_list * instancias_activas() {
	bool instancia_activa(t_instancia * i) {
		return i->estado == conectada ? true : false;
	}
	pthread_mutex_lock(&sem_instancias);
	t_list * result = list_filter(lista_instancias, instancia_activa);
	pthread_mutex_unlock(&sem_instancias);
	return result;
}

pthread_t nuevo_hilo(void *(* funcion ) (void *), t_list * parametros) {
	pthread_t thread = threads[i_thread];
	int thread_creacion = pthread_create(&thread, NULL, funcion, (void*) parametros);
	if (thread_creacion != 0) {
		perror("pthread_create");
	} else {
		i_thread++;
	}
	return thread;
}

void* instancia_conectada_funcion_thread(void* argumentos) {
	instancia_conectada((un_socket)list_get(argumentos, 0), list_get(argumentos, 1));
	list_destroy(argumentos);
	return NULL;
}

void* ESI_conectado_funcion_thread(void* argumentos) {
	t_ESI * ESI = list_get(argumentos, 0);
	list_destroy(argumentos);
	escuchar_ESI(ESI);
	pthread_detach(pthread_self());
	return NULL;
}

void escuchar_ESI(t_ESI * ESI) {
	bool escuchar = true;
	while(escuchar) {
		sem_wait(&operaciones_habilitadas);
		sem_post(&operaciones_habilitadas);
		t_paquete* paqueteRecibido = recibir(ESI->socket);
		char str[12];
		sprintf(str, "%d", ESI->id_ESI);
		log_and_free(logger, string_concat(3, "Ejecutando instrucciones del ESI ", str, ".. \n"));
		retardo();
		switch(paqueteRecibido->codigo_operacion) {
			case codigo_error:
				kill_ESI(ESI);
				escuchar = false;
			break;

			case cop_Coordinador_Ejecutar_Get:
				ejecutar_get(ESI, paqueteRecibido->data);
			break;

			case cop_Coordinador_Ejecutar_Set: ;
				int desplazamiento = 0;
				char* clave = deserializar_string(paqueteRecibido->data, &desplazamiento);
				char* valor = deserializar_string(paqueteRecibido->data, &desplazamiento);
				ejecutar_set(ESI, copy_string(clave), valor);
				free(clave);
				free(valor);
			break;

			case cop_Coordinador_Ejecutar_Store:
				ejecutar_store(ESI, paqueteRecibido->data);
			break;

		}
		liberar_paquete(paqueteRecibido);
	}
}

void* planificador_conectado_funcion_thread(void* argumentos) {
	escuchar_planificador(socket);
	pthread_detach(pthread_self());
	return NULL;
}

void escuchar_planificador() {
	bool escuchar = true;
	while(escuchar) {
		int desplazamiento = 0;
		t_paquete* paqueteRecibido = recibir(Planificador);
		switch(paqueteRecibido->codigo_operacion) {
			case codigo_error:
				log_info(logger, "Error en el Planificador. Abortando Planificador. \n");
				Planificador = codigo_error;
				escuchar = false;
			break;
			case cop_Planificador_Analizar_Deadlocks:
				enviar_claves_informacion_tomadas();
			break;
			case cop_Planificador_Consultar_Clave:
				handle_consulta_clave(paqueteRecibido->data);
			break;
			case cop_ESI_finalizado: ;
				int id_ESI = deserializar_int(paqueteRecibido->data, &desplazamiento);
				t_ESI * ESI = get_ESI_por_id(id_ESI);
				handle_ESI_finalizado(ESI);
			break;
			case cop_Coordinador_Bloquear_Claves_Iniciales: ;
				t_list * lista_claves_bloquear = deserializar_lista_strings(paqueteRecibido->data, &desplazamiento);
				bloquear_claves_iniciales(lista_claves_bloquear);
				destruir_lista_strings(lista_claves_bloquear);
			break;
			case cop_Coordinador_Liberar_Clave: ;
				log_and_free(logger, string_concat(3, "Desbloqueando clave ", paqueteRecibido->data, " \n"));
				liberar_clave_tomada(paqueteRecibido->data);
			break;
		}
		liberar_paquete(paqueteRecibido);
	}
}

void bloquear_claves_iniciales(t_list * lista_claves) {
	t_ESI * ESI_sistema;
	void bloquear_clave(void* item_clave){
		char* clave = (char*) item_clave;
		nueva_clave_tomada(ESI_sistema, clave);
	}
	list_iterate(lista_claves,bloquear_clave);
}

void instancia_conectada(un_socket socket_instancia, char* nombre_instancia) {
	bool instancia_ya_existente(t_instancia * ins){
		return strcmp(ins->nombre, nombre_instancia) == 0 ? true : false;
	}
	pthread_mutex_lock(&sem_instancias);
	t_instancia * instancia = list_find(lista_instancias, instancia_ya_existente);
	pthread_mutex_unlock(&sem_instancias);
	bool instancia_nueva = instancia == NULL;
	int codigo_respuesta = instancia_nueva ? cop_Instancia_Nueva : cop_Instancia_Vieja;
	if (instancia_nueva) {
		instancia = crear_instancia(socket_instancia, nombre_instancia);
	} else {
		instancia->socket = socket_instancia;
	}

	enviar(instancia->socket, codigo_respuesta, size_of_string(""), "");
	enviar_informacion_tabla_entradas(instancia);
	if (instancia_nueva) { // Si es una instancia nueva
		mensaje_instancia_conectada(nombre_instancia, 0);
		pthread_mutex_lock(&sem_instancias);
		list_add(lista_instancias, instancia);
		pthread_mutex_unlock(&sem_instancias);
	} else { // Si es una instancia ya creada reconectandose
		enviar_listado_de_strings(instancia->socket, instancia->keys_contenidas);
		mensaje_instancia_conectada(nombre_instancia, 1);
		instancia->estado = conectada;
	}

	// Le informo al Planificador sobre la coneccion de la instancia
	enviar(Planificador, codigo_respuesta, size_of_string(nombre_instancia), nombre_instancia);
}

int ejecutar_set(t_ESI * ESI, char* clave, char* valor) {
	log_and_free(logger, string_concat(5, "Ejecutando SET '", clave, "' : '", valor, "' \n"));
	if (!validar_tamanio_clave(clave)) {
		error_clave_larga(ESI, "SET", clave);
		return 0;
	}

	int id_ESI_con_clave = get_id_ESI_con_clave(clave);
	if (id_ESI_con_clave == NULL) {
		log_error_and_free(logger, string_concat(3, "SET rechazado. No solicito el GET para la clave '", clave, "'. \n"));
		notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_Clave_No_Pedida, clave);
		return 0;
	} else if (id_ESI_con_clave != ESI->id_ESI)  {
		log_error_and_free(logger, string_concat(3, "SET rechazado. La clave '%s' se encuentra tomada por otro ESI. '", clave, "'. \n"));
		notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_Clave_Tomada, clave);
		return 0;
	}

	// Si la clave ya se encuentra en alguna instancia la vuelvo a setear en la misma
	t_instancia * instancia = get_instancia_con_clave(clave);
	if (instancia != NULL) {
		if(health_check(instancia)) {
			char* valor_actual = get(clave);
			if (cantidad_entradas_necesarias(valor, configuracion.TAMANIO_ENTRADA) > cantidad_entradas_necesarias(valor_actual, configuracion.TAMANIO_ENTRADA)) {
				log_error(logger, "SET rechazado. El valor '%s' ocupa mas entradas que el valor anterior.. \n", valor);
				notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_Valor_Mayor_Anterior, clave);
			} else {
				ejecucion_set_caso_exito(instancia, ESI, clave, valor);
			}
		} else {
			log_error(logger, "SET rechazado. La instancia %s no se encuentra disponible. \n", instancia->nombre);
			notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_Instancia_No_Disponibe, instancia->nombre);
		}
		return 0;
	}

	bool inserted = false;
	while(!inserted) {
		instancia = instancia_a_guardar(clave);
		if (instancia == NULL) {
			log_error(logger, "No pudo ejecutarse el SET. No hay instancias disponibles. \n ");
			notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_No_Instancias, "");
			return 0;
		}
		if (health_check(instancia)) {
			ejecucion_set_caso_exito(instancia, ESI, clave, valor);
			inserted = true;
		} else {
			log_error(logger, "%s no disponible. \n", instancia->nombre);
		}
	}
	return 1;
}

void ejecucion_set_caso_exito(t_instancia * instancia, t_ESI * ESI, char* clave, char* valor) {
	validar_necesidad_compactacion(instancia, clave, valor);
	setear(instancia, clave, valor);
	actualizar_keys_contenidas(instancia);
	actualizar_cantidad_entradas_ocupadas(instancia);
	log_and_free(logger, string_concat(5, "SET ejecutado con exito. SET '", clave, "':'", valor, "' \n"));
	log_and_free(log_operaciones, string_concat(5, "SET ", clave, " ", valor," \n"));
	notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Exito, "");
}

int validar_necesidad_compactacion(t_instancia * instancia, char* clave, char* valor) {
	log_info(logger, "Validando necesidad de compactacion... \n");
	int tamanio_buffer = size_of_strings(2, clave, valor);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_string(buffer, &desplazamiento, clave);
	serializar_string(buffer, &desplazamiento, valor);
	pthread_mutex_lock(&instancia->sem_instancia);
	enviar(instancia->socket, cop_Instancia_Necesidad_Compactacion, tamanio_buffer, buffer);
	free(buffer);
	t_paquete* paqueteResultado = recibir(instancia->socket);
	pthread_mutex_unlock(&instancia->sem_instancia);
	if (paqueteResultado->codigo_operacion == cop_Instancia_Necesidad_Compactacion_True) { // Es necesario compactar
		ejecutar_compactacion();
	}
	liberar_paquete(paqueteResultado);
	return 0;
}

void ejecutar_compactacion() {
	log_info(logger, "Ejecutando compactacion de instancias \n ");
	sem_trywait(&operaciones_habilitadas);
	t_list* list_instancias_activas = instancias_activas();
	void enviar_mensaje_compactacion(t_instancia * instancia) {
		pthread_mutex_lock(&instancia->sem_instancia);
		enviar(instancia->socket, cop_Instancia_Ejecutar_Compactacion, size_of_string(""), "");
		t_paquete* paquete = recibir(instancia->socket);
		pthread_mutex_unlock(&instancia->sem_instancia);
		liberar_paquete(paquete);
	}
	list_iterate(list_instancias_activas, enviar_mensaje_compactacion);
	sem_post(&operaciones_habilitadas);
	list_destroy(list_instancias_activas);
}

int setear(t_instancia * instancia, char* clave, char* valor) {
	// list_add(instancia->keys_contenidas, clave); // Registro que esta instancia contendra la clave especificada
	int tamanio_buffer = size_of_strings(2, clave, valor);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_string(buffer, &desplazamiento, clave);
	serializar_string(buffer, &desplazamiento, valor);
	pthread_mutex_lock(&instancia->sem_instancia);
	enviar(instancia->socket, cop_Instancia_Ejecutar_Set, tamanio_buffer, buffer);
	pthread_mutex_unlock(&instancia->sem_instancia);
	free(buffer);
	return 0;
}

int actualizar_keys_contenidas(t_instancia * instancia) {
	pthread_mutex_lock(&instancia->sem_instancia);
	list_destroy_and_destroy_elements(instancia->keys_contenidas, free);
	instancia->keys_contenidas = recibir_listado_de_strings(instancia->socket);
	pthread_mutex_unlock(&instancia->sem_instancia);
	return 0;
}

int actualizar_cantidad_entradas_ocupadas(t_instancia * instancia) {
	pthread_mutex_lock(&instancia->sem_instancia);
	t_paquete* paqueteCantidadEntradasOcupadas = recibir(instancia->socket); // Recibe la cantidad de entradas
	pthread_mutex_unlock(&instancia->sem_instancia);
	instancia->cant_entradas_ocupadas = atoi(paqueteCantidadEntradasOcupadas->data);
	liberar_paquete(paqueteCantidadEntradasOcupadas);
	return 0;
}

int ejecutar_get(t_ESI * ESI, char* clave) {
	log_and_free(logger, string_concat(3, "Ejecutando GET:", clave," \n"));
	if (!validar_tamanio_clave(clave)) {
		error_clave_larga(ESI, "GET", clave);
		return 0;
	}

	int id_ESI_con_clave = get_id_ESI_con_clave(clave);
	if (id_ESI_con_clave != NULL && id_ESI_con_clave != ESI->id_ESI ) {
		log_error_and_free(logger, string_concat(3, "GET rechazado. La clave '", clave," se encuentra tomada por otro ESI. \n"));
		notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_Clave_Tomada, clave);
		return 0;
	}

	nueva_clave_tomada(ESI, clave);
	if (validar_clave_ingresada(clave)) {
		t_instancia * instancia = get_instancia_con_clave(clave);
		if (!health_check(instancia)) {
			log_error(logger, "GET rechazado. La instancia no se encuentra disponible. \n");
			notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_Instancia_No_Disponibe, instancia->nombre);
			return 0;
		}
		char* valor = get(clave);
		log_and_free(logger, string_concat(5, "GET ejecutado con exito. El valor de la clave '", clave, "' es '", valor, "'. \n"));
		log_and_free(log_operaciones, string_concat(3, "GET ", clave," \n"));
		notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Exito, valor);
	} else {
		log_and_free(logger, string_concat(3, "GET ejecutado con exito. La clave '", clave, "' todavia no tiene ningun valor. \n"));
		log_and_free(log_operaciones, string_concat(3, "GET ", clave," \n"));
		notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Exito_Clave_Sin_Valor, "");
	}

	return 1;
}

int get_id_ESI_con_clave(char* clave) {
	bool clave_match(t_clave_tomada * clave_comparar){
		return strcmp(clave, clave_comparar->clave) == 0 ? true : false;
	}
	pthread_mutex_lock(&sem_claves_tomadas);
	t_clave_tomada * t_clave = list_find(lista_claves_tomadas, clave_match);
	pthread_mutex_unlock(&sem_claves_tomadas);
	return t_clave == NULL ? NULL : t_clave->id_ESI;
}

bool validar_clave_ingresada(char* clave) {
	bool clave_match(void * key){
		char* string_key = (char*) key;
		return strcmp(clave, string_key) == 0 ? true : false;
	}
	bool instancia_tiene_clave(void * item){
		t_instancia * instancia = (t_instancia *) item;
		return list_any_satisfy(instancia->keys_contenidas, clave_match);
	}
	pthread_mutex_lock(&sem_instancias);
	bool result = list_any_satisfy(lista_instancias, instancia_tiene_clave);
	pthread_mutex_unlock(&sem_instancias);
	return result;
}

bool validar_clave_tomada(char* clave) {
	bool existe_bloqueo(void * bloqueo){
		t_clave_tomada * clave_tomada = (t_clave_tomada *) bloqueo;
		return strcmp(clave, clave_tomada->clave) == 0 ? true : false;
	}
	pthread_mutex_lock(&sem_claves_tomadas);
	bool result = list_any_satisfy(lista_claves_tomadas, existe_bloqueo);
	pthread_mutex_unlock(&sem_claves_tomadas);
	return result;
}

t_clave_tomada * nueva_clave_tomada(t_ESI * ESI, char* clave) {
	if (!validar_clave_tomada(clave)) {
		t_clave_tomada * t_clave = malloc(sizeof(t_clave_tomada));
		t_clave->id_ESI =  ESI->id_ESI;
		t_clave->clave = copy_string(clave);
		pthread_mutex_lock(&sem_claves_tomadas);
		list_add(lista_claves_tomadas, t_clave);
		pthread_mutex_unlock(&sem_claves_tomadas);
		return t_clave;
	}
	return NULL;
}

char* get(char* clave) {
	t_instancia * instancia = get_instancia_con_clave(clave);
	if (health_check(instancia)) {
		pthread_mutex_lock(&instancia->sem_instancia);
		enviar(instancia->socket, cop_Instancia_Ejecutar_Get, size_of_string(clave), clave); // Envia a la instancia la clave
		t_paquete* paqueteValor = recibir(instancia->socket); // Recibe el valor solicitado
		pthread_mutex_unlock(&instancia->sem_instancia);
		char* valor = copy_string(paqueteValor->data);
		liberar_paquete(paqueteValor);
		return valor;
	}
	log_and_free(logger, string_concat(3, "ERROR: No pudo ejecutarse el GET. ", instancia->nombre, " no disponible. \n"));
	return NULL;
}

int ejecutar_store(t_ESI * ESI, char* clave) {
	if (!validar_tamanio_clave(clave)) {
		error_clave_larga(ESI, "STORE", clave);
		return 0;
	}

	int id_ESI_con_clave = get_id_ESI_con_clave(clave);
	if (id_ESI_con_clave == NULL) {
		log_error_and_free(logger, string_concat(3, "STORE rechazado. No solicito el GET para la clave '", clave, "'. \n"));
		notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_Clave_No_Pedida, clave);
		return 0;
	} else if (id_ESI_con_clave != ESI->id_ESI ) {
		log_error_and_free(logger, string_concat(3, "STORE rechazado. La clave '", clave , "' se encuentra tomada por otro ESI \n"));
		notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_Clave_Tomada, clave);
		return 0;
	}

	t_instancia * instancia = get_instancia_con_clave(clave);
	liberar_clave_tomada(clave);
	if (instancia != NULL && health_check(instancia)) {
		pthread_mutex_lock(&instancia->sem_instancia);
		enviar(instancia->socket, cop_Instancia_Ejecutar_Store, size_of_string(clave), clave); // Envia a la instancia la clave
		t_paquete* paqueteEstadoOperacion = recibir(instancia->socket); // Aguarda a que la instancia le comunique que el STORE se ejectuo de forma exitosa
		pthread_mutex_unlock(&instancia->sem_instancia);
		int estado_operacion = paqueteEstadoOperacion->codigo_operacion;
		liberar_paquete(paqueteEstadoOperacion);
		if (estado_operacion == cop_Instancia_Ejecucion_Exito) {
			log_and_free(logger, string_concat(3, "STORE ejecutado con exito. La clave '", clave, "' fue guardada y liberada. \n"));
			log_and_free(log_operaciones, string_concat(3, "STORE ", clave," \n"));
			notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Exito, "");
		}
	} else if(validar_clave_tomada(clave)) {
		log_and_free(logger, string_concat(3, "STORE ejecutado con exito. La clave '", clave, "' fue liberada. \n"));
		log_and_free(log_operaciones, string_concat(3, "STORE ", clave," \n"));
		notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Exito, "");
	} else {
		log_error(logger, "STORE rechazado. La instancia no se encuentra disponible. Recurso liberada pero no guardado. \n");
		notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_No_Instancias, "");
	}
	return 1;
}

int dump() {
	log_info(logger,"DUMP instancias \n");
	t_list* list_instancias_activas = instancias_activas();
	list_iterate(list_instancias_activas, dump_instancia);
	list_destroy(list_instancias_activas);
	return 0;
}

int dump_instancia(t_instancia * instancia) {
	if (health_check(instancia)) {
		pthread_mutex_lock(&instancia->sem_instancia);
		enviar(instancia->socket, cop_Instancia_Ejecutar_Dump, size_of_string(""), "");
		t_paquete* paqueteEstadoOperacion = recibir(instancia->socket); // Aguarda a que la instancia le comunique que el STORE se ejectuo de forma exitosa
		pthread_mutex_unlock(&instancia->sem_instancia);
		int estado_operacion = paqueteEstadoOperacion->codigo_operacion;
		liberar_paquete(paqueteEstadoOperacion);
		if (estado_operacion == cop_Instancia_Ejecucion_Exito) {
			return 1;
		}
	}
	log_and_free(logger, string_concat(3, "ERROR: No pudo ejecutarse el DUMP. ", instancia->nombre, " no disponible. \n"));
	return 0;
}

bool health_check(t_instancia * instancia) {
	if (instancia->estado == desconectada) {
		return false;
	}
	pthread_mutex_lock(&instancia->sem_instancia);
	enviar(instancia->socket, codigo_healthcheck, size_of_string(""), ""); // Envio el request de healthcheck
	t_paquete* paqueteRecibido = recibir(instancia->socket); // Aguardo a recbir el OK de la instancia
	pthread_mutex_unlock(&instancia->sem_instancia);
	int codigo_recibido = paqueteRecibido->codigo_operacion;
	liberar_paquete(paqueteRecibido);
	if (codigo_recibido == codigo_healthcheck) {
		return true;
	}
	instancia->estado = desconectada;
	log_and_free(logger, string_concat(2, instancia->nombre, " no disponible. \n"));
	return false;
}

t_instancia * get_instancia_con_clave(char * clave) {
	bool instancia_tiene_clave(t_instancia * instancia){
		bool clave_match(char * clave_comparar){
			return strcmp(clave, clave_comparar) == 0 ? true : false;
		}
		return list_find(instancia->keys_contenidas, clave_match) != NULL ? true :  false;
	}
	pthread_mutex_lock(&sem_instancias);
	t_instancia * result = list_find(lista_instancias, instancia_tiene_clave);
	pthread_mutex_unlock(&sem_instancias);
	return result;
}

t_instancia * instancia_a_guardar(char* clave) {
	if (strings_equal(configuracion.ALGORITMO_DISTRIBUCION, "EL")) {
		return equitative_load();
	} else if (strings_equal(configuracion.ALGORITMO_DISTRIBUCION, "LSU")) {
		return least_space_used();
	}
	return key_explicit(clave);
}

t_instancia * crear_instancia(un_socket socket, char* nombre) {
	// Creo la estructura de la instancia y la agrego a la lista
	t_instancia * instancia_nueva = malloc(sizeof(t_instancia));
	pthread_mutex_init(&instancia_nueva->sem_instancia, NULL);
	instancia_nueva->socket = socket;
	instancia_nueva->nombre = nombre;
	instancia_nueva->estado = conectada;
	instancia_nueva->puntero_entradas = 0; //Este es el puntero de entradas
	instancia_nueva->cant_entradas_ocupadas = 0; // Contador Cantidad de entradas
	instancia_nueva->keys_contenidas = list_create();
	return instancia_nueva;
}

int enviar_informacion_tabla_entradas(t_instancia * instancia) {
	// Envio la cantidad de entradas que va a tener esa instancia y el tamaño que va a tener cada una
	int tamanio_buffer = sizeof(int) * 2;
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_int(buffer, &desplazamiento, configuracion.CANTIDAD_ENTRADAS);
	serializar_int(buffer, &desplazamiento, configuracion.TAMANIO_ENTRADA);
	enviar(instancia->socket, cop_generico, tamanio_buffer, buffer);
	free(buffer);
	return 0;
}

void mensaje_instancia_conectada(char* nombre_instancia, int estado) { // 0: Instancia nueva, 1: Instancia reconectandose
	char* mensaje = estado == 0 ? "Instancia conectada: " : "Instancia reconectada: ";
	mensaje = string_concat(3, mensaje, nombre_instancia, " \n");
	log_and_free(logger, mensaje);
}

// ALGORITMOS DE DISTRIBUCION

t_instancia * equitative_load() {
	t_list* list_instancias_activas = instancias_activas();

	if(siguiente_equitative_load+1 > list_size(list_instancias_activas))
	{
		siguiente_equitative_load = 0;
	}
	t_instancia * siguiente = list_get(list_instancias_activas, siguiente_equitative_load);
	siguiente_equitative_load++;
	list_destroy(list_instancias_activas);
	return siguiente;
}

t_instancia * least_space_used() {
	t_list* list_instancias_activas = instancias_activas();
	int i = 0;
	int menorEspacioInstancia = -1;
	t_instancia * instanciaConMayorEspacioDisponible = list_get(list_instancias_activas, 0);

	int getEspacio(t_instancia * element) {
		return (element)->cant_entradas_ocupadas;
	}

	void instancia_mas_vacia() {
		if (menorEspacioInstancia == -1 || menorEspacioInstancia > getEspacio(list_get(list_instancias_activas, i)))
		{
			menorEspacioInstancia = getEspacio(list_get(list_instancias_activas, i));
			instanciaConMayorEspacioDisponible = list_get(list_instancias_activas, i);
		}

		i++;
	}

	list_iterate(list_instancias_activas, instancia_mas_vacia);

	list_destroy(list_instancias_activas);
	return instanciaConMayorEspacioDisponible;
}

t_instancia * key_explicit(char* clave) {
	t_list* list_instancias_activas = instancias_activas();
	char primeraLetra = tolower(clave[0]);
	int letras = 26;
	int cantInstancias = list_size(list_instancias_activas);
	int resto_ultimo = 0;
	int resto_inicial = letras%cantInstancias;
	int cantidad_letras_x_instancia = letras/cantInstancias;
	t_instancia * instanciaSeleccionada;

	if(resto_inicial) {
		cantidad_letras_x_instancia ++;
		resto_ultimo = (cantidad_letras_x_instancia * cantInstancias) - letras;
	}

	int valorLetra = primeraLetra;
	int cont = 0;

	for (int i = 97; i < 97+letras-1; i+cantidad_letras_x_instancia)
	{
		if(valorLetra >= i && valorLetra < i+cantidad_letras_x_instancia)
		{
			instanciaSeleccionada = list_get(list_instancias_activas, cont);
		}
		i += cantidad_letras_x_instancia;
		cont ++;
	}

	list_destroy(list_instancias_activas);
	return instanciaSeleccionada;
}

void liberar_clave_tomada(char* clave) {
	pthread_mutex_lock(&sem_claves_tomadas);
	bool clave_match(void * clave_tomada){
		char* nombre_clave_tomada = ((t_clave_tomada *) clave_tomada)->clave;
		return strings_equal(nombre_clave_tomada, clave);
	}
	//list_remove_and_destroy_by_condition(lista_claves_tomadas, clave_match, clave_tomada_destroyer);
	list_remove_by_condition(lista_claves_tomadas, clave_match);
	pthread_mutex_unlock(&sem_claves_tomadas);
	enviar_mensaje_planificador(cop_Coordinador_Clave_Liberada, size_of_string(clave), clave);
}

void liberar_claves_ESI(t_ESI * ESI) {
	bool ESI_match(void * item) {
		t_clave_tomada * clave_tomada  = (t_clave_tomada *) item;
		return clave_tomada->id_ESI == ESI->id_ESI;
	}
	t_list * lista_claves_liberadas = list_remove_all_by_condition(lista_claves_tomadas, ESI_match);

	void enviar_clave_liberda(void * item) {
		t_clave_tomada * clave_tomada  = (t_clave_tomada *) item;
		enviar_mensaje_planificador(cop_Coordinador_Clave_Liberada, size_of_string(clave_tomada->clave), clave_tomada->clave);
	}
	list_iterate(lista_claves_liberadas, enviar_clave_liberda);
	list_destroy_and_destroy_elements(lista_claves_liberadas, clave_tomada_destroyer);
}

void kill_ESI(t_ESI * ESI) {
	bool encontrar_esi(void* esi) {
		return ((t_ESI*)esi)->id_ESI == ESI->id_ESI;
	}
	list_remove_by_condition(lista_ESIs, encontrar_esi);
	liberar_claves_ESI(ESI);
	char id_ESI[12];
	sprintf(id_ESI, "%d", ESI->id_ESI);
	log_and_free(logger, string_concat(3, "ESI: ", id_ESI, " desconectado. \n"));
	free(ESI);
}

t_ESI * generar_ESI(un_socket socket, int ID) {
	t_ESI * ESI = malloc(sizeof(t_ESI));
	ESI->socket = socket;
	ESI->id_ESI = ID;
	return ESI;
}

bool validar_tamanio_clave(char* clave) {
	return strlen(clave) <= MAX_TAMANIO_CLAVE;
}

void error_clave_larga(t_ESI * ESI, char* operacion, char* clave) {
	char str[12];
	sprintf(str, "%d", MAX_TAMANIO_CLAVE);
	log_error_and_free(logger, string_concat(6, operacion, " rechazado. La clave '", clave, "' supera los ", str, " caracteres. \n"));
	notificar_resultado_instruccion(ESI, cop_Coordinador_Sentencia_Fallo_Clave_Larga, "");
}

void notificar_resultado_instruccion(t_ESI * ESI, int cop, char* parametro) {
	// Envio la informacion al Planificador
	int tamanio_buffer = sizeof(int) * 2 + size_of_string(parametro);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_int(buffer, &desplazamiento, ESI->id_ESI);
	serializar_string(buffer, &desplazamiento, parametro);
	enviar_mensaje_planificador(cop, tamanio_buffer, buffer);
	free(buffer);

	// Envio la informacion al ESI
	enviar(ESI->socket, cop, size_of_string(parametro), parametro);
}

void funcion_exit(int sig) {
	log_info(logger, "Abortando Coordinador.. \n");
	free(configuracion.PUERTO_ESCUCHA);
	free(configuracion.ALGORITMO_DISTRIBUCION);
	list_iterate(lista_instancias, liberar_instancia);
	list_destroy_and_destroy_elements(lista_claves_tomadas, clave_tomada_destroyer);

	for(int i = 0; i < 10;i++) {
		pthread_join(threads[i], NULL);
	}
	exit(0);
}

void liberar_instancia(t_instancia * instancia) {
	free(instancia->nombre);
	destruir_lista_strings(instancia->keys_contenidas);
	free(instancia);
}

void clave_tomada_destroyer(t_clave_tomada * clave_tomada) {
	free(clave_tomada->clave);
	free(clave_tomada);
}

void retardo() {
	// sleep(configuracion.RETARDO);
	usleep(configuracion.RETARDO * 1000);
}

t_ESI * get_ESI_por_id(int id_ESI) {
	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == id_ESI;
	}
	return list_find(lista_ESIs, encontrar_esi);
}

void enviar_mensaje_planificador(int cop, int tamanio_buffer, void * buffer) {
	pthread_mutex_lock(&sem_planificador);
	enviar(Planificador, cop, tamanio_buffer, buffer);
	pthread_mutex_unlock(&sem_planificador);
}

void handle_consulta_clave(char* clave) {
	log_and_free(logger, string_concat(3, "Consulta realizada, clave: '", clave, "'. \n"));
	char* valor;
	char* nombre_instancia_actual;
	t_instancia * instancia = instancia_a_guardar(clave);
	char* nombre_instancia_a_guardar = instancia == NULL ? "No disponible" : instancia->nombre;

	if (validar_clave_ingresada(clave)) {
		t_instancia * instancia = get_instancia_con_clave(clave);
		nombre_instancia_actual = instancia->nombre;
		valor = health_check(instancia) ? get(clave) : "No disponible";
	} else {
		valor = "Clave sin valor";
		nombre_instancia_actual = "-";
	}

	int tamanio_buffer = size_of_strings(4, clave, valor, nombre_instancia_actual, nombre_instancia_a_guardar);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_string(buffer, &desplazamiento, clave);
	serializar_string(buffer, &desplazamiento, valor);
	serializar_string(buffer, &desplazamiento, nombre_instancia_actual);
	serializar_string(buffer, &desplazamiento, nombre_instancia_a_guardar);
	pthread_mutex_lock(&sem_planificador);
	enviar(Planificador, cop_Planificador_Consultar_Clave, tamanio_buffer, buffer);
	pthread_mutex_unlock(&sem_planificador);
	free(buffer);
}

void handle_ESI_finalizado(t_ESI *  ESI) {
	char str[12];
	sprintf(str, "%d", ESI->id_ESI);
	log_and_free(logger, string_concat(3, "ESI ", str," finalizado\n"));

	bool ESI_match(void * item) {
		t_clave_tomada * clave_tomada  = (t_clave_tomada *) item;
		return clave_tomada->id_ESI == ESI->id_ESI;
	}
	t_list * lista_claves_liberadas = list_remove_all_by_condition(lista_claves_tomadas, ESI_match);
	int tamanio_buffer = size_of_list_of_strings_to_serialize(lista_claves_liberadas);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_lista_strings(buffer, &desplazamiento, lista_claves_liberadas);
	enviar_mensaje_planificador(cop_Coordinador_Claves_ESI_finalizado_Liberadas, tamanio_buffer, buffer);
	free(buffer);
	destruir_lista_strings(lista_claves_liberadas);
}

void enviar_claves_informacion_tomadas() {
	//Serializo lista_claves_tomadas Todo revisar si esta bien posicionado
	int tamanio_buffer = sizeof(int);
	void calcular_tamanio_buffer(void * item_clave_tomada){
		t_clave_tomada* clave_tomada = (t_clave_tomada*) item_clave_tomada;
		tamanio_buffer += sizeof(int) * 2; // ID ESI + INT para tamanio clave
		tamanio_buffer += size_of_string(clave_tomada->clave); // Tamanio clave
	}
	list_iterate(lista_claves_tomadas, calcular_tamanio_buffer);

	void* buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_int(buffer, &desplazamiento, list_size(lista_claves_tomadas));
	void serializar_clave_tomada(void * item_clave_tomada){
		t_clave_tomada* clave_tomada = (t_clave_tomada*) item_clave_tomada;
		serializar_int(buffer, &desplazamiento, clave_tomada->id_ESI);
		serializar_string(buffer, &desplazamiento, clave_tomada->clave);
	}
	list_iterate(lista_claves_tomadas,serializar_clave_tomada);
	enviar_mensaje_planificador(cop_Planificador_Analizar_Deadlocks, tamanio_buffer, buffer);
	free(buffer);
}
