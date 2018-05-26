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

	Coordinador = conectar_a(configuracion.IP_COORDINADOR,configuracion.PUERTO_COORDINADOR);
	realizar_handshake(Coordinador, cop_handshake_ESI_Coordinador);
	int tamanio = 0; //Calcular el tamanio del paquete
	void* buffer = malloc(tamanio); //Info que necesita enviar al coordinador.
	enviar(Coordinador,cop_generico,tamanio,buffer);
	log_info(logger, "Me conecte con el Coordinador. \n");

	ejecutar_get("nombre");

	while(1) {}

	un_socket Planificador = conectar_a(configuracion.IP_PLANIFICADOR,configuracion.PUERTO_PLANIFICADOR);
	realizar_handshake(Planificador, cop_handshake_ESI_Planificador);
	int tamanio1 = 0; //Calcular el tamanio del paquete
	void* buffer1 = malloc(tamanio1); //Info que necesita enviar al Planificador.
	enviar(Planificador,cop_generico,tamanio1,buffer1);
	log_info(logger, "Me conecte con el Planificador. \n");

	char* path_script = "pathProvisorio.txt"; // ver de donde sacar el script
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

	fclose(archivo);
	if (line)
	free(line);

	int desplazamiento=0;

	void* bufferSentencias = malloc(2*sizeof(int));
	paqueteSentencias* paqueteSentencias = malloc(sizeof(paqueteSentencias));

	paqueteSentencias->cantidadInstrucciones = list_size(instrucciones);

	memcpy(bufferSentencias+desplazamiento, &paqueteSentencias->cantidadInstrucciones,sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(bufferSentencias+desplazamiento, &paqueteSentencias->idESI, sizeof(int));
	desplazamiento+=sizeof(int);
	enviar(Planificador,cop_ESI_Sentencia,sizeof(paqueteSentencias),bufferSentencias);

	int instruccionAEjecutar;

	while(paqueteSentencias->cantidadInstrucciones != 0 ){
		t_paquete* paquete = recibir(Planificador);
		memcpy(&instruccionAEjecutar,paquete->data,sizeof(int));
		paqueteSentencias->cantidadInstrucciones--;
		// todo ver que pasa si se desconecta del planificador
	}

	enviar(Coordinador,cop_ESI_Sentencia,sizeof(int),instruccionAEjecutar);

	while(1){
		t_paquete* resultado = recibir(Coordinador);
		enviar(Planificador, cop_Coordinador_Sentencia_Exito, sizeof(int),resultado);
	}

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
			printf("El valor de clave '%s' es '%s' \n", clave, paqueteValor->data);
		break;

		case cop_Coordinador_Sentencia_Fallo_Clave_Tomada:
			printf("La clave '%s' se encuentra tomada por otro ESI \n", clave);
		break;

		case cop_Coordinador_Sentencia_Fallo_Clave_No_Ingresada:
			printf("La clave '%s' no fue ingresada en el sistema \n", clave);
		break;
	}
	liberar_paquete(paqueteValor);
}


