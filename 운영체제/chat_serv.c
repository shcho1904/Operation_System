#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUF_SIZE 200
#define MAX_CLNT 512

typedef struct __client {	//client's information including socket's descriptor, index, flag for voting system
	int clnt_sock;
	int index;
	int vote_flag;
} client_t;

typedef struct __vote {	//for vote system
	int yes;
	int no;
} Vote;

void * handle_clnt(void * arg);	//instruction for each thread
void send_msg(char * msg, int len, int index);	//send message to all clients except who has index as  "index"
void error_handling(char * msg);	//error handling
void queue_add(client_t* cl);	//when client is connected, add that client to queue
void set_vote_flag(int setting);	//set vote condition

Vote vote_cnt;		//count the pros and cons
int clnt_cnt = 0;	//count connected client
int total_cnt = 0;	//count total client including disconnected client
client_t *clients[MAX_CLNT];	//array for client
pthread_mutex_t mutx;	//mutex(similar to semaphore) for client communication
pthread_mutex_t votex;	//mutex for voting system

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	int port, i;
	int clnt_index;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	client_t* cli;
	pthread_t t_id;

	printf("Server Port : ");
	scanf("%d", &port);
  
	pthread_mutex_init(&mutx, NULL);
	pthread_mutex_init(&votex, NULL);		//initialize mutex for atomic operation
	serv_sock=socket(PF_INET, SOCK_STREAM, 0);	//assign server socket, PF_INET = IPv4 internet protocol
	//SOCK_STREAM = TCP/IP, in this project, message format is quite simple, 
	//we don't have to use DGRAM for advancing speed
	for (int i = 0; i < MAX_CLNT; i++)	//initialize clients' array
		clients[i] = NULL;

	//initalize socket for server, AF_INET(IPV4 protocol address)
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET; 
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY); //htonl means change address format from local computer to network
	//e.g) big endian to little endian, INADDR_ANY means connect to any address
	serv_adr.sin_port=htons(port); //similar to above description
	
	//bind serv_adr to socket, error can be occured if there was uncomplete disconnection right before this program using same port,
	//don't worry about that error, waiting for 1~2minutes, you can reuse that port again
	if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr))==-1)
		error_handling("bind() error");
	//change socket condition available for connection
	if(listen(serv_sock, 5)==-1)
		error_handling("listen() error");
	
	printf("open!! server\n");
	printf("Chatting on\n");	

	while(1)
	{
		clnt_adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr,&clnt_adr_sz); //connect to client

		pthread_mutex_lock(&mutx);	//clnt_cnt and total_cnt is static variable, so make this area to critical section
		//to prevent distortion. Futhermore, index is related to total_cnt, so we have to protect this section from other thread
		cli = (client_t*)malloc(sizeof(client_t));
		cli->clnt_sock = clnt_sock;
		clnt_cnt++;
		total_cnt++;
		cli->index = total_cnt;

		for (i = 0; i < MAX_CLNT; i++) {	//if there are empty space, assign client to the space
			if (!clients[i]) {
				clients[i] = cli;
				break;
			}
		}
		pthread_mutex_unlock(&mutx);

		pthread_create(&t_id, NULL, handle_clnt, (void*)cli);	//make client thread
		pthread_detach(t_id);	//detach thread from main thread
		printf("connected to Client %d\n", cli->index);
	}
	close(serv_sock);	//close socket
	return 0;
}
	
void * handle_clnt(void * arg)
{
	int str_len = 0, i;
	int vote_len = 0;
	char msg[BUF_SIZE], initial[BUF_SIZE], sending_msg[BUF_SIZE], vote_msg[BUF_SIZE];
	char vote_notice_msg[BUF_SIZE];
	client_t* cli = (client_t*)arg;
	
	sprintf(initial, "Client %d has been connected!\n", cli->index);	//if client is connected, new socket is created
	send_msg(initial, strlen(initial), cli->index);	//send connection message to other clients

	while((str_len=read(cli->clnt_sock, msg, sizeof(msg)))!=0)
	{
		msg[str_len] = 0;
		if(!strcmp(msg, "q\n") || !strcmp(msg, "Q\n"))	//if client insert q, remove the thread
			break;
		if (!strcmp(msg, "vote\n") || !strcmp(msg, "Vote\n")) {	//if vote is transmitted, turn to vote mode
			pthread_mutex_lock(&votex);	//set critical section to avoid distrupt vote_cnt
			vote_cnt.yes = 0;	//initalize vote condition to 0
			vote_cnt.no = 0;
			set_vote_flag(1);	//ready for vote
			sprintf(vote_notice_msg, "Insert yes or no to vote\n");
			send_msg(vote_notice_msg, strlen(vote_notice_msg), -1);	//send vote message to all clients
			pthread_mutex_unlock(&votex);
		}
		printf("message from client %d : %s", cli->index, msg);
		if ((!strcmp(msg, "yes\n") || !strcmp(msg,"no\n")) && cli->vote_flag) {
			if (!strcmp(msg, "yes\n")) {
				vote_cnt.yes++;
				cli->vote_flag = 0;
			}
			if (!strcmp(msg, "no\n")) {
				vote_cnt.no++;
				cli->vote_flag = 0;
			}
			sprintf(msg, "anonymous voting is on progress\n");
		}
		
		sprintf(sending_msg, "Client %d : %s", cli->index, msg);
		send_msg(sending_msg, strlen(sending_msg), cli->index);	//send message to all clients except for a client having "index"

		if ((vote_cnt.yes + vote_cnt.no) == clnt_cnt) {	//if all clients finished vote, initialize vote condition and send result to all clients
			sprintf(vote_notice_msg, "Yes: %d, No: %d\n", vote_cnt.yes, vote_cnt.no);
			send_msg(vote_notice_msg, strlen(vote_notice_msg), -1);
			pthread_mutex_lock(&votex);
			vote_cnt.yes = 0;
			vote_cnt.no = 0;
			set_vote_flag(0);
			pthread_mutex_unlock(&votex);
		}
	}
	pthread_mutex_lock(&mutx);	//remove disconnected client's socket, clients is global array, so set this region to critical section
	for (i = 0; i < MAX_CLNT; i++) {
		if (clients[i]) {
			if (clients[i]->index == cli->index) {
				clients[i] = NULL;
				break;
			}
		}
	}
	clnt_cnt--;
	pthread_mutex_unlock(&mutx);
	close(cli->clnt_sock);
	return NULL;
}
void send_msg(char * msg, int len, int index)	//send clients a msg which has "len" length except for a "index" client
{
	int i;
	pthread_mutex_lock(&mutx);
	for(i=0; i<MAX_CLNT; i++){ 
		if (clients[i]) {
			if (clients[i]->index != index) {
				write(clients[i]->clnt_sock, msg, len);
			}
		}
	}
	pthread_mutex_unlock(&mutx);
}

void queue_add(client_t* cl) {	//add new client to array
	int i;
	pthread_mutex_lock(&mutx);
	clnt_cnt++;
	total_cnt++;
	cl->index = total_cnt;

	for (i = 0; i < MAX_CLNT; i++) {	//if array has an empty space, insert client
		if (!clients[i]) {
			clients[i] = cl;
			break;
		}
	}
	pthread_mutex_unlock(&mutx);
}

void set_vote_flag(int set) {	//set each clients' vote flag
	int i;
	for (i = 0; i < MAX_CLNT; i++) {
		if (clients[i]) {
			clients[i]->vote_flag = set;
		}
	}
	return;
}
void error_handling(char * msg)	//function for dealing with error
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
