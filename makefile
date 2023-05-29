client: pager.c node.c bptree.c bptreeIE.c bptreeS.c util.c cardT.c bankT.c client.c
	gcc -g -pg  -c pager.c
	gcc -g -pg  -c node.c
	gcc -g -pg  -c bptree.c
	gcc -g -pg  -c bptreeIE.c
	gcc -g -pg  -c bptreeS.c
	gcc -g -pg  -c util.c
	gcc -g -pg  -c cardT.c
	gcc -g -pg  -c bankT.c
	gcc -g -pg  client.c pager.o node.o bptree.o bptreeIE.o bptreeS.o util.o cardT.o bankT.o -o client

