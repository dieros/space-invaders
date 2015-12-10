#include "funciones.h"

//--------------------MAIN--------------------//

int main(){
	key_t clave;
	mipid=getpid();
	clave = ftok(".",234);
	srand(time(0));
	
	struct sigaction act;
	act.sa_sigaction = SIGCHLDHandler;
	sigfillset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGCHLD,&act,NULL);
	signal(SIGINT,SIGINTHandler);
	signal(SIGTERM,SIGTERMHandler);
	
	// Crea memorias compartidas
	memJugadores = shmget(clave,sizeof(t_jugadores),IPC_CREAT | 0660);
	
	jugadores = (t_jugadores *) shmat(memJugadores,NULL,0);
	
	// Crea un demonio para monitorear el server
	if ((pidDemonio=fork())==0){
		levantaProtector(mipid); 
		exit(1);
	}
	
	// Inicializo el entorno de SDL
	if(SDL_Init(SDL_INIT_VIDEO) < 0){
		printf("No se ha podido inicializar SDL: %s\n",SDL_GetError());
		pthread_exit((void *)1);
	}

	screen= SDL_SetVideoMode(768,600,24,SDL_HWSURFACE | SDL_DOUBLEBUF);
	if(screen == NULL){
		fprintf(stderr,"No se puede establecer el modo de video 640x480: %s\n",SDL_GetError());
		pthread_exit((void *)1);
	}
	
	if(TTF_Init() < 0){
		printf("No se pudo iniciar SDL_ttf: %s\n",SDL_GetError());
		pthread_exit((void*)1);
	}
	
	// Inicializa cantidad de jugadores
	jugadores->cantJugadores = 0;

	// Inicializa seguir
	jugadores->flagFinConexiones=0;	// Inicializa puntajes
	
	// Inicializa Cantidad de partidas totales
	jugadores->cantPartidasTotales=0;
	
	ciclo_escuchar_conexiones=1;
	flag_sigchld=0;
	jugadores->flag_sigchld_mc=0;
	
	// Crea los threads de Escucha y Dibuja
	pthread_t threadEscucha,
			  threadDemonio,
			  threadDibuja;
	
	// Crea semaforos Jugadores y Asignar
	semJugadores = sem_open("/semJugadores", O_CREAT,0644,1);
	semAsignar = sem_open("/asignarGeneral",O_CREAT,0644,0);
	
	flag_conexiones=1;
	
	// Inicializa Matriz de torneo
	inicializarMatrizTorneo();
	
	XInitThreads();
	configurarServidor();	
	
	// Crea el thread attr JOINEABLE
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	// Lanza los threads de Escucha, Demonio y  Dibuja
	pthread_create(&threadEscucha,&attr,escucharConexiones,NULL);
	pthread_create(&threadDibuja,&attr,dibujaThread,NULL);
	pthread_create(&threadDemonio,&attr,controlaDemonio,NULL);
	
	// Loop infinito (el servidor finaliza SOLO por tiempo o por señal)
	while(seguir){			
		sem_wait(semAsignar); 		
		
		asignarPartida(); 				
	}
	
	kill( getpid(), SIGINT );	
	return 1;
}

/**
* 
* ControlaDemonio()
* - Thread que controla que el demonio este activo. Para ello espera que el proceso del demonio muera y lo relanza
*
*/


void * controlaDemonio(void *arg){
	int demonioActivo = 1;
	while(demonioActivo){
		waitpid(pidDemonio,NULL,0);
		// Crea un demonio para monitorear el server
		if ((pidDemonio=fork())==0){

			levantaProtector(mipid); 
			//exit(1);
		}
		else{
			printf("\t El nuevo pid del demonio es: %d\n", pidDemonio);			
		}
	}
	exit(0);
}

/**
* 
* escucharConexiones(void *arg)
* - Thread que queda a la espera de nuevas conexiones de jugadores. Una vez detectada una nueva conexión, se setean las memorias compartidas de jugadores y se redimensiona la matriz del torneo
*
*/


void * escucharConexiones(void *arg){
	int listen_socket;
	unsigned short int listen_port = 0;
	unsigned long int listen_ip_address = 0;
	struct sockaddr_in listen_address, con_address;
	socklen_t con_addr_len;	
	char buf[TAMBUF];
	
	listen_socket = socket(AF_INET, SOCK_STREAM, 0);

	// Asignandole una dirección a éste
	bzero(&listen_address, sizeof(listen_address));
	listen_address.sin_family = AF_INET;
	listen_port = htons(PORT);
	listen_address.sin_port = listen_port;
	listen_address.sin_addr.s_addr = htonl(INADDR_ANY);	
	int var=1;
	setsockopt(listen_socket,SOL_SOCKET,SO_REUSEADDR,&var,sizeof(int));
	bind(listen_socket,(struct sockaddr*)&(listen_address), sizeof(struct sockaddr));
	listen(listen_socket, MAXQ);
	bzero(&con_address, sizeof(con_address));
	
	char nombrebuf[TAMBUF];
	
	while(ciclo_escuchar_conexiones>0){
		cSocket.comm_socket[indice] = accept(listen_socket, (struct sockaddr *)(&con_address), &con_addr_len);
		fflush(stdin);
		fflush(stdout);
			
		if(flag_conexiones==1){		
			//Bloqueado esperando el nombre del jugador, nadie mas se conecta hasta que no recibo en nombre del ultimo que intento entrar	
			if( recv(cSocket.comm_socket[indice],nombrebuf,TAMBUF,0) <=0){
				sem_wait(semJugadores);
			
				for(i=0;i<100;i++){
					jugadores->matrizTorneo[indice][i]=1;
					jugadores->matrizTorneo[i][indice]=1;	
				}
			
				jugadores->jugadores[indice].estado=3;
				jugadores->jugadores[indice].puntaje=0;
			
				sem_post(semJugadores);

			
			}
			
			int minimo = puntajeMinimo();
			sem_wait(semJugadores);	
				jugadores->jugadores[indice].socket=cSocket.comm_socket[indice];
				jugadores->jugadores[indice].puntaje=minimo;
				strcpy(jugadores->jugadores[indice].nombre,nombrebuf);
				jugadores->jugadores[indice].cantPartidas = 0;
				jugadores->jugadores[indice].servidorAsignado = 100;
				jugadores->jugadores[indice].id=indice;	// Le asigno este ID , hay jugador 0
				jugadores->cantJugadores++;
				jugadores->jugadores[indice].estado=0;			
			sem_post(semJugadores);
			
			// Ahora puedo asignar, habilito el semaforo 
			sem_post(semAsignar);
			
			//Indice que uso para ir guardando las conexiones
			indice++;			
		}else{
			// Las conexiones están cerradas, le aviso al jugador que no se puede conectar al torneo
			strcpy(buf,"");
			strcpy( buf , "9|FINALIZA|" );
			//send(cSocket.comm_socket[indice],buf,TAMBUF,0); 
			
			//send(server->socket1,buffer,TAMBUF,0);
			close(cSocket.comm_socket[indice]);
			printf("INSCRIPCIONES CERRADAS - INTENTO DE CONEXION POR SOCKET: %d\n", cSocket.comm_socket[indice]);	
		}
	}
	
	puts("********************************************************************");
	puts("Deje de escuchar conexiones");
	puts("********************************************************************");
	
	pthread_exit((void*)1);
}

/**
* 
* crearServidorPartida(void *arg)
* - Funcion que se encarga de crear una nueva partida, seteando el entorno necesario, así como los threads de comunicacion con los clientes con sus respectivos controles.
*
*/

void crearServidorPartida(int mem){
	if((vectorPid[cantServ]=fork())==0){ 
		// Es HIJO			
		
		char buffer[TAMBUF]="",buffer2[TAMBUF]="";
		int ciclos1 = 0;
		int ciclos2 = 0;

		t_servPart *server;
		pthread_t threadComunicacionJ1,threadComunicacionJ2;
		server = (t_servPart *) shmat(mem,NULL,0);//recibe de parametro	
		pthread_mutex_init(&server->mtxMemoria,NULL);
	
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		
		if(jugadores->flag_sigchld_mc == 0 ){
			sem_wait(semJugadores);
			jugadores->cantPartidasTotales++;
			sem_post(semJugadores);
		}else{
			// REVIVO PARTIDA
			jugadores->flag_sigchld_mc=0;
		}

		// ENVIO MENSAJE INICIAL A LOS 2 JUGADORES
		empaquetar0(JUG_LOC,jugadores->jugadores[server->jugador1].nombre,jugadores->jugadores[server->jugador2].nombre,VIDAS,buffer);			
		if(send(server->socket1,buffer,TAMBUF,0)<=0)
			puts("ERROR - EL SOCKET DEL JUGADOR 1 NO RESPONDE");
		
		empaquetar0(JUG_VIS,jugadores->jugadores[server->jugador1].nombre,jugadores->jugadores[server->jugador2].nombre,VIDAS,buffer2);	
		if(send(server->socket2,buffer2,TAMBUF,0)<=0)
			puts("ERROR - EL SOCKET DEL JUGADOR 2 NO RESPONDE");
			
		// LANZO THREADS DE COMUNICACION	
		XInitThreads();
		pthread_create(&threadComunicacionJ1,&attr,threadComunicacion,(void *)server);
		pthread_create(&threadComunicacionJ2,&attr,threadComunicacion2,(void *)server);
			
		flag_muevefantasmas[cantServ] = 1;
		flag_abrupto[cantServ].jug1=33;
		flag_abrupto[cantServ].jug2=33;
		flag_abrupto[cantServ].est_1=0;
		flag_abrupto[cantServ].est_2=0;		
		while(flag_muevefantasmas[cantServ] == 1){	
			// MUEVO LOS FANTASMAS
			strcpy(buffer,"4|"); 
			send(server->socket1,buffer,TAMBUF,0);
			send(server->socket2,buffer,TAMBUF,0);
			
			usleep(300000);
			ciclos1 ++;
			ciclos2 ++;
			
			//ACTIVAR NAVE ROJA Jugador Local
			if(ciclos1 == 10){ 			
				strcpy(buffer,"5|0|0|");
				send(server->socket1,buffer,TAMBUF,0);
				send(server->socket2,buffer,TAMBUF,0);
				ciclos1 = 0;		
			}
			
			//ACTIVAR NAVE ROJA Jugador Visitante
			if(ciclos2 == 30){
				strcpy(buffer,"5|0|1|");
				send(server->socket1,buffer,TAMBUF,0);
				send(server->socket2,buffer,TAMBUF,0);
				ciclos2 = 0;	
			}

			strcpy(buffer,"5|1|999|"); // MUEVE NAVE ROJA. 999 NUMERO SOLO PARA QUE FUNCIONE BIEN strtok.
			send(server->socket1,buffer,TAMBUF,0);
			send(server->socket2,buffer,TAMBUF,0);
		}
		
		// ESPERO A QUE LOS 2 CLIENTES TERMINEN LA PARTIDA
		sem_wait(server->semFinaliza);
		sem_wait(server->semFinaliza);
						
		// AVISO A LOS 2 CLIENTES QUE AMBOS TERMINARON LA PARTIDA
		
		char numero[TAMBUF];
		sprintf(numero,"%d",6); 
		strcat(numero,"|");
		
		//TERMINA
		if(flag_abrupto[cantServ].est_1==0)
			send(server->socket1,numero,TAMBUF,0);
		
		if(flag_abrupto[cantServ].est_2==0)	
			send(server->socket2,numero,TAMBUF,0);
		
		sem_wait(semJugadores);
		
		jugadores->cantPartidasTotales--;
		sem_post(semJugadores);
		
		// ESPERO EL CIERRE DE LA COMUNICACION
		pthread_join(threadComunicacionJ1,NULL);
		pthread_join(threadComunicacionJ2,NULL);
	
		// LIBERO AL JUGADOR Y ACTUALIZO PUNTAJE
		sem_wait(semJugadores);
			jugadores->jugadores[server->jugador1].servidorAsignado = 100;
			jugadores->jugadores[server->jugador2].servidorAsignado = 100;
			jugadores->jugadores[server->jugador1].puntaje+=server->puntaje[0];		
			jugadores->jugadores[server->jugador2].puntaje+=server->puntaje[1];
		sem_post(semJugadores);
			
		// AVISO A LOS 2 CLIENTES QUE LAS COMUNICACIONES ESTAN CERRADAS Y FINALIZO LA PARTIDA
		sprintf(numero,"%d",7);
		strcat(numero,"|");	
		
		if(flag_abrupto[cantServ].est_1==0)
			send(server->socket1,numero,TAMBUF,0);
			
		if(flag_abrupto[cantServ].est_2==0)
			send(server->socket2,numero,TAMBUF,0);
		
		// LIBERO LA MEMORIA DEL SERVER Y REASIGNO PARTIDAS
		shmctl(mem,IPC_RMID,0);
	
		flag_sigchld=0;
	
		sem_post(semAsignar);
		
		exit(1);	
	}
}

/**
* 
* asignarPartida()
* - Funcion que se encarga de controlar la matriz de jugadores, chequeando así que todos ellos hayan jugado entre sí, actualizando la memoria compartida de Jugadores
*
*/

void asignarPartida(){	
	int i,j;
	key_t clave;
	
	for(i=0;i<jugadores->cantJugadores;i++)
		for(j=i+1;j<jugadores->cantJugadores;j++){
			if((jugadores->jugadores[i].servidorAsignado==100 && jugadores->jugadores[i].estado!=3) && ( jugadores->jugadores[j].servidorAsignado==100 && jugadores->jugadores[j].estado!=3) )	
				if(jugadores->matrizTorneo[i][j]==0){ 	
					
					//CHEQUEO SI AMBOS ESTAN VIVOS
					
					// Si los jugadores no se cruzaron nunca...
					t_servPart *server;
					char semaforoMC[30],auxSem[30];
					strcpy(semaforoMC,"/semaforoMC");
			
					sprintf(auxSem,"%d",cantServ);
					strcat(semaforoMC,auxSem);
					
					// Genera una clave aleatoria para identifica esta memoria compartida (posiblemente identifica a esta partida especifica solamente)	
					clave = ftok(".",rand()+400); 
					memoria = shmget(clave,sizeof(t_servPart), IPC_CREAT | 0660);
					claves[cantServ]=memoria;
					server = (t_servPart *) shmat(memoria,NULL,0);
					server->semFinaliza = sem_open(semaforoMC,O_CREAT,0644,0);
					
					// Setea los sockets de los 2 jugadores para esta partida
					server->socket1=jugadores->jugadores[i].socket;
					server->socket2=jugadores->jugadores[j].socket;
					server->jugador1=i;
					server->jugador2=j;
					server->puntaje[0]=0;
					server->puntaje[1]=0;
					sem_wait(semJugadores);
					//semaforooo
						jugadores->jugadores[i].servidorAsignado=1;
						jugadores->jugadores[j].servidorAsignado=1;
						jugadores->jugadores[i].cantPartidas++;
						jugadores->jugadores[j].cantPartidas++;
						jugadores->matrizTorneo[i][j]=1;
						jugadores->matrizTorneo[j][i]=1;
					//devolver semaforo
					sem_post(semJugadores);					
					crearServidorPartida(memoria);
					
					cantServ++;
				}
		}

}

//THREAD DE COMUNICACION CON EL JUGADOR 1
void * threadComunicacion(void *arg){ 
	int done=1;
	t_servPart *dato = (t_servPart *) arg;
	int sock1,sock2;
	sock1= dato->socket1;
	sock2=dato->socket2;
	char buffer[TAMBUF];
	char aux[TAMBUF],*pc;
	int num,i,j;		
	while(done){
		
		if(recv(sock1,buffer,TAMBUF,0)<=0){	
			
			done =0;
			flag_abrupto[cantServ].jug1=dato->jugador1;
			flag_abrupto[cantServ].est_1=1;
			strcpy(buffer,"55|");
			send(sock2,buffer,TAMBUF,0);

			sem_wait(semJugadores);
		
			for(i=0;i<100;i++){
				jugadores->matrizTorneo[dato->jugador1][i]=1;
				jugadores->matrizTorneo[i][dato->jugador1]=1;	
			}
		
			jugadores->jugadores[dato->jugador1].estado=3;
			jugadores->jugadores[dato->jugador1].puntaje=0;			
			sem_post(semJugadores);
			
			flag_muevefantasmas[cantServ] = 0;
			sem_post(dato->semFinaliza);

			puts("----------------------------------------------------");
			puts("Un jugador abandono el torneo. Actualizando servidor");
			puts("----------------------------------------------------");
		
			pthread_exit((void *)1);
		}
		
		strcpy(aux,buffer);
		
		if(strlen(aux)>1){
			// buffer: Accion del teclado en el thread eventos
			pc = strtok(aux,"|"); 
			num = atoi(pc);
		}else{
			num=1;
		}	
	
		switch(num){
			
			case 1: // RECIBO MOVIMIENTO DEL JUGADOR LOCAL	
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);
					break;
			
			case 2: // RECIBO LA EJECUCION DE UN DISPARO	
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);
					break;
			
			case 7: // CIERRO LA COMUNICACION
					done =0;
					break;
			
			case 75: //	Se fue un cliente!		
					puts("----------------------------------------------------");
					puts("Un jugador abandono el torneo. Actualizando servidor");
					puts("----------------------------------------------------");	
									
					sem_wait(semJugadores);
						for(i=0;i<100;i++){
							jugadores->matrizTorneo[dato->jugador1][i]=1;
							jugadores->matrizTorneo[i][dato->jugador1]=1;
						}

						jugadores->jugadores[dato->jugador1].estado=3;
						jugadores->jugadores[dato->jugador1].puntaje = 0; 

					sem_post(semJugadores);
				
					flag_muevefantasmas[cantServ] = 0;
					sem_post(dato->semFinaliza);
					
					//aviso al cliente que se fue!
					strcpy(buffer, "45|");
					send(sock1, buffer, TAMBUF,0);
					
					//aviso al cliente que gano xq se fue un cliente
					strcpy(buffer, "55|");
					send(sock2, buffer, TAMBUF,0);	
					
					done=0;		
					break;


			case 88: // RECIBO MOVIMIENTO COLISION CON FANTASMITA	
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);				
					break;
			
			case 89: // RECIBO MOVIMIENTO COLISION CON FANTASMITA
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);				
					break;
			
			case 90: // RECIBO MOVIMIENTO COLISION CON TORRE
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);				
					break;
			
			case 91: // RECIBO DISPARO FANTASMA CONTRA JUGADOR
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);
					break;
			
			case 92: // RECIBO MOVIMIENTO COLISION CON TORRE pero de la bala del fantasmita
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);				
					break;
					
			case 120:// FINALIZO LA PARTIDA 1 CLIENTE
					flag_muevefantasmas[cantServ] = 0;
					sem_post(dato->semFinaliza);			
					// ACTUALIZO PUNTOS DEL JUGADOR
					int puntaje;
					char *pc2=NULL;
										
					pc2 = strtok(NULL,"|");
					sscanf(pc2,"%d",&puntaje);
					sem_wait(semJugadores);	
						jugadores->jugadores[dato->jugador1].puntaje += puntaje; 				
					sem_post(semJugadores);
					break;
		}
	}
	pthread_exit((void *)1);
}

//THREAD DE COMUNICACION DEL JUGADOR 2
void * threadComunicacion2(void *arg){
	int done=1;
	t_servPart *dato = (t_servPart *) arg;
	char buffer[TAMBUF];
	
	int sock1,sock2;
	sock1= dato->socket1;
	sock2=dato->socket2;
	char aux[TAMBUF],*pc;
	int num,i;
	int j;
	while(done){
		
		if(recv(sock2,buffer,TAMBUF,0)<=0){
			done =0;			
			flag_abrupto[cantServ].jug2=dato->jugador2;
			flag_abrupto[cantServ].est_2=1;
			strcpy(buffer,"55|");
			send(sock1,buffer,TAMBUF,0);
			

			sem_wait(semJugadores);
				for(i=0;i<100;i++){
					jugadores->matrizTorneo[dato->jugador2][i]=1;
					jugadores->matrizTorneo[i][dato->jugador2]=1;
				}
				jugadores->jugadores[dato->jugador2].estado=3;
				jugadores->jugadores[dato->jugador2].puntaje=0;
			sem_post(semJugadores);						
			flag_muevefantasmas[cantServ] = 0;
			sem_post(dato->semFinaliza);
			
			puts("----------------------------------------------------");
			puts("Un jugador abandono el torneo. Actualizando servidor");
			puts("----------------------------------------------------");
			pthread_exit((void *)1);
		}
		
		strcpy(aux,buffer);
		
		if(strlen(aux)>1){
			pc = strtok(aux,"|");
			num = atoi(pc);
		}else{
			num=1;
		}
		
		switch(num){
			
			case 1:	
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);
					break;
			
			case 2:
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);
					break;
			
			case 7: // CIERRO LA COMUNICACION
					done =0;
					break;
					
			case 44:
					done=0;
					break;
					
			case 75: //	Se fue un cliente!		
					puts("----------------------------------------------------");
					puts("Un jugador abandono el torneo. Actualizando servidor");
					puts("----------------------------------------------------");
					
					sem_wait(semJugadores);
						for(i=0;i<100;i++){
							jugadores->matrizTorneo[dato->jugador2][i]=1;
							jugadores->matrizTorneo[i][dato->jugador2]=1;
						}

						jugadores->jugadores[dato->jugador2].estado=3;
						jugadores->jugadores[dato->jugador2].puntaje = 0; 
					sem_post(semJugadores);
				
					flag_muevefantasmas[cantServ] = 0;
					sem_post(dato->semFinaliza);
					
					strcpy(buffer, "45|");
					send(sock2, buffer, TAMBUF,0);
					
					strcpy(buffer, "55|");
					send(sock1, buffer, TAMBUF,0);
					done=0;
					break;
					
					
			case 88: // RECIBO MOVIMIENTO COLISION CON FANTASMITA
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);				
					break;
			
			case 89: // RECIBO MOVIMIENTO COLISION CON FANTASMITA
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);				
					break;
			
			case 90: // RECIBO MOVIMIENTO COLISION CON TORRE
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);				
					break;
			
			case 91: // RECIBO DISPARO FANTASMA CONTRA JUGADOR
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);
					break;
			
			case 92: // RECIBO MOVIMIENTO COLISION CON TORRE pero de la bala del fantasmita
					send(sock1,buffer,TAMBUF,0);
					send(sock2,buffer,TAMBUF,0);				
					break;
			
			case 120: // FINALIZO LA PARTIDA 1 CLIENTE
					flag_muevefantasmas[cantServ] = 0;
					sem_post(dato->semFinaliza);
			
					int valor_sem;
					sem_getvalue(dato->semFinaliza, &valor_sem);
					// ACTUALIZO PUNTOS DEL JUGADOR
					int puntaje;
					char *pc2=NULL;
										
					pc2 = strtok(NULL,"|");
					sscanf(pc2,"%d",&puntaje);
					sem_wait(semJugadores);	
						jugadores->jugadores[dato->jugador2].puntaje += puntaje; 
					sem_post(semJugadores);
					break;
						
		}
	}
	pthread_exit((void *)1);
}


void inicializarMatrizTorneo(){
	int i,j;
	for(i=0;i<100;i++)
		jugadores->matrizTorneo[i][i]=1;
	
	for(i=0;i<100;i++)
		for(j=0;j<100;j++)
			if(i!=j)
				jugadores->matrizTorneo[i][j]=0;							
	
}

/**
* @Handler
* SIGCHLDHandler()
* - Handler para la senial CHILD, realiza el wait para que no queden procesos zombies
*
*/

void SIGCHLDHandler(int sig, siginfo_t *info, void *ni){
	int pid,claveMC, indicePid, aRestar;
	t_servPart *serv;
	char buff[TAMBUF];
	switch(info->si_code){
						
		case CLD_KILLED:
						
						if(info->si_pid != pidDemonio){
							puts("----------------------------------------------------");
							puts("Un proceso servidor de partida ha muerto, procederé a relanzarlo");
							puts("----------------------------------------------------");	
							jugadores->flag_sigchld_mc=1;
							pid = info->si_pid;
							
							/// Comparo por el pid del proceso y tomo el indice y la memoria compartida del mismo
							for(indicePid=0;indicePid<cantServ;indicePid++)
								if(vectorPid[indicePid]==pid){
									claveMC=claves[indicePid];
									break;
							}
							
							if(claveMC == 0)
								printf("Error en la busqueda de memoria compartida\n");
							
							// FINALIZACION ABRUPTA DEL SERVIDOR DE PARTIDA (COD: 77)
							serv = (t_servPart *) shmat(claveMC,NULL,0);
							
							if(server_caido==0){	
								sprintf(buff,"%d",77);
								strcat(buff,"|");
								send(serv->socket1,buff,TAMBUF,0);
								send(serv->socket2,buff,TAMBUF,0);
								
								///Le resto a cantServ el indice hallado para que al regenerar la partida pise dicho indice
								aRestar = cantServ - indicePid;
								cantServ -= aRestar;
								crearServidorPartida(claveMC);	// agregado
								cantServ+= aRestar;				
							}

						wait(NULL);//ESPERO A QUE TERMINE LA PARTIDA PARA QUE NO QUEDE ZOMBIE
						}
						else{
						printf("----------------------------------------------------\n");
						printf("El demonio PID: %d a muerto, procederé a relanzarlo\n", pidDemonio);
						printf("----------------------------------------------------\n");
						}	
						break;	
			
		case CLD_DUMPED:
						printf("\tEl proceso %d a terminado anormalmente\n",info->si_pid);
						break;
						
		case CLD_STOPPED:
						printf("\tEl proceso %d a sido detenido\n",info->si_pid);
						break;
						
		case CLD_CONTINUED:
						printf("\tEl proceso %d continua su ejecucion\n",info->si_pid);
						break;	
		default:			
			wait(NULL);
		break;
	}
	
	//wait(NULL);
	
}

/**
* @Handler
* SIGTERMHandler()
* - Handler para la senial TERM, se encarga de cerrar las memorias compartidas y semaforos
*
*/
void SIGTERMHandler(int sig){
	
	// Si es el server manda un killPG(Kill process group osea a todos los hijos) que liberan la variable memoria(su memoria) 	
	server_caido=1;
	if(mipid==getpid()){
		int i;
		char mensj[TAMBUF];
		sem_unlink("/semJugadores");
		sem_unlink("/asignarGeneral");
		system("rm /dev/shm/sem.semaforoMC*");
		puts("FINALIZANDO SERVIDOR - CIERRO CLIENTES");
		for(i=0;i<indice;i++)
			if(jugadores->jugadores[i].estado!=3){

				bzero(mensj,TAMBUF);
				sprintf(mensj,"%d",10);
				strcat(mensj,"|");
				send(jugadores->jugadores[i].socket,mensj,TAMBUF,0);
			}	
		
		shmctl(memJugadores,0,IPC_RMID);
		shmctl(memServi,0,IPC_RMID);
		
		killpg(getpgrp(),SIGTERM);
		
	}else{
		kill(getpid(),SIGKILL);
	}
	
	puts("HANDLER SIGTERM - Finalizando recursos...");
	exit(1);
}

/**
* @Handler
* SIGINTHandler()
* -Handler para la senial INT, se encarga de cerrar las memorias compartidas y semaforos
*
*/

void SIGINTHandler(int sig){
	// Si es el server manda un killPG(Kill process group osea a todos los hijos) que liberan la variable memoria(su memoria) 	
	server_caido=1;
	if(mipid==getpid()){
		int i;
		char mensj[TAMBUF];
		sem_unlink("/semJugadores");
		sem_unlink("/asignarGeneral");
		for(i=0;i<(indice-1);i++){
			if( jugadores->jugadores[i].estado != 3 ){
				bzero(mensj,TAMBUF);
				sprintf(mensj,"%d",10);
				strcat(mensj,"|");
				send(jugadores->jugadores[i].socket,mensj,TAMBUF,0);
			}
		}	
		
		shmctl(memJugadores,0,IPC_RMID);
		shmctl(memServi,0,IPC_RMID);
		
		killpg(getpgrp(),SIGTERM);
		
	}else{
		kill(getpid(),SIGKILL);
	}
	
	printf("HANDLER SIGINT - Finalizando recursos...\n");
	exit(1);
}

/**
* @Empaquetar / Desempaquetar
* empaquetar0 -> Se encarga de empaquetar los datos iniciales de la partida
*/
void empaquetar0(int jug,char *nombreLocal, char *nombreVisitante, int cant_vidas ,char *buffer){ // Genera un string con todo lo que le mande por parametro
	char aux[10]="";
	bzero(buffer,TAMBUF);
	sprintf(aux,"%d",0);
	strcat(buffer,aux);
	strcat(buffer,"|");
	sprintf(aux,"%d",jug);
	strcat(buffer,aux);
	strcat(buffer,"|");	
	strcat(buffer,nombreLocal);
	strcat(buffer,"|");
	strcat(buffer,nombreVisitante);
	strcat(buffer,"|");
	sprintf(aux,"%d",cant_vidas);
	strcat(buffer,aux);
	strcat(buffer,"|");
}

/**
* @Thread
* dibujaThread()
* - Thread que se encarga de dibujar la pantalla del Servidor Principal.
* - Dibuja la tabla de posiciones, el tiempo restante y muestra los mensajes sobre ocurrencias en el torneo.
*/

void * dibujaThread(void *arg){

	SDL_Surface *image;
	SDL_Rect dest;
	SDL_Color colorfuente2={255,255,255};
	SDL_Event event;
	
	int vectorPuntaje[100];
	char matrizNombres[100][TAMBUF];
	
	fgcolor.r=208; 
	fgcolor.g=52; 
	fgcolor.b=182;
	bgcolor.r=0; 
	bgcolor.g=0;
	bgcolor.b=0;
	int listo=0;
	int ciclo_dibujaThread=1;
	int i;

	//////////////////////INICIALIZACION SDL-TTF////////////////////////////		
	char msg_final[TAMBUF];
	strcpy(fuente_base,"dailypl2a.ttf");

	fuente = TTF_OpenFont(fuente_base,30);

	image = SDL_LoadBMP("final.bmp");
	if( image == NULL ){
		printf("ERROR - No pude cargar gráfico: %s\n", SDL_GetError());
		exit(1);
	}
		
	dest.x = 0;
	dest.y = 0;
	dest.w = 768;
	dest.h = 600;

	while(ciclo_dibujaThread>0){
		
		SDL_BlitSurface(image, NULL, screen, &dest);
		
		rectangulo_ttf.y=20;
		rectangulo_ttf.x=(768/2)-62;
		rectangulo_ttf.w=1;
		rectangulo_ttf.h=1;	
		
		if( flag_conexiones==0 )
			strcpy(msg_final,"Inscripcion CERRADA");
		else
			strcpy(msg_final,"Inscripcion Abierta");	
				
		ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente2);

		SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);

		strcpy(msg_final,"Tabla de posiciones");
		ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente2);
		rectangulo_ttf.y=80;
		rectangulo_ttf.x=(768/2)-350;
		rectangulo_ttf.w=1;
		rectangulo_ttf.h=1;
		SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);
		
		strcpy(msg_final,"Nombre                Puntaje");
		ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente2);
		rectangulo_ttf.y=120;
		rectangulo_ttf.x=(768/2)-300;
		rectangulo_ttf.w=1;
		rectangulo_ttf.h=1;
		SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);

		sem_wait(semJugadores);
			for(i=0;i<=(jugadores->cantJugadores-1);i++){
				strcpy(msg_final,jugadores->jugadores[i].nombre);
				ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente2);
				rectangulo_ttf.y=(rectangulo_ttf.y+40);
				rectangulo_ttf.x=(768/2)-300;
				rectangulo_ttf.w=1;
				rectangulo_ttf.h=1;
				SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);
			}
			
			rectangulo_ttf.y= 120;
			for(i=0;i<=(jugadores->cantJugadores-1);i++){	
				sprintf(msg_final,"%d",jugadores->jugadores[i].puntaje);
				fflush(stdout);
				ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente2);
				rectangulo_ttf.y=(rectangulo_ttf.y+40);
				rectangulo_ttf.x=(768/2)+140;
				rectangulo_ttf.w=1;
				rectangulo_ttf.h=1;
				SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);
			}
			
			fflush(stdout);
		sem_post(semJugadores);
		
		SDL_Flip(screen);
		SDL_FreeSurface(ttext_1);
		
		sleep(1);
		
		keys=SDL_GetKeyState(NULL);
		
		while(SDL_PollEvent(&event))
			if(event.type == SDL_KEYDOWN) 
				if(event.key.keysym.sym == SDLK_k){ 
					flag_conexiones=0;	
					// ACTIVO FLAG FIN CONEXIONES PARA QUE EL DIBUJA THREAD SE FIJE EN CADA PASADA SI YA TERMINARON TODAS LAS PARTIDAS
					puts("***********************************************************");
					puts("CIERRE DE INSCRIPCIONES");
					puts("***********************************************************");
					jugadores->flagFinConexiones=1;
			}
			
		///NUEVO
		if(jugadores->flagFinConexiones == 1) 
			if( jugadores->cantPartidasTotales == 0 )
				ciclo_dibujaThread=0;
	
	}  
	
	
	sem_wait(semJugadores);
		puts("FINALIZANDO TORNEO - Aviso a los jugadores");	
		for(i=0;i<indice;i++){
			char baf[TAMBUF];
			// Si el jugador ya termino, le envia un indicador de finalizacion
			
			if(jugadores->jugadores[i].estado!=3){ 

				sprintf(baf,"%d",8);
				strcat(baf,"|");
				send(cSocket.comm_socket[i],baf,TAMBUF,0);
			}
		}
	sem_post(semJugadores);
	
	sleep(1);
	
	ordenarTablaPuntaje(vectorPuntaje,matrizNombres);
	
	/////////////////////////////////////////////
	
	SDL_BlitSurface(image, NULL, screen, &dest);

	sprintf(msg_final,"%s","FIN DEL TORNEO");

	ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente2);
	rectangulo_ttf.y=20;
	rectangulo_ttf.x=(768/2)-62;
	rectangulo_ttf.w=1;
	rectangulo_ttf.h=1;		

	SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);

	strcpy(msg_final,"Tabla de posiciones");
	ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente2);
	rectangulo_ttf.y=80;
	rectangulo_ttf.x=(768/2)-350;
	rectangulo_ttf.w=1;
	rectangulo_ttf.h=1;
	SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);
			
	strcpy(msg_final,"Nombre                Puntaje");
	ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente2);
	rectangulo_ttf.y=120;
	rectangulo_ttf.x=(768/2)-300;
	rectangulo_ttf.w=1;
	rectangulo_ttf.h=1;
	SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);	
		
	
	for(i=0;i<=(jugadores->cantJugadores-1);i++){
		strcpy(msg_final,matrizNombres[i]);
		ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente2);
		rectangulo_ttf.y=(rectangulo_ttf.y+40);
		rectangulo_ttf.x=(768/2)-300;
		rectangulo_ttf.w=1;
		rectangulo_ttf.h=1;
		SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);
	}

	rectangulo_ttf.y= 120;

	for(i=0;i<=(jugadores->cantJugadores-1);i++){	
		sprintf(msg_final,"%d",vectorPuntaje[i]);
		fflush(stdout);
		ttext_1 = TTF_RenderText_Solid(fuente,msg_final,colorfuente2);
		rectangulo_ttf.y=(rectangulo_ttf.y+40);
		rectangulo_ttf.x=(768/2)+140;
		rectangulo_ttf.w=1;
		rectangulo_ttf.h=1;
		SDL_BlitSurface(ttext_1,NULL,screen,&rectangulo_ttf);
	}	
		
	SDL_Flip(screen);
	SDL_FreeSurface(ttext_1);
	
	/////////////////////////////////////////////
	
	// PIDO UNA TECLA
	while(SDL_WaitEvent(&event) && listo==0)
		if(event.type == SDL_KEYDOWN)
			listo=1;

	TTF_CloseFont(fuente);
	printf("FINALIZANDO TORNEO - FIN\n");
	kill(getpid(),SIGTERM);
}

//Funcion que busca la memoria compartida del pid dado por parámetro
int buscarMCdePid(int pid){

	int i,claveMC;
	for(i=0;i<cantServ;i++)
		if(vectorPid[i]==pid){
			claveMC=claves[i];
			return claveMC;
		}
	
	return 0;
}


/**
* 
* levantaProtector(int pidAMonitorear)
* Funcion DEMONIO, una vez que el pidAMonitorear deja de estar activo cierra los recursos y graba una serie de archivos. 
* 
*/
void levantaProtector(int pidAMonitorear){

	int sid = setsid();
	int a = 0, i;
	char comnd[100];

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	strcpy(comnd, "ls>~/asd");
	system(comnd);
	while((a = estaActivo(pidAMonitorear)) == 1)
		usleep(100000);
		
	//Cerrar bien todo
	FILE *pfS,*pfM,*pfA;
	
	pid_t pidAM;
	char comando[200], ultimaparte[100],comando2[200], matar[100];
	char usuario[100], linea[100], pidAMatar[10];
	puts("HAGO EL SYSTEM WHOAMI > USUARIO");
	strcpy(comando,"whoami > usuario");

	system(comando);
	pfS = fopen("usuario","r");
	fgets(usuario,sizeof(usuario), pfS);
	fclose(pfS);
	usuario[strlen(usuario)-1]='\0';
	strcpy(comando2,"ipcs | grep ");
	strcpy(ultimaparte, " | awk '{print $2}' > auxSm");
	
	strcat(comando2, usuario);
	strcat(comando2, ultimaparte);
	
	system(comando2);

	pfM = fopen("auxSm","r");
	fgets(linea,sizeof(linea),pfM);
	while(!feof(pfM)){
		int numero;
		numero = atoi(linea);
		shmctl(numero,0,IPC_RMID);
		fgets(linea,sizeof(linea),pfM);
	}
	
	fclose(pfM);
	
	strcpy(matar,"ps -a | grep server | awk '{print $1}'>aMatar");
	system(matar);
	
	pfA = fopen("aMatar","r");
	fgets(pidAMatar, sizeof(pidAMatar),pfA);
	while(!feof(pfM)){
		pidAM = atoi(pidAMatar);
		kill(pidAM,SIGKILL);
		fgets(pidAMatar, sizeof(pidAMatar),pfA);
	}
	fclose(pfA);
	
	for(i=0; i<(indice-1);i++){
		if(jugadores->jugadores[i].estado!=3){
			puts("cierro cSocket");
			close(cSocket.comm_socket[i]);
		}
	}
	system("rm /dev/shm/sem.*");
	puts("LLEGUE AL FINAL DE LEVANTAPROTECTOR");
	exit(0);
}

//Funcion que se encarga de corroborar que el pid dado por parámetro está activo
int estaActivo(int pid){
	FILE *actividad;
	int ret = 0;
	char orden[200], linea[255];
	sprintf(orden, "ps -al | awk '{print $4}' | grep %d > tmp", pid);
	system(orden);
	actividad = fopen("tmp","r");	

	fgets(linea, 255 ,actividad);
	
	if(!feof(actividad))
		ret = 1;

	pclose(actividad);
	return ret;
}

//Funcion que se encarga de leer el parametroBuscado del archivo dado por parámetro (aplicado principalmente para la configuración) devolviendo el mismo en "valor"
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
		printf("\nError al intentar abrir el archivo.\n");
		return -1;
	}
	return 0;
}

//Funcion que se encarga de leer la configuración del servidor y setear las variables requeridas
void configurarServidor(){
	char param[20], arch[20],puerto[20], vidas[20];
		
    strcpy(arch, "config");
    	
	strcpy(param, "PUERTO");
	leerParametro(puerto, param, arch);
	PORT = atoi(puerto);
	strcpy(param, "CANTIDAD VIDAS");
	leerParametro(vidas, param, arch);
	VIDAS = atoi(vidas);
	if(VIDAS > 10){
		puts("ERROR - ARCHIVO DE CONFIGURACION INCORRECTO: LAS VIDAS NO PUEDEN SER MÁS DE 10");
		exit(EXIT_FAILURE);
	}
	
}

//-------------FUNCIONES DE PUNTAJE-------------//
void initPuntajes(){
	int i;
	for(i=0; i<100; i++)
		jugadores->jugadores[i].puntaje=0;
}

int puntajeMinimo(){

	int minimo, i;
	
	minimo=jugadores->jugadores[0].puntaje;
	
	for(i=0; i<(indice-1); i++)
		if( jugadores->jugadores[i].puntaje > jugadores->jugadores[i+1].puntaje )
			minimo = jugadores->jugadores[i+1].puntaje;
			
	return minimo;
}

void ordenarTablaPuntaje(int vpuntos[100],char nombres[100][TAMBUF]){
	int i,j,aux;
	char nom[TAMBUF];
	
	for(i=0;i<100;i++){
		vpuntos[i]=jugadores->jugadores[i].puntaje;
		strcpy(nombres[i],jugadores->jugadores[i].nombre);
	}	
			
	for(i=0;i<(indice-1);i++)
		for(j=i+1;j<indice;j++)
			if(vpuntos[i] < vpuntos[j]){
				aux = vpuntos[i];
				strcpy(nom,nombres[i]);
				vpuntos[i] = vpuntos[j];
				strcpy(nombres[i],nombres[j]);
				vpuntos[j] = aux;
				strcpy(nombres[j],nom);
			}
		
	return ;
}
