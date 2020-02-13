#
# Copyright John La Velle 2020
#

all: client replicate coordinator

client: client.c client_ui.c common.c
	gcc -c client.c
	gcc -c client_ui.c
	gcc -c common.c
	gcc -o client client.o client_ui.o common.o

replicate: replicate.c common.c server_client.c
	gcc -c replicate.c
	gcc -c server_client.c
	gcc -c common.c
	gcc -o replicate replicate.o common.o server_client.o

coordinator: coordinator.c sequential_consistency.c common.c
	gcc -c coordinator.c
	gcc -c sequential_consistency.c
	gcc -c common.c
	gcc -o coordinator coordinator.o sequential_consistency.o common.o

clean:
	rm -rf *.o
	rm -rf coordinator client replicate
