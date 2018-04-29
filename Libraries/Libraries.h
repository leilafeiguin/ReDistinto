#ifndef LIBRARIES_H_
#define LIBRARIES_H_

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <commons/string.h>
#include <commons/error.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <unistd.h>
#include <assert.h>
#include <commons/collections/list.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/if_link.h>
#include <ifaddrs.h>

#define MAX_LEN 128


enum codigos_de_operacion {
	cop_generico = 0,
	cop_archivo = 1,

	cop_handshake_ESI_Coordinador = 10,
	cop_handshake_ESI_Planificador = 11,
	cop_handshake_Planificador_Coordinador = 12,
	cop_handshake_Instancia_Coordinador = 13,

	cop_Coordinador_Ejecutar = 20,
	cop_Instancia_Ejecucion_Fallo_TC = 21,
	cop_Instancia_Ejecucion_Fallo_CNI = 22,
	cop_Instancia_Ejecucion_Fallo_CI = 23,
	cop_Instancia_Ejecucion_Exito = 24,
	cop_Instancia_Ejecutar_Set = 25,
	cop_Instancia_Ejecutar_Get = 29,
	cop_Instancia_Guardar_OK = 26,
	cop_Instancia_Guardar_Error_FE = 27,
	cop_Instancia_Guardar_Error_FI = 28,

	cop_Planificador_Ejecutar_Sentencia = 30,
	cop_ESI_Sentencia = 31,
	cop_Coordinador_Sentencia_Exito = 32,
	cop_Coordinador_Sentencia_Fallo_TC = 33,
	cop_Coordinador_Sentencia_Fallo_CNI = 34,
	cop_Coordinador_Sentencia_Fallo_CI = 35,

	cop_handshake_Planificador_ejecucion = 50
};

typedef int un_socket;

typedef struct {
	int codigo_operacion;
	int tamanio;
	void * data;
} t_paquete;

typedef struct {
	int id_ESI;
	int estado; //Mirar enum estados
	un_socket socket;
	int cantidad_instrucciones;
	char* descripcion_estado;
} t_ESI;

typedef struct {
	char* clave; // Si esta vacia entonces la entrada esta vacia
	char* contenido; // Contenido de la entrada. Puede contener todo el valor de la clave o solo una parcialidad.
	int id; // Numero de entrada
	int espacio_ocupado;	// En bytes
	int cant_veces_no_accedida;
} t_entrada;

/*
 Observacion de la estructura de la instancia:

 Es redundante guardar el listado de las keys contenidas ya que se podria iterar sobre la tabla de entradas,
 pero se decidio hacerlo asi para ganar performance.
*/

typedef struct {
	char* nombre;
	un_socket socket;
	int estado;
	t_list * keys_contenidas; // Lista de keys que actuamente estan guardadas en la instancia
	t_list * entradas; // Tabla de entradas de la instancia
	int cant_entradas_ocupadas;
} t_instancia;

enum estados_instancia {
	desconectada = 0,
	conectada = 1
};

enum estados {
	listo = 0,
	ejecutando = 1,
	bloqueado = 2,
	finalizado = 3
};


/**	@NAME: conectar_a
 * 	@DESC: Intenta conectarse.
 * 	@RETURN: Devuelve el socket o te avisa si hubo un error al conectarse.
 *
 */

un_socket conectar_a(char *IP, char* Port);

/**	@NAME: socket_escucha
 * 	@DESC: Configura un socket que solo le falta hacer listen.
 * 	@RETURN: Devuelve el socket listo para escuchar o te avisa si hubo un error al conectarse.
 *
 */

un_socket socket_escucha(char* IP, char* Port);

/**	@NAME: enviar
 * 	@DESC: Hace el envio de la data que le pasamos. No hay que hacer más nada.
 *
 */

void enviar(un_socket socket_envio, int codigo_operacion, int tamanio,
		void * data);

/**	@NAME: recibir
 * 	@DESC: devuelve un paquete que está en el socket pedido
 *
 */
t_paquete* recibir(un_socket socket_para_recibir);

/**	@NAME: aceptar_conexion
 * 	@DESC: devuelve el socket nuevo que se quería conectar
 *
 */
un_socket aceptar_conexion(un_socket socket_escuchador);

/**	@NAME: liberar_paquete
 * 	@DESC: libera el paquete y su data.
 *
 */
void liberar_paquete(t_paquete * paquete);

/**	@NAME: realizar_handshake
 *
 */
bool realizar_handshake(un_socket socket_del_servidor, int cop_handshake);

/**	@NAME: esperar_handshake
 *
 */
bool esperar_handshake(un_socket socket_del_cliente,
		t_paquete* inicio_del_handshake, int cop_handshake);

char get_campo_config_char(t_config* archivo_configuracion, char* nombre_campo);

int get_campo_config_int(t_config* archivo_configuracion, char* nombre_campo);

char** get_campo_config_array(t_config* archivo_configuracion,
		char* nombre_campo);

char* get_campo_config_string(t_config* archivo_configuracion,
		char* nombre_campo);

void enviar_archivo(un_socket socket, char* path);

bool comprobar_archivo(char* path);

char* obtener_mi_ip();

void print_image(FILE *fptr);

void imprimir(char*);

char** str_split(char* a_str, const char a_delim);

char *str_replace(char *orig, char *rep, char *with);

int countOccurrences(char * str, char * toSearch);

char *randstring(size_t length);

unsigned long int lineCountFile(const char *filename);

void crear_subcarpeta(char* nombre);

char* generarDirectorioTemporal(char* carpeta);

void *get_in_addr(struct sockaddr *sa);

int size_of_string(char* string);

char* string_joins(char** strings);


#endif /* LIBRARIES_H_ */
