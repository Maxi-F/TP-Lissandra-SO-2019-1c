/*
 * structs-basicos.h
 *
 *  Created on: 25 abr. 2019
 *      Author: utnso
 */

#ifndef KERNEL_STRUCTS_BASICOS_H_
#define KERNEL_STRUCTS_BASICOS_H_
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commonsPropias/serializacion.h>
#include <stdlib.h>

typedef struct{
	char* operacion;
	int ejecutado; //0 si no se ejecuto, 1 si se ejecuto
	t_list* instruccion; //fila
	//t_list* pcb_siguiente;  //columna
	//TODO agregar mas campos 1
}pcb;
typedef struct{
	char* operacion;
	int ejecutado; //0 si no se ejecuto, 1 si se ejecuto
	//t_list* instruccion_siguiente;
	//TODO agregar mas campos 2
}instruccion;
/*typedef enum {
	SC, // UNA
	SH, // MUCHAS
	EC  // MUCHAS
}criterios;
*/
typedef struct{
	int numeroDeMemoria;
	consistencia *criterioAsociado; //malloc dps de saber cuantos criterios me devuelve el pool
}memoria;

typedef struct{
	consistencia unCriterio;
	int *memoriasAsociadas; //malloc dps de saber cuantas memorias me devuelve el pool
}criterio;

typedef struct{
	char* nombreDeTabla;
	consistencia criterioDeTabla;
}tabla; //ver si es necesario agregar algo mas

typedef enum {
	INSERT,
	CREATE,
	DESCRIBETABLE,
	DESCRIBEALL,
	DROP,
	JOURNAL,
	SELECT,
	RUN,
	METRICS,
	ADD
}caso;

typedef struct {
	t_config* config;
	t_log* log;
} configYLogs;

#endif /* KERNEL_STRUCTS_BASICOS_H_ */