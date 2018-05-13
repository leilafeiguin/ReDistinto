#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "Instancia.h"

void* archivo;
t_log* logger;

int main(int argc, char* arguments[]) {
	nombre_instancia = arguments[1]; // BORRAR PROXIMAMENTE

	imprimir("/home/utnso/workspace/tp-2018-1c-PuntoZip/Instancia/instancia_image.txt");
	char* fileLog;
	fileLog = "instancia_logs.txt";

	logger = log_create(fileLog, "Instancia Logs", 1, 1);
	log_info(logger, "Inicializando proceso Instancia. \n");

	instancia_configuracion configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

	// Realizar handshake con coordinador
	un_socket Coordinador = conectar_a(configuracion.IP_COORDINADOR,configuracion.PUERTO_COORDINADOR);
	realizar_handshake(Coordinador, cop_handshake_Instancia_Coordinador);

	instancia.nombre = configuracion.NOMBRE_INSTANCIA;
	instancia.estado = conectada;
	instancia.keys_contenidas = list_create();

	// Enviar al coordinador nombre de la instancia
	enviar(Coordinador,cop_generico, size_of_string(instancia.nombre), instancia.nombre);
	log_info(logger, "Me conecte con el Coordinador. \n");

	inicializar_instancia(Coordinador);
	esperar_instrucciones(Coordinador);
	return EXIT_SUCCESS;
}

instancia_configuracion get_configuracion() {
	printf("Levantando archivo de configuracion del proceso Instancia\n");
	instancia_configuracion configuracion;
	t_config* archivo_configuracion = config_create(pathInstanciaConfig);
	configuracion.IP_COORDINADOR = get_campo_config_string(archivo_configuracion, "IP_COORDINADOR");
	configuracion.PUERTO_COORDINADOR = get_campo_config_string(archivo_configuracion, "PUERTO_COORDINADOR");
	configuracion.ALGORITMO_REEMPLAZO = get_campo_config_string(archivo_configuracion, "ALGORITMO_REEMPLAZO");
	configuracion.PUNTO_MONTAJE = get_campo_config_string(archivo_configuracion, "PUNTO_MONTAJE");
	// configuracion.NOMBRE_INSTANCIA = get_campo_config_string(archivo_configuracion, "NOMBRE_INSTANCIA");
	configuracion.NOMBRE_INSTANCIA = nombre_instancia; // BORRAR PROXIMAMENTE
	configuracion.INTERVALO_DUMP = get_campo_config_int(archivo_configuracion, "INTERVALO_DUMP");
	return configuracion;
}

void inicializar_instancia(un_socket coordinador) {
	t_paquete* paqueteEstadoInstancia = recibir(coordinador); // Recibo si es una instancia nueva o se esta reconectado
	bool instancia_nueva = paqueteEstadoInstancia->codigo_operacion == cop_Instancia_Vieja ? false : true;
	liberar_paquete(paqueteEstadoInstancia);
	t_paquete* paqueteCantidadEntradas = recibir(coordinador) ; // Recibo la cantidad de entradas
	t_paquete* paqueteTamanioEntradas = recibir(coordinador); // Recibo tamaño de cada entrada
	cantidad_entradas = atoi(paqueteCantidadEntradas->data);
	tamanio_entradas = atoi(paqueteTamanioEntradas->data);

	// Se fija si la instancia es nueva o se esta reconectando, por ende tiene que levantar informacion del disco
	crear_tabla_entradas(cantidad_entradas, tamanio_entradas);
	if (instancia_nueva) {
		log_info(logger, "Tabla de entradas creada \n");
	} else {
		restaurar_claves(coordinador);
		log_info(logger, "Tabla de entradas restaurada del disco \n");
		mostrar_tabla_entradas();
	}
	liberar_paquete(paqueteCantidadEntradas);
	liberar_paquete(paqueteTamanioEntradas);
}

int esperar_instrucciones(un_socket coordinador) {
	while(1) {
		t_paquete* paqueteRecibido = recibir(coordinador);
		switch(paqueteRecibido->codigo_operacion) {
			case cop_Instancia_Ejecutar_Set:
				ejecutar_set(coordinador, paqueteRecibido->data);
			break;

			case cop_Instancia_Ejecutar_Get:
				ejecutar_get(coordinador, paqueteRecibido->data);
			break;

			case cop_Instancia_Ejecutar_Store:
				ejecutar_store(coordinador, paqueteRecibido->data);
			break;

			case cop_Instancia_Ejecutar_Dump:
				ejecutar_dump(coordinador);
			break;

			case codigo_error:
				log_info(logger, "Error en el coordinador. Abortando. \n");
				return 0;
			break;

			case codigo_healthcheck:
				enviar(coordinador, codigo_healthcheck, size_of_string(""), "");
			break;
		}
		liberar_paquete(paqueteRecibido);
	}
}

int verificar_set(char* valor) {
	/*
	 * TODO:
	 * Verificar si un valor se puede guardar o no.
	 * Valor de retorno:
	 * 	- cop_Instancia_Guardar_OK (Se puede guardar)
	 * 	- cop_Instancia_Guardar_Error_FE (No se puede guardar a causa de fragmentacion externa)
	 * 	- cop_Instancia_Guardar_Error_FI (No se puede guardar a causa de fragmentacion interna)
	 */
	return cop_Instancia_Guardar_OK;
}

t_entrada * get_entrada_x_index(int index) {
	return list_get(instancia.entradas, index);
}

// Detecta si no hay espacio disponible a causa de fragmentacion (y especifica el tipo) y aplica el algoritmo de reemplazo
t_entrada * get_entrada_a_guardar_algoritmo_reemplazo(char* clave, char* valor) {
	log_info(logger, "No hay entradas disponibles. Aplicando algoritmo de reemplazo");
	return get_entrada_x_index(0);
}

t_entrada * get_entrada_a_guardar(char* clave, char* valor) {
	/*
	 * Segun un algoritmo devuelve la entrada donde se guardara el valor.
	 * Si el valor es demasiado grande, ocupara las proximas entradas consecutivas a esa.
	 * En caso de que no haya espacio disponible ya sea por fragmentacion interna o externa, lo comunicara y
	 * luego aplicara el algoritmo de reemplazo.
	 */
	int cant_entradas_necesarias = ceil((float)(size_of_string(valor) - 1) / tamanio_entradas);
	t_entrada * entrada_guardar = NULL;	// Entrada inicial donde se guardara el valor
	int i_entrada = 0;
	while(entrada_guardar == NULL && i_entrada < cantidad_entradas) {
		t_entrada * entrada_i = get_entrada_x_index(i_entrada);
		if (entrada_i->espacio_ocupado == 0 || strcmp(entrada_i->clave, clave) == 0) {	// Si la entrada esta libre o se uso para esa misma clave
			int max_offset = i_entrada + cant_entradas_necesarias - 1;
			bool espacio_disponible = max_offset >= cantidad_entradas ? false : true;
			int j_entrada = i_entrada + 1;
			while (espacio_disponible && j_entrada <= max_offset) {
				t_entrada * entrada_j = get_entrada_x_index(j_entrada);
				if (entrada_j->espacio_ocupado == 0 || strcmp(entrada_j->clave, clave) == 0) {
					j_entrada++;
				} else {
					espacio_disponible = false;
				}
			}
			if (espacio_disponible) {
				entrada_guardar = entrada_i;
			} else {
				i_entrada = max_offset + 1;
			}
		}
		i_entrada++;
	}
	return entrada_guardar != NULL ? entrada_guardar : get_entrada_a_guardar_algoritmo_reemplazo(clave, valor);
}

t_entrada * get_next(t_entrada * entrada) {
	int index_entrada = entrada->id + 1;
	return get_entrada_x_index(index_entrada);
}

// Recibo la clave como parametro y espero a que me envien en el valor
int ejecutar_set(un_socket coordinador, char* clave) {
	t_paquete* paqueteValor = recibir(coordinador); // Recibo el valor a guardar
	char* valor = paqueteValor->data;
	int estado_set = verificar_set(valor);
	switch(estado_set) {
		case cop_Instancia_Guardar_OK:
			set(clave, valor);
		break;
	}
	liberar_paquete(paqueteValor);
	return estado_set;
}

int set(char* clave, char* valor) {
	t_entrada * entrada = get_entrada_a_guardar(clave, valor);
	// Guardo el valor en las entradas
	char* valor_restante_a_guardar = copy_string(valor);
	int espacio_restante_a_guardar = size_of_string(valor) - 1;
	while(espacio_restante_a_guardar > 0) {
		entrada->clave = copy_string(clave);
		char* contenido = string_substring(valor_restante_a_guardar, 0, tamanio_entradas);
		entrada->contenido = copy_string(contenido);
		free(contenido);
		entrada->espacio_ocupado = size_of_string(entrada->contenido) -1;
		entrada->cant_veces_no_accedida = 0;
		espacio_restante_a_guardar += (-1) * (entrada->espacio_ocupado);
		entrada = get_next(entrada);

		// Verifico si ya guarde todo el valor
		if (espacio_restante_a_guardar > 0) {
			char* valor_restante = string_substring(valor_restante_a_guardar, tamanio_entradas, strlen(valor_restante_a_guardar) - tamanio_entradas);
			strcpy(valor_restante_a_guardar, valor_restante);
			free(valor_restante);
		}
	}
	free(valor_restante_a_guardar);

	// Agrego la clave a la lista de claves
	list_add(instancia.keys_contenidas, copy_string(clave));

	log_and_free(logger, string_concat(5, "SET ", clave, ":'", valor, "' \n"));
	return 0;
}

int ejecutar_get(un_socket coordinador, char* clave) {
	char* valor = get(clave);
	enviar(coordinador, cop_Instancia_Ejecutar_Get, size_of_string(valor), valor); // Envia al coordinador el valor de la clave solicitada
	log_and_free(logger, string_concat(3, "GET ", clave," \n"));
	free(valor);
}

char* get(char* clave) {
	char* valor = string_new();
	t_list * entradas = get_entradas_con_clave(clave);
	void concatenar_valor(t_entrada * entrada){
		string_append(&valor, entrada->contenido);
	}
	list_iterate(entradas, concatenar_valor);
	list_destroy(entradas);
	return valor;
}

int ejecutar_store(un_socket coordinador, char* clave) {
	dump_clave(clave);
	enviar(coordinador, cop_Instancia_Ejecucion_Exito, size_of_string(""), ""); // Avisa al coordinador que el STORE se ejecuto de forma exitosa
	log_and_free(logger, string_concat(3, "STORE ", clave, " \n"));
}

int ejecutar_dump(un_socket coordinador) {
	log_info(logger, "Ejecutando DUMP \n");
	list_iterate(instancia.keys_contenidas, dump_clave);
	enviar(coordinador, cop_Instancia_Ejecucion_Exito, size_of_string(""), ""); // Avisa al coordinador que el DUMP se ejecuto de forma exitosa
}

int dump_clave(char* clave) {
	char* file_path = get_file_path(clave);
	remove(file_path); // Borro el archivo ya que voy a reemplazar el contenido
	char* file_content = get(clave);
	FILE* file = txt_open_for_append(file_path);
	txt_write_in_file(file, file_content);
	txt_close_file(file);
	free(file_content);
	free(file_path);
}

t_list * get_entradas_con_clave(char* clave) {
	bool entrada_tiene_la_clave(t_entrada * entrada){
		return strcmp(clave, entrada->clave) == 0 ? true : false;
	}
	return list_filter(instancia.entradas, entrada_tiene_la_clave);
}

char* get_file_path(char* clave) {
	return string_concat(3, pathInstanciaData, clave, ".txt");
}

void restaurar_clave(char* clave) {
	char* valor = "";

	char* file_path = get_file_path(clave);
	/* Implementacion mmap */
	int fd = open(file_path, O_RDONLY);
	if (fd > 0) {
		size_t pagesize = getpagesize();
		char * file_content = mmap(
			(void*) (pagesize * (1 << 20)), pagesize,
			PROT_READ, MAP_FILE|MAP_PRIVATE,
			fd, 0
		);
		valor = copy_string(file_content);
		int unmap_result = munmap(file_content, pagesize);
		close(fd);
	}
	/* !Implementacion mmap */

	set(clave, valor);
}

int restaurar_claves(un_socket coordinador) {
	t_paquete* paqueteCantidadClaves = recibir(coordinador); // Recibo la cantidad de claves
	int cantidad_keys = atoi(paqueteCantidadClaves->data);
	liberar_paquete(paqueteCantidadClaves);

	for(int i = 0;i < cantidad_keys;i++) {
		t_paquete* paqueteClave = recibir(coordinador); // Recibo la clave
		restaurar_clave(paqueteClave->data);
		liberar_paquete(paqueteClave);
	}
}

void crear_tabla_entradas(int cantidad_entradas, int tamanio_entrada) {
	instancia.entradas = list_create();
	for(int i = 0; i < cantidad_entradas; i++) {
		t_entrada * entrada = malloc(sizeof(t_entrada));
		entrada->id = i;
		entrada->espacio_ocupado = 0;
		entrada->cant_veces_no_accedida = 0;
		entrada->clave = "";
		entrada->contenido = "";
		list_add(instancia.entradas, entrada);
	}
}

void mostrar_tabla_entradas() {
	puts("_________________________________________________________________________");
	puts("| ID | Clave | Contenido | Espacio utilizado | Cant. veces no accedida |");
	puts("_________________________________________________________________________");
	void mostrar_entrada(t_entrada * entrada){
		printf("|   %d   |   %s   |   %s   |   %d   |   %d   | \n", entrada->id, entrada->clave,
				entrada->contenido, entrada->espacio_ocupado, entrada->cant_veces_no_accedida);
	}
	list_iterate(instancia.entradas, mostrar_entrada);
	puts("_________________________________________________________________________");
}
