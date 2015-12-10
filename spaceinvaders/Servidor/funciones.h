#ifndef FUNCIONES_H_INCLUDED
#define FUNCIONES_H_INCLUDED

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

///****************** DEFINES **********************///

#define TAMBUF 800
#define MAXQ 5
#define JUG_LOC 0
#define JUG_VIS 1

///***************** ESTRUCTURAS *********************///

typedef struct{
	int comm_socket[100];
	int index;
	int asignado[100];// Asignado: 0-50 (50 servidores de partida maximo)
}t_commSocket;

typedef struct{
	int  id,socket,puntaje,servidorAsignado,cantPartidas,estado;	// Servidor Asignado en 100 = Jugador Libre
	char nombre[TAMBUF];
}t_jugador;

typedef struct{
	int id,jugador1,jugador2,socket1,socket2;	
	int  puntaje[2];
	pthread_mutex_t mtxMemoria;
	sem_t *semFinaliza;
}t_servPart;
		
typedef struct{
	t_jugador jugadores[100];
	int matrizTorneo[100][100];
	int cantJugadores;
	int flagFinConexiones;	
	int cantPartidasTotales;
	int flag_sigchld_mc;	///NUEVO
}t_jugadores;

typedef struct{
	int jugador1,jugador2,idServidorFin, fin; // Con el ID ya podria entrar a su memoria compartida
}t_finPartida;


/************ THREADS *************/

void * escucharConexiones(void *);
void * threadComunicacion(void *);
void * threadComunicacion2(void *);
void * dibujaThread(void *);
void * controlaDemonio(void *);


/*********** FUNCIONES ************/
void crearServidorPartida();
void asignarPartida();
void inicializarMatrizTorneo();
int buscarMCdePid(int);
int estaActivo(int);
void levantaProtector(int);
int leerParametro(char*,char*,char*);
void configurarServidor();
void initPuntajes();
void empaquetar0(int,char *,char *,int,char *);
int puntajeMinimo();	//TODAVIA NO LA USAMOS!
void ordenarTablaPuntaje(int[100], char[100][TAMBUF]);


/*********** HANDLERS *************/
void SIGCHLDHandler(int,siginfo_t *, void *);
void SIGINTHandler(int);
void SIGTERMHandler(int);

/******** VARIABLES SERVER *********/
t_commSocket cSocket;
int cantServ=0, // Identifica la partida
	cantJugadores=0,cantPartidas=0, claves[50];
sem_t *semaforos[50],*semJugadores, *semAsignar, *semServPart;
int tablaPosiciones[100][2];
key_t claveServidores[100];
int memServi, memJugadores,seguir=1,vectorPid[50];
t_jugadores *jugadores;	
int mipid,pidDemonio, indice=0,memoria,pidDemonio,PORT, VIDAS;

/*************** VARIABLES SDL-TTF ***************/
SDL_Surface *screen;
TTF_Font *fuente;
SDL_Rect rectangulo_ttf;
SDL_Surface *ttext_1;
char fuente_base[20];
SDL_Color bgcolor,fgcolor;
int server_caido=0;
int flagCeros;
int i,j;

/************** VARIABLES GLOBALES ****************/

int flag_muevefantasmas[50]; 
int flag_conexiones;
Uint8 *keys;
int ciclo_escuchar_conexiones;
int flag_sigchld;

typedef struct{
	int jug1;
	int jug2;
	int est_1;
	int est_2;
}estado_abrupto;

estado_abrupto flag_abrupto[50];

#endif 
