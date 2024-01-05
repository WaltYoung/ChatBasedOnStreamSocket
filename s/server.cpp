#include <stdlib.h>
#include <stdio.h>
#include <iostream>
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
std::map<int,char*> sockfdToBuf;
std::map<std::string,int> useridToSockfd;
char serverID[IDLEN]="1000000";
void* server(void* sockfd);
void regisResponce(int sockfd, Packet* packet);
void loginResponce(int sockfd, Packet* packet);
void logoutResponce(int sockfd, Packet* packet);
void addForward(int sockfd, Packet* packet);
MYSQL* connectMySQL();
void insertData(MYSQL* con, std::string tableName, std::string columnName,const std::string data);

void updateData(MYSQL *con, std::string tableName, std::string columnName, const std::string newData, std::string target);
void queryData(MYSQL *con, std::string columnName, std::string tableName, std::vector< std::vector<std::string> >& results);
const char* jsonToChar(const char* inputJson, const char* key, const char* value);
void charToJson(const char* inputChar, const char* key, std::string* resultValue);
void sendPacket(int sockfd, int type, char* sender, char* recver, char* message, int code=200);
void* command(void* none);
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
	int lstsock;
	if( (lstsock = socket(PF_INET,SOCK_STREAM,0) ) == -1)
	{
		fprintf(stderr,"Create socket error: %s\n",strerror(errno) );
		exit(1);
	}
	socklen_t optlen;
	optlen=sizeof(optlen);
	setsockopt(lstsock, SOL_SOCKET, SO_REUSEADDR, &optlen, optlen);
	setsockopt(lstsock, SOL_SOCKET, SO_REUSEPORT, &optlen, optlen);

	struct sockaddr_in saddr;
	unsigned int addrlen;
	addrlen = sizeof(saddr);
	memset(&saddr,0,addrlen);
	saddr.sin_family=AF_INET;
	saddr.sin_port=htons(PORT);
	saddr.sin_addr.s_addr=htonl(INADDR_ANY);
	if( bind(lstsock,(struct sockaddr*)&saddr,addrlen) == -1)
	{
		fprintf(stderr,"Bind socket error: %s\n",strerror(errno) );
		exit(1);
	}
	listen(lstsock,BACKLOG);

	int sockfd;
	struct sockaddr_in caddr;
	for(int i=0;i<NUMSERVER;i++)
	{
		printf("Waiting for client connect......\n");
		sockfd= accept(lstsock,(struct sockaddr*)&caddr,&addrlen);
		if(sockfd == -1)
		{
			fprintf(stderr,"Accept client connection error: %s\n",strerror(errno) );
			exit(1);
		}
		printf("Got connection from: %s:%d\n",inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port) );
		pthread_create(&thread_do[i],NULL,server,(void*)&sockfd);
		char buf[BUFLEN];
		sockfdToBuf[sockfd]=buf;//以数组方式插入键值对
	}
	pthread_t thread;
	pthread_create(&thread,NULL,command,NULL);
	pthread_join(thread,NULL);
	
	return 0;
}

void* server(void* sockfd)
{
	pthread_detach(pthread_self());//线程被置为detach状态，一旦终止该线程就立刻回收它占用的所有资源，而不保留终止状态。不能对一个已经处于detach状态的线程调用pthread_join
//	printf("tid 子进程所属线程 ID 为 %d\n",gettid());
//	printf("pid 子进程所属线程 ID 为 %ld\n",pthread_self());
//	printf("%d\n",*(int*)sockfd);
//	std::map<int,char*>::iterator it=maps.find(*(int*)sockfd);
//	char* p_buf;//p_buf指向buf
//	p_buf=it->second;//first返回键，second返回值
//	struct timeval timeout;
//	timeout.tv_sec = SLEEP_TIME;
//	timeout.tv_usec = 0;//取值范围为[0,1000000)
//	socklen_t optlen = sizeof(timeout);//socklen_t = unsigned int
//	setsockopt(*(int*)sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, optlen);
//	setsockopt(*(int*)sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, optlen);
	Packet packet;
	while(1)
	{
		int len;
		len=(int)recv( (*(int*)sockfd), (char*)&packet, sizeof(packet), 0);
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
			regisResponce(*(int*)sockfd, &packet);
			break;
		case 2:
			loginResponce(*(int*)sockfd, &packet);
			break;
		case 3:
			logoutResponce(*(int*)sockfd, &packet);
			break;
		case 4:
			addForward(*(int*)sockfd, &packet);
			break;
		default:
			break;
		}
		memset(&packet, 0, sizeof(packet));
	}
	pthread_exit(0);
}

void regisResponce(int sockfd, Packet* packet)
{
	std::string columnName="MAX(userid)";
	std::string tableName="basic";
	std::vector< std::vector<std::string> > results;
	queryData( connectMySQL(), columnName, tableName, results);
//	for (int i = 0; i < results.size(); i++) {
//		for (int j = 0; j < results[i].size(); j++) {
//			std::cout << results[i][j] << " ";
//		}
//		std::cout << std::endl;
//	}
	int row=results.size()-1;
	int line=results[row].size()-1;
//	std::cout << results[row][line].c_str() << std::endl;
	int max_userid;
	max_userid=atoi(results[row][line].c_str());//string转int，it是一个迭代器，不是一个对象，不能直接使用it.data。只能使用it->data
	max_userid++;
	char zero[]="0";
	std::string userid = std::to_string(max_userid);
//	std::cout << userid << std::endl;
	sendPacket(sockfd, Regis, serverID, zero, const_cast<char *>(userid.c_str()));//string类型转char*类型
	std::string columnName_2="userid, username, password";
	std::string username="default",password="default";
	charToJson( (const char*)packet->message, "username", &username);
	charToJson( (const char*)packet->message, "password", &password);
	userid="'" + userid + "'";
	username="'" + username + "'";
	password="'" + password + "'";
	std::string data=userid + "," + username + "," + password ;
	std::cout << data << std::endl;
//	pause();
	insertData( connectMySQL(), tableName, columnName_2, data);
}

void loginResponce(int sockfd, Packet* packet)
{
	std::string userid="default",password="default";
	charToJson( (const char*)packet->message, "userid", &userid);
	charToJson( (const char*)packet->message, "password", &password);
	useridToSockfd[userid]=sockfd;//套接字与userid绑定
	std::string columnName="userid, password";
	std::string tableName="basic";
	std::vector< std::vector<std::string> > results;
	queryData( connectMySQL(), columnName, tableName, results);
	int row=results.size();//多行
	int line=results[row-1].size();//两列//必须减一否则将导致数组越界
//	printf("row = %d line = %d\n", row, line);
	char message[BUFLEN];//C语言不允许定义长度为0的字符数组
	memset(message,0,BUFLEN);//故以0填充
	for (int i = 0; i < row; i++)
	{
		for (int j = 0; j < line - 1; j++)//仅遍历第一列username
		{
			if( !strcmp(results[i][j].c_str(), userid.c_str()) )//ID存在
				if( !strcmp(results[i][j+1].c_str(), password.c_str()) )//密码正确
				{
					sendPacket(sockfd, Login, serverID, packet->sender, message);
					break;
				}
				else
					return sendPacket(sockfd, Login, serverID, packet->sender, message, PWDNOTTRUE);
		}
		if( i == row - 1 && ( !strcmp(results[i][line - 1].c_str(), userid.c_str()) ) )
			return sendPacket(sockfd, Login, serverID, packet->sender, message, USERIDNOTTRUE);
	}
	std::string columnName_2="online";
	std::string newData="1";//bool=true
	updateData( connectMySQL(), tableName, columnName_2, newData, userid);
	pass();
}

void logoutResponce(int sockfd, Packet* packet)
{
	std::string columnName="online";
	std::string tableName="basic";
	std::string newData="0";//bool=false
	updateData( connectMySQL(), tableName, columnName, newData, packet->sender);
}

void addForward(int sockfd, Packet* packet)
{}

// 连接MySQL数据库
MYSQL* connectMySQL()
{
	MYSQL* con = mysql_init(NULL);
	if (con == NULL)
	{
		std::cerr << "Init error: " << mysql_error(con) << std::endl;
		return nullptr;//任何情况下都表示空指针
	}
	if (mysql_real_connect(con, "127.0.0.1", "root", "root", "chat", 3306, NULL, 0) == NULL)//用户名为root，密码为root
	{
		std::cerr << "Connect error: " << mysql_error(con) << std::endl;
		return nullptr;
	}
	// 使用USE语句切换到特定的数据库
	if (mysql_query(con, "USE chat"))
	{
		std::cerr << "Use error: " << mysql_error(con) << std::endl;
		mysql_close(con);
		return nullptr;
	}
	return con;
}

// 插入数据
void insertData(MYSQL* con, std::string tableName, std::string columnName,const std::string data)
{
	std::string query = "INSERT INTO " + tableName + " (" + columnName +") VALUES (" + data + ")" + ";";
//	std::cout << query << std::endl;
	if (mysql_query(con, query.c_str())) 
		std::cerr << "Insert Error: " << mysql_error(con) << std::endl;
}

// 删除数据
void deleteData(MYSQL *con, std::string tableName, std::string target)
{
	std::string query = "DELETE FROM " + tableName + " WHERE userid = " + target + ";";
	if (mysql_query(con, query.c_str()))
		std::cerr << "Delete error: " << mysql_error(con) << std::endl;
}

// 更新数据
void updateData(MYSQL *con, std::string tableName, std::string columnName, const std::string newData, std::string target)
{
	std::string query = "UPDATE " + tableName + " SET " + columnName  +  " = " + newData + " WHERE userid = " + target + ";";
	std::cout << query << std::endl;
	if (mysql_query(con, query.c_str()))
		std::cerr << "Update error: " << mysql_error(con) << std::endl;
}

// 查询数据
void queryData(MYSQL *con, std::string columnName, std::string tableName, std::vector< std::vector<std::string> >& results)
{
	std::cout << ("SELECT " + columnName + " FROM " + tableName + ";").c_str() << std::endl;
	if (mysql_query(con, ("SELECT " + columnName + " FROM " + tableName).c_str() ))
		std::cerr << "Query error: " << mysql_error(con) << std::endl;
	else
	{
		MYSQL_RES *result = mysql_store_result(con);
		if (result != NULL)
		{
			int num_rows = mysql_num_rows(result);//获取结果集中的行数
			int num_fields = mysql_num_fields(result);//获取结果集中的列数
			MYSQL_ROW row = NULL;
			std::string temp;
			while ((row = mysql_fetch_row(result)) != NULL)
			{
				std::vector<std::string> linedata;
				for (int i = 0; i < num_fields; i++)//获取行中每一列的内容
				{
					if (row[i])
					{
						temp = row[i];
						linedata.push_back(temp);
					}
					else
					{
						temp = "NULL";
						linedata.push_back(temp);
					}
				}
				results.push_back(linedata);//C++ 11新增加的emplace_back() 的执行效率比 push_back() 高
			}
			mysql_free_result(result);
		}
		else
			std::cerr << "Query NULL result: " << mysql_error(con) << std::endl;
	}
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
	send(sockfd, (char*)&packet, sizeof(packet), 0);
}

void* command(void* none)
{
	char cmd[10];
	while( strcmp(cmd,"exit") )
		scanf("%s",cmd);
	for(int i=0;i<NUMSERVER;i++)
		pthread_cancel(thread_do[i]);
	pthread_exit(0);
}

void pass(){;}