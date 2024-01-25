#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <commctrl.h>

#include "resource.h"

#define WM_SOCKET  (WM_USER+1)

// ���� �ּ�
#define SERVERIPV4  "127.0.0.1"   // TCP ����
#define MULTICAST "239.255.255.250"
#define MULTICASTCLIENT "239.255.255.254" // UDP ����

// ��Ʈ ��ȣ ����
#define SERVERPORTCHAT  9000 //ĢĢ����
#define REMOTEPORTCHAT   9000 // ĢĢ����

#define   SERVERPORTDRAW   9010   //N�ױ׸���
#define REMOTEPORTDRAW   9010   //N�ױ׸���
// ��Ʈ ��ȣ ����  ��

#define BUFSIZE     284                
#define MSGSIZE     (BUFSIZE-(sizeof(int)*2)-NICKSIZE-CHECK_WHO)
#define NICKSIZE    30
#define CHECK_WHO   1

#define CHATTING      1000    
#define USERCONNECT       1010 // ����� ������ Ȯ���ϱ� ����
#define USEREXIT      1011 // ����� ���� ������ ����
#define SELFOUT         1012 // ����ڰ� ������ ��ư�� ���� ���
#define DRAWLINE       1013 // �޽��� Ÿ��: �� �׸���
#define NOTICE         1014 // ������ ��������  
#define USERWORD      1015 // ���þ�

typedef struct CHAT_MSG {
    int  type;               // �޼��� Ÿ�� (CHATTING: ä��, DRAWERS: �׸�, USERCONNECT: ����Ȯ�� ...)
    char nickName[NICKSIZE];   // ������� �г���
    char buf[BUFSIZE];      // �޼��� ����
    int whoSent;
};

// ����� ���� ���� �޼���
typedef struct EXIT_MSG {
    int type;
    char dummy[BUFSIZE - 4];
};

// ���þ� ����
typedef struct WORD_MSG {
    int type;
    char nickName[NICKSIZE];   // ������� �г���
    char buf[BUFSIZE];      // �޼��� ����
};

// �� �׸��� �޽��� ����
struct DRAWLINE_MSG
{
    int  type;
    int  figure_type;
    int line_thickness;
    int  color;
    int  x0, y0;
    int  x1, y1;
    char dummy[BUFSIZE - 6 * sizeof(int)];
    int whoSent;
};

// TCP ��� Ȱ��
struct SOCKETINFO_TCP {
    SOCKET      sock;
    bool      isIPv6;
    CHAT_MSG    chatmsg;
    int         recvbytes;
    char nickName[NICKSIZE];   // ������� �г���
};

//ĢĢ���� ������ ��
int nTotalSockets_game1 = 0;
SOCKETINFO_TCP* SocketInfoArray[FD_SETSIZE];

//N�ױ׸��� ������ ��
int nTotalSockets_game2 = 0;
SOCKETINFO_TCP* SocketInfoArray2[FD_SETSIZE];

// UDP
struct SOCKETINFO_ALL {
    SOCKET         sock;         // TRUE - TCP, FALSE - UDP
    char         username[NICKSIZE]; // ����� �г���
    int            userseqNum; // ����� �ĺ��� ���� ����
    CHAT_MSG      chatmsg;
    SOCKADDR_IN* sockaddrv4;
    bool         isIPv6;
    bool         isUDP;      //UDP or TCP �Ǻ�
    char         socktype[10]; // UDP or TCP
    int            recvbytes;
    int            sendbytes;
    BOOL         recvdelayed;
    SOCKETINFO_ALL* next;
};
// ĢĢ����
int nALLSockets = 0;
SOCKETINFO_ALL* AllSocketInfoArray = NULL;
int gameUser;

// N�ױ׸���
int nALLSockets2 = 0;
SOCKETINFO_ALL* AllSocketInfoArray2 = NULL;
int gameUser2;

typedef struct SERVER_SOCKET {
    SOCKET tcp;
    //   SOCKET tcp_game2;
    SOCKET recvUDP;
    SOCKET sendUDP;
    SOCKADDR_IN remoteaddr;
};
SERVER_SOCKET Connect_Socket; // ĢĢ����
SERVER_SOCKET Connect_SocketDraw; // N�ױ׸���

// �ۼ��� ����
struct SOCKET_SendnRecv {
    SOCKET recv;
    SOCKET send;
};

// ���þ� ������ ����
SOCKET send_sockDraw_UDP;

static HINSTANCE     g_hInst;         // �ν��Ͻ� �ڵ�

// ��� ������
static HANDLE       g_hServerThread;    // ���� ������ - ĢĢ����
static HANDLE       g_hServerThreadDraw; // ���� ������ - �Ѻױ׸���
static HANDLE* handleHandle; // ĢĢ���� ���� ������
static HANDLE* handleHandleDraw; // �Ѻױ׸��� ���� ������

// ĢĢ����
static HWND          hEditUserChat; // IDC_EDITCLICHAT1 - ĢĢ���� ����ڵ��� ��ȭ
static HWND          hUserList;         // IDC_LISTGAMEUSER1 - �����  List Box
static HWND          hUserCount;      // IDC_USERCOUNT - ����� �� ǥ��

// N�ױ׸���
static HWND          hEditUserChat2; // IDC_EDITCLICHAT2 - �Ѻױ׸��� ����ڵ��� ��ȭ
static HWND          hUserList2;         // IDC_LISTGAMEUSER2 - �����  List Box
static HWND          hUserCount2;      // IDC_USERCOUNT2 - ����� �� ǥ��

// ����, ����, ���þ� ����
static HWND          hServerNotice;      // IDC_EDITNOTICE - ���� ���� ���� �Է� �κ�
static HWND           hUserNames;      // IDC_USEROUT - ����� ���� ����
static HWND          hUserWord;         // IDC_USERWORD - ���þ� �Է� �κ�

// ���� ����, ���� ��ư
static HWND         serverOpenBtn; // IDC_SERVEROPEN
static HWND         serverCloseBtn; // IDC_SERVEREXIT

//���� ���� �Լ�
BOOL AddSocketInfo(SOCKET sock, bool isIPv6, int port);
void RemoveSocketInfo(int nIndex, int port);
BOOL AddAllSocketInfo(SOCKET sock, char* username, int userUniqudID, int checkIPv6, int checkUDP, SOCKADDR* peeraddr, int port);
void RemoveAllSocketInfo(int index, int port);

// ��� ������ - ĢĢ����
DWORD WINAPI ServerMain(LPVOID); // ĢĢ����
DWORD WINAPI TCPMain(LPVOID); // ĢĢ����
DWORD WINAPI UDPMain(LPVOID); // ĢĢ����

// ��� ������ - N�ױ׸���
DWORD WINAPI ServerMainGAME(LPVOID); // N�ױ׸���
DWORD WINAPI TCPMainGAME(LPVOID); //N�ױ׸���
DWORD WINAPI UDPMainGAME(LPVOID); // N�ױ׸���

// ��ȭ ���� - GUI
BOOL CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// ����Ʈ ��Ʈ�� ������Ʈ -> ����Ʈ ��Ʈ�ѿ� ���
void DisplayText_Send(char* fmt, ...);
void DisplayTextDraw_Send(char* fmt, ...);

// ������ ����ڸ� ����ϴ� ����Ʈ �ڽ� �κ�
char* getlist(char* fmt, ...); // ����Ʈ �ڽ� ���
void getconnectUser(int port); // ������ ����� �� ������Ʈ
void selectedUser(char* selectedItem, int index);   // ����Ʈ �ڽ��� �ִ� ����ڸ� �����Ͽ� �г����� �������� �κ�
void updateUserList(int port);   // ����Ʈ �ڽ��� ������Ʈ�ϴ� �κ�
//void BroadcastNotice(const char* notice, int port); // ���� ����

// ���� �Լ�
void err_quit(char* msg);
void err_display(char* msg);
void err_display(int errcode);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // ���� �ʱ�ȭ
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    // ��ȭ���� ����
    g_hInst = hInstance;
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, WndProc);

    // ���� ����
    WSACleanup();
    return 0;
}

// ��ȭ���� ���ν���
BOOL CALLBACK WndProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND          hServerNoticeBtn; // IDC_BTNSEND - �������� ��ư
    static HWND          hUserOutBtns; // IDC_BTNOUT  - ���� ����
    static HWND          hUserWordBtn; // IDC_BTNWORD - ���þ� ����

    switch (uMsg) {
    case WM_INITDIALOG: //��Ʈ�� �ڵ� ���
       // ĢĢ����
       //hEditUserConnect = GetDlgItem(hDlg, IDC_EDITCLIMSG); // ������� ���� â -> ���� �۵� Ȯ���� ����
        hEditUserChat = GetDlgItem(hDlg, IDC_EDITCLICHAT1); // ����� ��ȭ â
        hUserList = GetDlgItem(hDlg, IDC_LISTGAMEUSER1); // ĢĢ���� ������ �����
        hUserCount = GetDlgItem(hDlg, IDC_USERCOUNT); // ĢĢ���� ������ ����� ��

        // �Ѻױ׸���
        //hEditUserConnect2 = GetDlgItem(hDlg, IDC_EDITCLIMSG3); // ����� ���� â -> ���� �۵� Ȯ��
        hEditUserChat2 = GetDlgItem(hDlg, IDC_EDITCLICHAT2); // ����� ��ȭ â
        hUserList2 = GetDlgItem(hDlg, IDC_LISTGAMEUSER2); // N�ױ׸��⿡ ������ �����
        hUserCount2 = GetDlgItem(hDlg, IDC_USERCOUNT2); // N�ױ׸��⿡ ������ ����� ��
        hUserWord = GetDlgItem(hDlg, IDC_USERWORD); // ����ڿ��� ���þ� ���� �Է�
        hUserWordBtn = GetDlgItem(hDlg, IDC_BTNWORD); // ���þ� ���� ��ư

        // ĢĢ���� + N�ױ׸���
        hServerNoticeBtn = GetDlgItem(hDlg, IDC_BTNSEND); // �������� ���� ��ư
        hUserNames = GetDlgItem(hDlg, IDC_USEROUT); // ����Ʈ���� ���� ������ ������� �г���
        hUserOutBtns = GetDlgItem(hDlg, IDC_BTNOUT); // ���� ���� ��ư

        // ����
        serverOpenBtn = GetDlgItem(hDlg, IDC_SERVEROPEN); // ���� ����
        serverCloseBtn = GetDlgItem(hDlg, IDC_SERVEREXIT); // ���� �ݱ�

        // ��Ʈ�� �ʱ�ȭ
        EnableWindow(hUserWordBtn, FALSE); // ���þ� ����
        EnableWindow(hServerNoticeBtn, FALSE); // ���� ����
        EnableWindow(hUserOutBtns, FALSE); // ����
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SERVEROPEN: // ���� ����
            g_hServerThread = CreateThread(NULL, 0, ServerMain, NULL, 0, NULL);
            g_hServerThreadDraw = CreateThread(NULL, 0, ServerMainGAME, NULL, 0, NULL);

            EnableWindow(serverOpenBtn, FALSE);
            EnableWindow(hServerNoticeBtn, TRUE); // ���� ����
            EnableWindow(hUserOutBtns, TRUE); // ����
            EnableWindow(hUserWordBtn, TRUE); // ���þ� ����
            return TRUE;

        case IDC_SERVEREXIT: // ���� �ݱ�
            TerminateThread(handleHandle[0], 1); // ĢĢ����
            TerminateThread(handleHandle[1], 1);
            TerminateThread(handleHandleDraw[0], 1); // N�ױ׸���
            TerminateThread(handleHandleDraw[1], 1);
            return TRUE;

        case IDC_LISTGAMEUSER1: // ĢĢ���� ���� �����
            switch (HIWORD(wParam)) {
            case LBN_SELCHANGE:
                gameUser = SendMessage(hUserList, LB_GETCURSEL, 0, 0);
                char* selectedItem = (char*)malloc(256);
                SendMessage(hUserList, LB_GETTEXT, gameUser, (LPARAM)selectedItem);
                selectedUser(selectedItem, gameUser);
                EnableWindow(hUserOutBtns, TRUE);
                EnableWindow(hUserWordBtn, TRUE); // ���þ� ����
            }
            return TRUE;
        case IDC_LISTGAMEUSER2: // N�ױ׸��� ���� �����
            switch (HIWORD(wParam)) {
            case LBN_SELCHANGE:
                gameUser2 = SendMessage(hUserList2, LB_GETCURSEL, 0, 0);
                char* selectedItem = (char*)malloc(256);
                SendMessage(hUserList2, LB_GETTEXT, gameUser2, (LPARAM)selectedItem);
                selectedUser(selectedItem, gameUser2);
                EnableWindow(hUserWordBtn, TRUE); // ���þ� ��ư
                EnableWindow(hUserOutBtns, TRUE);
            }
            return TRUE;
        case IDC_BTNWORD: // �Ѻױ׸��� �����ڿ��� ���þ� ���� ��ư
        {
            char word[MSGSIZE]; // ���þ ������ ����
            GetDlgItemText(hDlg, IDC_USERWORD, word, MSGSIZE); // ��ȭ���ڿ��� ���þ �о��

            CHAT_MSG wordMsg;
            wordMsg.type = USERWORD; // �޽��� Ÿ���� USERWORD�� ����
            strncpy(wordMsg.buf, word, MSGSIZE); // ���þ �޽��� ���ۿ� ����

            // ��� Ŭ���̾�Ʈ���� ���þ� ����
            for (int i = 0; i < nALLSockets2; ++i) {
                SOCKETINFO_TCP* client = SocketInfoArray2[i];
                if (client != NULL && client->sock != INVALID_SOCKET) {
                    send(client->sock, (char*)&wordMsg, sizeof(wordMsg), 0); // ���þ� �޽��� ����
                    
                }
            }

            SetDlgItemText(hDlg, IDC_EDITCLICHAT2, "���þ� ���� �Ϸ�\n"); // ���� �޽��� ������Ʈ
        }
        return TRUE;

        case IDC_BTNSEND:
        {
            char noticeBuffer[BUFSIZE];
            GetDlgItemText(hDlg, IDC_EDITNOTICE, noticeBuffer, MSGSIZE); // ��ȭ���ڿ��� ���þ �о��

            CHAT_MSG notiMsg;
            notiMsg.type = NOTICE;
            strncpy(notiMsg.buf, noticeBuffer, MSGSIZE);

            // ��� ĢĢ���� Ŭ���̾�Ʈ���� ���� ����
            for (int i = 0; i < nALLSockets; ++i) {
                SOCKETINFO_TCP* client = SocketInfoArray[i];
                if (client != NULL && client->sock != INVALID_SOCKET) {
                    send(client->sock, (char*)&notiMsg, sizeof(notiMsg), 0);
                    sendto(Connect_Socket.sendUDP, (char*)&notiMsg, sizeof(notiMsg), 0, (SOCKADDR*)&(Connect_Socket.remoteaddr), sizeof(Connect_Socket.remoteaddr));
                }
            }

            // ��� ĢĢ���� Ŭ���̾�Ʈ���� ���� ����
            for (int i = 0; i < nALLSockets2; ++i) {
                SOCKETINFO_TCP* client = SocketInfoArray2[i];
                if (client != NULL && client->sock != INVALID_SOCKET) {
                    send(client->sock, (char*)&notiMsg, sizeof(notiMsg), 0);
                    sendto(Connect_SocketDraw.sendUDP, (char*)&notiMsg, sizeof(notiMsg), 0, (SOCKADDR*)&(Connect_SocketDraw.remoteaddr), sizeof(Connect_SocketDraw.remoteaddr));
                }
            }
            SetDlgItemText(hDlg, IDC_EDITCLICHAT1, "���� ���� �Ϸ�");
            SetDlgItemText(hDlg, IDC_EDITCLICHAT2, "���� ���� �Ϸ�");
        }
        return TRUE;


        case IDC_BTNOUT: // ����� ���� ���� ��ư
            if (SERVERPORTCHAT) {
                RemoveAllSocketInfo(gameUser, SERVERPORTCHAT);
            }
            if (SERVERPORTDRAW) {
                RemoveAllSocketInfo(gameUser2, SERVERPORTDRAW);
            }
            EnableWindow(hUserOutBtns, TRUE);
            return TRUE;
        }
    }
    return FALSE;
}

DWORD WINAPI ServerMain(LPVOID arg) { // ĢĢ����
   // ���� �ʱ�ȭ
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    /*----- TCP, UDP IPv4 ���� �ʱ�ȭ ���� -----*/
    // socket() -> TCP
    SOCKET listen_sock4 = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock4 == INVALID_SOCKET) err_quit("socket()");
    //  
       // socket() -> UDP
    SOCKET listen_sock_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sock_UDP == INVALID_SOCKET) err_quit("socket()");

    SOCKET send_sock_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sock_UDP == INVALID_SOCKET) err_quit("socket()");

    // ���� �ּ� ����ü �ʱ�ȭ
    SOCKADDR_IN remoteaddr_v4;
    ZeroMemory(&remoteaddr_v4, sizeof(remoteaddr_v4));
    remoteaddr_v4.sin_family = AF_INET;
    remoteaddr_v4.sin_addr.s_addr = inet_addr(MULTICASTCLIENT);
    remoteaddr_v4.sin_port = htons(REMOTEPORTCHAT);

    Connect_Socket = { listen_sock4,listen_sock_UDP, send_sock_UDP,remoteaddr_v4 };
    SERVER_SOCKET* Connect_Socket_ALL = &Connect_Socket;

    // ������ Ȱ��
    HANDLE hThread[2];
    hThread[0] = CreateThread(NULL, 0, TCPMain, (LPVOID)Connect_Socket_ALL, 0, NULL);
    hThread[1] = CreateThread(NULL, 0, UDPMain, (LPVOID)Connect_Socket_ALL, 0, NULL);

    handleHandle = hThread;
    DWORD please = WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

    closesocket(listen_sock4);
    closesocket(listen_sock_UDP);
    closesocket(send_sock_UDP);
    return 0;
}

DWORD WINAPI ServerMainGAME(LPVOID) { // N�ױ׸���

   // ���� �ʱ�ȭ
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    /*----- TCP, UDP IPv4 ���� �ʱ�ȭ ���� -----*/
    // socket() -> TCP
    SOCKET listen_sockDraw = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sockDraw == INVALID_SOCKET) err_quit("socket()");

    // socket() -> UDP
    SOCKET listen_sockDraw_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockDraw_UDP == INVALID_SOCKET) err_quit("socket()");

    send_sockDraw_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockDraw_UDP == INVALID_SOCKET) err_quit("socket()");

    // ���� �ּ� ����ü �ʱ�ȭ
    SOCKADDR_IN remoteaddr_Draw;
    ZeroMemory(&remoteaddr_Draw, sizeof(remoteaddr_Draw));
    remoteaddr_Draw.sin_family = AF_INET;
    remoteaddr_Draw.sin_addr.s_addr = inet_addr(MULTICASTCLIENT);
    remoteaddr_Draw.sin_port = htons(REMOTEPORTDRAW);

    Connect_SocketDraw = { listen_sockDraw, listen_sockDraw_UDP, send_sockDraw_UDP, remoteaddr_Draw };
    SERVER_SOCKET* Connect_Socket_Draw = &Connect_SocketDraw;

    // ������ Ȱ��
    HANDLE hThreadDarw[2];
    hThreadDarw[0] = CreateThread(NULL, 0, TCPMainGAME, (LPVOID)Connect_Socket_Draw, 0, NULL);
    hThreadDarw[1] = CreateThread(NULL, 0, UDPMainGAME, (LPVOID)Connect_Socket_Draw, 0, NULL);

    handleHandleDraw = hThreadDarw;
    DWORD please = WaitForMultipleObjects(2, hThreadDarw, TRUE, INFINITE);

    closesocket(listen_sockDraw);
    closesocket(listen_sockDraw_UDP);
    closesocket(send_sockDraw_UDP);
    return 0;
}

DWORD WINAPI TCPMain(LPVOID arg) { // ĢĢ����
    SERVER_SOCKET* socks = (SERVER_SOCKET*)arg;

    SOCKET listen_sock4 = socks->tcp;
    SOCKET send_sock_UDP = socks->sendUDP;
    SOCKADDR_IN remoteaddr_v4 = socks->remoteaddr;

    // ������ ��ſ� ����� ����(����)
    FD_SET rset;
    SOCKET client_sock;
    int addrlen, i, j;
    int retval, retvalUDP;
    SOCKADDR_IN clientaddrv4;

    // bind() -> ĢĢ����
    SOCKADDR_IN serveraddrv4;
    ZeroMemory(&serveraddrv4, sizeof(serveraddrv4));
    serveraddrv4.sin_family = AF_INET;
    serveraddrv4.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddrv4.sin_port = htons(SERVERPORTCHAT);
    retval = bind(listen_sock4, (SOCKADDR*)&serveraddrv4, sizeof(serveraddrv4));
    if (retval == SOCKET_ERROR) err_quit("bind()");

    // listen()
    retval = listen(listen_sock4, SOMAXCONN);
    if (retval == SOCKET_ERROR) err_quit("listen()");
    /*----- IPv4 ���� �ʱ�ȭ �� -----*/

    while (1) { // ĢĢ����
       // ���� �� �ʱ�ȭ
        FD_ZERO(&rset);
        FD_SET(listen_sock4, &rset);
        for (i = 0; i < nTotalSockets_game1; i++) {
            FD_SET(SocketInfoArray[i]->sock, &rset);
        }

        // select()
        retval = select(0, &rset, NULL, NULL, NULL);
        if (retval == SOCKET_ERROR) {
            err_display("select()");
            break;
        }

        // ���� �� �˻�(1): Ŭ���̾�Ʈ ���� ����
        if (FD_ISSET(listen_sock4, &rset)) {
            addrlen = sizeof(clientaddrv4);
            client_sock = accept(listen_sock4, (SOCKADDR*)&clientaddrv4, &addrlen);
            if (client_sock == INVALID_SOCKET) {
                err_display("accept()");
                break;
            }
            else {
                // ������ Ŭ���̾�Ʈ ���� ���
                DisplayText_Send("\n[TCP] Ŭ���̾�Ʈ ���� Ȯ��: [%s]:%d \r\n",
                    inet_ntoa(clientaddrv4.sin_addr), ntohs(clientaddrv4.sin_port));
                // ���� ���� �߰�
                AddSocketInfo(client_sock, false, SERVERPORTCHAT);
            }
        }
        // ���� �� �˻�(2): ������ ���
        for (i = 0; i < nTotalSockets_game1; i++) {
            SOCKETINFO_TCP* ptr = SocketInfoArray[i];
            if (FD_ISSET(ptr->sock, &rset)) {
                // ������ �ޱ�
                retval = recv(ptr->sock, (char*)&(ptr->chatmsg) + ptr->recvbytes,
                    BUFSIZE - ptr->recvbytes, 0);
                if (retval == 0 || retval == SOCKET_ERROR) {
                    RemoveSocketInfo(i, SERVERPORTCHAT);
                    continue;
                }
                if (ptr->chatmsg.type == SELFOUT) { //������ ������ ����
                    RemoveSocketInfo(i, SERVERPORTCHAT); // Ŭ���̾�Ʈ ���� ó��
                    continue;
                }
                if (ptr->chatmsg.type == USERCONNECT) { // ����� ������ Ȯ��
                    AddAllSocketInfo(ptr->sock, ptr->chatmsg.nickName, ptr->chatmsg.whoSent, ptr->isIPv6, FALSE, NULL, SERVERPORTCHAT);
                    continue;
                }
                if (ptr->chatmsg.type == CHATTING) // ä�� Ȯ��
                    DisplayText_Send("\n[%s(%d)]: %s \r\n", ptr->chatmsg.nickName, ptr->chatmsg.whoSent, ptr->chatmsg.buf);

                // ���� ����Ʈ �� ����
                ptr->recvbytes += retval;

                if (ptr->recvbytes == BUFSIZE) {
                    // ���� ����Ʈ �� ����
                    ptr->recvbytes = 0;

                    for (j = 0; j < nTotalSockets_game1; j++) { // ������ ����� ��ο��� ������ ����
                        SOCKETINFO_TCP* ptr2 = SocketInfoArray[j];
                        retval = send(ptr2->sock, (char*)&(ptr->chatmsg), BUFSIZE, 0);

                        if (retval == SOCKET_ERROR) {
                            err_display("send()");
                            RemoveSocketInfo(j, SERVERPORTCHAT);
                            --j; // ���� �ε��� ����
                            continue;
                        }
                    }
                    retvalUDP = sendto(send_sock_UDP, (char*)&(ptr->chatmsg), BUFSIZE, 0,
                        (SOCKADDR*)&remoteaddr_v4, sizeof(remoteaddr_v4));
                }
            }
        }
    }
    return 0;
}

DWORD WINAPI TCPMainGAME(LPVOID arg) { // �Ѻױ׸���
    SERVER_SOCKET* socks = (SERVER_SOCKET*)arg;

    SOCKET listen_sockDraw = socks->tcp;
    SOCKET send_sockDraw_UDP = socks->sendUDP;
    SOCKADDR_IN remoteaddr_Draw = socks->remoteaddr;

    // ������ ��ſ� ����� ����(����)
    FD_SET rsetDraw;
    SOCKET client_sockDraw;
    int addrlenDraw, i, j;
    int retvalDraw, retvalDraw_UDP;
    SOCKADDR_IN clientaddr_Draw;

    // bind() -> �Ѻױ׸���
    SOCKADDR_IN serveraddr_Draw;
    ZeroMemory(&serveraddr_Draw, sizeof(serveraddr_Draw));
    serveraddr_Draw.sin_family = AF_INET;
    serveraddr_Draw.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr_Draw.sin_port = htons(SERVERPORTDRAW);
    retvalDraw = bind(listen_sockDraw, (SOCKADDR*)&serveraddr_Draw, sizeof(serveraddr_Draw));
    if (retvalDraw == SOCKET_ERROR) err_quit("bind()");

    // listen()
    retvalDraw = listen(listen_sockDraw, SOMAXCONN);
    if (retvalDraw == SOCKET_ERROR) err_quit("listen()");
    /*----- IPv4 ���� �ʱ�ȭ �� -----*/

    while (1) {
        // ���� �� �ʱ�ȭ
        FD_ZERO(&rsetDraw);
        FD_SET(listen_sockDraw, &rsetDraw);
        for (i = 0; i < nTotalSockets_game2; i++) {
            FD_SET(SocketInfoArray2[i]->sock, &rsetDraw);
        }

        // select()
        retvalDraw = select(0, &rsetDraw, NULL, NULL, NULL);
        if (retvalDraw == SOCKET_ERROR) {
            err_display("select()");
            break;
        }

        // ���� �� �˻�(1): Ŭ���̾�Ʈ ���� ����
        if (FD_ISSET(listen_sockDraw, &rsetDraw)) {
            addrlenDraw = sizeof(clientaddr_Draw);
            client_sockDraw = accept(listen_sockDraw, (SOCKADDR*)&clientaddr_Draw, &addrlenDraw);
            if (client_sockDraw == INVALID_SOCKET) {
                err_display("accept()");
                break;
            }
            else {
                // ������ Ŭ���̾�Ʈ ���� ���
                DisplayTextDraw_Send("\n[TCP] Ŭ���̾�Ʈ ���� Ȯ��: [%s]:%d \r\n", inet_ntoa(clientaddr_Draw.sin_addr), ntohs(clientaddr_Draw.sin_port));
                // ���� ���� �߰�
                AddSocketInfo(client_sockDraw, false, SERVERPORTDRAW);
            }
        }
        // ���� �� �˻�(2): ������ ���
        for (i = 0; i < nTotalSockets_game2; i++) {
            SOCKETINFO_TCP* ptr = SocketInfoArray2[i];
            if (FD_ISSET(ptr->sock, &rsetDraw)) {
                // ������ �ޱ�
                retvalDraw = recv(ptr->sock, (char*)&(ptr->chatmsg) + ptr->recvbytes,
                    BUFSIZE - ptr->recvbytes, 0);
                if (retvalDraw == 0 || retvalDraw == SOCKET_ERROR) {
                    RemoveSocketInfo(i, SERVERPORTDRAW);
                    continue;
                }
                if (ptr->chatmsg.type == SELFOUT) { // ����� ������ ������  
                    RemoveSocketInfo(i, SERVERPORTDRAW); // Ŭ���̾�Ʈ ���� ó��
                    continue;
                }
                if (ptr->chatmsg.type == USERCONNECT) { // ����� ������ Ȯ��
                    AddAllSocketInfo(ptr->sock, ptr->chatmsg.nickName, ptr->chatmsg.whoSent, ptr->isIPv6, FALSE, NULL, SERVERPORTDRAW);
                    continue;
                }
                if (ptr->chatmsg.type == CHATTING) // ä�� Ȯ��
                    DisplayTextDraw_Send("\n[%s(%d)]: %s \r\n", ptr->chatmsg.nickName, ptr->chatmsg.whoSent, ptr->chatmsg.buf);

                // ���� ����Ʈ �� ����
                ptr->recvbytes += retvalDraw;

                if (ptr->recvbytes == BUFSIZE) {
                    // ���� ����Ʈ �� ����
                    ptr->recvbytes = 0;
                    
                    for (j = 0; j < nTotalSockets_game2; j++) { // ������ ����� ��ο��� ������ ����
                        SOCKETINFO_TCP* ptr2 = SocketInfoArray2[j];
                        retvalDraw = send(ptr2->sock, (char*)&(ptr->chatmsg), BUFSIZE, 0);

                        if (retvalDraw == SOCKET_ERROR) {
                            err_display("send()");
                            RemoveSocketInfo(j, SERVERPORTCHAT);
                            --j; // ���� �ε��� ����
                            continue;
                        }
                    }
                    retvalDraw_UDP = sendto(send_sockDraw_UDP, (char*)&(ptr->chatmsg), BUFSIZE, 0,
                        (SOCKADDR*)&remoteaddr_Draw, sizeof(remoteaddr_Draw));
                }
            }
        }
    }
    return 0;
}

DWORD WINAPI UDPMain(LPVOID arg) { // UDP ����
    SERVER_SOCKET* socks = (SERVER_SOCKET*)arg;
    SOCKET listen_sock_UDP = socks->recvUDP;

    SOCKET send_sock_UDP = socks->sendUDP;
    SOCKADDR_IN remoteaddr_v4 = socks->remoteaddr;
    SOCKADDR_IN peeraddr_v4;

    int retvalTCP, retvalUDP;
    int addrlen_UDP;

    // SO_REUSEADDR ���� �ɼ� ����
    BOOL optval = TRUE;
    retvalUDP = setsockopt(listen_sock_UDP, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
    if (retvalUDP == SOCKET_ERROR) {
        err_quit("setsockopt()");
    }

    SOCKADDR_IN serveraddrUDP;
    ZeroMemory(&serveraddrUDP, sizeof(serveraddrUDP));
    serveraddrUDP.sin_family = AF_INET;
    serveraddrUDP.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddrUDP.sin_port = htons(SERVERPORTCHAT);
    retvalUDP = bind(listen_sock_UDP, (SOCKADDR*)&serveraddrUDP, sizeof(serveraddrUDP));
    if (retvalUDP == SOCKET_ERROR) err_quit("bind()");

    // UDP -> MULTICAST
    struct ip_mreq mreq_v4;
    mreq_v4.imr_multiaddr.s_addr = inet_addr(MULTICAST);
    mreq_v4.imr_interface.s_addr = htonl(INADDR_ANY);
    retvalUDP = setsockopt(listen_sock_UDP, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq_v4, sizeof(mreq_v4));
    if (retvalUDP == SOCKET_ERROR) {
        err_quit("setsockopt()");
    }

    DWORD ttl = 2;
    retvalUDP = setsockopt(send_sock_UDP, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));
    if (retvalUDP == SOCKET_ERROR) {
        err_quit("setsockopt()");
    }

    char ipaddr[50];
    DWORD ipaddrlen = sizeof(ipaddr);
    Sleep(2000);
    while (1) {
        CHAT_MSG* chatmsg = (CHAT_MSG*)malloc(sizeof(CHAT_MSG));
        addrlen_UDP = sizeof(peeraddr_v4);

        retvalUDP = recvfrom(listen_sock_UDP, (char*)(chatmsg), BUFSIZE, 0, (SOCKADDR*)&peeraddr_v4, &addrlen_UDP);
        if (retvalUDP == SOCKET_ERROR) {
            err_display("recvfrom()");
            continue;
        }

        if (chatmsg->type == USERCONNECT) { // ����� ���� �˸���
            WSAAddressToString((SOCKADDR*)&peeraddr_v4, sizeof(peeraddr_v4), NULL, ipaddr, &ipaddrlen);
            DisplayText_Send("\n[UDP] Ŭ���̾�Ʈ ����: %s\r\n", ipaddr);
            AddAllSocketInfo(NULL, chatmsg->nickName, chatmsg->whoSent, FALSE, TRUE, (SOCKADDR*)&peeraddr_v4, SERVERPORTCHAT);
            continue;
        }

        if (chatmsg->type == CHATTING)
            DisplayText_Send("\n[%s(%d)]: %s\r\n", chatmsg->nickName, chatmsg->whoSent, chatmsg->buf);

        retvalUDP = sendto(send_sock_UDP, (char*)(chatmsg), BUFSIZE, 0,
            (SOCKADDR*)&remoteaddr_v4, sizeof(remoteaddr_v4));
        if (retvalUDP == SOCKET_ERROR) {
            err_display("sendto()");
            continue;
        }

        for (int j = 0; j < nTotalSockets_game1; j++) {
            SOCKETINFO_TCP* ptr2 = SocketInfoArray[j];
            retvalTCP = send(ptr2->sock, (char*)(chatmsg), BUFSIZE, 0);
            if (retvalTCP == SOCKET_ERROR) {
                err_display("send()");
                RemoveSocketInfo(j, SERVERPORTCHAT);
                --j; // ���� �ε��� ����
                continue;
            }
        }
        free(chatmsg);
    }
}

DWORD WINAPI UDPMainGAME(LPVOID arg) { // UDP ���� - �Ѻױ׸���
    SERVER_SOCKET* socks_Draw = (SERVER_SOCKET*)arg;
    SOCKET listen_sockDraw_UDP = socks_Draw->recvUDP;

    SOCKET send_sockDraw_UDP = socks_Draw->sendUDP;
    SOCKADDR_IN remoteaddr_Draw = socks_Draw->remoteaddr;

    SOCKADDR_IN peeraddr_Draw;

    int retvalTCP_Draw, retvalUDP_Draw;
    int addrlenDraw_UDP;

    // SO_REUSEADDR ���� �ɼ� ����
    BOOL optval = TRUE;
    retvalUDP_Draw = setsockopt(listen_sockDraw_UDP, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
    if (retvalUDP_Draw == SOCKET_ERROR) {
        err_quit("setsockopt()");
    }

    SOCKADDR_IN serveraddrUDP_Draw;
    ZeroMemory(&serveraddrUDP_Draw, sizeof(serveraddrUDP_Draw));
    serveraddrUDP_Draw.sin_family = AF_INET;
    serveraddrUDP_Draw.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddrUDP_Draw.sin_port = htons(SERVERPORTDRAW);
    retvalUDP_Draw = bind(listen_sockDraw_UDP, (SOCKADDR*)&serveraddrUDP_Draw, sizeof(serveraddrUDP_Draw));
    if (retvalUDP_Draw == SOCKET_ERROR) err_quit("bind()");

    // UDP -> MULTICAST
    struct ip_mreq mreq_Draw;
    mreq_Draw.imr_multiaddr.s_addr = inet_addr(MULTICAST);
    mreq_Draw.imr_interface.s_addr = htonl(INADDR_ANY);
    retvalUDP_Draw = setsockopt(listen_sockDraw_UDP, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq_Draw, sizeof(mreq_Draw));
    if (retvalUDP_Draw == SOCKET_ERROR) {
        err_quit("setsockopt()");
    }

    DWORD ttl = 2;
    retvalUDP_Draw = setsockopt(send_sockDraw_UDP, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));
    if (retvalUDP_Draw == SOCKET_ERROR) {
        err_quit("setsockopt()");
    }

    char ipaddr[70];
    DWORD ipaddrlen = sizeof(ipaddr);

    while (1) {
        CHAT_MSG* chatmsg = (CHAT_MSG*)malloc(sizeof(CHAT_MSG));
        addrlenDraw_UDP = sizeof(peeraddr_Draw);

        retvalUDP_Draw = recvfrom(listen_sockDraw_UDP, (char*)(chatmsg), BUFSIZE, 0, (SOCKADDR*)&peeraddr_Draw, &addrlenDraw_UDP);
        if (retvalUDP_Draw == SOCKET_ERROR) {
            err_display("recvfrom()");
            continue;
        }

        if (chatmsg->type == USERCONNECT) { // ����� ���� �˸���
            WSAAddressToString((SOCKADDR*)&peeraddr_Draw, sizeof(peeraddr_Draw), NULL, ipaddr, &ipaddrlen);
            DisplayTextDraw_Send("\r\n");
            DisplayTextDraw_Send("\n[UDP] Ŭ���̾�Ʈ ����: %s \r\n", ipaddr);
            AddAllSocketInfo(NULL, chatmsg->nickName, chatmsg->whoSent, FALSE, TRUE, (SOCKADDR*)&peeraddr_Draw, SERVERPORTDRAW);
            continue;
        }

        if (chatmsg->type == CHATTING)
            DisplayTextDraw_Send("\n[%s(%d)]: %s \r\n", chatmsg->nickName, chatmsg->whoSent, chatmsg->buf);

        retvalUDP_Draw = sendto(send_sockDraw_UDP, (char*)(chatmsg), BUFSIZE, 0,
            (SOCKADDR*)&remoteaddr_Draw, sizeof(remoteaddr_Draw));
        if (retvalUDP_Draw == SOCKET_ERROR) {
            err_display("sendto()");
            continue;
        }

        for (int j = 0; j < nTotalSockets_game2; j++) {
            SOCKETINFO_TCP* ptr2 = SocketInfoArray2[j];
            retvalTCP_Draw = send(ptr2->sock, (char*)(chatmsg), BUFSIZE, 0);
            if (retvalTCP_Draw == SOCKET_ERROR) {
                err_display("send()");
                RemoveSocketInfo(j, SERVERPORTDRAW);
                --j; // ���� �ε��� ����
                continue;
            }
        }
        free(chatmsg);
    }
}

// ���� ���� �߰�
BOOL AddSocketInfo(SOCKET sock, bool isIPv6, int port)
{
    if (nTotalSockets_game1 >= FD_SETSIZE) {
        DisplayText_Send("[����] ���� ������ �߰��� �� �����ϴ�! \r\n");
        return FALSE;
    }
    else if (nTotalSockets_game2 >= FD_SETSIZE) {
        DisplayTextDraw_Send("[����] ���� ������ �߰��� �� �����ϴ�! \r\n");
        return FALSE;
    }

    SOCKETINFO_TCP* ptr = new SOCKETINFO_TCP;
    if (ptr == NULL) {
        DisplayText_Send("[����] �޸𸮰� �����մϴ�! \r\n");
        DisplayTextDraw_Send("[����] �޸𸮰� �����մϴ�! \r\n");
        return FALSE;
    }
    ptr->sock = sock;
    ptr->isIPv6 = isIPv6;
    ptr->recvbytes = 0;
    if (port == SERVERPORTCHAT) {
        SocketInfoArray[nTotalSockets_game1++] = ptr;
    }
    else if (port == SERVERPORTDRAW) {
        SocketInfoArray2[nTotalSockets_game2++] = ptr;
    }

    return TRUE;
}

// ���� ���� ���� -> TCP
void RemoveSocketInfo(int nIndex, int port)
{
    SOCKETINFO_TCP* ptr = SocketInfoArray[nIndex]; // ĢĢ����
    SOCKETINFO_TCP* ptr2 = SocketInfoArray2[nIndex]; // N�ױ׸���

    if (port == SERVERPORTCHAT) {
        // ������ Ŭ���̾�Ʈ ���� ���
        if (ptr->isIPv6 == false) {
            SOCKADDR_IN clientaddrv4;
            int addrlen = sizeof(clientaddrv4);
            getpeername(ptr->sock, (SOCKADDR*)&clientaddrv4, &addrlen);
            DisplayText_Send("\n[TCP] Ŭ���̾�Ʈ ����: [%s]:%d \r\n", inet_ntoa(clientaddrv4.sin_addr), ntohs(clientaddrv4.sin_port));
        }
        closesocket(ptr->sock);
        delete ptr;

        if (nIndex != (nTotalSockets_game1 - 1))
            SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets_game1 - 1];

        --nTotalSockets_game1;
        // ����� ���� ������Ʈ
        getconnectUser(SERVERPORTCHAT);
        updateUserList(SERVERPORTCHAT);
    }
    else if (port == SERVERPORTDRAW) {
        // ������ Ŭ���̾�Ʈ ���� ���
        if (ptr2->isIPv6 == false) {
            SOCKADDR_IN clientaddr_draw;
            int addrLen = sizeof(clientaddr_draw);
            getpeername(ptr2->sock, (SOCKADDR*)&clientaddr_draw, &addrLen);
            DisplayTextDraw_Send("\n[TCP] Ŭ���̾�Ʈ ����: [%s]:%d \r\n", inet_ntoa(clientaddr_draw.sin_addr), ntohs(clientaddr_draw.sin_port));
        }
        closesocket(ptr2->sock);
        delete ptr2;

        if (nIndex != (nTotalSockets_game2 - 1))
            SocketInfoArray2[nIndex] = SocketInfoArray2[nTotalSockets_game2 - 1];

        --nTotalSockets_game2;
        // ����� ���� ������Ʈ
        getconnectUser(SERVERPORTDRAW);
        updateUserList(SERVERPORTDRAW);
    }
}

// ���� ���� -> UDP
BOOL AddAllSocketInfo(SOCKET sock, char* username, int userseqNumber, int isIPv6, int isUDP, SOCKADDR* peeraddr, int port) {
    SOCKETINFO_ALL* ptr = new SOCKETINFO_ALL;
    SOCKADDR_IN* sockaddr = new SOCKADDR_IN;

    int addrlen;
    char* listupMsg = (char*)malloc(256);
    char* ipaddr = (char*)malloc(INET_ADDRSTRLEN);
    DWORD ipaddrLen = INET_ADDRSTRLEN;

    if (ptr == NULL) {
        err_display("wrong socket info");
        return false;
    }

    ptr->sock = sock;
    ptr->isIPv6 = isIPv6;
    ptr->isUDP = isUDP;
    ptr->userseqNum = userseqNumber;

    // ������ ������� �г���
    int nicknameLen = strlen(username);
    int nicknameDum = NICKSIZE - nicknameLen;

    strncpy(ptr->username, username, nicknameLen);
    memset(ptr->username + nicknameLen, 0, nicknameDum);
    ptr->username[NICKSIZE - 1] = NULL;

    if (isUDP == FALSE) { // TCP
        if (isIPv6 == false) { // IPv4
            strncpy(ptr->socktype, "TCP", 6);
            addrlen = sizeof(SOCKADDR_IN);
            getpeername(sock, (SOCKADDR*)sockaddr, &addrlen);
            ptr->sockaddrv4 = sockaddr;

            if (port == SERVERPORTCHAT) {
                ptr->next = AllSocketInfoArray;
                AllSocketInfoArray = ptr;
                nALLSockets++;

                getconnectUser(SERVERPORTCHAT);
                updateUserList(SERVERPORTCHAT);
            }
            else if (port == SERVERPORTDRAW) {
                ptr->next = AllSocketInfoArray2;
                AllSocketInfoArray2 = ptr;
                nALLSockets2++;

                getconnectUser(SERVERPORTDRAW);
                updateUserList(SERVERPORTDRAW);
            }
        }
    }
    else { // UDP
        if (isIPv6 == FALSE) { // IPv4
            strncpy(ptr->socktype, "UDP", 6);
            ptr->sockaddrv4 = (SOCKADDR_IN*)peeraddr;

            if (port == SERVERPORTCHAT) {
                ptr->next = AllSocketInfoArray;
                AllSocketInfoArray = ptr;
                nALLSockets++;

                getconnectUser(SERVERPORTCHAT);
                updateUserList(SERVERPORTCHAT);
            }
            else if (port == SERVERPORTDRAW) {
                ptr->next = AllSocketInfoArray2;
                AllSocketInfoArray2 = ptr;
                nALLSockets2++;

                getconnectUser(SERVERPORTDRAW);
                updateUserList(SERVERPORTDRAW);
            }
        }
    }
    return TRUE;
}

void RemoveAllSocketInfo(int index, int port) { // UDP
    SOCKETINFO_ALL* ptr = (port == SERVERPORTCHAT) ? AllSocketInfoArray : AllSocketInfoArray2;
    SOCKETINFO_ALL* prev = NULL;

    char ipaddr[50];
    DWORD ipaddrLen = sizeof(ipaddr);
    int i = 0, retval;
    while (ptr != NULL) {
        if (index == i) {
            CHAT_MSG endMsg;
            endMsg.type = USEREXIT;
            strncpy(endMsg.nickName, ptr->username, NICKSIZE);
            endMsg.whoSent = ptr->userseqNum;
            if (ptr->isUDP == false)
                retval = send(ptr->sock, (char*)&endMsg, BUFSIZE, 0);
            else {
                if (ptr->isIPv6 == false) {
                    WSAAddressToString((SOCKADDR*)ptr->sockaddrv4, sizeof(SOCKADDR_IN), NULL, ipaddr, &ipaddrLen);
                    retval = sendto((port == SERVERPORTCHAT) ? Connect_Socket.sendUDP : Connect_SocketDraw.sendUDP, (char*)&endMsg, BUFSIZE, 0, (SOCKADDR*)&((port == SERVERPORTCHAT) ? Connect_Socket.remoteaddr : Connect_SocketDraw.remoteaddr), sizeof(SOCKADDR_IN));
                    DisplayText_Send("\r\n");
                    DisplayText_Send("\n[UDPv4] Ŭ���̾�Ʈ ����: %s \r\n", ipaddr);
                }
            }
            if (prev)
                prev->next = ptr->next;
            else {
                if (port == SERVERPORTCHAT)
                    AllSocketInfoArray = ptr->next;
                else
                    AllSocketInfoArray2 = ptr->next;
            }
            delete ptr;
            break;
        }
        prev = ptr;
        ptr = ptr->next;
        i++;
    }
    if (port == SERVERPORTCHAT) {
        --nALLSockets;
        --nTotalSockets_game1;
        getconnectUser(SERVERPORTCHAT);
        updateUserList(SERVERPORTCHAT);
    }
    else if (port == SERVERPORTDRAW) {
        --nALLSockets2;
        --nTotalSockets_game2;
        getconnectUser(SERVERPORTDRAW);
        updateUserList(SERVERPORTDRAW);
    }
}

char* getlist(char* fmt, ...) { // ����Ʈ �ڽ� ���ڿ� ��ȯ
    va_list arg;
    va_start(arg, fmt);

    char listString[256];
    vsprintf(listString, fmt, arg);

    return (char*)listString;
}

void getconnectUser(int port) { // ���� ������ ����� �� ������Ʈ
    if (port == SERVERPORTCHAT) { // ĢĢ���� �����  
        char count[4];
        ZeroMemory(count, 4);
        itoa(nALLSockets, count, 10);

        SendMessage(hUserCount, EM_SETSEL, 0, 4);
        SendMessage(hUserCount, WM_CLEAR, 0, 0);
        SendMessage(hUserCount, EM_REPLACESEL, FALSE, (LPARAM)count);
    }
    else if (port == SERVERPORTDRAW) { // N�ױ׸��� �����
        char count2[4];
        ZeroMemory(count2, 4);
        itoa(nALLSockets2, count2, 10);

        SendMessage(hUserCount2, EM_SETSEL, 0, 4);
        SendMessage(hUserCount2, WM_CLEAR, 0, 0);
        SendMessage(hUserCount2, EM_REPLACESEL, FALSE, (LPARAM)count2);
    }
}

void selectedUser(char* selectedItem, int index) {
    char username[20];

    char* ptr = strtok(selectedItem, "\t"); // ����� �г��� ����
    strncpy(username, ptr, 20);

    SendMessage(hUserNames, EM_SETSEL, 0, 20);
    SendMessage(hUserNames, WM_CLEAR, 0, 0);
    SendMessage(hUserNames, EM_REPLACESEL, FALSE, (LPARAM)username);
}

void updateUserList(int port) {
    if (port == SERVERPORTCHAT) { // ĢĢ����
        char* ipaddr = (char*)malloc(INET_ADDRSTRLEN);
        DWORD ipaddrLen = INET_ADDRSTRLEN;
        char* listupMsg = (char*)malloc(256);
        int i = 1;

        SOCKETINFO_ALL* ptr = AllSocketInfoArray;
        SendMessage(hUserList, LB_RESETCONTENT, 0, 0);

        while (ptr != NULL) { // ����ڸ� ����Ʈ �ڽ��� �߰��ϴ� �κ�
            if (ptr->isIPv6 == false) {
                WSAAddressToString((SOCKADDR*)ptr->sockaddrv4, sizeof(*ptr->sockaddrv4), NULL, ipaddr, &ipaddrLen);
                strncpy(listupMsg, getlist("%s\t %s \t%s\t(%d)\r", ptr->username, ipaddr, ptr->socktype, ptr->userseqNum), 256);
            }
            SendMessage(hUserList, LB_ADDSTRING, 0, (LPARAM)listupMsg);
            ptr = ptr->next;
        }
    }
    else if (port == SERVERPORTDRAW) { // N�ױ׸���
        char* ipaddr = (char*)malloc(INET_ADDRSTRLEN);
        DWORD ipaddrLen = INET_ADDRSTRLEN;
        char* listupMsg = (char*)malloc(256);
        int i = 1;

        SOCKETINFO_ALL* ptr2 = AllSocketInfoArray2;
        SendMessage(hUserList2, LB_RESETCONTENT, 0, 0);

        while (ptr2 != NULL) { // ����ڸ� ����Ʈ �ڽ��� �߰��ϴ� �κ�
            if (ptr2->isIPv6 == false) {
                WSAAddressToString((SOCKADDR*)ptr2->sockaddrv4, sizeof(*ptr2->sockaddrv4), NULL, ipaddr, &ipaddrLen);
                strncpy(listupMsg, getlist("%s\t %s \t%s\t(%d)\r", ptr2->username, ipaddr, ptr2->socktype, ptr2->userseqNum), 256);
            }
            SendMessage(hUserList2, LB_ADDSTRING, 0, (LPARAM)listupMsg);
            ptr2 = ptr2->next;
        }
    }
}

void DisplayText_Send(char* fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);

    char cbuf[1024];
    vsprintf_s(cbuf, fmt, arg);

    // ���� �ؽ�Ʈ ���� �޾ƿ� ��Ʈ�ѿ� �߰��ϴ� �κ�
    int nLength = GetWindowTextLength(hEditUserChat);
    SendMessage(hEditUserChat, EM_SETSEL, nLength, nLength);
    SendMessage(hEditUserChat, EM_REPLACESEL, FALSE, (LPARAM)cbuf);
    va_end(arg);
}

void DisplayTextDraw_Send(char* fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);

    char cbuf[1024];
    vsprintf_s(cbuf, fmt, arg);

    // ���� �ؽ�Ʈ ���� �޾ƿ� ��Ʈ�ѿ� �߰��ϴ� �κ�
    int nLength = GetWindowTextLength(hEditUserChat2);
    SendMessage(hEditUserChat2, EM_SETSEL, nLength, nLength);
    SendMessage(hEditUserChat2, EM_REPLACESEL, FALSE, (LPARAM)cbuf);
    va_end(arg);
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
    DisplayText_Send("[%s] %s", msg, (char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}