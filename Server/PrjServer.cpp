#pragma comment(lib, "ws2_32")
#pragma warning(disable : 4996)

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#define WM_SOCKET  (WM_USER+1)

#define MULTICAST_RECV_IPv4 "235.7.8.1"
#define MULTICAST_SEND_TO_CLIENT_IPv4 "235.7.8.2"

#define SERVERPORT 9000
#define REMOTEPORT 9000
#define BUFSIZE    256

// ���� ���� ������ ���� ����ü�� ����
struct SOCKETINFO
{
	SOCKET sock;
	bool   isIPv6;
	char   buf[BUFSIZE];
	int    recvbytes;
};

int nTotalSockets = 0;
SOCKETINFO *SocketInfoArray[FD_SETSIZE];

struct SOCKET_SendnRecv {
	SOCKET recv;
	SOCKET send;
};

// ���� ���� �Լ�
BOOL AddSocketInfo(SOCKET sock, bool isIPv6);
void RemoveSocketInfo(int nIndex);

DWORD WINAPI UDPv4_Multicast(LPVOID);

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// ���� ��� �Լ�
void err_quit(char *msg);
void err_display(char *msg);

int main(HINSTANCE hInstance, HINSTANCE hPrevInstance,LPSTR lpCmdLine, int nCmdShow)
{
	int retval, retvalUDP;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 1;

	/*----- IPv4 ���� �ʱ�ȭ ���� -----*/
	// socket()
	SOCKET listen_sockv4 = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sockv4 == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddrv4;
	ZeroMemory(&serveraddrv4, sizeof(serveraddrv4));
	serveraddrv4.sin_family = AF_INET;
	serveraddrv4.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddrv4.sin_port = htons(SERVERPORT);
	retval = bind(listen_sockv4, (SOCKADDR *)&serveraddrv4, sizeof(serveraddrv4));
	if(retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sockv4, SOMAXCONN);
	if(retval == SOCKET_ERROR) err_quit("listen()");
	/*----- IPv4 ���� �ʱ�ȭ �� -----*/

	/*----- UDP IPv4 ���� �ʱ�ȭ ���� -----*/
	//printf("IPv4 UDP\n");
	SOCKET listen_sock_UDPv4 = socket(AF_INET, SOCK_DGRAM, 0);
	if (listen_sock_UDPv4 == INVALID_SOCKET) err_quit("socket()");

	//printf("IPv4 UDP Send\n");
	SOCKET send_sock_UDPv4 = socket(AF_INET, SOCK_DGRAM, 0);
	if (send_sock_UDPv4 == INVALID_SOCKET) err_quit("socket()");

	struct SOCKET_SendnRecv UDPv4 = { listen_sock_UDPv4, send_sock_UDPv4 };
	SOCKET_SendnRecv* UDPv4_P = &UDPv4;

	/*----- UDP IPv4 ���� �ʱ�ȭ �� -----*/

	HANDLE hThread[1];
	hThread[0] = CreateThread(NULL, 0, UDPv4_Multicast, (LPVOID)UDPv4_P, 0, NULL);
	DWORD please = WaitForMultipleObjects(1, hThread, TRUE, INFINITE);

	// ������ ��ſ� ����� ����(����)
	FD_SET rset;
	SOCKET client_sock;
	int addrlen, i, j;
	// ������ ��ſ� ����� ����(IPv4)
	SOCKADDR_IN clientaddrv4;

	while(1){
		// ���� �� �ʱ�ȭ
		FD_ZERO(&rset);
		FD_SET(listen_sockv4, &rset);
		FD_SET(listen_sock_UDPv4, &rset);

		for(i=0; i<nTotalSockets; i++){
			FD_SET(SocketInfoArray[i]->sock, &rset);
		}

		// select()
		retval = select(0, &rset, NULL, NULL, NULL);
		if(retval == SOCKET_ERROR){
			err_display("select()");
			break;
		}

		// ���� �� �˻�(1): Ŭ���̾�Ʈ ���� ����
		if(FD_ISSET(listen_sockv4, &rset)){
			addrlen = sizeof(clientaddrv4);
			client_sock = accept(listen_sockv4, (SOCKADDR *)&clientaddrv4, &addrlen);
			if(client_sock == INVALID_SOCKET){
				err_display("accept()");
				break;
			}
			else{
				// ������ Ŭ���̾�Ʈ ���� ���
				printf("[TCPv4 ����] Ŭ���̾�Ʈ ����: [%s]:%d\n",
					inet_ntoa(clientaddrv4.sin_addr), ntohs(clientaddrv4.sin_port));
				// ���� ���� �߰�
				AddSocketInfo(client_sock, false);
			}
		}
		// ���� �� �˻�(2): ������ ���
		for(i=0; i<nTotalSockets; i++){
			SOCKETINFO *ptr = SocketInfoArray[i];
			if(FD_ISSET(ptr->sock, &rset)){
				// ������ �ޱ�
				retval = recv(ptr->sock, ptr->buf + ptr->recvbytes,
					BUFSIZE - ptr->recvbytes, 0);
				if(retval == 0 || retval == SOCKET_ERROR){
					RemoveSocketInfo(i);
					continue;
				}

				// ���� ����Ʈ �� ����
				ptr->recvbytes += retval;

				if(ptr->recvbytes == BUFSIZE){
					// ���� ����Ʈ �� ����
					ptr->recvbytes = 0;

					// ���� ������ ��� Ŭ���̾�Ʈ���� �����͸� ����!
					for(j=0; j<nTotalSockets; j++){
						SOCKETINFO *ptr2 = SocketInfoArray[j];
						retval = send(ptr2->sock, ptr->buf, BUFSIZE, 0);
						if(retval == SOCKET_ERROR){
							err_display("send()");
							RemoveSocketInfo(j);
							--j; // ���� �ε��� ����
							continue;
						}
					}
				}
			}
		}
	}

	return 0;
}

DWORD WINAPI UDPv4_Multicast(LPVOID arg) {
	SOCKET_SendnRecv* socks = (SOCKET_SendnRecv*)arg;
	SOCKET listen_sock_UDPv4 = socks->recv;
	SOCKET send_sock_UDPv4 = socks->send;
	SOCKADDR_IN peeraddr_v4;
	int addrlen_UDP;
	char buf_UDP[BUFSIZE];

	// <receiving>
	// SO_REUSEADDR ���� �ɼ� ����
	BOOL optval = TRUE;
	int retvalUDP = setsockopt(listen_sock_UDPv4, SOL_SOCKET,
		SO_REUSEADDR, (char*)&optval, sizeof(optval));
	if (retvalUDP == SOCKET_ERROR) {
		printf("sockopt()1\n");
		err_quit("setsockopt()");
	}

	SOCKADDR_IN serveraddrUDPv4;
	ZeroMemory(&serveraddrUDPv4, sizeof(serveraddrUDPv4));
	serveraddrUDPv4.sin_family = AF_INET;
	serveraddrUDPv4.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddrUDPv4.sin_port = htons(SERVERPORT);
	retvalUDP = bind(listen_sock_UDPv4, (SOCKADDR*)&serveraddrUDPv4, sizeof(serveraddrUDPv4));
	if (retvalUDP == SOCKET_ERROR) err_quit("bind()");

	// ��Ƽĳ��Ʈ �׷� ����
	struct ip_mreq mreq_v4;
	mreq_v4.imr_multiaddr.s_addr = inet_addr(MULTICAST_RECV_IPv4);
	mreq_v4.imr_interface.s_addr = htonl(INADDR_ANY);
	retvalUDP = setsockopt(listen_sock_UDPv4, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		(char*)&mreq_v4, sizeof(mreq_v4));
	if (retvalUDP == SOCKET_ERROR) {
		printf("sockopt()2\n");
		err_quit("setsockopt()");
	}

	SOCKADDR_IN remoteaddr_v4;
	ZeroMemory(&remoteaddr_v4, sizeof(remoteaddr_v4));
	remoteaddr_v4.sin_family = AF_INET;
	remoteaddr_v4.sin_addr.s_addr = inet_addr(MULTICAST_SEND_TO_CLIENT_IPv4);
	remoteaddr_v4.sin_port = htons(REMOTEPORT);
	char ipaddr[50];
	DWORD ipaddrlen = sizeof(ipaddr);

	while (1) {
		ZeroMemory(buf_UDP, BUFSIZE);
		addrlen_UDP = sizeof(peeraddr_v4);

		retvalUDP = recvfrom(listen_sock_UDPv4, buf_UDP, BUFSIZE, 0,
			(SOCKADDR*)&peeraddr_v4, &addrlen_UDP); // peeraddr NULL �� �ص� ��c 

		// ���������� BUFSIZE+1 ��ŭ �� ����
		// Ŭ���̾�Ʈ�� buf�� ����Ǿ��ִ� ���ڼ���ŭ�� ����
		if (retvalUDP == SOCKET_ERROR) {
			err_display("recvfrom()");
			continue;
		}

		WSAAddressToString((SOCKADDR*)&peeraddr_v4, sizeof(peeraddr_v4), NULL, ipaddr, &ipaddrlen);
		printf("[UDPv4 ����] Ŭ���̾�Ʈ ������ ����: %s\n", ipaddr); // ������ ������ �ƴ϶��, ������ �ǳ� ����Ʈ�� -1�� �ʱ�ȭ
		if (buf_UDP[BUFSIZE - 1] == -1) continue;

		buf_UDP[BUFSIZE - 1] = -1;
		retvalUDP = sendto(send_sock_UDPv4, buf_UDP, BUFSIZE, 0,
			(SOCKADDR*)&remoteaddr_v4, sizeof(remoteaddr_v4));
		if (retvalUDP == SOCKET_ERROR) {
			err_display("sendto()");
			continue;
		}
	}
}

// ���� ���� �߰�
BOOL AddSocketInfo(SOCKET sock, bool isIPv6)
{
	if(nTotalSockets >= FD_SETSIZE){
		printf("[����] ���� ������ �߰��� �� �����ϴ�!\n");
		return FALSE;
	}

	SOCKETINFO *ptr = new SOCKETINFO;
	if(ptr == NULL){
		printf("[����] �޸𸮰� �����մϴ�!\n");
		return FALSE;
	}

	ptr->sock = sock;
	ptr->isIPv6 = isIPv6;
	ptr->recvbytes = 0;
	SocketInfoArray[nTotalSockets++] = ptr;

	return TRUE;
}

// ���� ���� ����
void RemoveSocketInfo(int nIndex)
{
	SOCKETINFO *ptr = SocketInfoArray[nIndex];

	// ������ Ŭ���̾�Ʈ ���� ���
	if(ptr->isIPv6 == false){
		SOCKADDR_IN clientaddrv4;
		int addrlen = sizeof(clientaddrv4);
		getpeername(ptr->sock, (SOCKADDR *)&clientaddrv4, &addrlen);
		printf("[TCPv4 ����] Ŭ���̾�Ʈ ����: [%s]:%d\n", 
			inet_ntoa(clientaddrv4.sin_addr), ntohs(clientaddrv4.sin_port));
	}

	closesocket(ptr->sock);
	delete ptr;

	if(nIndex != (nTotalSockets-1))
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets-1];

	--nTotalSockets;
}

// ���� �Լ� ���� ��� �� ����
void err_quit(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// ���� �Լ� ���� ���
void err_display(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}