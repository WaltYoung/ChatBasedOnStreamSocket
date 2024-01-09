#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <utility>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <semaphore.h>
#include <map>
#include <vector>
#include <mysql.h>
#include "nlohmann/json.hpp"
#include "myError.h"
#define PORT 20000
#define BACKLOG 1010
#define MSGLEN 256
#define BUFLEN 512
#define FILENAMELEN 110
#define NUMSERVER 10
#define IDLEN 8
#define IPADDRLEN 16
#define MAXDEPTH 15
#define SLEEP_TIME 1
#define USLEEP_TIME 100000
#define SUCCESS 0
typedef struct {
	int type;
	char sender[IDLEN];
	char recver[IDLEN];
	int code;
	char message[MSGLEN];
}Packet;
pthread_t thread_do[NUMSERVER];
pthread_mutex_t A_LOCK=PTHREAD_MUTEX_INITIALIZER;
int sockfd;
const char* cmdlist[]={"HELP","REGIS","LOGIN","LOGOUT","ADD","ACK","DEL","BROADCAST","GROUP","RENAMEGROUP","DELGROUP","INVITE","DELMEMBER","MSG","GROUPMSG","DATE",NULL};
void command(std::string cmdline,int* code);
char serverID[IDLEN]="1000000";
char userID[IDLEN];
void* sendThread(void* sockfd);
void* recvThread(void* sockfd);
void regisRequest(int sockfd);
void regisAck(int sockfd, Packet* packet);
void loginRequest(int sockfd);
void loginAck(Packet* packet);
void logoutRequest(int sockfd);
void logoutAck(Packet* packet);
void addRequest(int sockfd);
void addAck(int sockfd, Packet* packet);
void ackAck(Packet* packet);
const char* jsonToChar(const char* inputJson, const char* key, const char* value);
void charToJson(const char* inputChar, const char* key, std::string* resultValue);
void sendPacket(int sockfd, int type, char* sender, char* recver, char* message, int code=200);
void help();
void pass();
enum type
{
	Regis = 0x01,
	Login,
	Logout,
	Add,
	Ack,
	Del,
	Broadcast,
	Group,
	RenameGroup,
	DelGroup,
	Invite,
	DelMember,
	Msg,
	GroupMsg,
	Date
};


int main()
{
	if( (sockfd = socket(PF_INET,SOCK_STREAM,0) ) == -1)
	{
		fprintf(stderr,"Create socket error: %s\n",strerror(errno) );
		exit(1);
	}
	printf("Please input server IP:");
	char ip[16];
	scanf("%s",ip);
	getchar();
	struct sockaddr_in saddr;
	int addrlen;
	addrlen = sizeof(saddr);
	memset(&saddr,0,addrlen);
	saddr.sin_family=AF_INET;
	saddr.sin_port=htons(PORT);
	saddr.sin_addr.s_addr=inet_addr(ip);
	if( connect(sockfd,(struct sockaddr*)&saddr,addrlen) == -1)
	{
		fprintf(stderr,"Connect to server error: %s\n",strerror(errno) );
		close(sockfd);
		exit(1);
	}
	pthread_t thread_send;
	pthread_create(&thread_send,NULL,sendThread,(void*)&sockfd);
	pthread_t thread_recv;
	pthread_create(&thread_recv,NULL,recvThread,(void*)&sockfd);
	pthread_join(thread_send,NULL);
	pthread_join(thread_recv,NULL);
}

void command(std::string cmdline,int* code)
{
	char* p;
	p=const_cast<char *>(cmdline.c_str());
	for(int i=0;*p != ' ' && *p != '\0' && i < sizeof(cmdline);p++,i++)
		if(*p >= 'a' && *p <= 'z')
			cmdline[i]=*p-'a'+'A';
		else
			cmdline[i]=*p;
	for(int j=0;cmdlist[j];j++)
		if( !strcmp(cmdline.c_str(),cmdlist[j]) )
		{
			*code=j;
			break;
		}
}

void* sendThread(void* sockfd)
{
	int code=0;
	printf("Please input command:");
	std::string cmdline;
	while( std::cin >> cmdline )
	{
		if( !strcmp(cmdline.c_str(),"") )
			continue;
		if( !strcmp(cmdline.c_str(),"exit") )
		{
			close(*(int*)sockfd);
			break;
		}
		command(cmdline,&code);
		switch(code)
		{
		case 0:
			help();
			break;
		case 1:
			regisRequest(*(int*)sockfd);
//			sem_post(&sem);//发送信号量
			break;
		case 2:
			loginRequest(*(int*)sockfd);
			break;
		case 3:
			logoutRequest(*(int*)sockfd);
			break;
		case 4:
			addRequest(*(int*)sockfd);
			break;
		case 5:
			pass();
			break;
		default:
			break;
		}
		code=0;
	}
	pthread_exit(0);
}

void* recvThread(void* sockfd)
{
	Packet packet;
	while(1)
	{
//		sem_wait(&sem);//等待信号量
		int len;
//		pthread_mutex_lock(&A_LOCK);
		len=(int)recv( (*(int*)sockfd), (char*)&packet, sizeof(packet), 0);//接收不上锁
//		pthread_mutex_unlock(&A_LOCK);
		if(len <= 0)
		{
//			fprintf(stderr,"Receive error: %s\n",strerror(errno) );
			pass();
			continue; 
		}
		packet.type=ntohl(packet.type);
		packet.code=ntohl(packet.code);
		switch(packet.type)
		{
		case 1:
			regisAck(*(int*)sockfd,&packet);
			printf("Please input command:");
			break;
		case 2:
			loginAck(&packet);
			printf("Please input command:");
			break;
		case 3:
			logoutAck(&packet);
			printf("Please input command:");
			break;
		case 4:
			addAck(*(int*)sockfd, &packet);
			break;
		case 5:
			ackAck(&packet);
			break;
		default:
			break;
		}
		memset(&packet, 0, sizeof(packet));
	}
	pthread_exit(0);
}

void regisRequest(int sockfd)
{
	printf("Please input user name:");
	std::string username;
	std::cin >> username;
	printf("Please input password:");
	std::string password;
	std::cin >> password;
	const char* inputJson = R"(
		{}
	)";
	const char* newJson = jsonToChar(inputJson, "username", (const char*)username.c_str());//string到const char*的转换
	const char* message = jsonToChar(newJson, "password", (const char*)password.c_str());
	//访问静态成员用ClassName::MemberName，访问实例成员用ClassName.MemberName
//	std::cout << message << std::endl;
	char zero[]="0";
	sendPacket(sockfd, Regis, zero, serverID, (char*)message);
}

void regisAck(int sockfd, Packet* packet)
{
	std::cout << "Your user id is " << packet->message << " Please remember" << std::endl;
	strncpy(userID, packet->message, IDLEN);
}

void loginRequest(int sockfd)
{
	printf("Please input user id:");
	std::string userid;
	std::cin >> userid;
	printf("Please input password:");
	std::string password;
	std::cin >> password;
	const char* inputJson = R"(
		{}
	)";
	const char* newJson = jsonToChar(inputJson, "userid", (const char*)userid.c_str());//string到const char*的转换
	const char* message = jsonToChar(newJson, "password", (const char*)password.c_str());
	//访问静态成员用ClassName::MemberName，访问实例成员用ClassName.MemberName
	std::cout << message << std::endl;
	sendPacket(sockfd, Login, const_cast<char *>(userid.c_str()), serverID, (char*)message);//使用userid而不使用userID
}

void loginAck(Packet* packet)
{
	if(packet->code == USERIDNOTTRUE)
		std::cout << "The userid not exist" << std::endl;
	else if(packet->code == PWDNOTTRUE)
		std::cout << "The password not ture" << std::endl;
	else
	{
		strncpy(userID, packet->recver, IDLEN);
		std::cout << "Login success" << std::endl;
	}
}

void logoutRequest(int sockfd)
{
	char message[BUFLEN];
	memset(message,0,BUFLEN);//以0填充
	sendPacket(sockfd, Logout, userID, serverID, (char*)message);
}

void logoutAck(Packet* packet)
{
	if(packet->code == Success)
		std::cout << "Logout success" << std::endl;
	else
		std::cout << "Logout failed" << std::endl;
}

void addRequest(int sockfd)
{
	char otherID[IDLEN];
	printf("Please input user id:");
	scanf("%s",otherID);
	char message[BUFLEN];
	memset(message,0,BUFLEN);//以0填充
	sendPacket(sockfd, Add, userID, otherID, (char*)message);
}

void addAck(int sockfd, Packet* packet)
{
	printf("%s apply to add friends with you (y or n)", packet->message);
	char message[BUFLEN];
	memset(message,0,BUFLEN);//以0填充
	char answer;
	scanf("%c",&answer);
	if(answer == 'y')
		sendPacket(sockfd, Ack, packet->recver, packet->sender, (char*)message);
	else
		sendPacket(sockfd, Ack, packet->recver, packet->sender, (char*)message, USERIDNOTACCESS);
}

void ackAck(Packet* packet)
{
	if(packet->code == USERIDNOTFOUND)
		std::cout << "Userid not found" << std::endl;
	else if(packet->code == USERIDNOTACCESS)
		std::cout << "User not access" << std::endl;
	else
		std::cout << "User access" << std::endl;
}

const char* jsonToChar(const char* inputJson, const char* key, const char* value)
{
	nlohmann::json jsonData = nlohmann::json::parse(inputJson);
	jsonData[key] = value;
	return strdup(jsonData.dump().c_str());
}

void charToJson(const char* inputChar, const char* key, std::string* resultValue)
{
	nlohmann::json jsonData = nlohmann::json::parse(inputChar);
	if (jsonData.find(key) != jsonData.end())
	{
		*resultValue = jsonData[key];
	}
}

void sendPacket(int sockfd, int type, char* sender, char* recver, char* message, int code)
{
	Packet packet;
	packet.type=htonl(type);
	strcpy(packet.sender,sender);
	strcpy(packet.recver,recver);
	packet.code=htonl(code);
	strcpy(packet.message,message);
	pthread_mutex_lock(&A_LOCK);
	send(sockfd, (char*)&packet, sizeof(packet), 0);//发送上锁
	pthread_mutex_unlock(&A_LOCK);
}

void help()
{
	printf("Command:\n");
	printf("\tregis\n");
	printf("\tlogin\n");
	printf("\tlogout\n");
	printf("\tadd\n");
	printf("\tack\n");
	printf("\tdel\n");
	printf("\tbroadcast\n");
	printf("\tgroup\n");
	printf("\trenamegroup\n");
	printf("\tdelgroup\n");
	printf("\tinvite\n");
	printf("\tdelmember\n");
	printf("\tmsg\n");
	printf("\tgroupmsg\n");
	printf("\tdate\n");
}

void pass(){;}