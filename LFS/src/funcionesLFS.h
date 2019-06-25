#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h> //malloc,alloc,realloc
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/io.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include "utils.h"
#include "compactador.h"


typedef enum {
	INSERT,
	CREATE,
	DESCRIBETABLE,
	DESCRIBEALL,
	DROP,
	JOURNAL,
	SELECT
} operacion;
registroConNombreTabla* registroError;

typedef struct {
	char* nombre;
	t_list* temporales;
} tablaTmp;


//Funciones


int verificarExistenciaDirectorioTabla(char* nombreTabla);
metadata* obtenerMetadata(char* nombreTabla); //habria que ver de pasarle la ruta de la tabla y de ahi buscar el metadata
int calcularParticion(int key,int cantidadParticiones);// Punto_Montaje/Tables/Nombre_tabla/Metadata
char* infoEnBloque(char* numeroBloque);
bool estaLaKey(int key,void* elemento);
bool esIgualAlNombre(char* nombreTabla,void * elemento);
bool agregarRegistro(char* nombreTabla, registro* unRegistro, tablaMem * tabla); //este es para memtable
registro* devolverRegistroDeMayorTimestampDeLaMemtable(t_list* listaRegistros, t_list* memtable, char* nombreTabla, int key);
void liberarDoblePuntero(char** doblePuntero);
void funcionSelect(char* argumentos,int socket);
void funcionInsert(char* argumentos,int socket);
//void guardarInfoEnArchivo(char* ruta, char* info);
void crearMetadata(char* ruta, char* consistenciaTabla, char* numeroParticiones, char* tiempoCompactacion);
void crearParticiones(char* ruta, int numeroParticiones); //se puede usar para los temporales.
void funcionCreate(char* argumentos,int socket);
int tamanioRegistros(char* nombreTabla);
int obtenerCantTemporales(char* nombreTabla);
void funcionDescribe(char* argumentos,int socket); //despues quizas haya que cambiar el tipo
void inicializarRegistroError();
pthread_mutex_t devolverSemaforoDeTabla(char* nombreTabla);
void funcionDrop(char* nombreTabla,int socket);
void agregarTablaALista(char* nombreTabla);
void cargarInfoDeTmpYParticion(char** buffer, char* nombreTabla,char** arrayDeParticion);

void inicializarRegistroError(){
	registroError = malloc(sizeof(registro));
	registroError->timestamp = 1;
	registroError->key = 1;
	registroError->value = string_duplicate("Error");
	registroError->nombreTabla = string_duplicate("1");
}


int verificarExistenciaDirectorioTabla(char* nombreTabla){
	int validacion;
	string_to_upper(nombreTabla);
	char* rutaDirectorio= string_new();
	string_append(&rutaDirectorio,puntoMontaje); //OJO ACA HAY QUE VER QUE EN EL CONFIG NO TE VENGA CON "" EL PUNTO DE MONTAJE
	string_append(&rutaDirectorio,"Tables/");
	string_append(&rutaDirectorio,nombreTabla);
	struct stat sb;
	pthread_mutex_lock(&mutexLoggerConsola);
	log_info(loggerConsola,"Determinando existencia de tabla en la ruta: %s",rutaDirectorio);
	pthread_mutex_unlock(&mutexLoggerConsola);
	if (stat(rutaDirectorio, &sb) == 0 && S_ISDIR(sb.st_mode))
	    {

			enviarOMostrarYLogearInfo(-1,"La tabla existe en el FS");
	    	validacion=1;
	    }
	    else
	    {
	    	pthread_mutex_lock(&mutexLoggerConsola);
	    	log_info(loggerConsola,"No existe tabla en ruta indicada %s \n",rutaDirectorio);
	    	pthread_mutex_unlock(&mutexLoggerConsola);
	    	validacion=0;
	    }
	free(rutaDirectorio);
	return validacion;
}

bool estaLaKey(int key,void* elemento){
	registro* unRegistro = (registro*) elemento;
//guarda basura en el value
	return (unRegistro->key == key);

}

//encontrar el nombre de la tabla, la tabla
//find y encontras la key

bool esIgualAlNombre(char* nombreTabla,void * elemento){
		tablaMem* tabla = elemento;

		return string_equals_ignore_case(tabla->nombre, nombreTabla);
}


bool agregarRegistro(char* nombreTabla, registro* unRegistro, tablaMem* tabla){

		if (string_equals_ignore_case(tabla->nombre, nombreTabla)){
			list_add(tabla->listaRegistros, unRegistro);
			pthread_mutex_lock(&mutexLogger);
			log_info(logger, "Se añadio registro");
			pthread_mutex_unlock(&mutexLogger);
			return true;
		}else{
			return false;
		}

}

//Guarda un registro en la memtable
void guardarRegistro(registro* unRegistro, char* nombreTabla) {

	bool buscarPorNombre(tablaMem* elemento){
		return agregarRegistro(nombreTabla, unRegistro, elemento);
	}
	pthread_mutex_lock(&mutexMemtable);
	if(!list_find(memtable, buscarPorNombre)){

		tablaMem* nuevaTabla = malloc(sizeof(tablaMem));
						nuevaTabla->nombre = string_duplicate(nombreTabla);
						nuevaTabla->listaRegistros = list_create();
						list_add(nuevaTabla->listaRegistros, unRegistro);
						list_add(memtable, nuevaTabla);
						//pthread_mutex_lock(&mutexLogger);
						log_info(loggerConsola, "Se añadio la tabla a la memtable");
						//pthread_mutex_unlock(&mutexLogger);
	}
	pthread_mutex_unlock(&mutexMemtable);
}



registro* devolverRegistroDeMayorTimestampDeLaMemtable(t_list* listaRegistros, t_list* memtable, char* nombreTabla, int key){


	bool encontrarLaKey(void *elemento){
		return estaLaKey(key, elemento);
	}

	void* cualEsElMayorTimestamp(void *elemento1, void *elemento2){
			registro* primerElemento = elemento1;
			registro* segundoElemento = elemento2;
			return devolverMayor(primerElemento, segundoElemento);
	}

	bool tieneElNombre(void *elemento){
	return esIgualAlNombre(nombreTabla, elemento);
	}

	void* liberarRegistro(registro* registro) {
		free(registro);
	}
	if(memtable->elements_count== 0){
		enviarYLogearMensajeError(-1,"Error la memtable esta vacia");
	}

	tablaMem* encuentraLista =  list_find(memtable, tieneElNombre);

//SAQUE SEMAFORO DEADLOCK !!!
	t_list* registrosConLaKeyEnMemtable = list_filter(encuentraLista->listaRegistros, encontrarLaKey);

	if (registrosConLaKeyEnMemtable->elements_count == 0){
		enviarOMostrarYLogearInfo(-1,"La key buscada no se encuentra en la memtable");
		return NULL;
	}

	registro* registroDeMayorTimestamp= list_fold(registrosConLaKeyEnMemtable, list_get(registrosConLaKeyEnMemtable,0), cualEsElMayorTimestamp);

	enviarOMostrarYLogearInfo(-1,"Registro encontrado en la memtable");

//	list_destroy_and_destroy_elements(registrosConLaKeyEnMemtable, liberarRegistro);

return registroDeMayorTimestamp;

}


metadata* obtenerMetadata(char* nombreTabla){
	string_to_upper(nombreTabla);
	t_config* configMetadata;
	int cantParticiones;
	int tiempoCompactacion;
	consistencia tipoConsistencia;
	metadata* unaMetadata= malloc(sizeof(metadata)); //ver si es necesario malloc, no me acuerdo porque lo pusimos


	char* ruta = string_new();
	string_append(&ruta, puntoMontaje);
	string_append(&ruta,"Tables/");
	string_append(&ruta,nombreTabla);
	string_append(&ruta,"/Metadata");

	configMetadata = config_create(ruta);

	cantParticiones = config_get_int_value(configMetadata, "PARTITIONS");
	tipoConsistencia = config_get_int_value(configMetadata, "CONSISTENCY"); //delegar a funcion con strcmp
	tiempoCompactacion = config_get_int_value(configMetadata, "COMPACTION_TIME"); //OJO ES COMPACTION TIME Y NO COMPACTATION

	unaMetadata->cantParticiones = cantParticiones;
	unaMetadata->tipoConsistencia = tipoConsistencia;
	unaMetadata->tiempoCompactacion = tiempoCompactacion;
	unaMetadata->nombreTabla = string_duplicate(nombreTabla);

	config_destroy(configMetadata);
	free(ruta);

	return unaMetadata;


}


void cargarInfoDeBloques(char*** buffer, char**arrayDeBloques){
	int i = 0;
		while(*(arrayDeBloques+i)!= NULL){
							char* informacion = infoEnBloque(*(arrayDeBloques+i));
							string_append(*buffer, informacion);
							i++;
						}
}

void cargarInfoDeTmpYParticion(char** buffer, char* nombreTabla,char** arrayDeParticion){
		t_config* part;
		int numeroTmp = obtenerCantTemporales(nombreTabla);

		for (int i = 0; i< numeroTmp; i++){
			char* ruta = string_new();
			char* numeroTmp =string_itoa(i);
			string_append(&ruta,puntoMontaje);
			string_append(&ruta,"Tables/");
			string_append(&ruta,nombreTabla);
			string_append(&ruta,"/"); //vamos a usar la convension partN.bin
			string_append(&ruta,numeroTmp);
			string_append(&ruta,".tmp");
			part = config_create(ruta);
			char** arrayDeBloques = config_get_array_value(part,"BLOCKS");


			cargarInfoDeBloques(&buffer, arrayDeBloques);

			free(numeroTmp);
			free(ruta);
			liberarDoblePuntero(arrayDeBloques);
			config_destroy(part);

		}
		cargarInfoDeBloques(&buffer, arrayDeParticion); //aca cargas lo de la particion

}

void cargarInfoDeBloque(char** arrayDeBloques, int sizeParticion, t_list* listaRegistros, char* buffer){

	int i = 0;

	while(*(arrayDeBloques+i)!= NULL){
			char* informacion = infoEnBloque(*(arrayDeBloques+i));
			string_append(&buffer, informacion);
			i++;
		}

}

registro* devolverRegistroDeListaDeRegistros(t_list* listaRegistros, int key, int socket ){
	registro* registroBuscado;
	bool encontrarLaKey(void *elemento){
					return estaLaKey(key, elemento);
				}
	void* cualEsElMayorTimestamp(void *elemento1, void *elemento2){
			registro* primerElemento = elemento1;
			registro* segundoElemento = elemento2;

			return devolverMayor(primerElemento, segundoElemento);

		}

		t_list* registrosConLaKeyEnListaRegistros = list_filter(listaRegistros, encontrarLaKey);
					if (registrosConLaKeyEnListaRegistros->elements_count == 0){
						enviarYLogearMensajeError(socket,"No se encuentra la key\n");
						return NULL;
						}
					else{
				registroBuscado= list_fold(registrosConLaKeyEnListaRegistros, list_get(registrosConLaKeyEnListaRegistros,0), cualEsElMayorTimestamp);
				enviarOMostrarYLogearInfo(-1,"Registro encontrado en bloques");
				}
				return registroBuscado;
}


void funcionSelect(char* argumentos,int socket){ //en la pos 0 esta el nombre y en la segunda la key
	char** argSeparados = string_n_split(argumentos,2," ");
	char* nombreTabla = *(argSeparados+0);
	char* particion;
	t_config* part;
	char* ruta = string_new();
	t_list* listaRegistros = list_create();
	int key = atoi(*(argSeparados+1));
	registro* registroBuscado;
	bool encontrarLaKey(void *elemento){
			return estaLaKey(key, elemento);
		}


	void liberarRegistro(registro* registro) {
			free(registro);
		}



	void* cualEsElMayorTimestamp(void *elemento1, void *elemento2){
		registro* primerElemento = elemento1;
		registro* segundoElemento = elemento2;

		return devolverMayor(primerElemento, segundoElemento);

	}
	if(verificarExistenciaDirectorioTabla(nombreTabla) ==0)
		{
	//	enviarOMostrarYLogearInfo(socket,"No se encontro el directorio");
		}
	else{
		enviarOMostrarYLogearInfo(-1,"Directorio de tabla valido");

		metadata* metadataTabla = obtenerMetadata(nombreTabla);


		enviarOMostrarYLogearInfo(-1,"Metadata cargado");

		particion = string_itoa(calcularParticion(key,metadataTabla->cantParticiones)); //cant de particiones de la tabla
		liberarMetadata(metadataTabla);
		char* buffer = string_new();
		string_append(&ruta,puntoMontaje);
		string_append(&ruta,"Tables/");
		string_append(&ruta,nombreTabla);
		string_append(&ruta,"/part"); //vamos a usar la convension PartN.bin
		string_append(&ruta,particion);
		string_append(&ruta,".bin");
		part = config_create(ruta);


		char** arrayDeBloques = config_get_array_value(part,"BLOCKS");
		pthread_mutex_t semaforoDeTabla =devolverSemaforoDeTabla(nombreTabla);
		pthread_mutex_lock(&semaforoDeTabla);
		cargarInfoDeTmpYParticion(&buffer, nombreTabla,arrayDeBloques); //poner nombre y de particion
		separarRegistrosYCargarALista(buffer, listaRegistros);
		pthread_mutex_unlock(&semaforoDeTabla);
		enviarOMostrarYLogearInfo(-1,"Informacion de bloques cargada");


		//habria que hacer el mismo while si hay temporales if(hayTemporales) habria que ver el tema de cuantos temporales hay, quizas convendria agregarlo en el metadata tipo array
		//puts(buffer);
		//y aca afuera haria la busqueda del registro.
		pthread_mutex_lock(&mutexMemtable);

		if (memtable->elements_count == 0){
			enviarOMostrarYLogearInfo(-1,"El registro no se encuentra en la memtable");

			registroBuscado = devolverRegistroDeListaDeRegistros(listaRegistros, key, socket);
		} else{
			if (!(registroBuscado = devolverRegistroDeMayorTimestampDeLaMemtable(listaRegistros, memtable,*(argSeparados+0), key))){
				soloLoggear(socket,"El registro no se encuentra en la memtable");
			registroBuscado = devolverRegistroDeListaDeRegistros(listaRegistros,key, socket); //me pa que esto no va aca
			}
			}

		pthread_mutex_unlock(&mutexMemtable);

		//if((devolverRegistroDeMayorTimestampYAgregarALista(listaRegistros, memtable,*(argSeparados+0), key)) == 0) return NULL;

		liberarDoblePuntero(arrayDeBloques);
		liberarDoblePuntero(argSeparados);

		config_destroy(part);
		free (ruta);
		list_destroy_and_destroy_elements(listaRegistros, (void*) liberarRegistro);
		free(buffer);
		free(particion);

		soloLoggear(socket,"Se encontro el registro buscado");

		if(socket!=-1){
			if(!registroBuscado) {
				serializarYEnviarRegistro(socket, registroError);
				return;
			}
			else {
				serializarYEnviarRegistro(socket,armarRegistroConNombreTabla(registroBuscado,*(argSeparados+0)));
				pthread_mutex_lock(&mutexLogger);
				log_info(logger,"El value del registro buscado es",registroBuscado->value);
				pthread_mutex_unlock(&mutexLogger);
				return;
			}
		}
		pthread_mutex_lock(&mutexLoggerConsola);
		log_info(loggerConsola,"El value del registro buscado es %d ",registroBuscado->value);
		pthread_mutex_unlock(&mutexLoggerConsola);
	}
}


void funcionInsert(char* argumentos,int socket) {

	//verificar que no se exceda el tamaño del value, tamanioValue (var. global
	char** argSeparados = string_n_split(argumentos,3,"\"");
	char* nombreYKey = *(argSeparados + 0);
	char** separarNombreYKey = string_split(nombreYKey, " ");
	char* nombreTabla = *(separarNombreYKey + 0);
	int key = atoi(*(separarNombreYKey + 1));
	char* value = *(argSeparados + 1);

	char* valorTimestamp = *(argSeparados + 2);
	int timestamp;

	if (!verificarExistenciaDirectorioTabla(nombreTabla)){
		liberarDoblePuntero(separarNombreYKey);
		liberarDoblePuntero(argSeparados);
		return;
	}
	soloLoggear(socket,"Directorio de tabla valido");

	if (valorTimestamp == NULL) {
		timestamp = (unsigned long)time(NULL);
	}else{
		timestamp = atoi(valorTimestamp);
	}

	registro* registroDePrueba = malloc(sizeof(registro));
	registroDePrueba -> key = key;
	registroDePrueba -> value= string_duplicate(value);
	registroDePrueba -> timestamp = timestamp;

	guardarRegistro(registroDePrueba, nombreTabla);
	enviarOMostrarYLogearInfo(-1,"Se guardo registro");

	liberarDoblePuntero(separarNombreYKey);
	liberarDoblePuntero(argSeparados);

}

void crearMetadata(char* ruta, char* consistenciaTabla, char* numeroParticiones, char* tiempoCompactacion) {

	char* infoDelMetadata = string_new();
	char* rutaMetadata = string_new();

	string_append(&infoDelMetadata, "CONSISTENCY=");
	string_append(&infoDelMetadata, consistenciaTabla);
	string_append(&infoDelMetadata, "\n");
	string_append(&infoDelMetadata, "PARTITIONS=");
	string_append(&infoDelMetadata, numeroParticiones);
	string_append(&infoDelMetadata, "\n");
	string_append(&infoDelMetadata, "COMPACTION_TIME=");
	string_append(&infoDelMetadata, tiempoCompactacion);

	string_append(&rutaMetadata, ruta);
	string_append(&rutaMetadata, "/Metadata");


	guardarInfoEnArchivo(rutaMetadata, infoDelMetadata);


	free(infoDelMetadata);
	free(rutaMetadata);
}

void crearParticiones(char* ruta, int numeroParticiones) {

	char* bloqueLibre;

	for (int i= 0; i < numeroParticiones; i++){

		char * rutaDeLaParticion = string_new();
		char* infoAGuardar = string_new();
		char* numeroParticion = string_itoa(i);

		string_append(&rutaDeLaParticion, ruta);
		string_append(&rutaDeLaParticion, "/part");
		string_append(&rutaDeLaParticion, numeroParticion);
		string_append(&rutaDeLaParticion, ".bin");

		string_append(&infoAGuardar, "SIZE=0"); //cuando se crea esta vacio el bloque, es 0
		string_append(&infoAGuardar, "\n");
		string_append(&infoAGuardar, "BLOCKS=[");
		bloqueLibre = devolverBloqueLibre();
		string_append(&infoAGuardar, bloqueLibre);
		string_append(&infoAGuardar, "]");

		guardarInfoEnArchivo(rutaDeLaParticion, infoAGuardar);

		free(infoAGuardar);
		free(rutaDeLaParticion);
		free(numeroParticion);
		free(bloqueLibre);
	}

}

void funcionCreate(char* argumentos,int socket) {


	char** argSeparados = string_n_split(argumentos,4," ");

	//es todo char porque cuando lo guardes en el metadata se guarda como caracteres
	char* nombreTabla = *(argSeparados + 0);
	string_to_upper(nombreTabla); //para que nos quede en el file system todo en mayuscula
    char* consistenciaTabla = *(argSeparados + 1);
	char* numeroParticiones = *(argSeparados + 2);
	char* tiempoCompactacion = *(argSeparados + 3);

	char* directorioTabla = string_new();
	if (!verificarExistenciaDirectorioTabla(nombreTabla)){

		string_append(&directorioTabla, puntoMontaje);
		string_append(&directorioTabla, "Tables/");
		string_append(&directorioTabla, nombreTabla);

		mkdir(directorioTabla, 0777);

		crearMetadata(directorioTabla, consistenciaTabla, numeroParticiones, tiempoCompactacion);
		int cantidadParticiones = atoi(numeroParticiones);
		crearParticiones(directorioTabla, cantidadParticiones);
		enviarOMostrarYLogearInfo(socket,"Se creo la tabla");
		agregarTablaALista(nombreTabla);

	}else{
		enviarOMostrarYLogearInfo(socket,"Error ya existe la tabla en FS");
		liberarDoblePuntero(argSeparados);
		free(directorioTabla);
		return;
	}
	liberarDoblePuntero(argSeparados);

	free(directorioTabla);

}

//hacerlo por tabla!! y que reciba el nombre de la tabla
int tamanioRegistros(char* nombreTabla){

	int tamanioTotal = 0;

	bool tieneElNombre(void *elemento){
		return esIgualAlNombre(nombreTabla, elemento);
	}

	void* sumarRegistros(int valor , registro* registro ){
		tamanioTotal = tamanioTotal + sizeof(registro->key)  + sizeof(registro->timestamp) + (strlen(registro->value) + 1);
	}

	pthread_mutex_lock(&mutexMemtable);
	tablaMem* encuentraTabla =  list_find(memtable, tieneElNombre);
	pthread_mutex_unlock(&mutexMemtable);

	list_fold(encuentraTabla->listaRegistros, 0, sumarRegistros);

return tamanioTotal;
}



void funcionDrop(char* nombreTabla,int socket){
	bool liberarTablaConEsteNombre(metadataConSemaforo* unMetadata){
		return string_equals_ignore_case(unMetadata->nombreTabla,nombreTabla);
}
	if(verificarExistenciaDirectorioTabla(nombreTabla)){
		char* ruta = string_new();
		string_append(&ruta,puntoMontaje);
		string_append(&ruta,"Tables/");
		string_append(&ruta,nombreTabla);
		DIR* dir=opendir(ruta);
		enviarOMostrarYLogearInfo(-1,"Liberando Bloques de los tmp y las particiones");
		struct dirent *sd;
		while((sd=readdir(dir))!=NULL){
			if (string_equals_ignore_case(sd->d_name, ".") || string_equals_ignore_case(sd->d_name, "..") ){continue;}
			liberarBloquesDeTmpYPart(sd->d_name,ruta);
		}
		rmdir(ruta);
		closedir(dir);
		//ver tema de un semaforo aca pero de la tabla
		pthread_mutex_lock(&mutexListaDeTablas);
		list_remove_and_destroy_by_condition(listaDeTablas,liberarTablaConEsteNombre,(void*) liberarMetadataConSemaforo);
		pthread_mutex_unlock(&mutexListaDeTablas);
		enviarOMostrarYLogearInfo(socket,"Se elimino la tabla");
		free(ruta);
		return;
	}
	enviarOMostrarYLogearInfo(socket,"No se encontro la tabla");
}
pthread_mutex_t devolverSemaforoDeTabla(char* nombreTabla){
		bool seEncuentraTabla(void* elemento){
			metadata* unMetadata = elemento;
			return string_equals_ignore_case(unMetadata->nombreTabla,nombreTabla);
		}
	pthread_mutex_lock(&mutexListaDeTablas);
	metadataConSemaforo* metadataBuscado = list_find(listaDeTablas,seEncuentraTabla);
	pthread_mutex_unlock(&mutexListaDeTablas);
	return metadataBuscado->semaforoTabla;
}
metadataConSemaforo* crearMetadataConSemaforo (metadata* unMetadata){
	pthread_mutex_t mutexTabla;
	pthread_t threadCompactacion;
	metadataConSemaforo* nuevoMetadata=malloc (sizeof(metadataConSemaforo));
	nuevoMetadata->hiloDeCompactacion = threadCompactacion;
	nuevoMetadata->cantParticiones = unMetadata->cantParticiones;
	nuevoMetadata->nombreTabla = string_duplicate(unMetadata->nombreTabla);
	nuevoMetadata->tiempoCompactacion = unMetadata->tiempoCompactacion;
	nuevoMetadata->tipoConsistencia = unMetadata->tipoConsistencia;
	nuevoMetadata->semaforoTabla = mutexTabla;
	liberarMetadata(unMetadata);
	return nuevoMetadata;
}

void agregarTablaALista(char* nombreTabla){
	metadataConSemaforo* metadataBuscado = crearMetadataConSemaforo(obtenerMetadata(nombreTabla));
		bool seEncuentraTabla(void* elemento){
			metadata* unMetadata = elemento;
			return string_equals_ignore_case(unMetadata->nombreTabla,nombreTabla);
		}
	pthread_mutex_lock(&mutexListaDeTablas);
	if(!list_find(listaDeTablas, seEncuentraTabla)){
		pthread_mutex_init(&(metadataBuscado->semaforoTabla),NULL); //inicias el semaforo de la nueva tabla
		pthread_create(&(metadataBuscado->hiloDeCompactacion),NULL,(void*) compactar,metadataBuscado);
		list_add(listaDeTablas,metadataBuscado);
		//pthread_detach(&threadCompactacion);
	}
	else{
		free(metadataBuscado->nombreTabla);
		free(metadataBuscado);
	}
	pthread_mutex_unlock(&mutexListaDeTablas);
}
void* tranformarMetadataSinSemaforo(metadataConSemaforo* metadataATransformar){
	metadata* metadataSinSemaforo; //ver si hay que hacer un malloc
	metadataSinSemaforo->cantParticiones = metadataATransformar->cantParticiones;
	metadataSinSemaforo->nombreTabla = metadataATransformar->nombreTabla;
	metadataSinSemaforo->tiempoCompactacion = metadataATransformar->tiempoCompactacion;
	metadataSinSemaforo->tipoConsistencia = metadataATransformar->tipoConsistencia;
	return metadataSinSemaforo;
}

void serializarMetadataConSemaforo(int socket){
	pthread_mutex_lock(&mutexListaDeTablas);
	serializarYEnviarPaqueteMetadatas(socket,list_map(listaDeTablas,tranformarMetadataSinSemaforo));
	pthread_mutex_unlock(&mutexListaDeTablas);
}

void funcionDescribe(char* argumentos,int socket) {
	void loggearYMostarTabla(metadata* unMetadata){
		pthread_mutex_lock(&mutexLoggerConsola);
		log_info(loggerConsola,"La tabla: %s, tiene %d particiones, consistencia= %d "
				"y tiempo de compactacion= %d \n",unMetadata->nombreTabla,unMetadata->cantParticiones,
				unMetadata->tipoConsistencia,unMetadata->tiempoCompactacion);
		pthread_mutex_unlock(&mutexLoggerConsola);
	}
	metadata* metadataBuscado = NULL;
	DIR* dir;
	struct dirent *sd;
	if(string_equals_ignore_case(argumentos,"ALL")){
		char* rutaDirectorioTablas = string_new();
		string_append(&rutaDirectorioTablas,puntoMontaje);
		string_append(&rutaDirectorioTablas,"Tables");
		if((dir = opendir(rutaDirectorioTablas))==NULL)
				enviarOMostrarYLogearInfo(-1,"No se pudo abrir el directorio");

		while((sd=readdir(dir))!=NULL){
			if (string_equals_ignore_case(sd->d_name, ".") || string_equals_ignore_case(sd->d_name, "..") ){continue;} //no me lo ignoraba sino de otra manera
			else{
				agregarTablaALista(sd->d_name);

			}

		}
		if(list_size(listaDeTablas)==0){
			enviarOMostrarYLogearInfo(socket,"No hay tablas en el FS"); //caso a revisar, que no haya tablas en LFS
			closedir(dir);
			free(rutaDirectorioTablas);
			//liberarMetadata(metadataBuscado);
			return;
		}
		enviarOMostrarYLogearInfo(-1,"Se cargaron todas las tablas del directorio");
		if(socket==-1){
			pthread_mutex_lock(&mutexListaDeTablas);
			list_iterate(listaDeTablas,(void*)loggearYMostarTabla);
			pthread_mutex_unlock(&mutexListaDeTablas);
		}
		else{
			serializarMetadataConSemaforo(socket);
		}
		closedir(dir);
		free(rutaDirectorioTablas );
	}
	else{ //este es el decribe de una tabla sola
		if(verificarExistenciaDirectorioTabla(argumentos)){
			metadataBuscado = obtenerMetadata(argumentos);
			enviarOMostrarYLogearInfo(-1,"Se encontro el metadata buscado");
					if(socket!=-1){
						serializarYEnviarMetadata(socket,metadataBuscado);
						liberarMetadata(metadataBuscado);
						return;
					}
					pthread_mutex_lock(&mutexLoggerConsola);
					log_info(loggerConsola,"La tabla: %s, tiene %d particiones, consistencia= %d "
									"y tiempo de compactacion= %d \n",metadataBuscado->nombreTabla,metadataBuscado->cantParticiones,
									metadataBuscado->tipoConsistencia,metadataBuscado->tiempoCompactacion);
					pthread_mutex_unlock(&mutexLoggerConsola);
					liberarMetadata(metadataBuscado);
		}
		else {
			enviarOMostrarYLogearInfo(socket,"No se encontro el metadata buscado");
			return;
		}
	}
}

