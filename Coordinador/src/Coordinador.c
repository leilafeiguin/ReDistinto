#include <stdio.h>
#include <stdlib.h>
#include "Coordinador.h"

void* archivo;
t_log* logger;
int maxEntradas = -1; //Equitative Load
char * nombreInstanciaEnUso = ""; //Equitative Load

int main(void) {
	imprimir("/home/utnso/workspace/tp-2018-1c-PuntoZip/Coordinador/coord_image.txt");
	iniciar_logger();
	log_info(logger, "Inicializando proceso Coordinador. \n");

	lista_instancias = list_create();
	lista_claves_tomadas = list_create();
	new_list_instancias_organized = list_create();

	configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

	iniciar_servidor();
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

void iniciar_logger() {
	char* fileLog;
	fileLog = "coordinador_logs.txt";
	logger = log_create(fileLog, "Coordinador Logs", 1, 1);
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
	esperar_handshake(planificador, paquetePlanificador,cop_handshake_Planificador_Coordinador);
	log_info(logger, "Realice handshake con Planificador \n");
	t_paquete* paqueteRecibido = recibir(planificador); // Info sobre el Planificador
	// liberar_paquete(paqueteRecibido);
	t_list* thread_params;
	nuevo_hilo(planificador_conectado_funcion_thread, thread_params);
}

void handle_instancia(un_socket instancia, t_paquete* paqueteInstancia) {
	esperar_handshake(instancia, paqueteInstancia,cop_handshake_Instancia_Coordinador);
	t_paquete* paqueteRecibido = recibir(instancia); // Info sobre la Instancia
	char* nombre_instancia = copy_string(paqueteRecibido->data);
	t_list* thread_params = list_create();
	list_add(thread_params, instancia);
	list_add(thread_params, nombre_instancia);
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
		// liberar_paquete(paqueteRecibido);
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
	return list_filter(lista_instancias, instancia_activa);
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
		printf("Aguardando al ESI %d.. \n", ESI->id_ESI);
		t_paquete* paqueteRecibido = recibir(ESI->socket);
		switch(paqueteRecibido->codigo_operacion) {
			case codigo_error:
				kill_ESI(ESI);
				free(ESI);
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
		puts("Aguardando al Planificador..");
		t_paquete* paqueteRecibido = recibir(Planificador);
		switch(paqueteRecibido->codigo_operacion) {
			case codigo_error:
				puts("Error en el Planificador. Abortando Planificador.");
				Planificador = codigo_error;
				escuchar = false;
			break;
		}
		liberar_paquete(paqueteRecibido);
	}
}

void instancia_conectada(un_socket socket_instancia, char* nombre_instancia) {
	bool instancia_ya_existente(t_instancia * ins){
		return strcmp(ins->nombre, nombre_instancia) == 0 ? true : false;
	}
	t_instancia * instancia = list_find(lista_instancias, instancia_ya_existente);
	bool instancia_nueva = instancia == NULL;
	int codigo_respuesta = cop_Instancia_Nueva;
	if (instancia_nueva) { // Si es una instancia nueva
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

	if (!instancia_nueva) {
		enviar_listado_de_strings(instancia->socket, instancia->keys_contenidas);
	}

	// BORRAR PROXIMAMENTE: Para probar las funciones
	/* if (instancia_nueva) {
		ejecutar_set("nombre", "tomas uriel chejanovich");
		ejecutar_get("nombre");
		ejecutar_store("nombre");
		ejecutar_set("Futbolista 1", "Cristiano ronaldo");
		ejecutar_set("Futbolista 2", "Carlos tevez");
		ejecutar_set("Futbolista 3", "Ronaldo asis moreira junior");
		ejecutar_set("nombre2", "tomas uriel chejanovich");
		dump();
	}*/
}

int ejecutar_set(t_ESI * ESI, char* clave, char* valor) {
	printf("Ejecutando SET '%s' : '%s' \n", clave, valor);
	if (!validar_tamanio_clave(clave)) {
		error_clave_larga(ESI, "SET", clave);
		return 0;
	}

	int id_ESI_con_clave = get_id_ESI_con_clave(clave);
	if (id_ESI_con_clave != NULL && id_ESI_con_clave != ESI->id_ESI ) {
		printf("SET rechazado. La clave '%s' se encuentra tomada por otro ESI. \n", clave);
		enviar(ESI->socket, cop_Coordinador_Sentencia_Fallo_Clave_Tomada, size_of_string(""), "");
		return 0;
	}

	bool inserted = false;
	while(!inserted) {
		t_instancia * instancia = instancia_a_guardar();
		if (instancia == NULL) {
			log_info(logger, "ERROR: No pudo ejecutarse el SET. No hay instancias disponibles. \n ");
			enviar(ESI->socket, cop_Coordinador_Sentencia_Fallo_No_Instancias, size_of_string(""), "");
			return 0;
		}
		if (health_check(instancia)) {
			validar_necesidad_compactacion(instancia, clave, valor);
			setear(instancia, clave, valor);
			actualizar_keys_contenidas(instancia);
			actualizar_cantidad_entradas_ocupadas(instancia);
			log_and_free(logger, string_concat(5, "SET '", clave, "':'", valor, "' \n"));
			inserted = true;
			enviar(ESI->socket, cop_Coordinador_Sentencia_Exito, size_of_string(""), "");
		} else {
			log_and_free(logger, string_concat(2, instancia->nombre, " no disponible. \n"));
		}
	}
	return 1;
}

int validar_necesidad_compactacion(t_instancia * instancia, char* clave, char* valor) {
	int tamanio_buffer = size_of_strings(2, clave, valor);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_string(buffer, &desplazamiento, clave);
	serializar_string(buffer, &desplazamiento, valor);
	enviar(instancia->socket, cop_Instancia_Necesidad_Compactacion, tamanio_buffer, buffer);
	free(buffer);
	t_paquete* paqueteResultado = recibir(instancia->socket);
	if (paqueteResultado->codigo_operacion == cop_Instancia_Necesidad_Compactacion_True) { // Es necesario compactar
		ejecutar_compactacion();
	}
	liberar_paquete(paqueteResultado);
	return 0;
}

void ejecutar_compactacion() {
	log_info(logger, "Ejecutando compactacion de instancias \n ");
	int cantidad_compactaciones_ejecutadas = 0;
	t_list* list_instancias_activas = instancias_activas();
	void enviar_mensaje_compactacion(t_instancia * instancia) {
		enviar(instancia->socket, cop_Instancia_Ejecutar_Compactacion, size_of_string(""), "");
		t_paquete* paquete = recibir(instancia->socket);
		if (paquete->codigo_operacion == cop_Instancia_Ejecucion_Exito) {
			cantidad_compactaciones_ejecutadas++;
		}
		if (cantidad_compactaciones_ejecutadas == list_size(list_instancias_activas)) {
			// Signal instancias
		}
		liberar_paquete(paquete);
	}
	list_iterate(list_instancias_activas, enviar_mensaje_compactacion);
	list_destroy(list_instancias_activas);
}

int setear(t_instancia * instancia, char* clave, char* valor) {
	list_add(instancia->keys_contenidas, clave); // Registro que esta instancia contendra la clave especificada
	int tamanio_buffer = size_of_strings(2, clave, valor);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_string(buffer, &desplazamiento, clave);
	serializar_string(buffer, &desplazamiento, valor);
	enviar(instancia->socket, cop_Instancia_Ejecutar_Set, tamanio_buffer, buffer);
	free(buffer);
	return 0;
}

int actualizar_keys_contenidas(t_instancia * instancia) {
	list_destroy(instancia->keys_contenidas);
	instancia->keys_contenidas = recibir_listado_de_strings(instancia->socket);
	return 0;
}

int actualizar_cantidad_entradas_ocupadas(t_instancia * instancia) {
	t_paquete* paqueteCantidadEntradasOcupadas = recibir(instancia->socket); // Recibe la cantidad de entradas
	instancia->cant_entradas_ocupadas = atoi(paqueteCantidadEntradasOcupadas->data);
	liberar_paquete(paqueteCantidadEntradasOcupadas);
	return 0;
}

int ejecutar_get(t_ESI * ESI, char* clave) {
	printf("Ejecutando GET:%s \n", clave);
	if (!validar_tamanio_clave(clave)) {
		error_clave_larga(ESI, "GET", clave);
		return 0;
	}

	int id_ESI_con_clave = get_id_ESI_con_clave(clave);
	if (id_ESI_con_clave != NULL && id_ESI_con_clave != ESI->id_ESI ) {
		printf("GET rechazado. La clave '%s' se encuentra tomada por otro ESI. \n", clave);
		enviar(ESI->socket, cop_Coordinador_Sentencia_Fallo_Clave_Tomada, size_of_string(""), "");
		return 0;
	}


	if (validar_clave_ingresada(clave)) {
		t_instancia * instancia = get_instancia_con_clave(clave);
		if (instancia == NULL || !health_check(instancia)) {
			log_info(logger, "ERROR: GET rechazado. La instancia no se encuentra disponible. \n");
			enviar(ESI->socket, cop_Coordinador_Sentencia_Fallo_No_Instancias, size_of_string(""), "");
			return 0;
		}
		char* valor = get(clave);
		printf("GET ejecutado con exito. El valor de la clave '%s' es '%s'. \n", clave, valor);
		enviar(ESI->socket, cop_Coordinador_Sentencia_Exito, size_of_string(valor), valor);
		free(valor);
	} else {
		nueva_clave_tomada(ESI, clave);
		printf("GET ejecutado con exito. La clave '%s' todavia no tiene ningun valor. \n", clave);
		enviar(ESI->socket, cop_Coordinador_Sentencia_Exito_Clave_Sin_Valor, size_of_string(""), "");
	}

	return 1;
}

int get_id_ESI_con_clave(char* clave) {
	bool clave_match(t_clave_tomada * clave_comparar){
		return strcmp(clave, clave_comparar->clave) == 0 ? true : false;
	}
	t_clave_tomada * t_clave = list_find(lista_claves_tomadas, clave_match);
	return t_clave == NULL ? NULL : t_clave->id_ESI;
}

bool validar_clave_ingresada(char* clave) {
	bool clave_match(char * key){
		return strcmp(clave, key) == 0 ? true : false;
	}
	bool instancia_tiene_clave(t_instancia * instancia){
		return list_any_satisfy(instancia->keys_contenidas, clave_match);
	}
	return list_any_satisfy(lista_instancias, instancia_tiene_clave);
}

t_clave_tomada * nueva_clave_tomada(t_ESI * ESI, char* clave) {
	t_clave_tomada * t_clave = malloc(sizeof(t_clave_tomada));
	t_clave->id_ESI = ESI->id_ESI;
	t_clave->clave = copy_string(clave);
	list_add(lista_claves_tomadas, t_clave);
	return t_clave;
}

char* get(char* clave) {
	t_instancia * instancia = get_instancia_con_clave(clave);
	if (health_check(instancia)) {
		enviar(instancia->socket, cop_Instancia_Ejecutar_Get, size_of_string(clave), clave); // Envia a la instancia la clave
		t_paquete* paqueteValor = recibir(instancia->socket); // Recibe el valor solicitado
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
	if (id_ESI_con_clave != NULL && id_ESI_con_clave != ESI->id_ESI ) {
		log_and_free(logger, string_concat(3, "ERROR: STORE rechazado. La clave '", clave , "' se encuentra tomada por otro ESI \n"));
		enviar(ESI->socket, cop_Coordinador_Sentencia_Fallo_Clave_Tomada, size_of_string(""), "");
		return 0;
	}

	liberar_clave_tomada(clave);
	t_instancia * instancia = get_instancia_con_clave(clave);
	if (instancia != NULL && health_check(instancia)) {
		enviar(instancia->socket, cop_Instancia_Ejecutar_Store, size_of_string(clave), clave); // Envia a la instancia la clave
		t_paquete* paqueteEstadoOperacion = recibir(instancia->socket); // Aguarda a que la instancia le comunique que el STORE se ejectuo de forma exitosa
		int estado_operacion = paqueteEstadoOperacion->codigo_operacion;
		liberar_paquete(paqueteEstadoOperacion);
		if (estado_operacion == cop_Instancia_Ejecucion_Exito) {
			log_and_free(logger, string_concat(3, "STORE ejecutado con exito. La clave '", clave, "' fue guardada y liberada. \n"));
			enviar(ESI->socket, cop_Coordinador_Sentencia_Exito, size_of_string(""), "");
		}
	} else {
		log_info(logger, "ERROR: STORE rechazado. La instancia no se encuentra disponible. Recurso liberada pero no guardado. \n");
		enviar(ESI->socket, cop_Coordinador_Sentencia_Fallo_No_Instancias, size_of_string(""), "");
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
		enviar(instancia->socket, cop_Instancia_Ejecutar_Dump, size_of_string(""), "");
		t_paquete* paqueteEstadoOperacion = recibir(instancia->socket); // Aguarda a que la instancia le comunique que el STORE se ejectuo de forma exitosa
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
	enviar(instancia->socket, codigo_healthcheck, size_of_string(""), ""); // Envio el request de healthcheck
	t_paquete* paqueteRecibido = recibir(instancia->socket); // Aguardo a recbir el OK de la instancia
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
	return list_find(lista_instancias, instancia_tiene_clave);
}

t_instancia * instancia_a_guardar() {
	t_list* list_instancias_activas = instancias_activas();
	t_instancia* result = list_get(list_instancias_activas, 0); // TODO: Utilizar algoritmo correspondiente
	list_destroy(list_instancias_activas);
	return result;
}

t_instancia * crear_instancia(un_socket socket, char* nombre) {
	// Creo la estructura de la instancia y la agrego a la lista
	t_instancia * instancia_nueva = malloc(sizeof(t_instancia));
	instancia_nueva->socket = socket;
	instancia_nueva->nombre = nombre;
	instancia_nueva->estado = conectada;
	instancia_nueva->cant_entradas_ocupadas = 0; // Contador Cantidad de entradas
	instancia_nueva->keys_contenidas = list_create();
	list_add(lista_instancias, instancia_nueva);
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

void * equitative_load(t_instancia * lista, int cant_entradas) {
	void incrementar_entrada(t_instancia * element) {
		(element)->cant_entradas_ocupadas += cant_entradas;
	}

	void show_cant_entradas(t_instancia * element) {
		printf("%i", (element)->cant_entradas_ocupadas);
		printf((element)->nombre);
	}

	incrementar_entrada(list_get(lista, 0));
	list_take_and_remove(new_list_instancias_organized, list_size(new_list_instancias_organized));
	list_add_all(new_list_instancias_organized, lista);
	list_remove(new_list_instancias_organized, 0);
	list_add(new_list_instancias_organized, list_get(lista, 0));
	list_take_and_remove(lista, list_size(lista));
	list_add_all(lista, new_list_instancias_organized);

}

void * least_space_used(t_instancia * lista, int espacio_entradas) {
	int i = 0;
	int menorEspacioInstancia = -1;
	t_instancia * instanciaConMayorEspacioDisponible = list_get(lista, 0);

	int getEspacio(t_instancia * element) {
		return (element)->cant_entradas_ocupadas;
	}

	void instancia_mas_vacia() {
		if (menorEspacioInstancia == -1 || menorEspacioInstancia > getEspacio(list_get(lista, i)))
		{
			menorEspacioInstancia = getEspacio(list_get(lista, i));
			instanciaConMayorEspacioDisponible = list_get(lista, i);
		}
		i++;
	}

	list_iterate(lista, instancia_mas_vacia);
	(instanciaConMayorEspacioDisponible)->cant_entradas_ocupadas += espacio_entradas;
	printf((instanciaConMayorEspacioDisponible)->nombre);
	printf("%i", menorEspacioInstancia);
}

void * key_explicit(t_instancia * lista, char clave[], int espacio_entradas) {

	void incrementar_entrada(t_instancia * element) {
		(element)->cant_entradas_ocupadas += espacio_entradas;
	}

	char primeraLetra = tolower(clave[0]);
	int letras = 26;
	int cantInstancias = list_size(lista);
	int resto_ultimo = 0;
	int resto_inicial = letras%cantInstancias;
	int cantidad_letras_x_instancia = letras/cantInstancias;

	if(resto_inicial) {
		cantidad_letras_x_instancia ++;
		resto_ultimo = (cantidad_letras_x_instancia * cantInstancias) - letras;
	}

	int valorLetra = primeraLetra;
	int cont = 0;

	for (int i = 97; i < 97+letras-1; i+cantidad_letras_x_instancia) {
		if(valorLetra >= i && valorLetra < i+cantidad_letras_x_instancia) {
			incrementar_entrada(list_get(lista, cont));
		}
		i += cantidad_letras_x_instancia;
		cont ++;
	}

	//printf("%i", 'a'); //ESTO ES IGUAL A 97
	//printf("%i", 'z'); //ESTO ES IGUAL A 122
}

void liberar_clave_tomada(char* clave) {
	t_list * nueva_lista = list_create();
	void add_clave_si_es_distinta(t_clave_tomada * clave_tomada){
		if (strcmp(clave, clave_tomada->clave) == 0) {
			free(clave_tomada->clave);
		} else {
			list_add(nueva_lista, clave_tomada->clave);
		}
	}
	list_iterate(lista_claves_tomadas, add_clave_si_es_distinta);
	list_destroy(lista_claves_tomadas);
	lista_claves_tomadas = nueva_lista;
}

void liberar_claves_ESI(t_ESI * ESI) {
	bool ESI_match(t_clave_tomada * clave_tomada){
		return clave_tomada->id_ESI == ESI->id_ESI ? true : false;
	}
	void eliminar_clave_tomada(t_clave_tomada * clave_tomada){
		liberar_clave_tomada(clave_tomada->clave);
	}
	t_list * claves_del_ESI = list_filter(lista_claves_tomadas, ESI_match);
	list_iterate(claves_del_ESI, eliminar_clave_tomada);
	list_destroy(claves_del_ESI);
}

void kill_ESI(t_ESI * ESI) {
	liberar_claves_ESI(ESI);
	printf("Error en el ESI: %d. Abortando ESI. \n", ESI->id_ESI);
}

t_ESI * generar_ESI(un_socket socket, int ID) {
	t_ESI * ESI = malloc(sizeof(t_ESI));
	ESI->socket = socket;
	ESI->id_ESI = ID;
	return ESI;
}

bool validar_tamanio_clave(char* clave) {
	return strlen(clave) <= 40;
}

void error_clave_larga(t_ESI * ESI, char* operacion, char* clave) {
	printf("%s rechazado. La clave '%s' supera los 40 caracteres. \n", operacion, clave);
	enviar(ESI->socket, cop_Coordinador_Sentencia_Fallo_Clave_Larga, size_of_string(""), "");
}

// !ALGORITMOS DE DISTRIBUCION

// ALGORITMOS DE REEMPLAZO

void * circular(t_instancia * lista, int cant_entradas) {

}

// !ALGORITMOS DE REEMPLAZO

void * crear_instancias_prueba_alan() {

	void show_cant_entradas(t_instancia * element) {
		printf("%i", (element)->cant_entradas_ocupadas);
		printf((element)->nombre);
	}

	crear_instancia(3, " Alan\n");
	crear_instancia(4, " Cheja\n");
	crear_instancia(3, " Marco\n");
//	equitative_load(lista_instancias, 1);
//	equitative_load(lista_instancias, 3);
//	equitative_load(lista_instancias, 2);
//	least_space_used(lista_instancias, 5);
//	least_space_used(lista_instancias, 5);
//	least_space_used(lista_instancias, 5);
//	least_space_used(lista_instancias, 5);
//	key_explicit(lista_instancias, "Alberto\n", 5);
//	key_explicit(lista_instancias, "Ignacio\n", 5);
//	key_explicit(lista_instancias, "Javier\n", 5);
//	key_explicit(lista_instancias, "Marcelo\n", 5);
//	key_explicit(lista_instancias, "Ximenez\n", 5);
//	crear_instancia(3, " Leila\n");
//	key_explicit(lista_instancias, "Ignacio\n", 33);
//	key_explicit(lista_instancias, "Ximenez\n", 22);
	circular(lista_instancias, 8);

	list_iterate(lista_instancias, show_cant_entradas);
}

