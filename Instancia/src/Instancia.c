#include <stdio.h>
#include <stdlib.h>
#include "Instancia.h"

void* archivo;
t_log* logger;

int main(void) {
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

	// Enviar al coordinador nombre de la instancia
	enviar(Coordinador,cop_generico, size_of_string(instancia.nombre), instancia.nombre);
	log_info(logger, "Me conecte con el Coordinador. \n");

	crear_tabla_entradas(Coordinador);
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
	configuracion.NOMBRE_INSTANCIA = get_campo_config_string(archivo_configuracion, "NOMBRE_INSTANCIA");
	configuracion.INTERVALO_DUMP = get_campo_config_int(archivo_configuracion, "INTERVALO_DUMP");
	return configuracion;
}

void crear_tabla_entradas(un_socket coordinador) {
	// Recibo la cantidad de entradas y tamaño de cada una
	t_paquete* paqueteCantidadEntradas = recibir(coordinador);
	t_paquete* paqueteTamanioEntradas = recibir(coordinador);
	cantidad_entradas = atoi(paqueteCantidadEntradas->data);
	tamanio_entradas = atoi(paqueteTamanioEntradas->data);

	// Se fija si la instancia es nueva o se esta reconectando, por ende tiene que levantar informacion del disco
	if (0) {

	} else {
		instancia.entradas = list_create(); // Instancia nueva
		for(int i = 0; i < cantidad_entradas; i++) {
			t_entrada * entrada = malloc(sizeof(t_entrada));
			entrada->id = i;
			entrada->espacio_ocupado = 0;
			entrada->cant_veces_no_accedida = 0;
			entrada->clave = "";
			entrada->contenido = "";
			list_add(instancia.entradas, entrada);
		}
		log_info(logger, "Tabla de entradas creada \n");
	}

}

int espacio_total() {
	return cantidad_entradas * tamanio_entradas;
}

int espacio_ocupado() {
	double espacio = 0;
	void sumar_espacio(t_entrada * entrada){
		espacio += entrada->espacio_ocupado;
	}
	list_iterate(instancia.entradas, sumar_espacio);
	return espacio;
}

int espacio_disponible() {
	return espacio_total() - espacio_ocupado();
}

void esperar_instrucciones(un_socket coordinador) {
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
		}
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

t_entrada * get_entrada_a_guardar(char* valor) {
	/*
	 * Segun un algoritmo devuelve la entrada donde se guardara el valor.
	 * Si el valor es demasiado grande, ocupara las proximas entradas consecutivas a esa.
	 */
	int index_entrada = 0;// TODO: Aplicar algoritmo aca
	return list_get(instancia.entradas, index_entrada);
}

t_entrada * get_next(t_entrada * entrada) {
	int index_entrada = entrada->id + 1;
	return list_get(instancia.entradas, index_entrada);
}

// Recibo la clave como parametro y espero a que me envien en el valor
int ejecutar_set(un_socket coordinador, char* clave) {
	t_paquete* paqueteValor = recibir(coordinador); // Recibo el valor a guardar
	char* valor = paqueteValor->data;
	int estado_set = verificar_set(valor);
	switch(estado_set) {
		case cop_Instancia_Guardar_OK:
			set(get_entrada_a_guardar(valor), clave, valor);
		break;
	}
	mostrar_tabla_entradas();
	return estado_set;
}

int set(t_entrada * entrada, char* clave, char* valor) {
	char* valor_restante_a_guardar = valor;
	int espacio_restante_a_guardar = size_of_string(valor) - 1;
	while(espacio_restante_a_guardar > 0) {
		entrada->clave = clave;
		entrada->contenido = string_substring(valor_restante_a_guardar, 0, tamanio_entradas);
		entrada->espacio_ocupado = size_of_string(entrada->contenido) -1;
		entrada->cant_veces_no_accedida = 0;
		espacio_restante_a_guardar += (-1) * (entrada->espacio_ocupado);
		entrada = get_next(entrada);

		// Verifico si ya guarde todo el valor
		if (espacio_restante_a_guardar > 0) {
			valor_restante_a_guardar = string_substring(valor_restante_a_guardar, tamanio_entradas, strlen(valor_restante_a_guardar) - tamanio_entradas);
		}
	}
	printf("Operacion SET, clave: %s, valor: %s \n", clave, valor);
	return 0;
}

int ejecutar_get(un_socket coordinador, char* clave) {
	char* valor = get(clave);
	enviar(coordinador, cop_Instancia_Ejecutar_Get, size_of_string(valor), valor); // Envia al coordinador el valor de la clave solicitada
	printf("GET %s \n", clave);
}

char* get(char* clave) {
	char* valor = string_new();
	t_list * entradas = get_entradas_con_clave(clave);
	void concatenar_valor(t_entrada * entrada){
		string_append(&valor, entrada->contenido);
	}
	list_iterate(entradas, concatenar_valor);
	return valor;
}

int ejecutar_store(un_socket coordinador, char* clave) {
	printf("STORE %s \n", clave);
	t_list * entradas = get_entradas_con_clave(clave);
	list_iterate(entradas, dump_entrada);
	enviar(coordinador, cop_Instancia_Ejecucion_Exito, size_of_string(""), ""); // Avisa al coordinador que el STORE se ejecuto de forma exitosa
}

int ejecutar_dump(un_socket coordinador) {
	puts("Ejecutando DUMP");
	list_iterate(instancia.entradas, dump_entrada);
	enviar(coordinador, cop_Instancia_Ejecucion_Exito, size_of_string(""), ""); // Avisa al coordinador que el DUMP se ejecuto de forma exitosa
}

int dump_entrada(t_entrada * entrada) {
	printf("Persistiendo entrada. ID %d , Valor '%s' \n", entrada->id, entrada->contenido);
	char* filePath = get_file_path(entrada);
	remove(filePath); // Borro el archivo ya que voy a reemplazar el contenido
	FILE* file = txt_open_for_append(filePath);
	txt_write_in_file(file, entrada->contenido);
	txt_close_file(file);
}

t_list * get_entradas_con_clave(char* clave) {
	bool entrada_tiene_la_clave(t_entrada * entrada){
		if (strcmp(clave, entrada->clave) == 0) {
			return true;
		}
		return false;
	}
	return list_filter(instancia.entradas, entrada_tiene_la_clave);
}

char* get_file_path(t_entrada * entrada) {
	char* filePath = string_new();
	string_append(&filePath, pathInstanciaData);
	string_append(&filePath, instancia.nombre);
	string_append(&filePath, "_Entrada_");
	char id_entrada[12];
	sprintf(id_entrada, "%d", entrada->id);
	string_append(&filePath, id_entrada);
	return filePath;
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
