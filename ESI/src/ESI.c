#include <stdio.h>
#include <stdlib.h>
#include "ESI.h"

int main(void) {
	imprimir("/home/utnso/workspace/tp-2018-1c-PuntoZip/ESI/esi_image.txt");
	char* fileLog;
	fileLog = "ESI_logs.txt";


	logger = log_create(fileLog, "ESI Logs", 1, 1);
	log_info(logger, "Inicializando proceso ESI. \n");

	ESI_configuracion configuracion = get_configuracion();
	log_info(logger, "Archivo de configuracion levantado. \n");

	un_socket Coordinador = conectar_a(configuracion.IP_COORDINADOR,configuracion.PUERTO_COORDINADOR);
	realizar_handshake(Coordinador, cop_handshake_ESI_Coordinador);
	int tamanio = 0; //Calcular el tamanio del paquete
	void* buffer = malloc(tamanio); //Info que necesita enviar al coordinador.
	enviar(Coordinador,cop_generico,tamanio,buffer);
	log_info(logger, "Me conecte con el Coordinador. \n");

	un_socket Planificador = conectar_a(configuracion.IP_PLANIFICADOR,configuracion.PUERTO_PLANIFICADOR);
	realizar_handshake(Planificador, cop_handshake_ESI_Planificador);
	int tamanio1 = 0; //Calcular el tamanio del paquete
	void* buffer1 = malloc(tamanio1); //Info que necesita enviar al Planificador.
	enviar(Planificador,cop_generico,tamanio1,buffer1);
	log_info(logger, "Me conecte con el Planificador. \n");

	char* path_script = "pathProvisorio.txt"; // ver de donde sacar el script
	leerScript(path_script);


	int desplazamiento=0;

	void* bufferSentencias = malloc(2*sizeof(int));
	paqueteSentencias* paqueteSentencias = malloc(sizeof(paqueteSentencias));


	memcpy(bufferSentencias+desplazamiento, &paqueteSentencias->cantidadInstrucciones,sizeof(int));
	desplazamiento+=sizeof(int);
	memcpy(bufferSentencias+desplazamiento, &paqueteSentencias->idESI, sizeof(int));
	desplazamiento+=sizeof(int);
	enviar(Planificador,cop_ESI_Sentencia,sizeof(paqueteSentencias),bufferSentencias);



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

char** enviarAlParser(void* archivo){
	char** instrucciones = NULL; // a la espera del parser
	return instrucciones;

}

char* ejecutar(){

}









