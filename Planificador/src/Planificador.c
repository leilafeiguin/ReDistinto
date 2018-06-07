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
	accion_a_tomar = list_create();

	// Ejecutar consola
	pthread_t hilo_consola;
	pthread_create(&hilo_consola, NULL, hiloPlanificador_Consola, NULL);

	// Ejecutar hilo de planificacion
	pthread_t hilo_de_planificacion;
	pthread_create(&hilo_de_planificacion, NULL, planificar, NULL);

	ESI_ejecutando = malloc(sizeof(t_ESI));
	Ultimo_ESI_Ejecutado = malloc(sizeof(t_ESI));

	conectar_con_coordinador();

	pthread_mutex_lock(&mutex_ESI_ejecutando);

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
						/*case cop_Planificador_Ejecutar_Sentencia:
							buffer = malloc(sizeof(int));
							enviar(atoi(paqueteRecibido->data),cop_Planificador_Ejecutar_Sentencia,sizeof(int),buffer);
							free(buffer);
							break;*/
						case -1:
							//Hubo una desconexion
						{
							FD_CLR(socketActual, &master); //Elimina del master SET
							bool es_el_esi(void* esi){
								return ((t_ESI*)esi)->socket == socketActual;
							}
							t_ESI* esiListo = list_find(cola_de_listos, es_el_esi);
							bool es_el_esi_bloqueado(void* esi){
								return ((t_bloqueado*)esi)->ESI->socket == socketActual;
							}
							t_bloqueado* esiBloqueado = list_find(cola_de_bloqueados, es_el_esi_bloqueado);

							if(esiListo != NULL){
								log_info(logger, "Se desconectó un ESI listo. \n");
								esiListo->estado = finalizado;
								list_remove_by_condition(cola_de_listos, es_el_esi);
							}else if(esiBloqueado != NULL){
								esiBloqueado->ESI->estado = finalizado;
								log_info(logger, "Se desconectó un ESI bloqueado. \n");
								list_remove_by_condition(cola_de_bloqueados, es_el_esi_bloqueado);
							}else if(ESI_ejecutando->socket == socketActual){
								ESI_ejecutando->estado = finalizado;
								log_info(logger, "Se desconectó el ESI que estaba ejecutando. \n");
								free(ESI_ejecutando);
							}else{
								log_info(logger, "Se desconectó el coordinador. \n");
								exit(-1);
							}
							break;
						}
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
	configuracion.PUERTO_ESCUCHA = get_campo_config_string(archivo_configuracion, "PUERTO_ESCUCHA");
	configuracion.ALGORITMO_PLANIFICACION = get_campo_config_string(archivo_configuracion, "ALGORITMO_PLANIFICACION");
	configuracion.ALFA_PLANIFICACION = get_campo_config_int(archivo_configuracion, "ALFA_PLANIFICACION");
	configuracion.ESTIMACION_INICIAL = get_campo_config_int(archivo_configuracion, "ESTIMACION_INICIAL");
	configuracion.IP_COORDINADOR = get_campo_config_string(archivo_configuracion, "IP_COORDINADOR");
	configuracion.PUERTO_COORDINADOR = get_campo_config_string(archivo_configuracion, "PUERTO_COORDINADOR");
	configuracion.CLAVES_BLOQUEADAS = get_campo_config_string(archivo_configuracion, "CLAVES_BLOQUEADAS");
	return configuracion;
}

void salir(int motivo){
	pthread_mutex_lock(&mutex_lista_de_ESIs);
	list_destroy(lista_de_ESIs);
	pthread_mutex_lock(&mutex_cola_de_finalizados);
	list_destroy(cola_de_finalizados);
	pthread_mutex_lock(&mutex_cola_de_bloqueados);
	list_destroy(cola_de_bloqueados);
	pthread_mutex_lock(&mutex_cola_de_listos);
	list_destroy(cola_de_listos);
	pthread_mutex_lock(&mutex_accion_a_tomar);
	list_destroy(accion_a_tomar);
	free(ESI_ejecutando);
	exit(motivo);
}

void planificar(void* unused){
	log_info(logger, "Aguardando para planificar... \n");
	while(list_size(cola_de_listos) != 0 || list_size(cola_de_bloqueados) != 0){
		pthread_mutex_lock(&mutex_pausa_por_consola);
		log_info(logger, "Planificando \n");

		//Ordenamos la cola de listos segun el algoritmo.
		if( strcmp(configuracion.ALGORITMO_PLANIFICACION,"SJF-SD") == 0 ){
			ordenar_por_sjf_sd();
		}else if( strcmp(configuracion.ALGORITMO_PLANIFICACION,"SJF-CD") == 0 ){
			ordenar_por_sjf_cd();
		}else if( strcmp(configuracion.ALGORITMO_PLANIFICACION,"HRRN") == 0 ){
			ordenar_por_hrrn();
		}
		pasar_ESI_a_ejecutando(((t_ESI*) list_get(cola_de_listos,0))->id_ESI);
		ESI_ejecutando = list_get(cola_de_listos,0);

		//Todo revisar si este hilo puede comunicarse con el ESI.
		void* buffer = malloc(sizeof(int));
		strcpy(buffer,&ESI_ejecutando->socket);
		enviar(ESI_ejecutando->socket,cop_Planificador_Ejecutar_Sentencia,sizeof(int),buffer);
		free(buffer);
		pthread_mutex_unlock(&mutex_pausa_por_consola);

		//Espero respuesta del coordinador
		pthread_mutex_lock(&mutex_ESI_ejecutando);


		pthread_mutex_lock(&mutex_pausa_por_consola);
		void actualizarRafaga();
		Ultimo_ESI_Ejecutado = ESI_ejecutando;
		pthread_mutex_lock(&mutex_ESI_ejecutando);
		pthread_mutex_unlock(&mutex_pausa_por_consola);
	}
	puts("offf");
}

void* hiloPlanificador_Consola(void * unused){
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

			if (strcmp(linea, "Pausar") == 0) {
				log_info(logger, "Eligio la opcion Pausar\n");
				pthread_mutex_lock(&mutex_pausa_por_consola);
				if(parametros != NULL){
					ejecutarPausar(parametros);
				}
				free(linea);
			}else if (strcmp(linea, "Continuar") == 0) {
				log_info(logger, "Eligio la opcion Continuar\n");
				pthread_mutex_unlock(&mutex_pausa_por_consola);
				if(parametros != NULL){
					ejecutarContinuar(parametros);
				}
				free(linea);
			} else if (strcmp(primeraPalabra, "bloquear") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				parametros = validaCantParametrosComando(linea,2);
				if(parametros != NULL){
					ejecutarBloquear(parametros);
				}
				free(linea);
			} else if (strcmp(primeraPalabra, "desbloquear") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				parametros = validaCantParametrosComando(linea,1);
				if(parametros != NULL){
					ejecutarDesbloquear(parametros);
				}
				free(linea);
			} else if (strcmp(primeraPalabra, "listar") == 0) {
				log_info(logger, "Eligio la opcion Listar\n");
				parametros = validaCantParametrosComando(linea,1);
				if(parametros != NULL){
					ejecutarListar(parametros);
				}
				free(linea);
			} else if (strcmp(primeraPalabra, "kill") == 0) {
				log_info(logger, "Eligio la opcion Kill\n");
				parametros = validaCantParametrosComando(linea,1);
				if(parametros != NULL){
					ejecutarKill(parametros);
				}
				//Todo
				free(linea);
			} else if (strcmp(primeraPalabra, "status") == 0) {
				log_info(logger, "Eligio la opcion Status\n");
				parametros = validaCantParametrosComando(linea,1);
				if(parametros != NULL){
					ejecutarStatus(parametros);
				}
				//Todo
				free(linea);
			} else if (strcmp(linea, "deadlock") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				//Todo
				free(linea);
			} else {
				log_error(logger, "Opcion no valida.\n");
				printf("Opcion no valida.\n");
				free(linea);
			}
			free(lineaCopia);
			if (parametros != NULL)
				free(parametros);
		}
	}
}

void ejecutarPausar(char** parametros){
	if(&mutex_pausa_por_consola.__data == 0){
		log_info(logger, "Planificador ya estaba pausado\n");
	}else{
		pthread_mutex_lock(&mutex_pausa_por_consola);
		log_info(logger, "Pausamos planificador\n");
	}
}

void ejecutarContinuar(char** parametros){
	if(&mutex_pausa_por_consola.__data==0){
		pthread_mutex_unlock(&mutex_pausa_por_consola);
		log_info(logger, "Habilitamos para dar ordenes de ejecucion\n");
	}else{
		log_info(logger, "Planificacion habilitado para dar ordenes de ejecucion\n");
	}
}

void ejecutarKill(char** parametros){
	int idESI = atoi(parametros[1]);

	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == idESI;
	}
	t_ESI* esi = list_find(lista_de_ESIs, encontrar_esi);
	if(esi == NULL){
		log_info(logger, "El ESI no existe. \n");
	}

	void* buffer = malloc(sizeof(int));
	memcpy(buffer, &esi->socket, sizeof(int));
	enviar(esi->socket, cop_Planificador_kill_ESI, sizeof(int), buffer);

	pasar_ESI_a_finalizado(idESI, "Finalizado por consola"); //todo descripcion de estado
	free(buffer);
}

void ejecutarStatus(char** parametros){
	int clave = atoi(parametros[1]);

}

void ejecutarBloquear(char** parametros){
	//Se bloqueará el proceso ESI hasta ser desbloqueado, especificado por dicho <ID> en la cola del recurso <clave>.
	if(validar_ESI_id(atoi(parametros[1]))){
		pasar_ESI_a_bloqueado(atoi(parametros[2]),parametros[1],bloqueado_por_consola);
	}else{
		log_info(logger, "El ID de ESI ingresado es invalido\n");
	}
}

void ejecutarDesbloquear(char** parametros){
	//Se desbloqueara el proceso ESI con el ID especificado.
	bool encontrar_ESIs_bloqueados_por_consola(void* esi){
		return ((t_bloqueado*)esi)->motivo == bloqueado_por_consola;
	}

	t_list* lista_de_ESIs_bloqueados_por_consola = list_create();
	lista_de_ESIs_bloqueados_por_consola = list_filter(cola_de_bloqueados,encontrar_ESIs_bloqueados_por_consola);

	if(lista_de_ESIs_bloqueados_por_consola != NULL){
		bool encontrar_esi_por_clave(void* esi){
			return strcmp(((t_bloqueado*)esi)->clave_de_bloqueo,parametros[1]);
		}
		t_bloqueado* ESI_para_clave = list_find(lista_de_ESIs_bloqueados_por_consola,encontrar_esi_por_clave);
		if(ESI_para_clave != NULL){
			pasar_ESI_a_listo(ESI_para_clave->ESI->id_ESI);
		}else{
			log_info(logger, "No existe ESI bloqueado por la clave %s\n",parametros[1]);
		}
	}else{
		log_info(logger, "No existen ESIs bloqueados\n");
	}
	list_destroy(lista_de_ESIs_bloqueados_por_consola);
}

void ejecutarListar(char** parametros){
	//Lista los procesos encolados esperando al recurso.
	bool encontrar_ESIs_por_clave_de_bloqueo(void* esi){
		return strcmp(((t_bloqueado*)esi)->clave_de_bloqueo,parametros[1]);
	}

	t_list* lista_de_ESIs_por_clave_de_bloqueo = list_create();
	lista_de_ESIs_por_clave_de_bloqueo = list_filter(cola_de_bloqueados,encontrar_ESIs_por_clave_de_bloqueo);
	if(lista_de_ESIs_por_clave_de_bloqueo != NULL){
		int acum = 0;
		void mostrar_id_de_esi_bloqueado(void* esi){
			log_info(logger, "%i - ID: %s\n",acum,((t_bloqueado*)esi)->ESI->id_ESI);
		}
		log_info(logger, "ESIs bloqueados por: %s\n",parametros[1]);
		list_iterate(lista_de_ESIs_por_clave_de_bloqueo,mostrar_id_de_esi_bloqueado);
	}else{
		log_info(logger, "No hay ESIs esperando por la clave %s\n",parametros[1]);
	}
	list_destroy(lista_de_ESIs_por_clave_de_bloqueo);
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

void pasar_ESI_a_bloqueado(int id_ESI, char* clave_de_bloqueo, int motivo){
	//Se debe considerar los posibles estandos en los que puede estar el ESI

	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == id_ESI;
	}

	t_ESI* esi = list_find(lista_de_ESIs, encontrar_esi);

	switch(esi->estado){
		case listo:
		{
			esi->estado = bloqueado;
			//Todo modificar descripcion de estado?

			t_bloqueado* esi_bloqueado = malloc(sizeof(t_bloqueado));
			esi_bloqueado->ESI = esi;
			esi_bloqueado->clave_de_bloqueo = malloc(strlen(clave_de_bloqueo)+1);
			strcpy(clave_de_bloqueo,esi_bloqueado->clave_de_bloqueo);
			esi_bloqueado->motivo = motivo;
			list_remove_by_condition(cola_de_listos,encontrar_esi);
			list_add(cola_de_bloqueados,esi_bloqueado);
			free(esi_bloqueado);
			free(clave_de_bloqueo);
		}
		break;
		case ejecutando:
		{
			t_accion_a_tomar* esi_accion_a_tomar = malloc(sizeof(t_accion_a_tomar));
			esi_accion_a_tomar->ESI = esi;
			esi_accion_a_tomar->ESI->estado = bloqueado;
			esi_accion_a_tomar->accion_a_tomar = bloquear;
			esi_accion_a_tomar->clave_de_bloqueo = malloc(strlen(clave_de_bloqueo)+1);
			strcpy(clave_de_bloqueo,esi_accion_a_tomar->clave_de_bloqueo);
			esi_accion_a_tomar->motivo = motivo;
			free(esi_accion_a_tomar);
			free(clave_de_bloqueo);
		}
		break;
	}

	free(esi);
}

void pasar_ESI_a_finalizado(int id_ESI, char* descripcion_estado){
	//Se debe considerar los posibles estandos en los que puede estar el ESI

	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == id_ESI;
	}

	bool encontrar_esi_bloqueado(void* esi){
		return ((t_bloqueado*)esi)->ESI->id_ESI == id_ESI;
	}

	t_ESI* esi = list_find(lista_de_ESIs, encontrar_esi);
	pthread_mutex_lock(&mutex_cola_de_finalizados);
	switch(esi->estado){
		case listo:
		{
			pthread_mutex_lock(&mutex_cola_de_listos);
			esi->descripcion_estado = malloc(strlen(descripcion_estado));
			strcpy(esi->descripcion_estado,descripcion_estado);
			esi->estado = finalizado;
			list_remove_by_condition(cola_de_listos,encontrar_esi);
			list_add(cola_de_finalizados,esi);
			pthread_mutex_unlock(&mutex_cola_de_listos);
		}
		break;
		case ejecutando:
		{
			pthread_mutex_lock(&mutex_ESI_ejecutando);
			ESI_ejecutando->estado = finalizado;
			ESI_ejecutando->descripcion_estado = malloc(strlen(descripcion_estado)+1);
			strcpy(ESI_ejecutando->descripcion_estado,descripcion_estado);
			list_add(cola_de_finalizados,ESI_ejecutando);
			//Todo liberar ESI Ejecutando
			pthread_mutex_unlock(&mutex_ESI_ejecutando);
		}
		break;
		case bloqueado:
		{
			pthread_mutex_lock(&mutex_cola_de_bloqueados);
			esi->estado = finalizado;
			esi->descripcion_estado = malloc(strlen(descripcion_estado)+1);
			strcpy(esi->descripcion_estado,descripcion_estado);
			list_remove_by_condition(cola_de_bloqueados,encontrar_esi_bloqueado);
			list_add(cola_de_finalizados,esi);
			pthread_mutex_unlock(&mutex_cola_de_bloqueados);
		}
		break;
	}
	pthread_mutex_unlock(&mutex_cola_de_finalizados);
	free(esi);
}

void pasar_ESI_a_listo(int id_ESI){
	//Se debe considerar los posibles estandos en los que puede estar el ESI

	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == id_ESI;
	}

	bool encontrar_esi_bloqueado(void* esi){
		return ((t_bloqueado*)esi)->ESI->id_ESI == id_ESI;
	}

	t_ESI* esi = list_find(lista_de_ESIs, encontrar_esi);

	switch(esi->estado){
		case ejecutando:
		{
			pthread_mutex_lock(&mutex_ESI_ejecutando);
			pthread_mutex_lock(&mutex_cola_de_listos);
			ESI_ejecutando->estado = listo;
			//Librar ESI Ejecutando
			list_add(cola_de_listos,ESI_ejecutando);
			pthread_mutex_unlock(&mutex_cola_de_listos);
			pthread_mutex_unlock(&mutex_ESI_ejecutando);
		}
		break;
		case bloqueado:
		{
			pthread_mutex_lock(&mutex_cola_de_bloqueados);
			esi->estado = listo;
			list_remove_by_condition(cola_de_bloqueados,encontrar_esi_bloqueado);
			list_add(cola_de_listos,esi);
			pthread_mutex_unlock(&mutex_cola_de_bloqueados);
		}
		break;
	}

	free(esi);
}

void pasar_ESI_a_ejecutando(int id_ESI){
	//Se debe considerar los posibles estandos en los que puede estar el ESI

	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == id_ESI;
	}

	bool encontrar_esi_bloqueado(void* esi){
		return ((t_bloqueado*)esi)->ESI->id_ESI == id_ESI;
	}

	pthread_mutex_lock(&mutex_ESI_ejecutando);

	t_ESI* esi = list_find(lista_de_ESIs, encontrar_esi);
	esi->w = 0;

	switch(esi->estado){
		case listo:
		{
			pthread_mutex_lock(&mutex_cola_de_listos);
			esi->estado = ejecutando;
			list_remove_by_condition(cola_de_listos, encontrar_esi);
			ESI_ejecutando = esi;
			pthread_mutex_unlock(&mutex_cola_de_listos);
		}
		break;
		case bloqueado:
		{
			pthread_mutex_lock(&mutex_cola_de_bloqueados);
			esi->estado = ejecutando;
			list_remove_by_condition(cola_de_bloqueados, encontrar_esi_bloqueado);
			ESI_ejecutando = esi;
			pthread_mutex_unlock(&mutex_cola_de_bloqueados);
		}
		break;
	}
	pthread_mutex_unlock(&mutex_ESI_ejecutando);

	pthread_mutex_lock(&mutex_cola_de_listos);

	void aumentarW(void* elem){
		((t_ESI*)elem)->w ++;
	}
	list_iterate(cola_de_listos, aumentarW);

	pthread_mutex_unlock(&mutex_cola_de_listos);

	free(esi);
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

void ordenar_por_sjf_sd(){
	pthread_mutex_lock(&mutex_cola_de_listos);

	bool sjf(void* esi1, void* esi2){
		return estimarRafaga(((t_ESI*)esi1)->id_ESI) < estimarRafaga(((t_ESI*)esi2)->id_ESI);
	}
	list_sort(cola_de_listos,sjf);

	bool es_ultimo_ejecutado(void* esi){
		return ((t_ESI*)esi)->id_ESI == Ultimo_ESI_Ejecutado->id_ESI;
	}
	if( list_find(cola_de_listos,es_ultimo_ejecutado) != NULL && Ultimo_ESI_Ejecutado->id_ESI != ((t_ESI*)list_get(cola_de_listos,0))->id_ESI ){
		list_remove_by_condition(cola_de_listos,es_ultimo_ejecutado);
		list_add_in_index(cola_de_listos,0,es_ultimo_ejecutado);
	}

	pthread_mutex_unlock(&mutex_cola_de_listos);
}

void ordenar_por_sjf_cd(){
	pthread_mutex_lock(&mutex_cola_de_listos);

	bool sjf(void* esi1, void* esi2){
		return estimarRafaga(((t_ESI*)esi1)->id_ESI) < estimarRafaga(((t_ESI*)esi2)->id_ESI);
	}
	list_sort(cola_de_listos,sjf);

	pthread_mutex_unlock(&mutex_cola_de_listos);
}

void ordenar_por_hrrn(){
	pthread_mutex_lock(&mutex_cola_de_listos);

	bool hrrn(void* esi1, void* esi2){
		float responseRatio1 = (estimarRafaga(((t_ESI*)esi1)->id_ESI) + ((t_ESI*)esi1)->w) / estimarRafaga(((t_ESI*)esi1)->id_ESI);
		float responseRatio2 = (estimarRafaga(((t_ESI*)esi2)->id_ESI) + ((t_ESI*)esi2)->w) / estimarRafaga(((t_ESI*)esi2)->id_ESI);
		return responseRatio1 > responseRatio2;
	}
	list_sort(cola_de_listos,hrrn);

	pthread_mutex_unlock(&mutex_cola_de_listos);
}


float estimarRafaga(int id_ESI){
	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == id_ESI;
	}

	t_ESI* esi = list_find(lista_de_ESIs, encontrar_esi);
	int tn = esi->duracionRafaga; //Duracion de la rafaga anterior
	float Tn = esi->estimacionUltimaRafaga; // Estimacion anterior
	float estimacion = (configuracion.ALFA_PLANIFICACION / 100)* tn + (1 - (configuracion.ALFA_PLANIFICACION / 100))* Tn;
	esi->estimacionUltimaRafaga = estimacion;
	return estimacion;
}

void actualizarRafaga(){
	if(Ultimo_ESI_Ejecutado == ESI_ejecutando){
		ESI_ejecutando->duracionRafaga += 1;
	}else{
		Ultimo_ESI_Ejecutado->duracionRafaga = 0;
	}
}

t_ESI* esi_por_id(int id_ESI){
	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == id_ESI;
	}

	t_ESI* esi = list_find(lista_de_ESIs, encontrar_esi);
	return esi;
}

void liberarESI(){

}

void conectar_con_coordinador() {
	Coordinador = conectar_a(configuracion.IP_COORDINADOR,configuracion.PUERTO_COORDINADOR);
	realizar_handshake(Coordinador, cop_handshake_Planificador_Coordinador);
	enviar(Coordinador,cop_generico,size_of_string(""),"");
	log_info(logger, "Me conecte con el Coordinador. \n");
	void * argumentos;
	pthread_t hilo_coordinador;
	pthread_create(&hilo_coordinador, NULL, escuchar_coordinador, argumentos);
}

void * escuchar_coordinador(void * argumentos) {
	bool escuhar = true;
	while(escuhar) {
		log_info(logger, "Aguardando al coordinador... \n");
		t_paquete* paqueteRecibido = recibir(Coordinador);
		t_ESI* esi = esi_por_id(atoi(paqueteRecibido->data));

		switch(paqueteRecibido->codigo_operacion) {
		 case codigo_error:
			 log_info(logger, "Error en el Coordinador. Abortando. \n");
			 escuhar = false;
		 break;

		 case cop_Coordinador_Sentencia_Exito:
			pthread_mutex_lock(&mutex_ESI_ejecutando);
			ESI_ejecutando->cantidad_instrucciones --;

			if(ESI_ejecutando->cantidad_instrucciones == 0){
				pasar_ESI_a_finalizado(ESI_ejecutando->id_ESI, "Finalizo correctamente");
			}else{
				pasar_ESI_a_listo(ESI_ejecutando->id_ESI);
			}
			pthread_mutex_unlock(&mutex_ESI_ejecutando);
			pthread_mutex_unlock(&mutex_ESI_ejecutando);
			break;
		 case cop_Coordinador_Sentencia_Fallo_Clave_Tomada:
			 enviar(esi->socket,cop_Planificador_kill_ESI,sizeof(int),paqueteRecibido->data);
			 //Matar al ESI
			break;

		 case cop_Coordinador_Sentencia_Exito_Clave_Sin_Valor:
			enviar(esi->socket,cop_Planificador_kill_ESI,sizeof(int),paqueteRecibido->data);
			//Matar al ESI
			break;
		case cop_Coordinador_Sentencia_Fallo_No_Instancias:
			enviar(esi->socket,cop_Planificador_kill_ESI,sizeof(int),paqueteRecibido->data);
			//Matar al ESI
			break;
		case cop_Coordinador_Sentencia_Fallo_Clave_Larga:
			enviar(esi->socket,cop_Planificador_kill_ESI,sizeof(int),paqueteRecibido->data);
			//Matar al ESI
			break;
		}
	}
	pthread_detach(pthread_self());
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
	int id_nuevo_ESI = nuevo_ESI(socketESI, cantidad_instrucciones);
	serializar_int(buffer, &desp, id_nuevo_ESI);
	enviar(socketESI, cop_handshake_Planificador_ESI, tamanio_buffer, buffer);
	free(buffer);
}

int nuevo_ESI(un_socket socket, int cantidad_instrucciones) {
	//Todo actualizar estructuras necesarias con datos del ESI
	t_ESI* newESI = malloc(sizeof(t_ESI));
	newESI->estimacionUltimaRafaga = configuracion.ESTIMACION_INICIAL;
	newESI->estado = 0;
	newESI->socket = socket;
	newESI->id_ESI = idESI;
	newESI->cantidad_instrucciones = cantidad_instrucciones;
	newESI->duracionRafaga = 0;
	list_add(lista_de_ESIs,newESI);
	list_add(cola_de_listos,newESI);
	idESI++;
	return newESI->id_ESI;
}


