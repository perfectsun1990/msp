
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>   
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h> 
#include <netinet/udp.h> 
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>

#define HIG                         "\e[1m"          //高亮
#define UND                         "\e[4m"          //下划
#define BLK                         "\e[5m"          //闪烁
#define INV                         "\e[7m"          //反显
#define FAD                         "\e[8m"          //消隐
#define NON                         "\e[0m"          //默认
#define RED                         "\e[0;32;31m"    
#define YEL                         "\e[0;33m"      
#define GRE                         "\e[0;32;32m"  
#define BLU                         "\e[0;32;34m"  
#define arr_size(x) (sizeof(x)/sizeof(x[0]))
#ifndef WIN32
#define closesocket(x)  close(x)
#endif

#include "mempool.hpp"

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
}
#endif

#define ACCEPT4

////////////////////////////////////////////////////////////////////////////////////////////
//SOCKET

void 
DeftSigFunc( int32_t signo )
{
	printf("Hi! I capt sig= %d\n", signo );
	exit(0);
}

/**
 *@brief 注册信号处理动作
 */
int32_t 
SetSigalAct( int32_t flag  )
{
	switch( flag ){
		case 0:  break;
		case 1:
			signal(SIGABRT, DeftSigFunc);//abort()触发
			signal(SIGALRM, DeftSigFunc);//alarm()触发
			signal(SIGQUIT, DeftSigFunc);//ctrl+ \触发
			signal(SIGINT,  DeftSigFunc);//ctrl+ c触发.
			signal(SIGCHLD, SIG_IGN);	 //将僵尸进程交给init进程来领养，回收子进程退出状态和其他信息。
			signal(SIGPIPE, SIG_IGN);	 //防止程序收到SIGPIPE后自动退出
			break;
		default: break;
	}
	return 0;
}

/**
 *@brief 设置非阻塞fd.
 */
bool 
SetNonBlock(int32_t fd)
{
    int32_t opts = -1;
    opts = fcntl(fd, F_GETFL);
	if (-1  != opts){
		opts = opts | O_NONBLOCK;
		opts = fcntl(fd, F_SETFL, opts);
	}
	return (-1 != opts);
}

/**
 *@brief 创建TCP服务器
 */
int32_t 
CreatTcpSrv(int32_t port)
{
	int32_t sockfd = -1;
	// 创建socket套接字
	if ( (sockfd = socket( AF_INET, SOCK_STREAM, 0 )) < 0 ){
		perror("socket");
		return -1;
	}
	// 设置socket的参数
	struct sockaddr_in m_srvrAddr;
    m_srvrAddr.sin_family       = AF_INET;
    m_srvrAddr.sin_port         = htons(port);
    m_srvrAddr.sin_addr.s_addr  = htonl(INADDR_ANY);//inet_addr("192.168.3.222");	
    // 绑定具体端口和ip
    if (0 != bind(sockfd, (struct sockaddr*)&m_srvrAddr, sizeof(struct sockaddr))){
		perror("bind");
		return -1;
	}
    // 开始监听当前端口
	// 128:完成握手正等待accept的socket最大队列长度,若syncookies使能,该项无效!
    listen(sockfd, 128);
	return sockfd;
}

/**
 *@brief 创建UDP服务器
 */
int32_t 
CreatUdpSrv(int32_t port)
{
	int32_t sockfd=-1;
	// 创建socket套接字
	if ( (sockfd = socket( AF_INET, SOCK_DGRAM, 0 )) < 0 ){
		perror("socket");
		return -1;
	}
	// 设置socket的参数
	struct sockaddr_in m_srvrAddr;
    m_srvrAddr.sin_family       = AF_INET;
    m_srvrAddr.sin_port         = htons(port);
    m_srvrAddr.sin_addr.s_addr  = htonl(INADDR_ANY);//inet_addr("192.168.3.222");	
    // 绑定具体端口和ip
    if (0 != bind(sockfd, (struct sockaddr*)&m_srvrAddr, sizeof(struct sockaddr))){
		perror("bind");
		return -1;
	}//udp 不需要listen. 直接：sendto recvfrom.
	return sockfd;
}

/**
 *@brief 设置socket选项.
 */
int32_t 
SetSockOpts(int32_t sockfd, bool is_tcp=true)
{
	/* 设置SOC通用属性 */
	int32_t reuseAddr = 1;	
	int32_t keepalive = 1;
	int32_t nRecvBuff = 32*1024;//设置为32K
	int32_t nSendBuff = 32*1024;//设置为32K, 高性能=0.
	int32_t broadcast = 1;
	int32_t tcpNDelay = 0;//default: 0.
	struct  timeval opTimeout={3,0};//3s
	struct  linger ling = {1,0};	
	//ling.l_onoff  = 1; l_onoff = 0,l_linger忽略,优雅退出; l_onoff = 1,自定义退出
	//ling.l_linger = 0; l_onoff=0时，发REST包强制退出，否则，超时退出。

	// 立即重用(多个socket可用bind到同一个端口和IP, 唯一标识：(src[ip,port], dst[ip,port])
	if (0 != setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(char*)&reuseAddr, sizeof(reuseAddr)))
		goto ErrorHandle;	
	// 心跳检测
	if (0 != setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,(char*)&keepalive, sizeof(reuseAddr)))
		goto ErrorHandle;
	// 发送时限
	if (0 != setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char*)&opTimeout, sizeof(opTimeout)))
		goto ErrorHandle;
	// 接收时限
	if (0 != setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&opTimeout, sizeof(opTimeout)))
		goto ErrorHandle;
	// 接收缓冲
	if (0 != setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,   (char*)&nRecvBuff, sizeof(nRecvBuff)))
		goto ErrorHandle;
	// 发送缓冲
	if (0 != setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF,   (char*)&nRecvBuff, sizeof(nRecvBuff)))
		goto ErrorHandle;
	
	/* 设置UDP通用属性 */
	if (0 ==  is_tcp)
	{//携带广播特性
		if ( 0 != setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast)) )
			goto ErrorHandle;
	}
	
	/* 设置TCP通用属性 */
	if (1 ==  is_tcp)
	{	//立即关闭退出,或者经历超时退出。
		if ( 0 != setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling)) )
			goto ErrorHandle;
		//服务端监听的socket是否接受新到来的connect请求。
		//int32_t conAccept = 1;
		//if ( 0 != setsockopt(sockfd, SOL_SOCKET, SO_CONDITIONAL_ACCEPT, (char*)&conAccept, sizeof(int32_t)) )
		//	goto ErrorHandle;
		//禁止使用Nagle算法，可加速小包发送但会降低性能。
		if ( 0 != setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&tcpNDelay, sizeof(int32_t)) )
			goto ErrorHandle;
	}
	return  0;
ErrorHandle:
	perror("setsockopt");
	return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////
//EPOLL
struct  UserData;
typedef void(*HandleT)(void*);
typedef struct UserData
{
	int32_t 	epllfd;
	int32_t 	connfd;
	void* 		params;
	HandleT 	handle;
	void* 		reserv;
}UserData;


int32_t
EpollCreated(int32_t flag)
{//EPOLL_CLOEXEC
	return epoll_create1(flag);
}

int32_t
EpollDestroy(int32_t epfd)
{
	return close(epfd);
}

void
EpollDispath(int32_t epfd, int32_t sockfd, int32_t timeout)
{
#define MAX_EVENT_NUMS					128
	static struct epoll_event event_list[MAX_EVENT_NUMS] = {0};//改进用链表。
	for(;;)
	{
		memset(event_list, 0, sizeof(event_list));
		int32_t nevents = epoll_wait( epfd, event_list, sizeof(event_list), timeout);
		if (nevents <= 0)
			perror("epoll_wait");
		printf(BLU "\nepool nEvent= %d\n" NON, nevents);
		
		for ( int32_t i = 0; i < nevents; i++ ) 
		{
			uint32_t  events = event_list[i].events;
			UserData* userdb = (UserData*)event_list[i].data.ptr;
			
			// Error event
			if (events & (EPOLLERR|EPOLLHUP)) {
				printf("epoll_wait() error on fd:%d ev:%04XD",
							  event_list[i].data.fd, events);
			}
			if ((events & (EPOLLERR|EPOLLHUP))
				 && (events & (EPOLLIN|EPOLLOUT)) == 0) {
				/*
				 * if the error events were returned without EPOLLIN or EPOLLOUT,
				 * then add these flags to handle the events at least in one
				 * active handler
				 */
				events |= EPOLLIN|EPOLLOUT;
			}
			// links event 
			if (userdb->connfd==sockfd){
				userdb->handle(userdb->params);//这个地方可用使用异步任务来做。post().
			}
			// Reads event
			else if (events & EPOLLIN) {
				userdb->handle(userdb->params);
			}
			// Write event
			else if (events & EPOLLOUT) {
				userdb->handle(userdb->params);
			}
		}
	}
}

bool
EpollAdd(int32_t epfd, int32_t fd, uint32_t opts, UserData *hdwl)
{
	struct epoll_event ev;
	ev.events  	= opts;// EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP;
	ev.data.ptr = (void*)hdwl;
	if ( epoll_ctl( epfd, EPOLL_CTL_ADD, fd, &ev ) < 0 ){
		perror("epoll_ctl");
		return false;
	}
	return true;
}

bool
EpollMod(int32_t epfd, int32_t fd, uint32_t opts, UserData *hdwl)
{
	struct epoll_event ev;
	ev.events   = opts;// EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP;
	ev.data.ptr = (void*)hdwl;
	if ( epoll_ctl( epfd, EPOLL_CTL_MOD, fd, &ev ) < 0 ){
		perror("epoll_ctl");
		return false;
	}
	return true;
}

bool
EpollDel(int32_t epfd, int32_t fd)
{
	struct epoll_event ev;
	ev.data.ptr = NULL;
	ev.events  = 0;// EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP;
	if ( epoll_ctl( epfd, EPOLL_CTL_DEL, fd, &ev ) < 0 ){
		perror("epoll_ctl");
		return false;
	}
	return true;
}

void 
RecvEvHandle(void* params);
void 
SendEvHandle(void* params)
{
	UserData* userdata =(UserData*)params;
	char buff[] = "HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nHello World";
	//RecvBuffer(userdata->connfd, buff, sizeof(buff));
	int32_t ssize = send(userdata->connfd, buff, sizeof(buff), MSG_DONTWAIT);
	if (ssize < 0) {
		if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK) {
			EpollMod(userdata->epllfd, userdata->connfd, EPOLLOUT|EPOLLET|EPOLLRDHUP, userdata);
			return;
		}
		perror("send:");
		EpollDel(userdata->epllfd, userdata->connfd);
		close(userdata->connfd);
		userdata->connfd = -1;
		free((void*)userdata);
	} else {
		//如果自己MOD自己一次，那么下次还会被触发的哦，类似于LT模式,但若不MOD自己，那么ET模式下
		//仅通知一次，然后无论缓存是否读完都不再通知，这时要求应用必须读完网卡缓存。
		printf("[%d] send: %s\n",userdata->connfd, buff);
		userdata->handle = RecvEvHandle;
		EpollMod(userdata->epllfd, userdata->connfd, EPOLLIN|EPOLLET|EPOLLRDHUP, userdata);
	}
}

void 
RecvEvHandle(void* params)
{
	UserData* userdata =(UserData*)params;
	char buff[32*1024] = {0};
	int32_t rsize = recv(userdata->connfd, buff, sizeof(buff), MSG_DONTWAIT);
	if (rsize <=0) {
		if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK) {
			EpollMod(userdata->epllfd, userdata->connfd, EPOLLIN|EPOLLET|EPOLLRDHUP, userdata);
			return;
		}
		if (0 != rsize && errno != ECONNRESET)
			perror("recv:");
		printf("[%d] is closed by remote peer! rsize=%d!\n",userdata->connfd, rsize);			
		EpollDel(userdata->epllfd, userdata->connfd);
		close(userdata->connfd);
		userdata->connfd = -1;			
		free((void*)userdata);
	} else {
		//如果自己MOD自己一次，那么下次还会被触发的哦，类似于LT模式,但若不MOD自己，那么ET模式下
		//仅通知一次，然后无论缓存是否读完都不再通知，这时要求应用必须读完网卡缓存。
		printf("[%d] recv: %s\n",userdata->connfd, buff);
		userdata->handle = SendEvHandle;	//ud->handle = RecvEvHandle;
		EpollMod(userdata->epllfd, userdata->connfd, EPOLLOUT|EPOLLET|EPOLLRDHUP, userdata);
	}
}

void 
AcceptHandle(void* params)
{
	UserData* userdata =(UserData*)params;
	struct sockaddr_in 	c_addr;
	socklen_t 			c_size = sizeof(struct sockaddr);
#ifdef ACCEPT4
	int32_t connfd = accept4(userdata->connfd, (struct sockaddr*)&c_addr, &c_size, SOCK_NONBLOCK);
#else
	int32_t connfd = accept(userdata->connfd, (struct sockaddr*)&c_addr, &c_size);
#endif
	if (connfd < 0) {
		if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK) {
			EpollMod(userdata->epllfd, userdata->connfd, EPOLLIN|EPOLLET|EPOLLRDHUP, userdata);
			return;
		}
		perror("accept");
		return;
	}
	if ( !SetNonBlock(connfd)   < 0 )
		return;
	printf("--->>> connfd=%d connect from %s:%d\n",
		connfd, inet_ntoa(c_addr.sin_addr), ntohs(c_addr.sin_port));
	
	// UserData For New Conncetion!
	UserData* ud = (UserData*)calloc(1, sizeof(UserData));	
	ud->epllfd = userdata->epllfd;
	ud->connfd = connfd;
	ud->handle = RecvEvHandle;
	ud->params = ud;
	EpollAdd(userdata->epllfd, connfd, EPOLLIN|EPOLLET|EPOLLRDHUP, ud);
}


void
test_epoll(int32_t port )
{
	int32_t sockfd = -1, connfd = -1;
	int32_t epllfd = -1,   nfds = -1;
	
	/* 1.信号服务 */
	if ( (SetSigalAct(1)) < 0 )
		return;
	
	/* 2.网络服务 */
	if ( (sockfd = CreatTcpSrv( port )) < 0 )
		return;
	if ( !SetNonBlock(sockfd)   < 0 )
		return;
	if ( SetSockOpts(sockfd, 1) < 0 )
		return;
	
	/* 3.事件监控 */
	// 创建epoll句柄(size-已弃用>0即可)
    epllfd = EpollCreated(EPOLL_CLOEXEC);
	
    /* 4.注册事件 */
	UserData* userdata = (UserData*)calloc(1, sizeof(UserData));
	userdata->epllfd = epllfd;
	userdata->connfd = sockfd;
	userdata->handle = AcceptHandle;
	userdata->params = userdata;
	EpollAdd(epllfd, sockfd, EPOLLIN|EPOLLET|EPOLLRDHUP, userdata);
	
	/* 5.事件循环 */
	return EpollDispath(epllfd, sockfd, -1);
	//free((void*)userdata);为了和C兼容,暂不用智能指针，实际中定长内存池更合适。
}

int32_t main()
{
	test_epoll(8080);
	return 0;
}

