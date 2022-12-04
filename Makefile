all: ./Client/client2.o ./Serveur/server2.o
	gcc -o ./Client/client2 ./Client/client2.o
	gcc -o ./Serveur/server2 ./Serveur/server2.o

./Client/client2.o: ./Client/client2.c
	gcc -c ./Client/client2.c -o ./Client/client2.o

./Serveur/server2.o: ./Serveur/server2.c
	gcc -c ./Serveur/server2.c -o ./Serveur/server2.o

clean:
	rm -f ./Client/client2 ./Serveur/server2