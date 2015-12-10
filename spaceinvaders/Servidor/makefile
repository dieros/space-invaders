server: servidor.o
	gcc -o server servidor.c -lpthread -lSDL -lSDLmain -lSDL_ttf -lX11

servidor.o: servidor.c funciones.h
	gcc -c servidor.c -lpthread -lSDLmain -lSDL -lSDL_ttf -lX11

clean:	
	rm aMatar auxSm tmp usuario server *.o
