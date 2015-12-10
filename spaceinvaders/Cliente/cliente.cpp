#include "funciones.h"
#include <SDL/SDL_ttf.h>
#include <SDL/SDL.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include "csprite.h"

//--------------------MAIN--------------------//

int main(){

	//	INICIALIZO VARIABLES DE TORNEO
	int valor_sem;	
	
	torneo=1;

	strcpy(fuente_base,"dailypl2a.ttf");
	//	INICIALIZO LOS SEMAFOROS DE PARTIDA
	char nomSem[30];
	
	// 	SEMAFORO: INICIO DE PARTIDA
	sprintf(nomSem,"%d",getpid());
	strcpy(auxSemInicio,"/semInicioPartida");
	strcat(auxSemInicio,nomSem);
	semInicioPartida=sem_open(auxSemInicio,O_CREAT, 0644, 0);
	
	// 	SEMAFORO NUEVO: FIN DE PARTIDA
	sprintf(nomSem,"%d",getpid());
	strcpy(auxSemFin,"/semFinPartida");
	strcat(auxSemFin,nomSem);
	semFinPartida=sem_open(auxSemFin,O_CREAT, 0644, 0);
	
	// 	HANDLERS
	signal(SIGINT,SIGINTHandler);
	signal(SIGTERM,SIG_IGN);
	
	// 	SE CONECTA
	if(conectar()){
		printf("Fallo la conexion del cliente: %d", getpid());
		return 1;		
	}

	// 	LANZO THREAD DE ESCUCHA
	XInitThreads();
	int socket = getSocket();
	pthread_t tid, tidEventos;
	pthread_create(&tid, NULL, threadEscucha, (void *)&socket );
	
	// 	FUNCION PARA CONFIGURAR TECLAS
	configurarTeclado();

	//	INICIALIZO SDL Y TTF
	inicializarSDL();
	
	//	VARIABLES DE SDL Y TTF
	fuente = TTF_OpenFont(fuente_base,TAM_FUENTE/2);
	
	SDL_Flip(screen);

	//	PANTALLA DE PRESENTACION
	pantallaPresentacion();
	
	//	INGRESO NOMBRE Y SE LO ENVIO AL SERVER
	ingresarNombre(alias);
		
	if(send(getSocket(),alias, sizeof(alias),0)<=0){
		exit(1);
	}
	
	// 	CREO EL THREAD DE ENVIO DE EVENTOS HACIA EL SERVIDOR
	pthread_create(&tidEventos,NULL,threadEventos,NULL); // Thread que se encarga de enviar todas las comunicaciones al servidor
	
	while(torneo){
				
		esperarRival(puntaje_acumulado);

		sem_wait(semInicioPartida);
		// TORNEO YA TERMINO. EL CLIENTE DEBE CERRAR RECURSOS Y FINALIZAR
		if(torneo==0){ 		
			funcionTerminoTorneo();
			//NUEVO (comentamos el exit)
			exit(1);
		}		
		
		pantallaEspera();

		// INICIALIZO VARIABLES DE CADA PARTIDA	
		done=1;
		hubo_disparo[JUG_LOC]=6;
		hubo_disparo[JUG_VIS]=8;
		flag=0;
		
		inicializa();
		iniciaEnemigos();
		InitSprites();
		
		DrawScene(screen);
		
		if(screen == NULL){
			printf("No se puede inicializar el modo gráfico: %s \n",SDL_GetError());
			return 1;
		}		
		atexit(SDL_Quit);
		
		// Inicializamos SDL_ttf
		if(TTF_Init() < 0){
			printf("No se pudo iniciar SDL_ttf: %s\n",SDL_GetError());
			return 1;

		}
		atexit(TTF_Quit);
			
		// carga la fuente de letra
		fuente = TTF_OpenFont(fuente_base,18);
			
		// GAMELOOP
		while (done<10) {
		
			if(mi_bala[JUG_LOC].accion==1)
				if(hubo_disparo[JUG_LOC]==0)
					muevebalas(JUG_LOC);	
		
			//para las balas malas	
			aux2_ciclo=ciclo%200;	
						
			if(mi_bala_enemiga[JUG_LOC].accion== 0 && aux2_ciclo==0)
				creardisparoEnemigo(JUG_LOC);

			if(mi_bala_enemiga[JUG_LOC].accion==1)
				muevebalas_enemigas(JUG_LOC);
				
			if(mi_bala[JUG_VIS].accion==1)
				if(hubo_disparo[JUG_VIS]==0)
					muevebalas(JUG_VIS);
					
			//para las balas malas del jugador 2
			aux2_ciclo2=ciclo%200;
				
			if(mi_bala_enemiga[JUG_VIS].accion== 0 && aux2_ciclo2==0)
				creardisparoEnemigo(JUG_VIS);

			if(mi_bala_enemiga[JUG_VIS].accion==1)
				muevebalas_enemigas(JUG_VIS);
			
			// dibujamos el frame
			DrawScene(screen);
			
			//para revivir los fantasmas si es que no estan mas
			if(murieronFantasmas() == 1 ){
				bala[JUG_LOC].setx(-50);
				bala[JUG_LOC].sety(-50);
				iniciaEnemigos();		
			}
						
			if(mis_vidas[JUG_LOC]==0 || mis_vidas[JUG_VIS]==0)	
				done=100;
			
			usleep(50000);
		}

		
		// FLAG ACTIVO = SIGTERM AL SERVIDOR DE PARTIDA
		if(flag_fin_abrupto == 0){ 
			
			if(mis_vidas[JUG_LOC]==0 && mis_vidas[JUG_VIS]!=0){
				puts("GANO EL JUGADOR VISITANTE!");
				mis_puntos[JUG_VIS]+=1000;
			}
			
			if(mis_vidas[JUG_VIS]==0 && mis_vidas[JUG_LOC]!=0){
				puts("GANO EL JUGADOR LOCAL!");
				mis_puntos[JUG_LOC]+=1000;
			}
			
			puntaje_acumulado+=mis_puntos[nro_jugador];
			
			//AVISO AL SERVIDOR QUE TERMINO LA PARTIDA
			char bufaux[TAMBUF];
			char num_auxi[20];
			strcpy(bufaux,"");
			strcpy(bufaux, "120|");
			sprintf(num_auxi,"%d",mis_puntos[nro_jugador]);
			strcat(bufaux,num_auxi);
			strcat(bufaux, "|");
			
			if(send(getSocket(),bufaux, sizeof(bufaux),0)<=0){
				puts("ERROR AL ENVIAR EL MENSAJE");
				exit(1);
			}
		}
		
		esperarRival(puntaje_acumulado);
		
		sem_wait(semFinPartida);

		if(flag_fin_abrupto==3){ // SI EL JUGADOR SE FUE DEL TORNEO 
			funcionTerminoTorneo();
			torneo=0;
		}
		
		// 	EN CASO DE QUE HAYA FINALIZADO ABRUPTAMENTE VUELVO EL FLAG PARA QUE FINALICE NORMAL	
		flag_fin_abrupto = 0; 
		
	}
	///FIN DE TORNEO

	puts("FINALIZANDO TORNEO - FIN");

	return 1;


}

//-------------------THREADS DE ESCUCHA Y TECLADO---------------------//

void * threadEscucha(void *arg){
	
	int *caller = (int *)arg;
	char buf[TAMBUF];
	char aux[TAMBUF], aux3[TAMBUF], *pc, *pt;
	int num;
	int i;
	num=0;

	while(flag_escucha){
		usleep(10000);
		bzero(buf,TAMBUF);
		if(recv(*caller,buf,TAMBUF,0)<=0){	
			puts("***********************************************************");
			puts("El servidor no se encuentra disponible, o las inscripciones se encuentra cerradas");
			puts("\tIntentelo nuevamente más tarde");
			puts("***********************************************************");
			funcionTerminoTorneo();
			kill(getpid(),SIGKILL);
		}
		
		if(strcmp(buf,"")!=0){
			
			strcpy(aux,buf);
			
			pc = strtok(aux,"|");
			num = atoi(pc);
			strcpy(aux3,buf);
			pt=strchr(aux3,'|');
			for(i=0;i<TAMBUF;i++) 
				aux3[i]=*(pt+i);
						
			switch(num){
			
				case 0: // 	RECIBO DATOS DE PARTIDA - HAGO SEM_POST(INICIO DE PARTIDA)					
						desempaquetar0(aux3);	
						sem_post(semInicioPartida);
						break;
	
				case 1:
						desempaquetar1(aux3);
						break;
					
				case 2:
						desempaquetar2(aux3);	
						break;
				
				case 4: // 	ME AVISA DE MOVER LOS FANTASMAS
						mueveFantasmas();
						break;
					
				case 5: //	ACCIONES DE NAVE ROJA
						desempaquetar5(aux3);
						break;
					
				case 6: // 	LOS 2 JUGADORES TERMINARON LA PARTIDA - 
						//	Termino la partida (partida=1) y aviso al server que cierre comunicacion (7|)			
						done=100;
						bzero(buf,TAMBUF);
						sprintf(buf,"%d",7);
						strcat(buf,"|");
						
						if(send(getSocket(),buf,TAMBUF,0)<=0){
							puts("ERROR - Socket no disponible");
							exit(1);
						}
						break;
					
				case 7: // 	LIBERO SEMAFORO DE PARTIDA - CERRADAS LAS COMUNICACIONES	
						done=100;
						sem_post(semFinPartida);
						break;
						
				case 8: //	FIN DEL TORNEO	
						puts("El torneo ha finalizado");
						finalizoServer(puntaje_acumulado);
						// YA AVISE POR PANTALLA QUE TERMINA EL TORNEO. VUELVO CDO PRESIONAN UNA TECLA
						torneo=0; // FINALIZO EL TORNEO						
						flag_escucha=0;
						// NUEVO
						sleep(1);
						//
						sem_post(semInicioPartida); 
						sem_post(semInicioPartida);
						break;
						
				case 10:
						funcionTerminoTorneo();
						exit(1);
						break;
				
				case 33:
						done=100;
						break;
						
				case 45: //	Me desconecte con SIGINT 
						done=1000;
						flag_int_termine=0;

						//NUEVO 
						flag_escucha=0;

						//PROXIMA
						sem_post(semFinPartida); // DESTRABO EL SEMAFORO PARA QUE SALGA BIEN EN EL MAIN
						sem_post(semInicioPartida);
						break;	
				
				case 55: //	Gane porque el otro jugador se fue!
						done=100;
						sem_post(semFinPartida);
						break;
											
				case 77: // CASO EN QUE FINALIZE ABRUPTAMENTE 
						done=100;
						flag_fin_abrupto=1;
						sem_post(semFinPartida);
						break;
				
				case 88: //	colision bala jugador con fantasma						
						desempaquetar88(aux3);
						break;
						
				case 89: //	colision bala jugador con nave roja		
						desempaquetar89(aux3);
						break;
				
				case 90: //	colision bala de jugador con su torre correspondiente			
						desempaquetar90(aux3);
						break;
						
				case 91:
						desempaquetar91(aux3);
						break;
				
				case 92: //	colision bala de fantasma con cualquier torre						
						desempaquetar92(aux3);
						break;
			
				default: //	Nada			
						break;
			}
		}
	}
	pthread_exit((void*)1);
}

void * threadEventos(void *){

	while(torneo){
		if(send(getSocket(),buffer, sizeof(buffer),0)<=0)
			exit(1);
			
		//consultamos el estado del teclado
		keys=SDL_GetKeyState(NULL);
		while(SDL_WaitEvent(&event)){
			switch(event.type){	
				case SDL_KEYDOWN:

						// MOVIMIENTO A LA IZQUIERDA
						if((event.key.keysym.sym == teclaIzquierda) && (jugador[nro_jugador].x > 0)){
							strcpy(buffer, "");
							if(nro_jugador == JUG_LOC)
								strcpy(buffer, "1|0|0");
							
							if(nro_jugador == JUG_VIS)
								strcpy(buffer, "1|0|1");
							
							send(getSocket(),buffer, sizeof(buffer),0);
							break;
						}
				
						// MOVIMIENTO A LA DERECHA
						if((event.key.keysym.sym == teclaDerecha) && (jugador[nro_jugador].x < 620)){ 
							strcpy(buffer, "");
							if(nro_jugador == JUG_LOC)
								strcpy(buffer, "1|1|0");
							
							if(nro_jugador == JUG_VIS)
								strcpy(buffer, "1|1|1");
							
							send(getSocket(),buffer, sizeof(buffer),0);
							break;
						}
				
						// DISPARO	
						if((event.key.keysym.sym == teclaDisparo) && (mi_bala[nro_jugador].accion==0)){
							strcpy(buffer, "");
							if(nro_jugador == JUG_LOC)
								strcpy(buffer, "2|0");
							
							if(nro_jugador == JUG_VIS)
								strcpy(buffer, "2|1");
							
							send(getSocket(),buffer, sizeof(buffer),0);
							break;
						}
						
						///
						if(event.key.keysym.sym == teclaSalir){
							kill(getpid(),SIGINT); 
						}
			}
		}
	}
	
	return 0;
}

//------------------FUNCIONES DE CONEXION-------------------//

void cerrarConexion(){
	close(callersocket);
}

int getSocket(){
	return callersocket;
}

int conectar(){

    char param[20], arch[20];
    strcpy(arch, "config");
    strcpy(param, "HOST");
    
    //la funcion carga en host_name el ip
    leerParametro(host_name, param, arch); 
    strcpy(param, "PUERTO");
    
    //la funcion carga en s_host_port el puerto por el cual se va a comunicar
    leerParametro(s_host_port, param, arch);
    host_port= atoi(s_host_port);

	callersocket = socket(AF_INET, SOCK_STREAM, 0); 
    if(callersocket == -1){
        printf("Error de inicialización de socket %d\n",errno);
        return -1;
    }
 
	int var=1;
	// para hacer reutilizable el socket
	setsockopt(callersocket, SOL_SOCKET, SO_REUSEADDR, &var, sizeof(int));

    my_addr.sin_family = AF_INET ;
    my_addr.sin_port = htons(host_port);
    memset(&(my_addr.sin_zero), 0, 8);
    my_addr.sin_addr.s_addr = inet_addr(host_name);

	//me conecto a servidor
	if(connect(callersocket,(struct sockaddr*)&my_addr,sizeof(struct sockaddr))<0){ //my_addr es caller_address
		puts("***********************************************************");
		puts("El servidor no se encuentra disponible, o las inscripciones se encuentra cerradas");
		puts("\tIntentelo nuevamente más tarde");
		puts("***********************************************************");
		exit(1);
	}
	
	return 0;
}

//-------------------FUNCIONES DE JUEGO---------------------//

void DrawScene(SDL_Surface *screen) {
	int cant,i, aux_vidas_x, aux_vidas_y;
	
	// borramos todo
	rectangulo.x=0;
	rectangulo.y=0;
	rectangulo.w=868;
	rectangulo.h=600;
	SDL_FillRect(screen,&rectangulo,SDL_MapRGB(screen->format,0,0,0));
	
	//FONDO - Cargamos gráfico
	SDL_Rect dest;
	dest.x = 0;
	dest.y = 0;
	dest.w = 868;
	dest.h = 600;
	
	SDL_FillRect(screen,NULL,SDL_MapRGB(screen->format,0,0,0));
	SDL_BlitSurface(fondoPartida, NULL, screen, &dest);
	
	if(hubo_disparo[JUG_LOC]==0)
		colisionBalaJugLoc();
	if(hubo_disparo[JUG_VIS]==0)
		colisionBalaJugVis();
	
	
	if(mi_bala_enemiga[JUG_LOC].accion == 1)
		colisionBalaFantasmitaLoc();
		
	if(mi_bala_enemiga[JUG_VIS].accion == 1)
		colisionBalaFantasmitaVis();
	
	//creo el rectangulo donde esta la informacion de juego
	rectangulo_jugador_1.x = 690;
	rectangulo_jugador_1.y = 0;
	rectangulo_jugador_1.w = 180;
	rectangulo_jugador_1.h = 640;
	SDL_FillRect(screen,&rectangulo_jugador_1,SDL_MapRGB(screen->format,100,100,100));
	
	// COLOR DE TRANSPARENCIA
	SDL_SetColorKey(screen,SDL_SRCCOLORKEY|SDL_RLEACCEL, SDL_MapRGB(screen->format,199,199,199));		

	//creo el rectangulo donde esta el nombre del jugador
	ttext_1 = TTF_RenderText_Shaded(fuente,nombre[JUG_LOC],fgcolor,bgcolor);
	rectangulo_ttf.y=450;
	rectangulo_ttf.x=705;
	rectangulo_ttf.w= ttext_1->w;
	rectangulo_ttf.h= ttext_1->h;; 

	// Usamos color rojo para la transparencia del fondo
	SDL_SetColorKey(ttext_1,SDL_SRCCOLORKEY|SDL_RLEACCEL, SDL_MapRGB(ttext_1->format,199,199,199));		
	
	// Volcamos la superficie a la pantalla
	SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);
	
	//creo el rectangulo donde esta el nombre del jugador2
	ttext_1v = TTF_RenderText_Shaded(fuente,nombre[JUG_VIS],fgcolor,bgcolor);
	rectangulo_ttf2.y=50;
	rectangulo_ttf2.x=705;
	rectangulo_ttf2.w= ttext_1v->w;
	rectangulo_ttf2.h= ttext_1v->h;; 
	
	// Usamos color rojo para la transparencia del fondo
	SDL_SetColorKey(ttext_1v,SDL_SRCCOLORKEY|SDL_RLEACCEL, SDL_MapRGB(ttext_1v->format,199,199,199));		
	
	// Volcamos la superficie a la pantalla
	SDL_BlitSurface(ttext_1v,NULL,screen,&rectangulo_ttf2);
	
	//aca armo los puntos del jugador 2
	sprintf(cad_puntos[JUG_VIS],"%d",mis_puntos[JUG_VIS]);
	strcat(msg_puntos[JUG_VIS], cad_puntos[JUG_VIS]);
	
	//creo el rectangulo donde estan los puntos del jugador 2
	ttext_2v = TTF_RenderText_Shaded(fuente,msg_puntos[JUG_VIS],fgcolor,bgcolor);
	rect_puntos2.y=160;
	rect_puntos2.x=720;
	rect_puntos2.w= ttext_2v->w; 
	rect_puntos2.h= ttext_2v->h;
	
	strcpy(msg_puntos[JUG_VIS],"");
	strcpy(msg_puntos[JUG_VIS],"Puntaje:  ");
	
	// Usamos color rojo para la transparencia del fondo
	SDL_SetColorKey(ttext_2v,SDL_SRCCOLORKEY|SDL_RLEACCEL, SDL_MapRGB(ttext_2v->format,199,199,199));	
	
	// Volcamos la superficie a la pantalla
	SDL_BlitSurface(ttext_2v,NULL,screen,&rect_puntos2);
	
	// VIDAS VISITANTE 	
	aux_vidas_x=705;
	aux_vidas_y=100;
	int vidas_a_dibujar = mis_vidas[JUG_VIS];
	int vidas_dibujadas = 1;
	int flag = 0;
	
	while(vidas_dibujadas <= vidas_a_dibujar){	
		corazones[JUG_VIS].setx(aux_vidas_x);	
		corazones[JUG_VIS].sety(aux_vidas_y);	
		corazones[JUG_VIS].draw(screen);	
		aux_vidas_x+= 25;
		vidas_dibujadas ++;
		if(vidas_dibujadas>5 && flag == 0){
			aux_vidas_y +=30;
			aux_vidas_x = 705;
			flag = 1;
		}
	}
	
	//aca armo los puntos
	sprintf(cad_puntos[JUG_LOC],"%d",mis_puntos[JUG_LOC]);
	strcat(msg_puntos[JUG_LOC], cad_puntos[JUG_LOC]);
	
	//creo el rectangulo donde estan los puntos del jugador
	ttext_2 = TTF_RenderText_Shaded(fuente,msg_puntos[JUG_LOC],fgcolor,bgcolor);
	rect_puntos.y=560;
	rect_puntos.x=720;
	rect_puntos.w= ttext_2->w; 
	rect_puntos.h= ttext_2->h;
	
	strcpy(msg_puntos[JUG_LOC],"");
	strcpy(msg_puntos[JUG_LOC],"Puntaje:  ");
	// Usamos color rojo para la transparencia del fondo
	SDL_SetColorKey(ttext_2,SDL_SRCCOLORKEY|SDL_RLEACCEL, SDL_MapRGB(ttext_2->format,199,199,199));	
	// Volcamos la superficie a la pantalla
	SDL_BlitSurface(ttext_2,NULL,screen,&rect_puntos);

	//aca armo las vidas	
	aux_vidas_x=705;
	aux_vidas_y=490;
	
	// VIDAS LOCAL
	vidas_a_dibujar = mis_vidas[JUG_LOC];
	vidas_dibujadas = 1;
	flag = 0;
	
	while(vidas_dibujadas <= vidas_a_dibujar){	
		corazones[JUG_LOC].setx(aux_vidas_x);	
		corazones[JUG_LOC].sety(aux_vidas_y);	
		corazones[JUG_LOC].draw(screen);	
		aux_vidas_x+= 25;
		vidas_dibujadas ++;
		if(vidas_dibujadas>5 && flag == 0){
			aux_vidas_x = 705;
			aux_vidas_y +=30;
			flag = 1;
		}
	}
	
	// dibuja nave del jugador Local
	nave[JUG_LOC].setx(jugador[JUG_LOC].x);
	nave[JUG_LOC].sety(jugador[JUG_LOC].y);
	nave[JUG_LOC].draw(screen);
	
	// dibuja nave del jugador Visitante
	nave[JUG_VIS].setx(jugador[JUG_VIS].x);
	nave[JUG_VIS].sety(jugador[JUG_VIS].y);
	nave[JUG_VIS].draw(screen);
	
	//dibujo las torres que esten vivas	
	for(cant=0; cant<16; cant++)
		if(torre_defensa[JUG_LOC][cant].viva==1){
			j_torre_loc[cant].setx(torre_defensa[JUG_LOC][cant].x);
			j_torre_loc[cant].sety(torre_defensa[JUG_LOC][cant].y);
			j_torre_loc[cant].draw(screen);
		}

	//dibujo las torres que esten vivas del jugador 2
	for(cant=0; cant<16; cant++)
		if(torre_defensa[JUG_VIS][cant].viva==1){
			j_torre_vis[cant].setx(torre_defensa[JUG_VIS][cant].x);
			j_torre_vis[cant].sety(torre_defensa[JUG_VIS][cant].y);
			j_torre_vis[cant].draw(screen);
		}
	
	//dibujo la nave roja si tiene que estar
	if(naveroja[JUG_LOC].su_estado==1){
		snave_roja[JUG_LOC].setx(naveroja[JUG_LOC].x);
		snave_roja[JUG_LOC].sety(naveroja[JUG_LOC].y);
		snave_roja[JUG_LOC].draw(screen);
	}
	
	//dibujo la nave roja si tiene que estar jug2
	if(naveroja[JUG_VIS].su_estado==1){
		snave_roja[JUG_VIS].setx(naveroja[JUG_VIS].x);
		snave_roja[JUG_VIS].sety(naveroja[JUG_VIS].y);
		snave_roja[JUG_VIS].draw(screen);
	}
	
	//dibujo los fantasmistas que esten vivos	
	for(i=0; i<60;i++)
		if(mis_enemigos[i].su_estado==1){
			enemigos[i].setx(mis_enemigos[i].x);
			enemigos[i].sety(mis_enemigos[i].y);
			enemigos[i].draw(screen);
		}

	//si ya habia lanzado mi bala, la dibujo.
	if(mi_bala[JUG_LOC].accion==1){
		bala[JUG_LOC].setx(mi_bala[JUG_LOC].x);
		bala[JUG_LOC].sety(mi_bala[JUG_LOC].y);
		bala[JUG_LOC].draw(screen);		
	}

	//si ya habia lanzado mi bala, la dibujo. jug2
	if(mi_bala[JUG_VIS].accion==1){
		bala[JUG_VIS].setx(mi_bala[JUG_VIS].x);
		bala[JUG_VIS].sety(mi_bala[JUG_VIS].y);
		bala[JUG_VIS].draw(screen);		
	}		
	
	//si la bala de los fantasmitas esta en accion la dibujo
	if(mi_bala_enemiga[JUG_LOC].accion==1){
		bala_fantasmas[JUG_LOC].setx(mi_bala_enemiga[JUG_LOC].x);
		bala_fantasmas[JUG_LOC].sety(mi_bala_enemiga[JUG_LOC].y);
		bala_fantasmas[JUG_LOC].draw(screen);	
	}		
	
	//si la bala de los fantasmitas esta en accion la dibujo jug2
	if(mi_bala_enemiga[JUG_VIS].accion==1){
		bala_fantasmas[JUG_VIS].setx(mi_bala_enemiga[JUG_VIS].x);
		bala_fantasmas[JUG_VIS].sety(mi_bala_enemiga[JUG_VIS].y);
		bala_fantasmas[JUG_VIS].draw(screen);	
	}

	// Mostramos todo el frame
	SDL_Flip(screen);
	//SDL_Delay(10); todavia no nos vamos a meter con esto.	
}

//------------EMPAQUETAR Y DESEMPAQUETAR--------------//
//ENVIO COLISION
void empaquetar88(int jug,int pos,char *buffer){ 
	
	bzero(buffer, TAMBUF);
	char auxcad[30]="";
	char auxNum[5];
	char auxJug[1];
	
	strcpy(auxcad,"88|");
	sprintf(auxJug,"%d",jug);
	strcat(auxcad, auxJug);
	strcat(auxcad, "|");
	sprintf(auxNum,"%d",pos);
	strcat(auxcad, auxNum);
	strcat(auxcad, "|");
	strcpy(buffer,auxcad);
}

// RECIBO MOVIMIENTO
void desempaquetar1(char *buffer){ 
	
	char *pc;
	char aux[TAMBUF];
	int movimiento;
	int jug;
	int valor;
	
	strcpy(aux, buffer);
	pc = strtok(aux,"|");
	sscanf(pc,"%d",&valor); // Capturo el tipo de movimiento (0 = IZQ / 1 = DER)
	movimiento = valor; 
	pc = strtok(0,"|");
	sscanf(pc,"%d",&valor); // Capturo que jugador se movió (0 = LOC / 1 = VIS)
	jug = valor; 
	if(movimiento==0)
		jugador[jug].x-= 25;
	if(movimiento==1)
		jugador[jug].x+= 25;	
}

//RECIBO DISPARO DEL OPONENTE
void desempaquetar2(char *buffer){ 

	char *pc;
	char aux[TAMBUF];	
	int valor;
	int jug;
	
	
	strcpy(aux, buffer);
	pc = strtok(aux,"|");
	sscanf(pc,"%d",&valor);
	jug=valor;
	creadisparo(jug);
}

//RECIBO DISPARO DEL OPONENTE
void desempaquetar5(char *buffer){ 

	char *pc;
	char aux[TAMBUF];	
	int valor;
	int accion;
	int jug_nave_roja;

	strcpy(aux, buffer);
	pc = strtok(aux,"|");
	sscanf(pc,"%d",&valor);
	accion=valor;
	valor=0;
	pc = strtok(NULL,"|");
	sscanf(pc,"%d",&valor);
	jug_nave_roja=valor;
	
	// INICIA NAVES ROJAS
	if(accion == 0){ 
		
		// INICIALIZA NAVE ROJA LOCAL
		if(jug_nave_roja==0 && naveroja[JUG_LOC].su_estado == 0 ){ 
			naveroja[JUG_LOC].x= 10;
			naveroja[JUG_LOC].y= POS_Y_NAVEROJA_1;
			naveroja[JUG_LOC].su_estado=0;
			naveroja[JUG_LOC].su_estado=1;		
		}
		
		// INICIALIZA NAVE ROJA VISITANTE	
		if(jug_nave_roja==1 && naveroja[JUG_VIS].su_estado == 0){ 
			naveroja[JUG_VIS].x= 10;
			naveroja[JUG_VIS].y= POS_Y_NAVEROJA_1 - 300;
			naveroja[JUG_VIS].su_estado=0;				
			naveroja[JUG_VIS].su_estado=1;
		}
	
	}
	
	// MUEVO NAVES ROJAS
	if(accion == 1){ 
		if(naveroja[JUG_LOC].su_estado==1)
			mueveNaveRoja(JUG_LOC);
		if(naveroja[JUG_VIS].su_estado==1)
			mueveNaveRoja(JUG_VIS);
	}
}

void desempaquetar88(char *buffer){
	char aux[TAMBUF];
	char *pc;
	int valor;
	int jug;
	int punt;
	
	strcpy(aux, buffer);
	pc = strtok(aux,"|");
	sscanf(pc, "%d", &jug);
	pc = strtok(NULL, "|");
	sscanf(pc, "%d", &valor);
	pc = strtok(NULL, "|");
	sscanf(pc, "%d", &punt);

	mis_enemigos[valor].su_estado=0;
	mi_bala[jug].accion=0;
	bala[jug].setx(-50);
	bala[jug].sety(-50);
	hubo_disparo[jug]=1;
	mis_puntos[jug]+=punt;	
	
	return;
}

void desempaquetar89(char *buffer){
	char aux[TAMBUF];
	char *pc;
	int jug;
	int punt;	

	strcpy(aux, buffer);
	pc = strtok(aux,"|");
	sscanf(pc, "%d", &jug);	
	pc = strtok(NULL,"|");
	sscanf(pc, "%d", &punt);	

	naveroja[jug].su_estado=0;
	mi_bala[jug].accion=0;
	bala[jug].setx(-50);
	bala[jug].sety(-50);
	hubo_disparo[jug]=1;

	mis_puntos[jug]+=punt;		

	return;
}

void desempaquetar90(char *buffer){
	char aux[TAMBUF];
	char *pc;
	int valor;
	int jug;
	int numtorre;
	
	strcpy(aux, buffer);
	pc = strtok(aux,"|");
	sscanf(pc, "%d", &jug);
	pc = strtok(NULL, "|");
	sscanf(pc, "%d", &numtorre);
	pc = strtok(NULL, "|");
	sscanf(pc, "%d", &valor);

	if(numtorre==0)
		torre_defensa[JUG_LOC][valor].viva=0;
	else
		torre_defensa[JUG_VIS][valor].viva=0;
	
	mi_bala[jug].accion=0;
	bala[jug].setx(-50);
	bala[jug].sety(-50);
	hubo_disparo[jug]=1;
	
	return;	
}

void desempaquetar91(char *buffer){
	
	char aux[TAMBUF];
	char *pc;
	int jug;
	
	strcpy(aux, buffer);
	pc = strtok(aux,"|");
	sscanf(pc, "%d", &jug);
		
	mi_bala_enemiga[jug].accion=0;
	bala_fantasmas[jug].setx(-50);
	bala_fantasmas[jug].sety(-50);
	
	mis_vidas[jug]--;	
}

void desempaquetar92(char *buffer){
	char aux[TAMBUF];
	char *pc;
	int valor;
	int jug;
	
	strcpy(aux, buffer);
	pc = strtok(aux,"|");
	sscanf(pc, "%d", &jug);
	pc = strtok(NULL, "|");
	sscanf(pc, "%d", &valor);
	
	torre_defensa[jug][valor].viva=0;		
	mi_bala_enemiga[jug].accion=0;
	bala_fantasmas[jug].setx(-50);
	bala_fantasmas[jug].sety(-50);
	
	return;
}

// RECIBO NRO_JUG
void desempaquetar0(char *buffer){ 
	
	char *pc;
	char aux[TAMBUF];
	int valor,cant_vidas;

	strcpy(aux,buffer);
	pc = strtok(aux,"|");
	sscanf(pc,"%d",&valor);		
		nro_jugador = valor;
	
	pc = strtok(0,"|");
		strcpy(nombre[JUG_LOC], pc);

	pc = strtok(0,"|");
		strcpy(nombre[JUG_VIS], pc);
	pc = strtok(0,"|");
		cant_vidas = atoi(pc);
	
	mis_vidas[JUG_LOC]=cant_vidas;
	mis_vidas[JUG_VIS]=cant_vidas;
}


//-------------------HANDLERS--------------------------//

/**
* @Handler
* SIGINTHandler()
* -Handler para la senial INT, se encarga de avisar al servidor que termino el cliente y cierra todo
*
*/

void SIGINTHandler(int sig){
	char buffi[TAMBUF];
	strcpy(buffi, "75|");
	//aviso al servidor que mori!
	send(getSocket(),buffi, sizeof(buffi),0);
	
		
	flag_fin_abrupto= 3;
	while(flag_int_termine){
		usleep(30000);
		//me quedo ciclando hasta que el threadDeEscucha me destrabe !
	}
	
	bzero(buffi,TAMBUF);	
	if(recv(getSocket(),buffi,TAMBUF,0)<=0){
			exit(1);
	}	

	//Recibo el caso 6 para cerrar los threads
	bzero(buffi,TAMBUF);	
	if(recv(getSocket(),buffi,TAMBUF,0)<=0){
			exit(1);
	}
	
	//Recibo el caso 7 y seteo los siguientes valores para finalizar
	flag_escucha=0;
	torneo=0;
}


//------------------FUNCIONES DE INICIALIZACION----------------------//

void configurarTeclado(){
	
	leerParametro(auxa,left,archivo);
	leerParametro(auxd,right,archivo);
	leerParametro(auxdisp,disparo,archivo);	// para el disparo
	leerParametro(auxsalir,salir,archivo);	// para salir del torneo

	chara = *auxa;		//	izquierda
	chard = *auxd;		//	derecha
	chardisp = *auxdisp; 	// 	disparo
	chart = *auxsalir;	//	salir torneo
	
	// IZQUIERDA
	if(strlen(auxa)==2)
		teclaIzquierda = (int)chara;
	else
		teclaIzquierda = (int)SDLK_LEFT;
	
	// DERECHA
	if(strlen(auxd)==2)
		teclaDerecha = (int)chard;
	else
		teclaDerecha = (int)SDLK_RIGHT;
	
	// DISPARO
	if(strlen(auxdisp)==2)
		teclaDisparo = (int)chardisp;
	else
		teclaDisparo = (int)SDLK_SPACE;
	
	// SALIR DEL TORNEO
	if (strlen(auxsalir)==2)
		teclaSalir = (int)chart;
	else
		teclaSalir = (int)SDLK_ESCAPE;
}
	
int leerParametro(char valor[255], char* parametroBuscado, char* archivo){
	FILE *f_arch;
	char linea[255],*punt;

	//abre archivo
	f_arch = fopen(archivo, "r+");
	if(f_arch != NULL){
		//leo las lineas hasta fin de archivo
		fgets(linea, sizeof(linea), f_arch);
		while (!feof(f_arch)){
			punt = strchr(linea,'=');
			int lParametro = strlen(linea) - strlen(punt++);
			//si encuentra el parámetro, lo copia en valor, cierra archivo y sale
			if (strncmp(linea, parametroBuscado, lParametro) == 0){
				strncpy(valor, punt, strlen(punt));
				fclose(f_arch);
				return 0;
			}
			fgets(linea, sizeof(linea), f_arch);
		}
		//cierra archivo
		fclose(f_arch);

	}else{
		printf("\nError al intentar abrir el archivo.");
		return -1;
	}
	
	return 0;
}

void inicializarSDL(){
	
	if(SDL_Init(SDL_INIT_VIDEO) < 0 ){
		printf("No se ha podido inicializar SDL: %s\n",SDL_GetError());
		exit(1);
	}
	
	screen= SDL_SetVideoMode(868,600,24,SDL_HWSURFACE | SDL_DOUBLEBUF);
	if(screen == NULL){
		fprintf(stderr,"No se puede establecer el modo de video 640x480: %s\n",SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);
	
	if(TTF_Init() < 0){
		printf("No se pudo iniciar SDL_ttf: %s\n",SDL_GetError());
		exit(1);
	}
	atexit(TTF_Quit);

}		

void iniciaEnemigos(){
	int cant, fil, col, aux_x, aux_y;
	
	for(cant=0; cant<60; cant++){
		mis_enemigos[cant].su_estado=1;
		if( (cant>=0 && cant <=9) || (cant>=50 && cant<=59) )
			mis_enemigos[cant].tipo=1;
		
		if( (cant>=10 && cant<=19) || (cant>=40 && cant<=49) )
			mis_enemigos[cant].tipo=2;
		
		if(cant>=20 && cant<=39)	
			mis_enemigos[cant].tipo=3;
			
		if(cant>=0 && cant<=29){
			fil= mis_enemigos[cant].tipo - 1;
			col= cant - (fil*10); 
		}else{
			fil= 6 - mis_enemigos[cant].tipo;
			col= cant - (fil*10);	
		}
		
		aux_x = POS_X_FANTASMAS_1 + 40*col;
		aux_y = POS_Y_FANTASMAS_1 + 40*fil;
		
		mis_enemigos[cant].x= aux_x;
		mis_enemigos[cant].y= aux_y;
	}	
}

int InitSprites(){
	int cant;
	char cade1[60], cade2[60], cade4[60], cade14[60];
	char cade5[60], cade6[60], cade7[60], cade8[60], cade15[60], cade17[60], cade18[60];
	char cade9[60], cade10[60], cade11[60], cade12[60], cade13[60], cade16[60];
	
	//FONDO
	fondoPartida = SDL_LoadBMP("./imagenes/fondoPartida.bmp");
	if( fondoPartida == NULL ){
		printf("No pude cargar gráfico: %s\n", SDL_GetError());
		exit(1);
	}
	
	//NAVES
	strcpy(cade1, "./imagenes/mi_nave.bmp");
	fnave.load(cade1);
	nave[JUG_LOC].addframe(fnave);
	
	strcpy(cade2, "./imagenes/mi_nave_vis.bmp");
	fnave2.load(cade2);
	nave[JUG_VIS].addframe(fnave2);

	//TORRES DE DEFENSA
	strcpy(cade4, "./imagenes/torre_sec_0.bmp");
	ftorreNew0.load(cade4);
	
	strcpy(cade5, "./imagenes/torre_sec_1.bmp");
	ftorreNew1.load(cade5);
	
	strcpy(cade6, "./imagenes/torre_sec_2.bmp");
	ftorreNew2.load(cade6);
	
	strcpy(cade7, "./imagenes/torre_sec_3.bmp");
	ftorreNew3.load(cade7);
	
	strcpy(cade14, "./imagenes/torre_sec_0_a.bmp");
	ftorreNew0a.load(cade14);
	
	strcpy(cade15, "./imagenes/torre_sec_1_a.bmp");
	ftorreNew1a.load(cade15);
	
	strcpy(cade17, "./imagenes/torre_sec_2_a.bmp");
	ftorreNew2a.load(cade17);
	
	strcpy(cade18, "./imagenes/torre_sec_3_a.bmp");
	ftorreNew3a.load(cade18);
	
	for(cant=0; cant<16; cant++){
		if(torre_defensa[JUG_LOC][cant].sector==0)
			j_torre_loc[cant].addframe(ftorreNew0);
		if(torre_defensa[JUG_LOC][cant].sector==1)
			j_torre_loc[cant].addframe(ftorreNew1);
		if(torre_defensa[JUG_LOC][cant].sector==2)
			j_torre_loc[cant].addframe(ftorreNew2);
		if(torre_defensa[JUG_LOC][cant].sector==3)
			j_torre_loc[cant].addframe(ftorreNew3);		
	}
	
	for(cant=0; cant<16; cant++){
		if(torre_defensa[JUG_VIS][cant].sector==0)
			j_torre_vis[cant].addframe(ftorreNew0a);
		if(torre_defensa[JUG_VIS][cant].sector==1)
			j_torre_vis[cant].addframe(ftorreNew1a);
		if(torre_defensa[JUG_VIS][cant].sector==2)
			j_torre_vis[cant].addframe(ftorreNew2a);
		if(torre_defensa[JUG_VIS][cant].sector==3)
			j_torre_vis[cant].addframe(ftorreNew3a);		
	}
	
	strcpy(cade8, "./imagenes/navecita.bmp");
	fnroja.load(cade8);
	snave_roja[JUG_LOC].addframe(fnroja);
	snave_roja[JUG_VIS].addframe(fnroja);

	strcpy(cade9, "./imagenes/pascualitos_1.bmp");
	enemigo_t1.load(cade9);
	
	strcpy(cade10, "./imagenes/pascualitos_2.bmp");
	enemigo_t2.load(cade10);
	
	strcpy(cade11, "./imagenes/pascualitos_3.bmp");
	enemigo_t3.load(cade11);
	
	for(cant=0; cant<60; cant++){
		if(mis_enemigos[cant].tipo==1)
			enemigos[cant].addframe(enemigo_t1);
		if(mis_enemigos[cant].tipo==2)
			enemigos[cant].addframe(enemigo_t2);
		if(mis_enemigos[cant].tipo==3)
			enemigos[cant].addframe(enemigo_t3);
	}
	
	strcpy(cade12, "./imagenes/bala.bmp");
	labala.load(cade12);
	bala[JUG_LOC].addframe(labala);
	
	bala[JUG_VIS].addframe(labala);
	
	strcpy(cade13, "./imagenes/balamala.bmp");
	labala_fantasmas.load(cade13);
	bala_fantasmas[JUG_LOC].addframe(labala_fantasmas);
	
	bala_fantasmas[JUG_VIS].addframe(labala_fantasmas);
	
	strcpy(cade16, "./imagenes/corazon2.bmp");
	fcorazones.load(cade16);
	
	corazones[JUG_LOC].addframe(fcorazones);
	corazones[JUG_VIS].addframe(fcorazones);
	return 0;
}

void inicializa(){
	
	jugador[JUG_LOC].x=350;
	jugador[JUG_LOC].y=570;
	
	jugador[JUG_VIS].x=350;
	jugador[JUG_VIS].y=5;
	
//torre 1 empieza	
	torre_defensa[JUG_LOC][0].x= 80;
	torre_defensa[JUG_LOC][0].y= POS_Y_TORRES_1;
	torre_defensa[JUG_LOC][0].viva=1;
	torre_defensa[JUG_LOC][0].sector=0;
	
	torre_defensa[JUG_LOC][1].x= 80 + 30;
	torre_defensa[JUG_LOC][1].y= POS_Y_TORRES_1;
	torre_defensa[JUG_LOC][1].viva=1;
	torre_defensa[JUG_LOC][1].sector=1;
	
	torre_defensa[JUG_LOC][2].x= 80;
	torre_defensa[JUG_LOC][2].y= POS_Y_TORRES_1 + 20;
	torre_defensa[JUG_LOC][2].viva=1;
	torre_defensa[JUG_LOC][2].sector=2;
	
	torre_defensa[JUG_LOC][3].x= 80 + 30;
	torre_defensa[JUG_LOC][3].y= POS_Y_TORRES_1 + 20;
	torre_defensa[JUG_LOC][3].viva=1;
	torre_defensa[JUG_LOC][3].sector=3;
//termina torre 1

//torre 2 empieza
	torre_defensa[JUG_LOC][4].x= 230;
	torre_defensa[JUG_LOC][4].y= POS_Y_TORRES_1;
	torre_defensa[JUG_LOC][4].viva=1;
	torre_defensa[JUG_LOC][4].sector=0;
	
	torre_defensa[JUG_LOC][5].x= 230 + 30;
	torre_defensa[JUG_LOC][5].y= POS_Y_TORRES_1;
	torre_defensa[JUG_LOC][5].viva=1;
	torre_defensa[JUG_LOC][5].sector=1;
	
	torre_defensa[JUG_LOC][6].x= 230;
	torre_defensa[JUG_LOC][6].y= POS_Y_TORRES_1 + 20;
	torre_defensa[JUG_LOC][6].viva=1;
	torre_defensa[JUG_LOC][6].sector=2;
	
	torre_defensa[JUG_LOC][7].x= 230 + 30;
	torre_defensa[JUG_LOC][7].y= POS_Y_TORRES_1 + 20;
	torre_defensa[JUG_LOC][7].viva=1;
	torre_defensa[JUG_LOC][7].sector=3;
//termina torre 2

//torre 3 empieza
	torre_defensa[JUG_LOC][8].x= 400;
	torre_defensa[JUG_LOC][8].y= POS_Y_TORRES_1;
	torre_defensa[JUG_LOC][8].viva=1;
	torre_defensa[JUG_LOC][8].sector=0;
	
	torre_defensa[JUG_LOC][9].x= 400 + 30;
	torre_defensa[JUG_LOC][9].y= POS_Y_TORRES_1;
	torre_defensa[JUG_LOC][9].viva=1;
	torre_defensa[JUG_LOC][9].sector=1;
	
	torre_defensa[JUG_LOC][10].x= 400;
	torre_defensa[JUG_LOC][10].y= POS_Y_TORRES_1 + 20;
	torre_defensa[JUG_LOC][10].viva=1;
	torre_defensa[JUG_LOC][10].sector=2;
	
	torre_defensa[JUG_LOC][11].x= 400 + 30;
	torre_defensa[JUG_LOC][11].y= POS_Y_TORRES_1 + 20;
	torre_defensa[JUG_LOC][11].viva=1;
	torre_defensa[JUG_LOC][11].sector=3;
//termina torre 3

//torre 4 empieza
	torre_defensa[JUG_LOC][12].x= 570;
	torre_defensa[JUG_LOC][12].y= POS_Y_TORRES_1;
	torre_defensa[JUG_LOC][12].viva=1;
	torre_defensa[JUG_LOC][12].sector=0;
	
	torre_defensa[JUG_LOC][13].x= 570 + 30;
	torre_defensa[JUG_LOC][13].y= POS_Y_TORRES_1;
	torre_defensa[JUG_LOC][13].viva=1;
	torre_defensa[JUG_LOC][13].sector=1;
	
	torre_defensa[JUG_LOC][14].x= 570;
	torre_defensa[JUG_LOC][14].y= POS_Y_TORRES_1 + 20;
	torre_defensa[JUG_LOC][14].viva=1;
	torre_defensa[JUG_LOC][14].sector=2;
	
	torre_defensa[JUG_LOC][15].x= 570 + 30;
	torre_defensa[JUG_LOC][15].y= POS_Y_TORRES_1 + 20;
	torre_defensa[JUG_LOC][15].viva=1;
	torre_defensa[JUG_LOC][15].sector=3;
//termina torre 4

//torre 1 VIS empieza	
	torre_defensa[JUG_VIS][0].x= 80;
	torre_defensa[JUG_VIS][0].y= POS_Y_TORRES_2;
	torre_defensa[JUG_VIS][0].viva=1;
	torre_defensa[JUG_VIS][0].sector=2;
	
	torre_defensa[JUG_VIS][1].x= 80 + 30;
	torre_defensa[JUG_VIS][1].y= POS_Y_TORRES_2;
	torre_defensa[JUG_VIS][1].viva=1;
	torre_defensa[JUG_VIS][1].sector=3;
	
	torre_defensa[JUG_VIS][2].x= 80;
	torre_defensa[JUG_VIS][2].y= POS_Y_TORRES_2 + 20;
	torre_defensa[JUG_VIS][2].viva=1;
	torre_defensa[JUG_VIS][2].sector=0;
	
	torre_defensa[JUG_VIS][3].x= 80 + 30;
	torre_defensa[JUG_VIS][3].y= POS_Y_TORRES_2 + 20;
	torre_defensa[JUG_VIS][3].viva=1;
	torre_defensa[JUG_VIS][3].sector=1;
//termina torre 1 VIS

//torre 2 VIS empieza
	torre_defensa[JUG_VIS][4].x= 230;
	torre_defensa[JUG_VIS][4].y= POS_Y_TORRES_2;
	torre_defensa[JUG_VIS][4].viva=1;
	torre_defensa[JUG_VIS][4].sector=2;
	
	torre_defensa[JUG_VIS][5].x= 230 + 30;
	torre_defensa[JUG_VIS][5].y= POS_Y_TORRES_2;
	torre_defensa[JUG_VIS][5].viva=1;
	torre_defensa[JUG_VIS][5].sector=3;
	
	torre_defensa[JUG_VIS][6].x= 230;
	torre_defensa[JUG_VIS][6].y= POS_Y_TORRES_2 + 20;
	torre_defensa[JUG_VIS][6].viva=1;
	torre_defensa[JUG_VIS][6].sector=0;
	
	torre_defensa[JUG_VIS][7].x= 230 + 30;
	torre_defensa[JUG_VIS][7].y= POS_Y_TORRES_2 + 20;
	torre_defensa[JUG_VIS][7].viva=1;
	torre_defensa[JUG_VIS][7].sector=1;
//termina torre 2 VIS

//torre 3 VIS empieza
	torre_defensa[JUG_VIS][8].x= 400;
	torre_defensa[JUG_VIS][8].y= POS_Y_TORRES_2;
	torre_defensa[JUG_VIS][8].viva=1;
	torre_defensa[JUG_VIS][8].sector=2;
	torre_defensa[JUG_VIS][9].x= 400 + 30;
	torre_defensa[JUG_VIS][9].y= POS_Y_TORRES_2;
	torre_defensa[JUG_VIS][9].viva=1;
	torre_defensa[JUG_VIS][9].sector=3;
	
	torre_defensa[JUG_VIS][10].x= 400;
	torre_defensa[JUG_VIS][10].y= POS_Y_TORRES_2 + 20;
	torre_defensa[JUG_VIS][10].viva=1;
	torre_defensa[JUG_VIS][10].sector=0;
	
	torre_defensa[JUG_VIS][11].x= 400 + 30;
	torre_defensa[JUG_VIS][11].y= POS_Y_TORRES_2 + 20;
	torre_defensa[JUG_VIS][11].viva=1;
	torre_defensa[JUG_VIS][11].sector=1;
//termina torre 3 VIS

//torre 4 VIS empieza
	torre_defensa[JUG_VIS][12].x= 570;
	torre_defensa[JUG_VIS][12].y= POS_Y_TORRES_2;
	torre_defensa[JUG_VIS][12].viva=1;
	torre_defensa[JUG_VIS][12].sector=2;
	torre_defensa[JUG_VIS][13].x= 570 + 30;
	torre_defensa[JUG_VIS][13].y= POS_Y_TORRES_2;
	torre_defensa[JUG_VIS][13].viva=1;
	torre_defensa[JUG_VIS][13].sector=3;
	
	torre_defensa[JUG_VIS][14].x= 570;
	torre_defensa[JUG_VIS][14].y= POS_Y_TORRES_2 + 20;
	torre_defensa[JUG_VIS][14].viva=1;
	torre_defensa[JUG_VIS][14].sector=0;
	
	torre_defensa[JUG_VIS][15].x= 570 + 30;
	torre_defensa[JUG_VIS][15].y= POS_Y_TORRES_2 + 20;
	torre_defensa[JUG_VIS][15].viva=1;
	torre_defensa[JUG_VIS][15].sector=1;
//termina torre 4 VIS

	naveroja[JUG_LOC].x= 10;
	naveroja[JUG_LOC].y= POS_Y_NAVEROJA_1;
	naveroja[JUG_LOC].su_estado=0;
	
	naveroja[JUG_VIS].x= 10;
	naveroja[JUG_VIS].y= POS_Y_NAVEROJA_1 - 300;
	naveroja[JUG_VIS].su_estado=0;
	
	mi_bala[JUG_LOC].accion=0;
	mi_bala[JUG_VIS].accion=0;
	
	mi_bala_enemiga[JUG_LOC].accion=0;
	mi_bala_enemiga[JUG_VIS].accion=0;
		
	//PARA TTF
	fgcolor.r=200; 
	fgcolor.g=100;
	fgcolor.b=10;
	bgcolor.r=100;
	bgcolor.g=0;
	bgcolor.b=0;

	mis_puntos[JUG_LOC]=0;
	mis_puntos[JUG_VIS]=0;
}	

//------------------UTILIDADES----------------------//
	
void esperarRival(int puntaje){
	SDL_Surface *image;
	SDL_Rect dest;
	//Para el cartel de TTF
	SDL_Surface *ttext_cartel;
	SDL_Color colorfuente={208,52,182};
	SDL_Rect rect_cartel={0,0,0,0};
	TTF_Font *fuente;
	char msg[50]="ESPERANDO OPONENTE...";
	//Para los puntos
	SDL_Surface *ttext_puntaje;
	SDL_Rect rect_puntaje={0,0,0,0};	
	char msg_puntaje[60]="Puntaje acumulado:  ";
	char msg_puntos[10];
	sprintf(msg_puntos, "%d", puntaje);
	strcat(msg_puntaje, msg_puntos);
		
	// carga la funte de letra
	fuente = TTF_OpenFont("dailypl2a.ttf",TAM_FUENTE);
	
	ttext_cartel = TTF_RenderText_Solid(fuente,msg,colorfuente);
	rect_cartel.x= 50;
	rect_cartel.y= 50;
	rect_cartel.w= ttext_cartel->w;
	rect_cartel.h= ttext_cartel->h;

	ttext_puntaje = TTF_RenderText_Solid(fuente,msg_puntaje,colorfuente);
	rect_puntaje.x= 100;
	rect_puntaje.y= 150;
	rect_puntaje.w= ttext_puntaje->w;
	rect_puntaje.h= ttext_puntaje->h;
			
	// Cargamos gráfico
	image = SDL_LoadBMP("./imagenes/fondo.bmp");
	if ( image == NULL ) {
		printf("No pude cargar gráfico: %s\n", SDL_GetError());
		exit(1);
	}
		
	dest.x = 0;
	dest.y = 0;
	dest.w = 868;
	dest.h = 600;
		
    SDL_FillRect(screen,NULL,SDL_MapRGB(screen->format,0,0,0));
		
	SDL_BlitSurface(image, NULL, screen, &dest);
        
    SDL_BlitSurface(ttext_cartel,NULL,screen,&rect_cartel);
 
    SDL_BlitSurface(ttext_puntaje,NULL,screen,&rect_puntaje);
    
	SDL_Flip(screen);
	//SDL_Delay(30);
	
	// Definimos color para la transparencia
	SDL_SetColorKey(image,SDL_SRCCOLORKEY|SDL_RLEACCEL,SDL_MapRGB(image->format,255,0,0));

	// Mostramos la pantalla
	SDL_Flip(screen);

	// liberar superficie
	SDL_FreeSurface(image);
	
	TTF_CloseFont(fuente);
	SDL_FreeSurface(ttext_cartel);
	SDL_FreeSurface(ttext_puntaje);	
}
		
void cargarFondo(){
	SDL_Surface *image;
	SDL_Rect rect={0,0,0,0};
	// FONDO
	image = SDL_LoadBMP("./imagenes/fondo.bmp");
	
	if( image == NULL ){
		printf("No pude cargar gráfico: %s\n", SDL_GetError());
		exit(1);
	}
	
	rect.x = 0;
	rect.y = 0;
	rect.w = 868;
	rect.h = 600;
		
    SDL_FillRect(screen,NULL,SDL_MapRGB(screen->format,0,0,0));
	SDL_BlitSurface(image, NULL, screen, &rect);
}

void pantallaPresentacion(){
	
	int listo=0;
	SDL_Event event;
	SDL_Surface *ttext;
	SDL_Color colorfuente={200,200,255};
	TTF_Font *fuente;
	char msg[50]="Presione una tecla";
	
	// FONDO
	cargarFondo();
	
	// TEXTO
	fuente = TTF_OpenFont(fuente_base,TAM_FUENTE);
	
	ttext = TTF_RenderText_Solid(fuente,msg,colorfuente);
	
	rect.x= 100;
	rect.y= 50;
	rect.w= ttext->w;
	rect.h= ttext->h;
		
	ttext = TTF_RenderText_Solid(fuente,msg,colorfuente);
	SDL_BlitSurface(ttext,NULL,screen,&rect);
	SDL_Flip(screen);

	// PIDO UNA TECLA
	while(SDL_WaitEvent(&event) && listo==0)
		if(event.type == SDL_KEYDOWN)
			listo=1;

	TTF_CloseFont(fuente);
	SDL_FreeSurface(ttext);
}

void pantallaEspera(){
	
	int i=0;
	SDL_Surface *ttext;
	SDL_Color colorfuente={255,255,255};
	SDL_Rect rect={0,0,0,0};
	TTF_Font *fuente;
	char msg[50]="Preparando partida...";
	
	fuente = TTF_OpenFont(fuente_base,TAM_FUENTE);
	ttext = TTF_RenderText_Solid(fuente,msg,colorfuente);
	
	for(i=0; i<4;i++){	
		rect.x= 200;
		rect.y= 100;
		rect.w= ttext->w;
		rect.h= ttext->h;
		
		ttext = TTF_RenderText_Solid(fuente,msg,colorfuente);
		SDL_FillRect(screen,NULL,SDL_MapRGB(screen->format,0,0,0)); 
		SDL_BlitSurface(ttext,NULL,screen,&rect);
		SDL_Flip(screen);
		sprintf(msg, "%d", (3-i));
		sleep(1);
	}

	TTF_CloseFont(fuente);
	SDL_FreeSurface(ttext);
}

int ingresarNombre(char *alias){
	
	SDL_Surface *ttext_1, *ttext_2;
	SDL_Event event;
	int fin = 0;
	SDL_Color colorfuente={255,255,255};
	SDL_Rect rect={0,0,0,0};
	SDL_Rect rect2={0,0,0,0};
	TTF_Font *fuente;
	char msg[50]="\0";
	char msg_final[50]="Ingrese su nombre :";
	
	// MANEJO TEXTO - TTF
	fuente = TTF_OpenFont(fuente_base,TAM_FUENTE);
	ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente);
    rect2.x= 50;
    rect2.y= 50;
    rect2.w= 100;	//	ttext_1->w;
    rect2.h= 100;	//	ttext_1->h;
    
    
	while(!fin) {
      
        SDL_FillRect(screen,NULL,SDL_MapRGB(screen->format,0,0,0));
     
        SDL_BlitSurface(ttext_1,NULL,screen,&rect2);
 
        if(msg[0]){		
            ttext_2 = TTF_RenderText_Solid(fuente,msg,colorfuente);
            rect.x=50;
            rect.y=150;
            rect.w=100;		//	ttext_2->w;
            rect.h=100;		//	ttext_2->h;
            SDL_BlitSurface(ttext_2,NULL,screen,&rect);   
        }

		while(SDL_PollEvent(&event)){
    		switch(event.type){
                case SDL_KEYDOWN:
							if(event.key.keysym.sym>='a'&&event.key.keysym.sym<='z'){
								if(strlen(msg)<30-1)
									msg[strlen(msg)]=event.key.keysym.sym;
									
								if(strlen(msg)==2)
									msg[0]=msg[0]+('A'-'a');
							}
							
							if(event.key.keysym.sym == SDLK_BACKSPACE){
								if(strlen(msg)>0)
									msg[strlen(msg)-1]='\0';
							}
							
							if(event.key.keysym.sym == SDLK_ESCAPE)
								fin = 1;
							
							 if(event.key.keysym.sym == SDLK_RETURN){
								if(strlen(msg)>0)
									fin = 1;
							}
							break;
							
                case SDL_QUIT:
							fin=1;
							break;
            }
		}
		
		SDL_Flip(screen);
		SDL_Delay(30);
	}
	
	TTF_CloseFont(fuente);
	SDL_FreeSurface(ttext_1);
	SDL_FreeSurface(ttext_2);
	strcpy(alias, msg);
	
	return 0;
}

//---------------FUNCIONES PROPIAS DEL SPACE INVADER ------------------//

int murieronFantasmas(){
	int cant;
	
	for(cant=0; cant<60; cant++)
		if(mis_enemigos[cant].su_estado==1)
			return 0;
	
	return 1;
}

//Inicializa el disparo de la nave del jugador "nro_jugador"
void creadisparo(int nro_jugador){
	int resta_en_y;
	
	if (nro_jugador == JUG_LOC)
		resta_en_y=0;
	if (nro_jugador == JUG_VIS)
		resta_en_y=-25;
	
	mi_bala[nro_jugador].accion=1;
	mi_bala[nro_jugador].x=jugador[nro_jugador].x + 17;
	mi_bala[nro_jugador].y=jugador[nro_jugador].y - resta_en_y;
	hubo_disparo[nro_jugador]=0;
}

// Inicializa la bala del fantasmaLo que hace es inicilizar la bala del fantasma, busca que fantasma la va a tirar, y le toma las coordenadas para la posicion de la bala */
void creardisparoEnemigo(int jug){
	int pos;
	
	if(jug == JUG_LOC){
		pos=59;
		while(pos>=30){
			if(mis_enemigos[pos].su_estado==1)
				break;
			pos--;
		}
		
		if(pos<30){
			mi_bala_enemiga[jug].accion=0;
			return;
		}
	}
	
	if(jug == JUG_VIS){
		pos=0;
	
		while(pos<=29){
			if(mis_enemigos[pos].su_estado==1)
				break;
			pos++;
		}
		
		if(pos>29){
			mi_bala_enemiga[jug].accion=0;
			return;
		}
	}

	mi_bala_enemiga[jug].accion=1;
	mi_bala_enemiga[jug].x= mis_enemigos[pos].x + 12;
	mi_bala_enemiga[jug].y= mis_enemigos[pos].y + 12;
}

int mueveNaveRoja(int jug){
	
	if(naveroja[jug].x < 630){ 
		naveroja[jug].x+= 15;
		return 1;
	}
	
	naveroja[jug].x=0;	
	naveroja[jug].su_estado=0;
	return 1;
}

void mueveFantasmas(){
	int i;	
	
	if(flag==2 && mis_enemigos[0].x <= 30)
		flag=0;
	
	if(flag==1 && mis_enemigos[59].x <= 630)
		flag=0;

	if(flag==0 && mis_enemigos[59].x < 630 )	
		flag=1;
		
	if(flag==1)
		for(i=0; i<60; i++)
			mis_enemigos[i].x += 5;
	
	if(flag==0 && mis_enemigos[0].x > 30)
		flag=2;
		
	if(flag==2)
		for(i=0; i<60; i++)
			mis_enemigos[i].x -= 5;
}

void muevebalas(int jug){
	int operando_y;
	
	if(jug == JUG_LOC)
		operando_y = -10;
	if(jug == JUG_VIS)
		operando_y = +10;

	if(mi_bala[jug].accion==1)
		if(mi_bala[jug].x > 2){
			mi_bala[jug].y= mi_bala[jug].y + operando_y;
			// si el disparo sale de la pantalla la desactivamos	
			if((jug == JUG_LOC && mi_bala[jug].y < 0) || (jug == JUG_VIS && mi_bala[jug].y > 600))
				mi_bala[jug].accion=0;			
		}
}

/** Lo que hace es mover la bala que lanzo el fantasmita en caso de que siga activa */
void muevebalas_enemigas(int jug){
	int desplazamiento_y;
	
	if(jug == JUG_LOC)
		desplazamiento_y = 10;
	if(jug == JUG_VIS)
		desplazamiento_y = -10;
	
	if(mi_bala_enemiga[jug].accion==1){
		mi_bala_enemiga[jug].y= mi_bala_enemiga[jug].y + desplazamiento_y;
		// si el disparo sale de la pantalla la desactivamos	
		if ((jug == JUG_LOC && mi_bala_enemiga[jug].y > 600)||(jug == JUG_VIS && mi_bala_enemiga[JUG_VIS].y < 0)){	
			mi_bala_enemiga[jug].accion=0;
			mi_bala_enemiga[jug].x=0;
			mi_bala_enemiga[jug].y=0;
			bala_fantasmas[jug].setx(-10);
			bala_fantasmas[jug].sety(-10);
		}
	}
}

//--------------------FUNCIONES PARA TRATAR LAS COLISIONES -----------------------//

void colisionBalaJugLoc(){
int i, cant;
char auxcad[70];
char auxNum[5];
	//chequeo las colisiones con alguno de los fantasmistas (de la bala que lanza el jugador)
	
	for(i=0; i<60; i++)		
		if( mis_enemigos[i].su_estado == 1 && enemigos[i].colision(bala[JUG_LOC])==TRUE){
			mi_bala[JUG_LOC].accion=0;
			bala[JUG_LOC].setx(-50);
			bala[JUG_LOC].sety(-50);
			hubo_disparo[JUG_LOC]=1;
			//le envio la colision al server!
			strcpy(auxcad,"88|0|");
			sprintf(auxNum,"%d",i);
			strcat(auxcad, auxNum);
			strcat(auxcad, "|100|");
			strcpy(buffer,auxcad);
			
			if(send(getSocket(),buffer, sizeof(buffer),0)<=0)
				exit(1);
			
			return ;
		}

	//chequeo la colision de la nave roja con la bala que lanza el jugador
	if(mi_bala[JUG_LOC].accion==1 && naveroja[JUG_LOC].su_estado==1 && snave_roja[JUG_LOC].colision(bala[JUG_LOC])== TRUE){
		mi_bala[JUG_LOC].accion=0;
		bala[JUG_LOC].setx(-50);
		bala[JUG_LOC].sety(-50);
		hubo_disparo[JUG_LOC]=1;	
		naveroja[JUG_LOC].x=10;
		
		strcpy(auxcad,"89|0");
		strcat(auxcad, "|1000|");
		strcpy(buffer,auxcad);
		
		if(send(getSocket(),buffer, sizeof(buffer),0)<=0)
			exit(1);
		
		return ;
	}
	
	//chequeo la colision de torre con la bala que lanza el jugador
	
	for(int cant=0; cant<16; cant++){
		if(torre_defensa[JUG_LOC][cant].viva==1 && j_torre_loc[cant].colision(bala[JUG_LOC])== TRUE){
			mi_bala[JUG_LOC].accion=0;
			hubo_disparo[JUG_LOC]=1;
			bala[JUG_LOC].setx(-50);	
			bala[JUG_LOC].sety(-50);
			//le envio la colision al server!
			strcpy(auxcad,"90|0|0|");
			sprintf(auxNum,"%d",cant);
			strcat(auxcad, auxNum);
			strcat(auxcad, "|");
			strcpy(buffer,auxcad);
			if(send(getSocket(),buffer, sizeof(buffer),0)<=0)
				exit(1);
			
			return ;	
		}	
	}
	
	//chequeo la colision de torre2 con la bala que lanza el jugador1
	for(cant=0; cant<16; cant++){
		if(torre_defensa[JUG_VIS][cant].viva==1 && j_torre_vis[cant].colision(bala[JUG_LOC])== TRUE){
			mi_bala[JUG_LOC].accion=0;
			hubo_disparo[JUG_LOC]=1;
			bala[JUG_LOC].setx(-50);	
			bala[JUG_LOC].sety(-50);
			//le envio la colision al server!
			strcpy(auxcad,"90|0|1|");
			sprintf(auxNum,"%d",cant);
			strcat(auxcad, auxNum);
			strcat(auxcad, "|");
			strcpy(buffer,auxcad);
			
			if(send(getSocket(),buffer, sizeof(buffer),0)<=0)
				exit(1);

			return ;	
		}	
	}
	
	//chequeo la colision de mi bala JUGADOR 1 respecto a la nave del jugador 2
	if(mi_bala[JUG_LOC].accion==1 && nave[JUG_VIS].colision(bala[JUG_LOC])==TRUE){
		mis_vidas[JUG_VIS]--;
		mi_bala[JUG_LOC].accion=0;
		hubo_disparo[JUG_LOC]=1;
		bala[JUG_LOC].setx(-50);
		bala[JUG_LOC].sety(-50);
		mis_puntos[JUG_LOC]+=500;
		return ;
	}
	
	///seria el caso de que no colisione con nada
	return ;
}

void colisionBalaJugVis(){
int i, cant;
char auxcad[70];
char auxNum[5];	
	//chequeo las colisiones de la bala del Jugador Visitante con alguno de los fantasmistas
	for(i=0; i<60; i++)
		if( mis_enemigos[i].su_estado == 1 && enemigos[i].colision(bala[JUG_VIS])==TRUE){
			mi_bala[JUG_VIS].accion=0;
			bala[JUG_VIS].setx(-50);
			bala[JUG_VIS].sety(-50);
			hubo_disparo[JUG_VIS]=1;
			
			//le envio la colision al server!
			strcpy(auxcad,"88|1|");
			sprintf(auxNum,"%d",i);
			strcat(auxcad, auxNum);
			strcat(auxcad, "|100|");
			strcpy(buffer,auxcad);
			
			if(send(getSocket(),buffer, sizeof(buffer),0)<=0)
				exit(1);
			
			return ;
		}
		
	//chequeo la colision de la nave roja con la bala que lanza el jugador 2
	if(mi_bala[JUG_VIS].accion==1 && naveroja[JUG_VIS].su_estado==1 && snave_roja[JUG_VIS].colision(bala[JUG_VIS])== TRUE){
		mi_bala[JUG_VIS].accion=0;
		bala[JUG_VIS].setx(-50);
		bala[JUG_VIS].sety(-50);
		hubo_disparo[JUG_VIS]=1;
		naveroja[JUG_VIS].x=10;
		
		//le envio la colision al server!
		strcpy(auxcad,"89|1");
		strcat(auxcad, "|1000|");
		strcpy(buffer,auxcad);
		
		if(send(getSocket(),buffer, sizeof(buffer),0)<=0){
					exit(1);
		}
		return ;
	}
	
	//chequeo la colision de torre2 con la bala que lanza el jugador 2
	for(cant=0; cant<16; cant++){
		if(torre_defensa[JUG_LOC][cant].viva==1 && j_torre_loc[cant].colision(bala[JUG_VIS])== TRUE){
			mi_bala[JUG_VIS].accion=0;
			hubo_disparo[JUG_VIS]=1;
			bala[JUG_VIS].setx(-50);	
			bala[JUG_VIS].sety(-50);
			strcpy(auxcad,"90|1|0|");
			sprintf(auxNum,"%d",cant);
			strcat(auxcad, auxNum);
			strcat(auxcad, "|");
			strcpy(buffer,auxcad);
			
			if(send(getSocket(),buffer, sizeof(buffer),0)<=0){
						exit(1);
			}
			return ;	
		}	
	}
	
	//chequeo la colision de torre con la bala que lanza el jugador2
	for(cant=0; cant<16; cant++){
		if(torre_defensa[JUG_VIS][cant].viva==1 && j_torre_vis[cant].colision(bala[JUG_VIS])== TRUE){
			mi_bala[JUG_VIS].accion=0;
			hubo_disparo[JUG_VIS]=1;
			bala[JUG_VIS].setx(-50);	
			bala[JUG_VIS].sety(-50);
			//le envio la colision al server!
			strcpy(auxcad,"90|1|1|");
			sprintf(auxNum,"%d",cant);
			strcat(auxcad, auxNum);
			strcat(auxcad, "|");
			strcpy(buffer,auxcad);
			
			if(send(getSocket(),buffer, sizeof(buffer),0)<=0)
				exit(1);
			
			
			return ;
		}	
	}
	//chequeo la colision de la bala del jugador 2 respecto la nave del jugador 1
	if(mi_bala[JUG_VIS].accion==1 && nave[JUG_LOC].colision(bala[JUG_VIS])==TRUE){
		mis_vidas[JUG_LOC]--;
		mi_bala[JUG_VIS].accion=0;
		hubo_disparo[JUG_VIS]=1;
		bala[JUG_VIS].setx(-50);
		bala[JUG_VIS].sety(-50);
		mis_puntos[JUG_VIS]+=500;
		return ;
	}
	
	///seria el caso de que no colisione con nada
	return ;
}

void colisionBalaFantasmitaLoc(){
	int cant;
	char auxcad[70];
	char auxNum[5];	
	
	//chequeo la colision con la torre (de la bala que lanza el fantasmita	
	for(cant=0; cant<16; cant++){
		if(torre_defensa[JUG_LOC][cant].viva==1 && j_torre_loc[cant].colision(bala_fantasmas[JUG_LOC])== TRUE){
			mi_bala_enemiga[JUG_LOC].accion=0;
			bala_fantasmas[JUG_LOC].setx(-50);	
			bala_fantasmas[JUG_LOC].sety(-50);
			strcpy(auxcad,"92|0|");
			sprintf(auxNum,"%d",cant);
			strcat(auxcad, auxNum);
			strcat(auxcad, "|");
			strcpy(buffer,auxcad);
			if(send(getSocket(),buffer, sizeof(buffer),0)<=0){
				exit(1);
			}
			return ;	
		}	
	}
	
	if(mi_bala_enemiga[JUG_LOC].accion==1 && nave[JUG_LOC].colision(bala_fantasmas[JUG_LOC])== TRUE){
		strcpy(auxcad,"");
		strcpy(auxcad,"91|0|");
		strcpy(buffer,auxcad);
		if(send(getSocket(),buffer, sizeof(buffer),0)<=0)
			exit(1);
		
		return ;
	}
	///seria el caso en el que la bala que lanzo el fantasmita enemigo no colisiono
	return ;
}

void colisionBalaFantasmitaVis(){
int cant;
char auxcad[70];
char auxNum[5];
	for(cant=0; cant<16; cant++){
		if(torre_defensa[JUG_VIS][cant].viva==1 && j_torre_vis[cant].colision(bala_fantasmas[JUG_VIS])== TRUE){
			mi_bala_enemiga[JUG_VIS].accion=0;
			bala_fantasmas[JUG_VIS].setx(-50);	
			bala_fantasmas[JUG_VIS].sety(-50);
			strcpy(auxcad,"92|1|");
			sprintf(auxNum,"%d",cant);
			strcat(auxcad, auxNum);
			strcat(auxcad, "|");
			strcpy(buffer,auxcad);		
			if(send(getSocket(),buffer, sizeof(buffer),0)<=0)
				exit(1);
			
			return ;	
		}	
	}
	
	//chequeo la colision con mi nave, respecto a la bala que lanza el fantasmita (y si colisiono pierdo una vida!)	para jugador2
	if(mi_bala_enemiga[JUG_VIS].accion==1 && nave[JUG_VIS].colision(bala_fantasmas[JUG_VIS])== TRUE){	
		//le envio la colision al server!
		strcpy(auxcad,"");
		strcpy(auxcad,"91|1|");
		strcpy(buffer,auxcad);
		if(send(getSocket(),buffer, sizeof(buffer),0)<=0)
			exit(1);
	
		return;
	}
	///seria el caso en el que la bala que lanzo el fantasmita enemigo no colisiono
	return;
}

//------------------ FUNCIONES DE FINALIZACION ---------------//

void finalizoServer(int puntaje){
	SDL_Surface *image;
	SDL_Rect dest;
	int listo = 0;
	//Para el cartel de TTF
	SDL_Surface *ttext_cartel2, *ttext_cartel3;
	SDL_Color colorfuente={208,52,182};
	SDL_Rect rect_cartel={0,0,0,0};
	TTF_Font *fuente;
	inicializarSDL();
	char msg[50]="El torneo ha finalizado..";
	char msg_salir[50]="Presione una tecla para salir...";

	//Para los puntos
	SDL_Surface *ttext_puntaje;
	SDL_Rect rect_puntaje={0,0,0,0}, rect_salir;	
	char msg_puntaje[60]="Su puntaje es:  ";
	char msg_puntos[10];
	sprintf(msg_puntos, "%d", puntaje);
	strcat(msg_puntaje, msg_puntos);
		
	//Carga la funte de letra
	fuente = TTF_OpenFont(fuente_base,TAM_FUENTE);
	
	ttext_cartel2 = TTF_RenderText_Solid(fuente,msg,colorfuente);
	rect_cartel.x= 50;
	rect_cartel.y= 50;
	rect_cartel.w= ttext_cartel2->w;
	rect_cartel.h= ttext_cartel2->h;

	ttext_puntaje = TTF_RenderText_Solid(fuente,msg_puntaje,colorfuente);
	rect_puntaje.x= 100;
	rect_puntaje.y= 150;
	rect_puntaje.w= ttext_puntaje->w;
	rect_puntaje.h= ttext_puntaje->h;
		
	ttext_cartel3 = TTF_RenderText_Solid(fuente,msg_salir,colorfuente);
	rect_salir.x= 50;
	rect_salir.y= 250;
	rect_salir.w= ttext_cartel3->w;
	rect_salir.w= ttext_cartel3->h;	
		
	// Cargamos gráfico
	image = SDL_LoadBMP("./imagenes/fondo.bmp");
	if( image == NULL ){
		printf("No pude cargar gráfico: %s\n", SDL_GetError());
		exit(1);
	}
	dest.x = 0;
	dest.y = 0;
	dest.w = 868;
	dest.h = 600;
	
	SDL_FillRect(screen,NULL,SDL_MapRGB(screen->format,0,0,0));
	SDL_BlitSurface(image, NULL, screen, &dest);
	SDL_BlitSurface(ttext_cartel2,NULL,screen,&rect_cartel);
	SDL_BlitSurface(ttext_puntaje,NULL,screen,&rect_puntaje);
	SDL_BlitSurface(ttext_cartel3,NULL,screen,&rect_salir);
	SDL_Flip(screen);
	
	while(SDL_WaitEvent(&event) && listo==0)
		if(event.type == SDL_KEYDOWN)
			listo=1;
		
	// Libero la superficie
	SDL_FreeSurface(image);
	TTF_CloseFont(fuente);
	SDL_FreeSurface(ttext_cartel2);
	SDL_FreeSurface(ttext_puntaje);	
	SDL_FreeSurface(ttext_cartel3);
}

void finaliza(){
	TTF_CloseFont(fuente);
	SDL_FreeSurface(ttext_1);
	SDL_FreeSurface(ttext_2);
}

void funcionTerminoTorneo(){		
	sem_unlink(auxSemInicio);
	sem_unlink(auxSemFin);
	close(getSocket());
	atexit(SDL_Quit);
	atexit(TTF_Quit);
	usleep(10000);
}
