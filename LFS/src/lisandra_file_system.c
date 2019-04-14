/*
 ============================================================================
 Name        : lisandra_file_system.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description :
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h> //malloc,alloc,realloc
#include <string.h>
#include<readline/readline.h>
#include <time.h>
#include <commons/string.h>


#define CANTPARTICIONES 5 // esto esta en el metadata

typedef enum{
	SELECT, //Select
	INSERT, //Insert
	CREATE, //Create
	DESCRIBEALL, //Describe All
	DESCRIBETABLE, //Describe Table
	DROP //Drop
}casos;

typedef enum {
	SC,
	SH,
	EC
}consistencia;

typedef struct {
	time_t timestamp;
	u_int16_t key;
	char* value;  //no seria siempre un char*?
} registro;

typedef struct {
	int numeroBloque;
	int sizeDeBloque;

} bloque;

typedef struct {
	int size;
	int numeroParticion; // para saber que keys estan ahi,por el modulo
	registro *registros;
	bloque block[/*CANTIDADBLOQUES*/];
} particion;

typedef struct {
	char* nombre;
	int cantParticiones;
	particion particiones[CANTPARTICIONES]; //HAY QUE VER COMO HACER QUE DE CADA PARTICION SALGAN SUS REGISTROS.
	consistencia tipoDeConsistencia;
	int tiempoDeCompactacion;
} tabla;

void api(casos caso){
	//leerConsola();
	switch (caso){
		case SELECT:
			//Select
			break;
		case INSERT:
			//Insert
			break;
		case CREATE:
			//Create
			break;
		case DESCRIBEALL:
			//Describe todas las tablas(All)
			break;
		case DESCRIBETABLE:
			//Describe una tabla
			break;
		case DROP:
			//Drop table
			break;
		default:
			printf("Error del header");
			//agregar al archivo de log
	}
}

void agregarRegistro(tabla unaTabla,registro unRegistro){

}

int main(int argc, char* argv[]) {

	return EXIT_SUCCESS;
}

