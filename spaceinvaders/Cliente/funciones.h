#ifndef FUNCIONES_H_INCLUDED
#define FUNCIONES_H_INCLUDED
#include <SDL/SDL_ttf.h>
#include <SDL/SDL.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "csprite.h"


#define TAMBUF 800
#define TAM_FUENTE 40
#define POS_Y_TORRES_1 490
#define POS_Y_TORRES_2 50
#define POS_Y_NAVEROJA_1 450
#define POS_X_FANTASMAS_1 30
#define POS_Y_FANTASMAS_1 200
#define JUG_LOC 0
#define JUG_VIS 1

///*******************ESTRUCTURAS**********************/
// estructura que contiene la información de la nave del jugador
typedef struct{
	int x,y;
}minave;

// Estructura que contiene información de las torres
typedef struct{
	int x,y, viva, sector;
}torre;

// Estructura que contiene información de la nave roja
typedef struct{
	int x,y, su_estado;
}navemala;

// Estructura que contiene la informacion de la bala que lanza el jugador
typedef struct{
	int x,y, accion;
}t_bala; 

// Estructura que contiene la informacion de los enemigos de UN jugador
typedef struct{
	int x, y, tipo, su_estado;
}t_enemigos;

// Estructura que contiene la informacion de la bala enemiga de los fantasmas
typedef struct{
	int x,y, accion;
}t_bala_enemiga;


///*************************SEMAFOROS*********************************//
sem_t *semInicioPartida, *semFinPartida;

///**************************CONEXION**********************************//

char host_name[255], s_host_port[255];
char auxa[255],auxd[255],auxdisp[255],auxsalir[255], auxSemFin[30], auxSemInicio[30]; 
int host_port;
int callersocket;
sockaddr_in my_addr;
////////////// Cabeceras ///////////
int conectar();
void cerrarConexion();
int getSocket();
//////////////////////////////////////

///**********************************SDL**********************************//
int teclaIzquierda,teclaDerecha,teclaDisparo,teclaSalir; //izq-der-disp
char chara,chard, chart, chardisp;
int inta,intd,intdisp;
char alias[TAMBUF];

SDL_Surface *screen; //la pantalla completa
SDL_Surface *ttext_1, *ttext_2; //parte de la pantalla para la informacion del jugador
SDL_Surface *ttext_1v, *ttext_2v;//parte de la pantalla para la informacion del jugador 2
SDL_Surface *fondoPartida; // fondoDelaPartida
										
SDL_Rect rectangulo; //para limpiar la pantalla
SDL_Rect rectangulo_jugador_1; //sector del jugador, acutalmente considerado para ambos
SDL_Rect rectangulo_ttf; //para el nombre del jugador
SDL_Rect rectangulo_ttf2; //para el nombre del jugador2
					
SDL_Rect rect_puntos; //para los puntos del jugador
SDL_Rect rect_puntos2; //para los puntos del jugador2

CFrame fnave; //cargo la imagen de la nave del jugador 1
CFrame fnave2; //cargo la imagen de la nave del jugador 2
CSprite nave[2]; //sprite de la nave del jugador 1

CFrame ftorreNew0;
CFrame ftorreNew1;
CFrame ftorreNew2;
CFrame ftorreNew3;
CFrame ftorreNew0a;
CFrame ftorreNew1a;
CFrame ftorreNew2a;
CFrame ftorreNew3a;

CSprite j_torre_loc[16];			
CSprite j_torre_vis[16];			
				
CFrame fnroja;
CSprite snave_roja[2]; // SPRITE DE LA NAVE ROJA (0 = JUG_LOC | 1 = JUG_VIS)
				
CFrame enemigo_t1; 
CFrame enemigo_t2;
CFrame enemigo_t3;
CSprite enemigos[78]; //segun el tipo de enemigo, le cargo el frame correspondiente

CFrame labala;
CSprite bala[2];//para las balas de las naves
				
CFrame labala_fantasmas;
CSprite bala_fantasmas[2];

CFrame fcorazones;
CSprite corazones[2];

SDL_Color bgcolor,fgcolor;

SDL_Rect rect;
SDL_Surface *imagen;

SDL_Event event;	

///*************************************TTF************************************//
TTF_Font *fuente;
char fuente_base[20]="dailypl2a.ttf";
char nombre[2][25];
char msg_puntos[2][100]={{"PUNTOS:  "},{"PUNTOS:  "}};
char cad_puntos[2][10]={{""},{""}};

///****************************VARIABLES*************************************//
int nro_jugador;
char buffer[TAMBUF];
minave jugador[2]; 
torre torre_defensa[2][16]; 
navemala naveroja[2];
t_bala mi_bala[2]; 
t_enemigos mis_enemigos[60];
t_bala_enemiga mi_bala_enemiga[2]; 


///*****************VARIABLES GLOBALES PARA EL JUEGO EN CUESTION***************************//
int done=0; //para que cicle el gameloop
int hubo_disparo[2]={6,8};	///si empieza en cero HAY PROBLEMAS! xq nosotros trabajamos como el flag "activo = 1 "
int ciclo=0; //para mover nave roja!
			
int aux2_ciclo=1; //flag para activar la bala enemiga de los fantasmas
int aux2_ciclo2=1; //flag para activar la bala enemiga de los fantasmas del jugador 2

int mis_puntos[2];	///PASA A VECTOR
int mis_vidas[2];	///PASA A VECTOR (podria incializar en inicializa() )

int flag=0;
int flag_escucha=1;
int flag_int_termine=1;

char 	archivo[]="config";
char 	left[]="MUEVE IZQUIERDA",
		right[]="MUEVE DERECHA",
		disparo[]="DISPARA",
		salir[]="SALIR TORNEO";
int	puntaje_acumulado=0;
int flag_fin_abrupto=0;
Uint8 *keys;
int torneo;


///****************************CABECERAS FUNCIONES************************//
int ingresarNombre(char *);
int leerParametro(char*, char*, char*);
void inicializa();
void finaliza();
void DrawScene(SDL_Surface *screen);
int InitSprites();
void desempaquetar0(char *);
void desempaquetar1(char *);
void desempaquetar2(char *);
void desempaquetar5(char *);
void desempaquetar88(char *);
void desempaquetar89(char *);
void desempaquetar90(char *);
void desempaquetar91(char *);
void desempaquetar92(char *);
void * threadEscucha(void *);
void * threadEventos(void *);
void pantallaEspera();
void SIGINTHandler(int);
void SIGTERMHandler(int);
void configurarTeclado();
void inicializarSDL();
void esperarRival(int);
void pantallaPresentacion();
void finalizoServer(int);
void iniciaEnemigos();
void murieronfantasmas();
void creadisparo(int);
void muevebalas_enemigas(int );
void funcionTerminoTorneo(void);
void mueveFantasmas();
int murieronFantasmas();
void muevebalas(int );
int mueveNaveRoja(int );
void creardisparoEnemigo(int );
void colisionBalaJugLoc(void);
void colisionBalaJugVis(void);
void colisionBalaFantasmitaLoc(void);
void colisionBalaFantasmitaVis(void);
#endif 



