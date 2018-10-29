#NAME:Jose Flores Martinez
#EMAIL:joseflores2395@gmail.com
#ID:404474130



lab1b: lab1b-client.c lab1b-server.c
	gcc -o lab1b-client lab1b-client.c -Wall -Wextra -lz
	gcc -o lab1b-server lab1b-server.c -Wall -Wextra -lz

lab1b-client: lab1b-client.c
	gcc -o lab1b-client lab1b-client.c -Wall -Wextra -lz

lab1b-server: lab1b-server.c
	gcc -o lab1b-server lab1b-server.c -Wall -Wextra -lz

clean:
	rm -rf lab1b-server lab1b-client lab1b-404474130.tar.gz

dist:
	tar -zcvf lab1b-404474130.tar.gz README Makefile lab1b-client.c lab1b-server.c
