#include <stdio.h>
#include <stdlib.h>
#include "Planificador.h"

void* archivo;
t_log* logger;

int main(void) {
	char* fileLog;
	fileLog = "planificador_logs.txt";

	logger = log_create(fileLog, "Planificador Logs", 1, 1);
	log_info(logger, "Inicializando proceso Planificador. \n");

	planificador_configuracion configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

	lista_de_ESIs = list_create();
	cola_de_listos = list_create();
	cola_de_bloqueados = list_create();
	cola_de_finalizados = list_create();
	accion_a_tomar = list_create();

	iniciarConsolaPlanificador();

	un_socket Coordinador = conectar_a(configuracion.IP_COORDINADOR,configuracion.PUERTO_COORDINADOR);
	realizar_handshake(Coordinador, cop_handshake_Planificador_Coordinador);
	int tamanio = 0; //Calcular el tamanio del paquete
	void* buffer = malloc(tamanio); //Info que necesita enviar al coordinador.
	enviar(Coordinador,cop_generico,tamanio,buffer);
	log_info(logger, "Me conecte con el Coordinador. \n");

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
		exit(1);
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
	// add the Coordinador to the master set
	FD_SET(Coordinador, &master);
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

							esperar_handshake(socketActual,paqueteRecibido,cop_handshake_ESI_Planificador);
							log_info(logger, "Realice handshake con ESI \n");
							paqueteRecibido = recibir(socketActual); // Info sobre el ESI

							//Todo actualizar estructuras necesarias con datos del ESI

						break;
						case cop_Coordinador_Sentencia_Exito:

						break;
						case cop_Coordinador_Sentencia_Fallo_TC:

						break;
						case cop_Coordinador_Sentencia_Fallo_CNI:

						break;
						case cop_Coordinador_Sentencia_Fallo_CI:

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
	configuracion.PUERTO_ESCUCHA = get_campo_config_string(archivo_configuracion, "PUERTO_ESCUCHA");
	configuracion.ALGORITMO_PLANIFICACION = get_campo_config_string(archivo_configuracion, "ALGORITMO_PLANIFICACION");
	configuracion.ESTIMACION_INICIAL = get_campo_config_int(archivo_configuracion, "ESTIMACION_INICIAL");
	configuracion.IP_COORDINADOR = get_campo_config_string(archivo_configuracion, "IP_COORDINADOR");
	configuracion.PUERTO_COORDINADOR = get_campo_config_string(archivo_configuracion, "PUERTO_COORDINADOR");
	configuracion.CLAVES_BLOQUEADAS = get_campo_config_string(archivo_configuracion, "CLAVES_BLOQUEADAS");
	return configuracion;
}

void salir(int motivo){
	list_destroy(lista_de_ESIs);
	list_destroy(cola_de_finalizados);
	list_destroy(cola_de_bloqueados);
	list_destroy(cola_de_listos);
	list_destroy(accion_a_tomar);
	exit(motivo);
}

void iniciarConsolaPlanificador(){
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

				//El Planificador no le dará nuevas órdenes de ejecución a ningún ESI mientras se encuentre pausado.

				free(linea);
			}else if (strcmp(linea, "Continuar") == 0) {
				log_info(logger, "Eligio la opcion Continuar\n");
				free(linea);
			} else if (strcmp(primeraPalabra, "bloquear") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				parametros = validaCantParametrosComando(linea,2);

				//Se bloqueará el proceso ESI hasta ser desbloqueado, especificado por dicho <ID> en la cola del recurso <clave>.

				if(validar_ESI_id(atoi(parametros[1]))){
					pasar_ESI_a_bloqueado(atoi(parametros[1]),parametros[0],bloqueado_por_consola);
				}else{
					log_info(logger, "El ID de ESI ingresado es invalido\n");
				}

				free(linea);
			} else if (strcmp(primeraPalabra, "desbloquear") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
				parametros = validaCantParametrosComando(linea,1);

				//Se desbloqueara el proceso ESI con el ID especificado.
				bool encontrar_ESIs_bloqueados_por_consola(void* esi){
					return ((t_bloqueado*)esi)->motivo == bloqueado_por_consola;
				}

				t_list* lista_de_ESIs_bloqueados_por_consola = list_create();
				lista_de_ESIs_bloqueados_por_consola = list_filter(cola_de_bloqueados,encontrar_ESIs_bloqueados_por_consola);

				if(lista_de_ESIs_bloqueados_por_consola != NULL){
					bool encontrar_esi_por_clave(void* esi){
						return strcmp(((t_bloqueado*)esi)->clave_de_bloqueo,parametros[0]);
					}
					t_bloqueado* ESI_para_clave = list_find(lista_de_ESIs_bloqueados_por_consola,encontrar_esi_por_clave);
					if(ESI_para_clave != NULL){
						pasar_ESI_a_listo(ESI_para_clave->ESI->id_ESI);
					}else{
						log_info(logger, "No existe ESI bloqueado por la clave %s\n",parametros[0]);
					}
				}else{
					log_info(logger, "No existen ESIs bloqueados\n");
				}
				list_destroy(lista_de_ESIs_bloqueados_por_consola);
				free(linea);
			} else if (strcmp(primeraPalabra, "listar") == 0) {
				log_info(logger, "Eligio la opcion Listar\n");
				parametros = validaCantParametrosComando(linea,1);

				//Lista los procesos encolados esperando al recurso.
				bool encontrar_ESIs_por_clave_de_bloqueo(void* esi){
					return strcmp(((t_bloqueado*)esi)->clave_de_bloqueo,parametros[0]);
				}

				t_list* lista_de_ESIs_por_clave_de_bloqueo = list_create();
				lista_de_ESIs_por_clave_de_bloqueo = list_filter(cola_de_bloqueados,encontrar_ESIs_por_clave_de_bloqueo);
				if(lista_de_ESIs_por_clave_de_bloqueo != NULL){
					int acum = 0;
					void mostrar_id_de_esi_bloqueado(void* esi){
						log_info(logger, "%i - ID: %s\n",acum,((t_bloqueado*)esi)->ESI->id_ESI);
					}
					log_info(logger, "ESIs bloqueados por: %s\n",parametros[0]);
					list_iterate(lista_de_ESIs_por_clave_de_bloqueo,mostrar_id_de_esi_bloqueado);
				}else{
					log_info(logger, "No hay ESIs esperando por la clave %s\n",parametros[0]);
				}
				list_destroy(lista_de_ESIs_por_clave_de_bloqueo);
				free(linea);
			} else if (strcmp(primeraPalabra, "kill") == 0) {
				log_info(logger, "Eligio la opcion Kill\n");
				parametros = validaCantParametrosComando(linea,1);
				free(linea);
			} else if (strcmp(primeraPalabra, "status") == 0) {
				log_info(logger, "Eligio la opcion Status\n");
				parametros = validaCantParametrosComando(linea,1);
				free(linea);
			} else if (strcmp(linea, "deadlock") == 0) {
				log_info(logger, "Eligio la opcion Bloquear\n");
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

	switch(esi->estado){
		case listo:
		{
			esi->descripcion_estado = malloc(strlen(descripcion_estado));
			strcpy(esi->descripcion_estado,descripcion_estado);
			esi->estado = finalizado;
			list_remove_by_condition(cola_de_listos,encontrar_esi);
			list_add(cola_de_finalizados,esi);
		}
		break;
		case ejecutando:
		{
			ESI_ejecutando->estado = finalizado;
			ESI_ejecutando->descripcion_estado = malloc(strlen(descripcion_estado));
			strcpy(ESI_ejecutando->descripcion_estado,descripcion_estado);
			list_add(cola_de_finalizados,ESI_ejecutando);
			free(ESI_ejecutando);
		}
		break;
		case bloqueado:
		{
			esi->estado = finalizado;
			esi->descripcion_estado = malloc(strlen(descripcion_estado));
			strcpy(esi->descripcion_estado,descripcion_estado);
			list_remove_by_condition(cola_de_bloqueados,encontrar_esi_bloqueado);
			list_add(cola_de_finalizados,esi);
		}
		break;
	}

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
			ESI_ejecutando->estado = listo;
			free(ESI_ejecutando);
			list_add(cola_de_listos,ESI_ejecutando);
		}
		break;
		case bloqueado:
		{
			esi->estado = listo;
			list_remove_by_condition(cola_de_bloqueados,encontrar_esi_bloqueado);
			list_add(cola_de_listos,esi);
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

	t_ESI* esi = list_find(lista_de_ESIs, encontrar_esi);

	switch(esi->estado){
		case listo:
		{
			esi->estado = ejecutando;
			list_remove_by_condition(cola_de_listos, encontrar_esi);
			ESI_ejecutando = malloc(sizeof(t_ESI));
			ESI_ejecutando = esi;
		}
		break;
		case bloqueado:
		{
			esi->estado = ejecutando;
			list_remove_by_condition(cola_de_bloqueados, encontrar_esi_bloqueado);
			ESI_ejecutando = malloc(sizeof(t_ESI));
			ESI_ejecutando = esi;
		}
		break;
	}

	free(esi);
}

bool validar_ESI_id(int id_ESI){

	bool encontrar_esi(void* esi){
		return ((t_ESI*)esi)->id_ESI == id_ESI;
	}

	t_ESI* esi = list_find(lista_de_ESIs, encontrar_esi);

	if(esi != NULL){
		return true;
	}else{
		return false;
	}
}

