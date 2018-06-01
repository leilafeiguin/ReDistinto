#include <stdio.h>
#include <stdlib.h>
#include "ESI.h"


int main(int argc, char **argv) {
	imprimir("/home/utnso/workspace/tp-2018-1c-PuntoZip/ESI/esi_image.txt");
	char* fileLog;
	fileLog = "ESI_logs.txt";

	logger = log_create(fileLog, "ESI Logs", 1, 1);
	log_info(logger, "Inicializando proceso ESI. \n");

	ESI_configuracion configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

	char* path_script = "/home/utnso/workspace/tp-2018-1c-PuntoZip/ESI/pathProvisorio.txt"; // script se ingresa por consola
	leerScript(path_script);

	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	t_list* instrucciones = list_create();

	archivo = fopen(argv[1], "r");
	if (archivo == NULL){
		perror("Error al abrir el archivo: ");
		exit(EXIT_FAILURE);
	}

	while ((read = getline(&line, &len, archivo)) != -1) {
	        t_esi_operacion parsed = parse(line);
	        list_add(instrucciones, &parsed);
	        if(parsed.valido){
	            switch(parsed.keyword){
	                case GET:
	                    printf("GET\tclave: <%s>\n", parsed.argumentos.GET.clave);
	                    break;
	                case SET:
	                    printf("SET\tclave: <%s>\tvalor: <%s>\n", parsed.argumentos.SET.clave, parsed.argumentos.SET.valor);
	                    break;
	                case STORE:
	                    printf("STORE\tclave: <%s>\n", parsed.argumentos.STORE.clave);
	                    break;
	                default:
	                    fprintf(stderr, "No pude interpretar <%s>\n", line);
	                    exit(EXIT_FAILURE);
	            }

	            destruir_operacion(parsed);
	        } else {
	            fprintf(stderr, "La linea <%s> no es valida\n", line);
	            exit(EXIT_FAILURE);
	        }
	    }

	un_socket Planificador = conectar_a(configuracion.IP_PLANIFICADOR,configuracion.PUERTO_PLANIFICADOR);
	realizar_handshake(Planificador, cop_handshake_ESI_Planificador);

	fclose(archivo);
	if (line)
	free(line);

	int desplazamiento = 0;

	void* bufferSentencias = malloc(2*sizeof(int));
	paqueteSentencias* paqueteSentencias = malloc(sizeof(paqueteSentencias));
	paqueteSentencias->cantidadInstrucciones = list_size(instrucciones);

	memcpy(bufferSentencias+desplazamiento, &paqueteSentencias->cantidadInstrucciones,sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(bufferSentencias+desplazamiento, &paqueteSentencias->idESI, sizeof(int));
	desplazamiento+=sizeof(int);
	enviar(Planificador,cop_handshake_ESI_Planificador,sizeof(paqueteSentencias),bufferSentencias);
	log_info(logger, "Me conecte con el Planificador. \n");

	free(bufferSentencias);
	free(paqueteSentencias);

	realizar_handshake(Planificador, cop_handshake_Planificador_ESI);

	t_paquete* paquetePlanif = recibir(Planificador);
	int ID = atoi(paquetePlanif->data);

	int i =0;

	for(i=0;list_size(instrucciones);i++){
		t_paquete* paquete = recibir(Planificador);
		t_esi_operacion* instruccionAEjecutar;
		instruccionAEjecutar = list_get(instrucciones,i);

		enviar(Coordinador,cop_ESI_Sentencia,sizeof(int),&instruccionAEjecutar);
		t_paquete* resultado = recibir(Coordinador);

			if(resultado->codigo_operacion==cop_Coordinador_Sentencia_Exito){
				ejecutar(instruccionAEjecutar);
				instrucciones[i];
			}else{
				i--;
			}
		}

	Coordinador = conectar_a(configuracion.IP_COORDINADOR,configuracion.PUERTO_COORDINADOR);
	realizar_handshake(Coordinador, cop_handshake_ESI_Coordinador);

	// Envia su ID al Coordinador
	int tamanio_buffer = sizeof(int);
	void * buffer = malloc(tamanio_buffer);
	int desp = 0;

	serializar_int(buffer, &desp, ID);
	enviar(Coordinador, cop_generico, tamanio_buffer, buffer);
	free(buffer);
	log_info(logger, "Me conecte con el Coordinador. \n");

	ejecutar_get("nombre");
	ejecutar_set("nombre", "cheja");
	ejecutar_get("nombre");
	ejecutar_store("nombre");


while(1) {}

	return EXIT_SUCCESS;
}

ESI_configuracion get_configuracion() {
	printf("Levantando archivo de configuracion del proceso ESI\n");
	ESI_configuracion configuracion;
	t_config* archivo_configuracion = config_create(pathESIConfig);
	configuracion.IP_COORDINADOR = get_campo_config_string(archivo_configuracion, "IP_COORDINADOR");
	configuracion.PUERTO_COORDINADOR = get_campo_config_string(archivo_configuracion, "PUERTO_COORDINADOR");
	configuracion.IP_PLANIFICADOR = get_campo_config_string(archivo_configuracion, "IP_PLANIFICADOR");
	configuracion.PUERTO_PLANIFICADOR = get_campo_config_string(archivo_configuracion, "PUERTO_PLANIFICADOR");
	return configuracion;
}

void leerScript(char* path_script){
	struct stat sb;
	if(access(path_script, F_OK) == -1) {
		FILE* fd=fopen(path_script, "a+");
		ftruncate(fileno(fd), sizeof(path_script)); //ver si es sb o path
		fclose(fd);
	}

	int fd=open(path_script, O_RDWR);
		fstat(fd, &sb);
		archivo= mmap(NULL,sb.st_size,PROT_READ | PROT_WRITE,  MAP_SHARED,fd,0);
}

void ejecutar_get(char* clave) {
	enviar(Coordinador,cop_Coordinador_Ejecutar_Get, size_of_string(clave), clave);
	t_paquete* paqueteValor = recibir(Coordinador);
	switch(paqueteValor->codigo_operacion) {
		case cop_Coordinador_Sentencia_Exito:
			printf("El valor de clave '%s' es '%s'. \n", clave, paqueteValor->data);
		break;

		case cop_Coordinador_Sentencia_Fallo_Clave_Tomada:
			printf("La clave '%s' se encuentra tomada por otro ESI. \n", clave);
		break;

		case cop_Coordinador_Sentencia_Exito_Clave_Sin_Valor:
			printf("La clave '%s' todavia no tiene ningun valor. \n", clave);
		break;

		case cop_Coordinador_Sentencia_Fallo_No_Instancias:
			printf("La operacion GET '%s' fallo. La instancia no se encuentra disponible.. \n", clave);
		break;

		case cop_Coordinador_Sentencia_Fallo_Clave_Larga:
			printf("La operacion GET '%s' fallo. La clave supera los 40 caracteres. \n", clave);
		break;
	}
	liberar_paquete(paqueteValor);
}

void ejecutar_set(char* clave, char* valor) {
	int tamanio_buffer = size_of_strings(2, clave, valor);
	void * buffer = malloc(tamanio_buffer);
	int desplazamiento = 0;
	serializar_string(buffer, &desplazamiento, clave);
	serializar_string(buffer, &desplazamiento, valor);
	enviar(Coordinador,cop_Coordinador_Ejecutar_Set, tamanio_buffer, buffer);
	free(buffer);

	t_paquete* paqueteResultadoOperacion = recibir(Coordinador);
	switch(paqueteResultadoOperacion->codigo_operacion) {
		case cop_Coordinador_Sentencia_Exito:
			printf("La operacion SET '%s' : '%s' fue ejecutada exitosamente. \n", clave, valor);
		break;

		case cop_Coordinador_Sentencia_Fallo_Clave_Tomada:
			printf("La clave '%s' se encuentra tomada por otro ESI. \n", clave);
		break;

		case cop_Coordinador_Sentencia_Fallo_No_Instancias:
			printf("La operacion SET '%s' : '%s' fallo. No hay instancias en el sistema. \n", clave, valor);
		break;

		case cop_Coordinador_Sentencia_Fallo_Clave_Larga:
			printf("La operacion SET '%s' : '%s' fallo. La clave supera los 40 caracteres. \n", clave, valor);
		break;
	}
	liberar_paquete(paqueteResultadoOperacion);
}

void ejecutar_store(char* clave) {
	enviar(Coordinador,cop_Coordinador_Ejecutar_Store, size_of_string(clave), clave);
	t_paquete* paqueteResultadoOperacion = recibir(Coordinador);
	switch(paqueteResultadoOperacion->codigo_operacion) {
		case cop_Coordinador_Sentencia_Exito:
			printf("La operacion STORE '%s' fue ejecutada exitosamente. \n", clave);
		break;

		case cop_Coordinador_Sentencia_Fallo_Clave_Tomada:
			printf("La clave '%s' se encuentra tomada por otro ESI. \n", clave);
		break;

		case cop_Coordinador_Sentencia_Fallo_No_Instancias:
			printf("La operacion STORE '%s' fallo. La instancia no se encuentra disponible. Recurso liberada pero no guardado. \n", clave);
		break;

		case cop_Coordinador_Sentencia_Fallo_Clave_Larga:
			printf("La operacion STORE '%s' fallo. La clave supera los 40 caracteres. \n", clave);
		break;
	}
	liberar_paquete(paqueteResultadoOperacion);
}

void ejecutar(t_esi_operacion* instruccionAEjecutar) {
//todo
	return;
}


