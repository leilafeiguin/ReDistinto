#include <stdio.h>
#include <stdlib.h>
#include "Instancia.h"

void* archivo;
t_log* logger;

int main(void) {
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
	int tamanio = strlen(instancia.nombre) * sizeof(char); //Calcular el tamanio del paquete
	enviar(Coordinador,cop_generico,tamanio,instancia.nombre);
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
	int cantidad_entradas = atoi(paqueteCantidadEntradas->data);
	int tamanio_entradas = atoi(paqueteTamanioEntradas->data);

	// Se fija si la instancia es nueva o se esta reconectando, por ende tiene que levantar informacion del disco
	if (0) {

	} else {
		// Instancia nueva
		instancia.entradas = list_create();
		for(int i = 0; i < cantidad_entradas; i++) {
			t_entrada * entrada = malloc(sizeof(t_entrada));
			entrada->id = i;
			entrada->tamanio_ocupado = 0;
			entrada->cant_veces_no_accedida = 0;
			list_add(instancia.entradas, entrada);
		}
		log_info(logger, "Tabla de entradas creada \n");
	}

}

void esperar_instrucciones(un_socket coordinador) {
	while(1) {
		t_paquete* paqueteRecibido = recibir(coordinador);
		puts(paqueteRecibido->data);
	}
}
