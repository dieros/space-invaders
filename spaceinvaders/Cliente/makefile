cliente: cliente.o csprite.o 
	g++ -o cliente cliente.o csprite.o -lSDL -lSDLmain -lSDL_ttf -lpthread -lX11

cliente.o: cliente.cpp funciones.h csprite.h 
	g++ -c cliente.cpp -lSDLmain -lSDL -lSDL_ttf -lpthread	-lX11

csprite.o: csprite.cpp csprite.h
	g++ -c csprite.cpp -lSDLmain -lSDL -lSDL_ttf -lpthread -lX11

clean:
	rm cliente *.o

