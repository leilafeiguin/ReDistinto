#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "Instancia.h"

int main(int argc, char* arguments[]) {
	pthread_mutex_init(&mutex_keys_contenidas, NULL);

	nombre_instancia = arguments[1]; // BORRAR PROXIMAMENTE

	(void) signal(SIGINT, funcion_exit);

	imprimir("/home/utnso/workspace/tp-2018-1c-PuntoZip/Instancia/instancia_image.txt");
	char* fileLog = string_concat(2, nombre_instancia, ".txt");

	logger = log_create(fileLog, "Instancia Logs", 1, 1);
	log_info(logger, "Inicializando proceso Instancia. \n");

	configuracion = get_configuracion();

	log_info(logger, "Archivo de configuracion levantado. \n");

	// Realizar handshake con coordinador
	Coordinador = conectar_a(configuracion.IP_COORDINADOR,configuracion.PUERTO_COORDINADOR);
	realizar_handshake(Coordinador, cop_handshake_Instancia_Coordinador);

	instancia.nombre = configuracion.NOMBRE_INSTANCIA;
	instancia.estado = conectada;
	instancia.keys_contenidas = list_create();

	// Enviar al coordinador nombre de la instancia
	enviar(Coordinador,cop_generico, size_of_string(instancia.nombre), instancia.nombre);
	log_info(logger, "Me conecte con el Coordinador. \n");

	inicializar_instancia();
	esperar_instrucciones();
	return EXIT_SUCCESS;
}

instancia_configuracion get_configuracion() {
	log_info(logger, "Levantando archivo de configuracion del proceso Instancia \n");
	instancia_configuracion configuracion;
	t_config* archivo_configuracion = config_create(pathInstanciaConfig);
	configuracion.IP_COORDINADOR = copy_string(get_campo_config_string(archivo_configuracion, "IP_COORDINADOR"));
	configuracion.PUERTO_COORDINADOR = copy_string(get_campo_config_string(archivo_configuracion, "PUERTO_COORDINADOR"));
	configuracion.ALGORITMO_REEMPLAZO = copy_string(get_campo_config_string(archivo_configuracion, "ALGORITMO_REEMPLAZO"));
	configuracion.PUNTO_MONTAJE = copy_string(get_campo_config_string(archivo_configuracion, "PUNTO_MONTAJE"));
	//configuracion.NOMBRE_INSTANCIA = copy_string(get_campo_config_string(archivo_configuracion, "NOMBRE_INSTANCIA"));
	configuracion.NOMBRE_INSTANCIA = copy_string(nombre_instancia); // BORRAR PROXIMAMENTE
	configuracion.INTERVALO_DUMP = get_campo_config_int(archivo_configuracion, "INTERVALO_DUMP");
	pathInstanciaData = string_concat(2, configuracion.PUNTO_MONTAJE, configuracion.NOMBRE_INSTANCIA);
	config_destroy(archivo_configuracion);
	return configuracion;
}

void inicializar_instancia() {
	t_paquete* paqueteEstadoInstancia = recibir(Coordinador); // Recibo si es una instancia nueva o se esta reconectado
	bool instancia_nueva = paqueteEstadoInstancia->codigo_operacion == cop_Instancia_Vieja ? false : true;
	liberar_paquete(paqueteEstadoInstancia);
	t_paquete* paqueteTablaEntradas = recibir(Coordinador) ; // Recibo la informacion de la tabla de entradas
	int desplazamiento = 0;
	cantidad_entradas = deserializar_int(paqueteTablaEntradas->data, &desplazamiento);
	tamanio_entradas = deserializar_int(paqueteTablaEntradas->data, &desplazamiento);

	// Se fija si la instancia es nueva o se esta reconectando, por ende tiene que levantar informacion del disco
	crear_tabla_entradas(cantidad_entradas, tamanio_entradas);
	if (instancia_nueva) {
		log_info(logger, "Tabla de entradas creada \n");
	} else {
		t_list * claves = recibir_listado_de_strings(Coordinador);
		list_iterate(claves, restaurar_clave);
		list_destroy(claves);
		log_info(logger, "Tabla de entradas restaurada del disco \n");
		mostrar_tabla_entradas();
	}
	liberar_paquete(paqueteTablaEntradas);

	// Iniciar hilo de dumps
	pthread_create(&hilo_dump, NULL, funcion_hilo_dump, NULL);
}

void* funcion_hilo_dump(void * unused) {
	while(1) {
		sleep(configuracion.INTERVALO_DUMP);
		ejecutar_dump();
	}
}

int esperar_instrucciones() {
	while(1) {
		t_paquete* paqueteRecibido = recibir(Coordinador);
		switch(paqueteRecibido->codigo_operacion) {
			case cop_Instancia_Ejecutar_Get:
				ejecutar_get(paqueteRecibido->data);
			break;

			case cop_Instancia_Ejecutar_Set:
				ejecutar_set(paqueteRecibido->data);
			break;

			case cop_Instancia_Ejecutar_Store:
				ejecutar_store(paqueteRecibido->data);
			break;

			case cop_Instancia_Ejecutar_Dump:
				ejecutar_dump();
			break;

			case cop_Instancia_Necesidad_Compactacion: ;
				int desplazamiento = 0;
				char* clave = deserializar_string(paqueteRecibido->data, &desplazamiento);
				char* valor = deserializar_string(paqueteRecibido->data, &desplazamiento);
				validar_necesidad_compactacion(clave, valor);
				free(clave);
				free(valor);
			break;

			case cop_Instancia_Ejecutar_Compactacion:
				compactar_tabla_entradas();
				enviar(Coordinador, cop_Instancia_Ejecutar_Compactacion, size_of_string(""), "");
			break;

			case codigo_error:
				log_info(logger, "Error en el coordinador. Abortando. \n");
				return 0;
			break;

			case codigo_healthcheck:
				enviar(Coordinador, codigo_healthcheck, size_of_string(""), "");
			break;
		}
		liberar_paquete(paqueteRecibido);
	}
}

t_entrada * get_entrada_x_index(int index) {
	return list_get(instancia.entradas, index);
}

// Detecta si no hay espacio disponible a causa de fragmentacion (y especifica el tipo) y aplica el algoritmo de reemplazo
t_entrada * get_entrada_a_guardar_algoritmo_reemplazo(char* clave, char* valor) {
	t_entrada * entrada_reemplazante;
	log_info(logger, "No hay entradas disponibles. Aplicando algoritmo de reemplazo");
	if (strings_equal(configuracion.ALGORITMO_REEMPLAZO, "LRU")) {
		entrada_reemplazante = least_recently_used();
	} else if (strings_equal(configuracion.ALGORITMO_REEMPLAZO, "BSU")) {
		entrada_reemplazante = biggest_space_used();
	} else {
		entrada_reemplazante = algoritmo_circular(valor);
	}

	log_and_free(logger, string_concat(5, "La clave '", entrada_reemplazante->clave, "' fue reemplazada por la clave '", clave ,"' \n"));
	pthread_mutex_lock(&mutex_keys_contenidas);
	remover_clave(entrada_reemplazante->clave);
	pthread_mutex_unlock(&mutex_keys_contenidas);
	return entrada_reemplazante;
}

t_entrada * get_entrada_a_guardar(char* clave, char* valor) {
	/*
	 * Segun un algoritmo devuelve la entrada donde se guardara el valor.
	 * Si el valor es demasiado grande, ocupara las proximas entradas consecutivas a esa.
	 * En caso de que no haya espacio disponible ya sea por fragmentacion interna o externa, lo comunicara y
	 * luego aplicara el algoritmo de reemplazo.
	 */
	int cant_entradas_necesarias = cantidad_entradas_necesarias(valor, tamanio_entradas);
	t_entrada * entrada_guardar = NULL;	// Entrada inicial donde se guardara el valor
	int i_entrada = 0;
	while(entrada_guardar == NULL && i_entrada < cantidad_entradas) {
		t_entrada * entrada_i = get_entrada_x_index(i_entrada);
		if (entrada_disponible(entrada_i, clave)) {
			int max_offset = i_entrada + cant_entradas_necesarias - 1;
			bool espacio_disponible = max_offset < cantidad_entradas;
			int j_entrada = i_entrada + 1;
			while (espacio_disponible && j_entrada <= max_offset) {
				t_entrada * entrada_j = get_entrada_x_index(j_entrada);
				if (entrada_disponible(entrada_j, clave)) {
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
	return entrada_guardar;
}

bool entrada_disponible(t_entrada * entrada, char* clave) {
	return entrada->espacio_ocupado == 0 || strings_equal(entrada->clave, clave);
}

t_entrada * get_next(t_entrada * entrada) {
	int index_entrada = entrada->id + 1;
	return get_entrada_x_index(index_entrada);
}

// Recibo la clave como parametro y espero a que me envien en el valor
int ejecutar_set(void * clave_valor) {
	int desplazamiento = 0;
	char* clave = deserializar_string(clave_valor, &desplazamiento);
	char* valor = deserializar_string(clave_valor, &desplazamiento);
	set(clave, valor, true); // Seteo el valor
	pthread_mutex_lock(&mutex_keys_contenidas);
	enviar_listado_de_strings(Coordinador, instancia.keys_contenidas); // Le envio al coordinador el listado de claves actualizado
	pthread_mutex_unlock(&mutex_keys_contenidas);
	enviar_cantidad_entradas_ocupadas();
	return 1;
}

void enviar_cantidad_entradas_ocupadas() {
	// Le envio al coordinador la cantidad de entradas ocupadas actualizadas
	char* cant_entradas_ocupadas = string_itoa(cantidad_entradas_ocupadas());
	enviar(Coordinador, cop_generico, size_of_string(cant_entradas_ocupadas), cant_entradas_ocupadas);
	free(cant_entradas_ocupadas);
}

int set(char* clave, char* valor, bool log_mensaje) {
	// Borro el valor actual si clave ya fue ingresada
	if (clave_ingresada(clave)) {
		pthread_mutex_lock(&mutex_keys_contenidas);
		remover_clave(clave);
		pthread_mutex_unlock(&mutex_keys_contenidas);
	}

	if (log_mensaje) {
		log_and_free(logger, string_concat(5, "SET ", clave, ":'", valor, "' \n"));
	}

	t_entrada * entrada = get_entrada_a_guardar(clave, valor);
	if (entrada ==  NULL) { // Es necesario aplicar el algoritmo de reemplazo
		entrada = get_entrada_a_guardar_algoritmo_reemplazo(clave, valor);
	}

	// Guardo el valor en las entradas
	char* valor_restante_a_guardar = valor;
	int espacio_restante_a_guardar = size_of_string(valor) - 1;
	while(espacio_restante_a_guardar > 0) {
		entrada->clave = clave;
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
	pthread_mutex_lock(&mutex_keys_contenidas);
	list_add(instancia.keys_contenidas, clave);
	pthread_mutex_unlock(&mutex_keys_contenidas);

	// Le sumo uno a las entradas no accedidas
	sumar_a_entradas_no_modificadas(clave);

	mostrar_tabla_entradas();
	return 0;
}

void sumar_a_entradas_no_modificadas(char* clave) {
	void sumar_no_accedida(void * item_entrada) {
		t_entrada * entrada = (t_entrada *) item_entrada;
		if (!strings_equal(entrada->clave, clave)) {
			entrada->cant_veces_no_accedida++;
		}
	}
	list_iterate(instancia.entradas, sumar_no_accedida);
}

int ejecutar_get(char* clave) {
	char* valor = get(clave);
	enviar(Coordinador, cop_Instancia_Ejecutar_Get, size_of_string(valor), valor); // Envia al coordinador el valor de la clave solicitada
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

int ejecutar_store(char* clave) {
	dump_clave(clave);
	enviar(Coordinador, cop_Instancia_Ejecucion_Exito, size_of_string(""), ""); // Avisa al coordinador que el STORE se ejecuto de forma exitosa
	log_and_free(logger, string_concat(3, "STORE ", clave, " \n"));
}

int ejecutar_dump() {
	log_info(logger, "Ejecutando DUMP..\n");
	pthread_mutex_lock(&mutex_keys_contenidas);
	list_iterate(instancia.keys_contenidas, dump_clave);
	pthread_mutex_unlock(&mutex_keys_contenidas);
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

int remover_clave(char* clave) {
	// Vacio todas las entradas que ocupaba esa clave
	t_list * entradas = get_entradas_con_clave(clave);
	void borrar_contenido(t_entrada * entrada) {
		entrada->espacio_ocupado = 0;
		entrada->clave = "";
		entrada->contenido = "";
	}
	list_iterate(entradas, borrar_contenido);
	list_destroy(entradas);

	// Saco la clave de la lista de keys contenidas
	bool clave_match(char* key_lista) {
		return strcmp(clave, key_lista) == 0 ? true : false;
	}
	list_remove_by_condition(instancia.keys_contenidas, clave_match);
}

bool clave_ingresada(char* clave) {
	bool clave_match(char* key_lista) {
		return strings_equal(clave, key_lista);
	}
	pthread_mutex_lock(&mutex_keys_contenidas);
	bool result = list_find(instancia.keys_contenidas, clave_match) == NULL ? false : true;
	pthread_mutex_unlock(&mutex_keys_contenidas);
	return result;
}

t_list * get_entradas_con_clave(char* clave) {
	bool entrada_tiene_la_clave(void * item_entrada){
		t_entrada * entrada = (t_entrada *) item_entrada;
		return strings_equal(clave, entrada->clave);
	}
	return list_filter(instancia.entradas, entrada_tiene_la_clave);
}

void validar_directorio_data() {
	struct stat punto_montaje = {0};
	if (stat(configuracion.PUNTO_MONTAJE, &punto_montaje) == -1) {
		mkdir(configuracion.PUNTO_MONTAJE, 0700);
	}

	struct stat carpeta_instancia = {0};
	if (stat(pathInstanciaData, &carpeta_instancia) == -1) {
		mkdir(pathInstanciaData, 0700);
	}
}

char* get_file_path(char* clave) {
	validar_directorio_data();
	return string_concat(4, pathInstanciaData, "/", clave, ".txt");
}

void restaurar_clave(char* clave) {
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
		char* valor = copy_string(file_content);
		int unmap_result = munmap(file_content, pagesize);
		close(fd);
		set(clave, valor, false);
	}
	/* !Implementacion mmap */
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
	log_info(logger, "_________________________________________________________________________ \n");
	log_info(logger, "| ID | Clave | Contenido | Espacio utilizado | Cant. veces no accedida | \n");
	log_info(logger, "_________________________________________________________________________ \n");
	void mostrar_entrada(t_entrada * entrada){
		char str1[12], str2[12], str3[12];
		sprintf(str1, "%d", entrada->id);
		sprintf(str2, "%d",  entrada->espacio_ocupado);
		sprintf(str3, "%d", entrada->cant_veces_no_accedida);
		log_and_free(logger, string_concat(11, "|   ", str1, "   |   ", entrada->clave, "   |   ", entrada->contenido, "   |   ", str2, "   |   ", str3, "   | \n"));
	}
	list_iterate(instancia.entradas, mostrar_entrada);
	log_info(logger, "_________________________________________________________________________ \n");
}

void compactar_tabla_entradas() {
	log_info(logger, "Compactando tabla de entradas \n");
	typedef struct {
		char* clave;
		char* valor;
	} t_clave_valor;
	t_list * lista_claves = list_create();
	void agregar_clave_valor(char* clave) {
		t_clave_valor * clave_valor = malloc(sizeof(t_clave_valor));
		char* valor = get(clave);
		clave_valor->clave = clave;
		clave_valor->valor = valor;
		list_add(lista_claves, clave_valor);
	}
	pthread_mutex_lock(&mutex_keys_contenidas);
	list_iterate(instancia.keys_contenidas, agregar_clave_valor);
	list_iterate(instancia.keys_contenidas, remover_clave);
	pthread_mutex_unlock(&mutex_keys_contenidas);

	void restaurar_clave_valor(t_clave_valor* clave_valor) {
		set(clave_valor->clave, clave_valor->valor, false);
		free(clave_valor);
	}
	list_iterate(lista_claves, restaurar_clave_valor);
	// list_destroy_and_destroy_elements(lista_claves, free);
}

int cantidad_entradas_ocupadas() {
	int cant = 0;
	void sumar_entradas(t_entrada * entrada) {
		if (entrada->espacio_ocupado > 0) {
			cant++;
		}
	}
	list_iterate(instancia.entradas, sumar_entradas);
	return cant;
}

int validar_necesidad_compactacion(char* clave, char* valor) {
	log_info(logger, "Validando necesidad de compactar... \n");
	int cant_entradas_necesarias = cantidad_entradas_necesarias(valor, tamanio_entradas);
	int cantidad_entradas_libres = cantidad_entradas - cantidad_entradas_ocupadas();
	if (clave_ingresada(clave)) { // Si la clave ya fue ingresada le resto la cantidad de entradas que ocupa
		char* valor_actual = get(clave);;
		cantidad_entradas_libres += cantidad_entradas_necesarias(valor_actual, tamanio_entradas);
	}

	bool necesidad_compactacion = cant_entradas_necesarias <= cantidad_entradas_libres && get_entrada_a_guardar(clave, valor) == NULL; // Hay fragmentacion externa
	if (necesidad_compactacion) {
		log_and_free(logger, string_concat(5, "Es necesario compactar para ingresar '", clave, "' : '", valor, "' \n"));
	}

	int cod_op = necesidad_compactacion ? cop_Instancia_Necesidad_Compactacion_True : cop_Instancia_Necesidad_Compactacion_False;
	enviar(Coordinador, cod_op, size_of_string(""), "");
}

void funcion_exit(int sig) {
	log_and_free(logger, string_concat(3, "Abortando ", instancia.nombre, ".. \n"));
	free(configuracion.ALGORITMO_REEMPLAZO);
	free(configuracion.IP_COORDINADOR);
	free(configuracion.NOMBRE_INSTANCIA);
	free(configuracion.PUERTO_COORDINADOR);
	free(configuracion.PUNTO_MONTAJE);
	list_destroy_and_destroy_elements(instancia.entradas, free_t_entrada);
	exit(0);
}

void free_t_entrada(void * item_entrada) {
	t_entrada * entrada = (t_entrada*) item_entrada;
	// free(entrada->clave);
	//free(entrada->contenido);
	free(entrada);
}


// ALGORITMOS DE REEMPLAZO

t_entrada * algoritmo_circular(char* valor) {
	t_entrada * result = NULL;
	int i = 0;
	while (result == NULL && i < cantidad_entradas) {
		t_entrada * entrada = get_entrada_x_index(punteroInstancia);
		char* valor = get(entrada->clave);
		if (cantidad_entradas_necesarias(valor, tamanio_entradas) == 1) {
			result = entrada;
		}
		punteroInstancia++;
		if (punteroInstancia >= cantidad_entradas) {
			punteroInstancia = 0;
		}
		i++;
	}
	return result;
}

t_entrada * least_recently_used() {
	t_entrada * entradaMenosAccedida = list_get(instancia.entradas, 0);

	void show_entrada_menos_accedida(void * item_entrada) {
		t_entrada * entrada = (t_entrada *) item_entrada;
		char* valor = get(entrada->clave);
		if(cantidad_entradas_necesarias(valor, tamanio_entradas) == 1 && entrada->cant_veces_no_accedida > entradaMenosAccedida->cant_veces_no_accedida)
		{
			entradaMenosAccedida = entrada;
		}

	}

	list_iterate(instancia.entradas, show_entrada_menos_accedida);

	return entradaMenosAccedida;
}

t_entrada * biggest_space_used() {
	char * clave_mas_grande = NULL;
	int mayor_espacio = 0;

	void tamanio_clave(void * item_clave) {
		char* clave = (char*) item_clave;
		char* valor = get(clave);
		int espacio_ocupado = size_of_string(valor);

		if(cantidad_entradas_necesarias(valor, tamanio_entradas) == 1 && espacio_ocupado > mayor_espacio) {
			mayor_espacio = espacio_ocupado;
			clave_mas_grande = clave;
		}
	}

	pthread_mutex_lock(&mutex_keys_contenidas);
	list_iterate(instancia.keys_contenidas, tamanio_clave);
	pthread_mutex_unlock(&mutex_keys_contenidas);

	t_list * entradas_con_clave = get_entradas_con_clave(clave_mas_grande);
	t_entrada * primera_entrada_clave_mas_grande = list_get(entradas_con_clave, 0);
	list_destroy(entradas_con_clave);
	return primera_entrada_clave_mas_grande;
}


// !ALGORITMOS DE REEMPLAZO
