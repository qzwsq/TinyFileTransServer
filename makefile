default: Client.c EpollServer.c
	gcc Client.c -o Client.out
	gcc -pthread EpollServer.c -o Server.out
cli: Client.c
	gcc Client.c -o Client.out
server: EpollServer.c
	gcc EpollServer.c -o Server.out
clean: 
	rm *.out *.log
