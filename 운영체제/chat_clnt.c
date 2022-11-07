#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
	
#define BUF_SIZE 100
#define NAME_SIZE 20
	
void * send_msg(void * arg);	//send messasge to server
void * recv_msg(void * arg);	//receive message from server
void error_handling(char * msg);	//error handling
	
char msg[BUF_SIZE];		//output buffer
	
int main(int argc, char *argv[])
{
	int sock, port,c;
	char ip[20];
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread;
	void * thread_return;

	printf("Input Server IP Address : ");	//get ip address and port information
	fgets(ip, sizeof(ip), stdin);
	ip[strlen(ip)-1] = '\0';
	printf("Input Server Port Number : ");
	scanf("%d", &port);
	while ((c = getchar()) != '\n' && c != EOF) {}	//flush buffer

	sock=socket(PF_INET, SOCK_STREAM, 0);
	
	//initalize socket for server, AF_INET(IPV4 protocol address)
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr(ip); //htonl means change address format from local computer to network
	serv_addr.sin_port=htons(port); //hton means change address format from local computer to network
	  
	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) //connect to server
		error_handling("connect() error");
	printf("Success to connect to server!\n");
	printf("Server : welcome to chatting server!!\n");
	printf("input \'Q\' to exit\n");
	
	pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);	//create thread
	pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
	pthread_join(snd_thread, &thread_return);	//Wait until snd_thread is terminated, and release the resource when the pthread exits
	pthread_join(rcv_thread, &thread_return);	//Wait until rcv_thread is terminated, and release the resource when the pthread exits
	close(sock);  //close socket
	return 0;
}
	
void * send_msg(void * arg)   // send thread main
{
	int sock=*((int*)arg);	//get sockect descriptor from argument
	while(1) 
	{
		fgets(msg, BUF_SIZE, stdin);	//get message from keyboard
		write(sock, msg, strlen(msg));	//send message
		if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n"))	//if message is Q, terminate the program
		{
			close(sock);
			exit(0);
		}
	}
	return NULL;
}
	
void * recv_msg(void * arg)   // receive message
{
	int sock=*((int*)arg);	//get sockect descriptor from argument
	char got_msg[BUF_SIZE]; //receive buffer
	int str_len;
	while(1)
	{
		bzero(got_msg, BUF_SIZE);	//clear buffer
		str_len=read(sock, got_msg, BUF_SIZE);	//receive message
		if(str_len==-1)		//error condition
			return (void*)-1;
		fputs(got_msg, stdout);	//print message
	}
	return NULL;
}
	
void error_handling(char *msg)	//function for dealing with error
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
