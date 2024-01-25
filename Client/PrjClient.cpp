#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string> 
#include <math.h>
#include <time.h>
#include <commdlg.h>
#include "resource.h"

#define WM_DRAWIT  (WM_USER+1)

// ���� �ּ�
#define SERVERIPV4  "127.0.0.1"	// TCP ����
#define MULTICAST "239.255.255.250" // UDP ����
#define MULTICASTSERVER "239.255.255.254" // UDP ���� 

// ��Ʈ ��ȣ ���� 
#define SERVERPORTCHAT  9000 // ĢĢ���� 
#define REMOTEPORTCHAT	9000 // ĢĢ����

#define	SERVERPORTDRAW	9010	//N�ױ׸���
#define REMOTEPORTDRAW	9010	//N�ױ׸���

#define BUFSIZE     284                 
#define MSGSIZE     (BUFSIZE-(sizeof(int)*2)-NICKSIZE-CHECK_WHO) // �����ϰ��� �ϴ� �޼��� ����
#define NICKSIZE    30
#define CHECK_WHO   1 // ���� ������ �˾Ƴ��� ���� �߰���

#define CHATTING		1000    
#define USERCONNECT	    1010 // ����� ������ Ȯ���ϱ� ����
#define USEREXIT		1011 // ����� ���� ������ ����
#define SELFOUT			1012 // ����ڰ� ������ ��ư�� ���� ��� 
#define DRAWLINE	    1013 // �޽��� Ÿ��: �� �׸���
#define NOTICE			1014 // ������ ��������   
#define USERWORD		1015 // ���þ� 

struct COMM_MSG {
	int  type;
	char dummy[BUFSIZE - 4];
};

struct CHAT_MSG {
	int  type;
	char nickname[NICKSIZE];
	char buf[MSGSIZE];
	int whoSent;
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

// ���� - ĢĢ����
static SOCKET        g_sock; //  TCP
static SOCKET	     listen_sock_UDP; // UDP
static SOCKET	     send_sock_UDP; // UDP
SOCKADDR_IN			 remoteaddr_v4;

// ���� - N�ױ׸���
static SOCKET        g_sockDraw; //  TCP
static SOCKET	     listen_sockDraw_UDP; // UDP
static SOCKET	     send_sockDraw_UDP; // UDP
SOCKADDR_IN			 remoteaddr_Draw;

static char          g_ipaddr[64];					// ���� IP �ּ�
static u_short       g_port;						// ���� ��Ʈ ��ȣ
static BOOL          g_isIPv6;						// IPv4 or IPv6 �ּ�?
static BOOL			 g_isUDP;						// üũ�ϸ� UDP, �ƴϸ� �׳� TCP
static BOOL			 g_isGame1;						// üũ�ϸ� ĢĢ����
static BOOL			 g_isGame2;						// üũ�ϸ� N�ױ׸���
static HWND			 g_hButtonExit;					// ������ ��ư 

// ������ �ڵ� - ĢĢ����
static HANDLE        g_hClientThread; // ĢĢ���� ������ 
static HANDLE        g_hReadEvent, g_hWriteEvent;	// �̺�Ʈ �ڵ�
static volatile BOOL g_bStart;						// ��� ���� ����

// ������ �ڵ� - N�ױ׸���
static HANDLE		 g_hClientDrawThread; // N�ױ׸��� ������ 
static HANDLE        g_hReadDrawEvent, g_hWriteDrawEvent;	// �̺�Ʈ �ڵ�
static volatile BOOL g_bStartDraw;						// ��� ���� ����

// ������ ���� ����
static HINSTANCE     g_hInst;						// ���� ���α׷� �ν��Ͻ� �ڵ�
static HWND          g_hDrawWnd;					// �׸��� ������
static HWND          g_hButtonSendMsg;				// '�޽��� ����' ��ư
static HWND			 g_hEditSend;					// ���� �޽��� ���
//static HWND			 g_hEditRecv;					// ���� �޽��� ��� -> ���� �޽��� ��� (����)
static HWND			 hEditUserID;					// ����� ID
static CHAT_MSG      g_chatmsg;						// �⺻ ä�ø޼��� �������� ����
static DRAWLINE_MSG  g_drawlinemsg;						// �׸� ���� �޼��� �������� ����
static int           g_drawcolor;					// �� ����
static int			 g_button; //a,b ���� ��ư
static int			 userSeqNum;	// ����� �ĺ� ��ȣ
static char			 strUniqueID[5] = { 0,0,0,0,0 };

static int			g_combobox;
static int           figure_type; // ���õ� ���� Ÿ��

static HBITMAP		 hBitmap;

static BOOL			onEraser;
static int			 prevFigure;  //���찳 �Ѱ� �� �� ��� ����
static int			 prevColor;	  //���찳 �Ѱ� �� �� ���� ����

static int			 line_thickness = 5; //�� �β� �����ϱ�
static int			 remaining = 5;			// ���� ȹ ��
static int			 total_number = 5;	//��ü ȹ ��
static HWND			 hPrintRemaining;	// ȹ �� ǥ���� �ؽ�Ʈ�ڽ�


HDC hDC;
int cx, cy;
static HDC hDCMem;
char strRemain[15] = "n / m";			//���� ȹ �� ǥ���� ���ڿ� ������ �Լ�
void printRemain(HWND hWnd, int remaining);
char s[255] = "";
char workChainGame[100] = "aa";

static HBRUSH g_brush;
static CHAT_MSG		g_initmsg;	// ����� �г��� ����

// TCP ���� ��� ������
DWORD WINAPI ClientMain(LPVOID arg); // ĢĢ����
DWORD WINAPI ReadThread(LPVOID arg);// ĢĢ����
DWORD WINAPI WriteThread(LPVOID arg);// ĢĢ����
////////////////////////////////////////////////////
DWORD WINAPI ClientDrawMain(LPVOID arg); // N�ױ׸��� 
DWORD WINAPI ReadDrawThread(LPVOID arg); // N�ױ׸��� 
DWORD WINAPI WriteDrawThread(LPVOID arg); // N�ױ׸��� 
int recvn(SOCKET s, char* buf, int len, int flags);

// UDP ���� ��� ������
DWORD WINAPI WriteThread_UDP(LPVOID);
DWORD WINAPI ReadThread_UDP(LPVOID);
DWORD WINAPI ClientMainUDP(LPVOID);  // ĢĢ����
////////////////////////////////////////////////////
DWORD WINAPI WriteThreadDraw_UDP(LPVOID); // N�ױ׸��� 
DWORD WINAPI ReadThreadDraw_UDP(LPVOID); // N�ױ׸��� 
DWORD WINAPI ClientDrawMainUDP(LPVOID arg); // N�ױ׸���

//  Ŭ���̾�Ʈ GUI 
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);		// ��ȭ���� ���ν���
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);	// �ڽ� ������ ���ν���

void printRemain(HWND hWnd, int remaining);
void SaveBitmapToFile(const char* filename);
void DisplayText(char* fmt, ...); // ����Ʈ ��Ʈ�� ������Ʈ 

// ���� ��� �Լ�
void err_quit(char* msg);
void err_display(char* msg);

// �ӼӸ� ������� �г����� �����ϴ� ����
char g_whisperTarget[256] = "";

// ���� �Լ�
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	srand(time(NULL));
	userSeqNum = (rand() * 19 - 19) % 256;
	itoa(userSeqNum, strUniqueID, 10);

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

	// �̺�Ʈ �ڵ� ����
	g_hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (g_hReadEvent == NULL) return 1;
	g_hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (g_hWriteEvent == NULL) return 1;

	// �̺�Ʈ �ڵ� ����
	g_hReadDrawEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (g_hReadDrawEvent == NULL) return 1;
	g_hWriteDrawEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (g_hWriteDrawEvent == NULL) return 1;

	// �������� ���� �ʱ�ȭ
	g_chatmsg.type = CHATTING;
	g_drawlinemsg.type = DRAWLINE;
	g_drawlinemsg.color = RGB(0, 0, 0);
	g_initmsg.type = USERCONNECT;    // ����� ���� 
	strncpy(g_initmsg.buf, "���� ���� ! ", MSGSIZE);

	g_chatmsg.whoSent = g_drawlinemsg.whoSent = g_initmsg.whoSent = userSeqNum;

	// ��ȭ���� �ν��Ͻ� ����
	g_hInst = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// �̺�Ʈ ����
	CloseHandle(g_hReadEvent);
	CloseHandle(g_hWriteEvent);
	// �̺�Ʈ ����
	CloseHandle(g_hReadDrawEvent);
	CloseHandle(g_hWriteDrawEvent);
	// ���� ����
	WSACleanup();
	return 0;
}

// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hButtonIsIPv6;
	static HWND	hUDPCheck;
	static HWND hGame1Check;		// ĢĢ���� ���� ���� CheckBox
	static HWND hGame2Check;		// N�ױ׸��� ���� ���� CheckBox
	static HWND hEditIPaddr;
	static HWND hEditPort;
	static HWND hButtonConnect;
	static HWND hEditMsg;
	static HWND hUserUniqueID;	// ����� �ĺ�
	static HWND hButtonSendMsg;

	static HWND hColorRed;
	static HWND hColorGreen;
	static HWND hColorBlue;
	static HWND hColorYellow;
	static HWND hColorBlack;
	static HBRUSH hDlgBrush = CreateSolidBrush(RGB(173, 216, 230));  // RGB(135, 206, 250)�� �ϴû��Դϴ�.

	//�׸��� ���� �ڵ鷯
	static HWND hEraser;
	static HWND hThicknessHigh;
	static HWND hThicknessMiddle;
	static HWND hThicknessLow;
	static HWND hComboBox;
	static HWND hReset;
	static HWND hDrawSave;		//�׸��� ����
	//���⼭ ����ϴ°� �߰� 

	switch (uMsg) {
	case WM_INITDIALOG:
		// ��Ʈ�� �ڵ� ���
		hButtonIsIPv6 = GetDlgItem(hDlg, IDC_ISIPV6);
		hEditIPaddr = GetDlgItem(hDlg, IDC_IPADDR);
		hEditPort = GetDlgItem(hDlg, IDC_PORT);
		hButtonConnect = GetDlgItem(hDlg, IDC_CONNECT);			// ���� Btn
		g_hButtonSendMsg = GetDlgItem(hDlg, IDC_SENDMSG);		// ���� Btn
		hEditMsg = GetDlgItem(hDlg, IDC_CHATMSG);
		hEditUserID = GetDlgItem(hDlg, IDC_ID);				// ClientID EditControll
		hUDPCheck = GetDlgItem(hDlg, IDC_CHECKUDP);				// UDP ���ӿ���
		hGame1Check = GetDlgItem(hDlg, IDC_CHECK1);				// ĢĢ���� ���� ����
		hGame2Check = GetDlgItem(hDlg, IDC_CHECK2);				// N�ױ׸��� ���� ����
		hUserUniqueID = GetDlgItem(hDlg, IDC_UNIQUEID);			// ����� �ĺ� 
		g_hEditSend = GetDlgItem(hDlg, IDC_STATUS);				// ä��â 
		g_hButtonExit = GetDlgItem(hDlg, IDC_EXIT);				// ������ ��ư ���� 
		hDrawSave = GetDlgItem(hDlg, IDC_DRAWSAVE);				// �׸��� ���� ��ư 

		hColorRed = GetDlgItem(hDlg, IDC_COLORRED);
		hColorGreen = GetDlgItem(hDlg, IDC_COLORGREEN);
		hColorBlue = GetDlgItem(hDlg, IDC_COLORBLUE);
		hColorYellow = GetDlgItem(hDlg, IDC_COLORYELLOW);
		hColorBlack = GetDlgItem(hDlg, IDC_COLORBLACK);
		hEraser = GetDlgItem(hDlg, IDC_ERASER);
		hThicknessHigh = GetDlgItem(hDlg, IDC_PEN1);
		hThicknessMiddle = GetDlgItem(hDlg, IDC_PEN2);
		hThicknessLow = GetDlgItem(hDlg, IDC_PEN3);
		hComboBox = GetDlgItem(hDlg, IDC_SHAPES);
		hPrintRemaining = GetDlgItem(hDlg, IDC_time);
		hReset = GetDlgItem(hDlg, IDC_RESET);

		// ��Ʈ�� �ʱ�ȭ
		SendMessage(hEditMsg, EM_SETLIMITTEXT, MSGSIZE, 0);
		SendMessage(hEditUserID, EM_SETLIMITTEXT, NICKSIZE - 1, 0);
		EnableWindow(g_hButtonSendMsg, FALSE);

		SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
		SetDlgItemInt(hDlg, IDC_PORT, NULL, FALSE);
		SetDlgItemText(hDlg, IDC_UNIQUEID, strUniqueID);

		SendMessage(hColorBlack, BM_SETCHECK, BST_CHECKED, 0);

		// ������ Ŭ���� ���
		WNDCLASS wndclass;
		wndclass.style = CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc = WndProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = g_hInst;
		wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = "MyWndClass";
		if (!RegisterClass(&wndclass)) return 1;

		// �ڽ� ������ ����
		g_hDrawWnd = CreateWindow("MyWndClass", "�׸� �׸� ������", WS_CHILD,
			450, 100, 450, 350, hDlg, (HMENU)NULL, g_hInst, NULL);
		if (g_hDrawWnd == NULL) return 1;
		ShowWindow(g_hDrawWnd, SW_SHOW);
		UpdateWindow(g_hDrawWnd);

		{
			// �޺��ڽ� �ڵ� ���
			hComboBox = GetDlgItem(hDlg, IDC_SHAPES);
			// �޺��ڽ��� �� ����� �迭�� �ۼ�
			const char* shapeNames[] = { "Line", "Square", "Circle", "Triangle" };

			// �������� �߰��� ����
			// for (int i = 0; i < 3; ++i) {
			for (int i = 3; i >= 0; --i) {
				SendMessage(hComboBox, CB_INSERTSTRING, 0, (LPARAM)shapeNames[i]);
			}
			// ù ��° �׸��� �⺻ �������� ����
			SendMessage(hComboBox, CB_SETCURSEL, 0, 0);
			return TRUE;
		}
		return TRUE;

		// �߰��� �κ� ����
	case WM_CTLCOLORDLG:
		return (INT_PTR)hDlgBrush;

	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		SetBkMode(hdcStatic, TRANSPARENT);
		/*		SetTextColor(hdcStatic, RGB(0, 0, 255));*/  // �ؽ�Ʈ ������ �Ķ������� ����
		return (INT_PTR)hDlgBrush;
	}
	break;
	// �߰��� �κ� ��

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ISIPV6:
			g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);
			g_isUDP = SendMessage(hUDPCheck, BM_GETCHECK, 0, 0);
			g_isGame1 = SendMessage(hGame1Check, BM_GETCHECK, 0, 0);
			g_isGame2 = SendMessage(hGame2Check, BM_GETCHECK, 0, 0);

			if (g_isUDP == false) { // TCP 
				if (g_isIPv6 == false)
					SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
			}
			else { // UDP
				if (g_isIPv6 == false)
					SetDlgItemText(hDlg, IDC_IPADDR, MULTICASTSERVER);
			}
			return TRUE;
		case IDC_CHECKUDP: // UDP 
			g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);
			g_isUDP = SendMessage(hUDPCheck, BM_GETCHECK, 0, 0);
			g_isGame1 = SendMessage(hGame1Check, BM_GETCHECK, 0, 0);
			g_isGame2 = SendMessage(hGame2Check, BM_GETCHECK, 0, 0);
			if (g_isUDP == true) { // UDP 
				if (g_isIPv6 == false)
					SetDlgItemText(hDlg, IDC_IPADDR, MULTICASTSERVER);
			}
			else { // TCP
				if (g_isIPv6 == false)
					SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
			}
			return TRUE;
		case IDC_CHECK1: // ĢĢ����
			g_port = SERVERPORTCHAT; // ĢĢ���� ��Ʈ ��ȣ 
			g_isUDP = SendMessage(hUDPCheck, BM_GETCHECK, 0, 0);
			g_isGame1 = SendMessage(hGame1Check, BM_GETCHECK, 0, 0);
			g_isGame2 = SendMessage(hGame2Check, BM_GETCHECK, 0, 0);
			if (g_isUDP == true && g_isGame2 == false) { // UDP üũ
				if (g_isIPv6 == false)
					SetDlgItemText(hDlg, IDC_IPADDR, MULTICASTSERVER);
			}
			else if (g_isUDP == false && g_isGame2 == false) { // tcp
				if (g_isIPv6 == false)
					SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
			}
			SetDlgItemInt(hDlg, IDC_PORT, g_port, FALSE);
			return TRUE;
		case IDC_CHECK2: // N�ױ׸���
			g_port = SERVERPORTDRAW;
			g_isUDP = SendMessage(hUDPCheck, BM_GETCHECK, 0, 0);
			g_isGame1 = SendMessage(hGame1Check, BM_GETCHECK, 0, 0);
			g_isGame2 = SendMessage(hGame2Check, BM_GETCHECK, 0, 0);
			if (g_isUDP == true && g_isGame1 == false) { // UDP üũ
				if (g_isIPv6 == false)
					SetDlgItemText(hDlg, IDC_IPADDR, MULTICASTSERVER);
			}
			else if (g_isUDP == false && g_isGame1 == false) { // tcp 
				if (g_isIPv6 == false)
					SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
			}
			SetDlgItemInt(hDlg, IDC_PORT, g_port, FALSE);
			return TRUE;
		case IDC_CONNECT:
			if (GetDlgItemText(hDlg, IDC_ID, (LPSTR)g_chatmsg.nickname, NICKSIZE) != NULL) {
				g_isUDP = SendMessage(hUDPCheck, BM_GETCHECK, 0, 0);
				g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);
				g_isGame1 = SendMessage(hGame1Check, BM_GETCHECK, 0, 0);
				g_isGame2 = SendMessage(hGame2Check, BM_GETCHECK, 0, 0);
				GetDlgItemText(hDlg, IDC_ID, (LPSTR)g_initmsg.nickname, NICKSIZE);
				g_port = GetDlgItemInt(hDlg, IDC_PORT, NULL, FALSE);

				if (g_port == SERVERPORTCHAT) { // ĢĢ����
					if (g_isUDP == false) { // TCP ����
						GetDlgItemText(hDlg, IDC_IPADDR, g_ipaddr, sizeof(g_ipaddr));
						g_hClientThread = CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);

					}
					else { // UDP ����
						g_hClientThread = CreateThread(NULL, 0, ClientMainUDP, NULL, 0, NULL);
					}

					if (g_hClientThread == NULL) {
						MessageBox(hDlg, "UDP Ŭ���̾�Ʈ�� ������ �� �����ϴ�."
							"\r\n���α׷��� �����մϴ�.", "����!", MB_ICONERROR);
						EndDialog(hDlg, 0);
					}
					else {
						EnableWindow(hButtonConnect, FALSE);
						while (g_bStart == FALSE); // ���� ���� ���� ��ٸ�
						EnableWindow(hButtonIsIPv6, FALSE);
						EnableWindow(hEditIPaddr, FALSE);
						EnableWindow(hEditPort, FALSE);
						EnableWindow(g_hButtonSendMsg, TRUE);
						EnableWindow(hUDPCheck, FALSE);
						EnableWindow(hGame1Check, FALSE);
						EnableWindow(hGame2Check, FALSE);
						SetFocus(hEditMsg);
					}
					return TRUE;
				}
				else if (g_port == SERVERPORTDRAW) { // N�ױ׸��� 
					if (g_isUDP == false) { // TCP ����
						GetDlgItemText(hDlg, IDC_IPADDR, g_ipaddr, sizeof(g_ipaddr));
						g_hClientDrawThread = CreateThread(NULL, 0, ClientDrawMain, NULL, 0, NULL);

					}
					else { // UDP ����
						g_hClientDrawThread = CreateThread(NULL, 0, ClientDrawMainUDP, NULL, 0, NULL);
					}

					if (g_hClientDrawThread == NULL) {
						MessageBox(hDlg, "UDP Ŭ���̾�Ʈ�� ������ �� �����ϴ�."
							"\r\n���α׷��� �����մϴ�.", "����!", MB_ICONERROR);
						EndDialog(hDlg, 0);
					}
					else {
						EnableWindow(hButtonConnect, FALSE);
						while (g_bStartDraw == FALSE); // ���� ���� ���� ��ٸ�
						EnableWindow(hButtonIsIPv6, FALSE);
						EnableWindow(hEditIPaddr, FALSE);
						EnableWindow(hEditPort, FALSE);
						EnableWindow(g_hButtonSendMsg, TRUE);
						EnableWindow(g_hButtonExit, TRUE);
						EnableWindow(hUDPCheck, FALSE);
						EnableWindow(hGame1Check, FALSE);
						EnableWindow(hGame2Check, FALSE);
						SetFocus(hEditMsg);
					}
					return TRUE;
				}
			}
			return TRUE;

		case IDC_SENDMSG:
			if (g_port == SERVERPORTCHAT) {
				// �б� �ϷḦ ��ٸ�
				WaitForSingleObject(g_hReadEvent, INFINITE);
				GetDlgItemText(hDlg, IDC_CHATMSG, g_chatmsg.buf, MSGSIZE);

				// �ӼӸ� üũ
				if (g_chatmsg.buf[0] == '@') {
					// �ӼӸ� ����� ����
					char targetNickname[256];
					sscanf(g_chatmsg.buf, "@%s", targetNickname);
					strcpy(g_whisperTarget, targetNickname);

					// ä�� �޽������� �ӼӸ� ����� ����
					memmove(g_chatmsg.buf, g_chatmsg.buf + strlen(targetNickname) + 1, strlen(g_chatmsg.buf));
				}
				// ���� �ϷḦ �˸�
				SetEvent(g_hWriteEvent);
				// �Էµ� �ؽ�Ʈ ��ü�� ���� ǥ��
				SendMessage(hEditMsg, EM_SETSEL, 0, -1);
			}
			else if (g_port == SERVERPORTDRAW) {
				// �б� �ϷḦ ��ٸ�
				WaitForSingleObject(g_hReadDrawEvent, INFINITE);
				GetDlgItemText(hDlg, IDC_CHATMSG, g_chatmsg.buf, MSGSIZE);

				// �ӼӸ� üũ
				if (g_chatmsg.buf[0] == '@') {
					// �ӼӸ� ����� ����
					char targetNickname[256];
					sscanf(g_chatmsg.buf, "@%s", targetNickname);
					strcpy(g_whisperTarget, targetNickname);

					// ä�� �޽������� �ӼӸ� ����� ����
					memmove(g_chatmsg.buf, g_chatmsg.buf + strlen(targetNickname) + 1, strlen(g_chatmsg.buf));
				}
				// ���� �ϷḦ �˸�
				SetEvent(g_hWriteDrawEvent);
				// �Էµ� �ؽ�Ʈ ��ü�� ���� ǥ��
				SendMessage(hEditMsg, EM_SETSEL, 0, -1);
			}
			return TRUE;
		case IDC_EXIT:
			// ������ ����� ���� �޽����� �����ϴ�.
			COMM_MSG exitMsg;
			exitMsg.type = SELFOUT;
			if (g_port == SERVERPORTCHAT) { // ĢĢ����
				send(g_sock, (char*)&exitMsg, sizeof(exitMsg), 0);
				closesocket(g_sock);
			}
			else if (g_port == SERVERPORTDRAW) { // N�ױ׸���
				send(g_sockDraw, (char*)&exitMsg, sizeof(exitMsg), 0);
				closesocket(g_sockDraw);
			}

			if (g_hClientThread) {
				TerminateThread(g_hClientThread, 0);
				CloseHandle(g_hClientThread);
				g_hClientThread = NULL;
			}
			if (g_hClientDrawThread) {
				TerminateThread(g_hClientDrawThread, 0);
				CloseHandle(g_hClientDrawThread);
				g_hClientDrawThread = NULL;
			}
			EnableWindow(hButtonConnect, TRUE);
			EnableWindow(hButtonIsIPv6, TRUE);
			EnableWindow(hEditIPaddr, TRUE);
			EnableWindow(hEditPort, TRUE);
			EnableWindow(g_hButtonSendMsg, FALSE);
			EnableWindow(hUDPCheck, TRUE);
			EnableWindow(hGame1Check, TRUE);
			EnableWindow(hGame2Check, TRUE);
			return TRUE;
		case IDC_COLORRED:
			g_drawlinemsg.color = RGB(255, 0, 0);
			return TRUE;

		case IDC_COLORGREEN:
			g_drawlinemsg.color = RGB(0, 255, 0);
			return TRUE;

		case IDC_COLORBLUE:
			g_drawlinemsg.color = RGB(0, 0, 255);
			return TRUE;

		case IDC_COLORYELLOW:
			g_drawlinemsg.color = RGB(255, 255, 0);
			return TRUE;

		case IDC_COLORBLACK:
			g_drawlinemsg.color = RGB(0, 0, 0);
			return TRUE;

		case IDCANCEL:
			if (MessageBox(hDlg, "������ �����Ͻðڽ��ϱ�?",
				"����", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				closesocket(g_sock);
				EndDialog(hDlg, IDCANCEL);
			}
			return TRUE;
		case IDC_ERASER:
			if (onEraser == FALSE)
			{
				onEraser = TRUE;
				prevColor = g_drawlinemsg.color;
				g_drawlinemsg.color = RGB(255, 255, 255);
				prevFigure = figure_type;
				figure_type = 0;
			}
			else
			{
				onEraser = FALSE;
				g_drawlinemsg.color = prevColor;
				figure_type = prevFigure;
			}
			return TRUE;

		case IDC_RESET:
			remaining = total_number;
			hDC = GetDC(g_hDrawWnd);
			RECT rect;
			GetClientRect(g_hDrawWnd, &rect);
			FillRect(hDC, &rect, (HBRUSH)(COLOR_WINDOW + 1));
			ReleaseDC(g_hDrawWnd, hDC);
			printRemain(hPrintRemaining, remaining);

		case IDC_SHAPES:
			switch (HIWORD(wParam)) {
			case CBN_SELCHANGE:
				// �޺� �ڽ����� ������ ����Ǿ��� ���� ó��
				int selectedIndex = SendMessage(GetDlgItem(hDlg, IDC_SHAPES), CB_GETCURSEL, 0, 0);
				// ����: ���õ� �׸� ���� Ư�� ���� ����
				switch (selectedIndex) {
				case 0:

					MessageBox(hDlg, "Line ����", "�˸�", MB_OK);
					figure_type = 0; \
						break;
				case 1:
					MessageBox(hDlg, "Square ����", "�˸�", MB_OK);
					figure_type = 1;
					break;
				case 2:
					MessageBox(hDlg, "Circle ����", "�˸�", MB_OK);
					figure_type = 2;
					break;
				case 3:
					MessageBox(hDlg, "Triangle ����", "�˸�", MB_OK);
					figure_type = 3;
					break;
				}
				g_drawlinemsg.figure_type = figure_type;
				return TRUE;
			}
			return TRUE;
			//���� ũ�� �����ϱ� - ����ȭ �ʿ�
		case IDC_PEN1:
			line_thickness = 20;
			return TRUE;
		case IDC_PEN2:
			line_thickness = 10;
			return TRUE;
		case IDC_PEN3:
			line_thickness = 5;
			return TRUE;

		}
	}
	return FALSE;
}

// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;

	if (g_isIPv6 == false) {
		// socket()
		g_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (g_sock == INVALID_SOCKET) err_quit("socket()");

		// connect()
		SOCKADDR_IN serveraddr;
		ZeroMemory(&serveraddr, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_addr.s_addr = inet_addr(g_ipaddr);
		serveraddr.sin_port = htons(g_port);
		retval = connect(g_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
		if (retval == SOCKET_ERROR) err_quit("connect()");
	}
	retval = send(g_sock, (char*)&g_initmsg, BUFSIZE, 0);
	MessageBox(NULL, "TCP �����߽��ϴ�.", "����!", MB_ICONINFORMATION);

	// �б� & ���� ������ ����
	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, ReadThread, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, WriteThread, NULL, 0, NULL);
	if (hThread[0] == NULL || hThread[1] == NULL) {
		MessageBox(NULL, "�����带 ������ �� �����ϴ�."
			"\r\n���α׷��� �����մϴ�.",
			"����!", MB_ICONERROR);
		exit(1);
	}
	g_bStart = TRUE;
	// ������ ���� ���
	retval = WaitForMultipleObjects(2, hThread, FALSE, INFINITE);
	retval -= WAIT_OBJECT_0;
	if (retval == 0)
		TerminateThread(hThread[1], 1);
	else
		TerminateThread(hThread[0], 1);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	g_bStart = FALSE;
	MessageBox(NULL, "������ ������ �������ϴ�", "�˸�", MB_ICONINFORMATION);
	EnableWindow(g_hButtonSendMsg, FALSE);

	closesocket(g_sock);
	return 0;
}

// ���� ��� ������ �Լ�
DWORD WINAPI ClientDrawMain(LPVOID arg)
{
	int retvalDraw;

	if (g_isIPv6 == false) {
		// socket()
		g_sockDraw = socket(AF_INET, SOCK_STREAM, 0);
		if (g_sockDraw == INVALID_SOCKET) err_quit("socket()");

		// connect()
		SOCKADDR_IN serveraddrDraw;
		ZeroMemory(&serveraddrDraw, sizeof(serveraddrDraw));
		serveraddrDraw.sin_family = AF_INET;
		serveraddrDraw.sin_addr.s_addr = inet_addr(g_ipaddr);
		serveraddrDraw.sin_port = htons(g_port);
		retvalDraw = connect(g_sockDraw, (SOCKADDR*)&serveraddrDraw, sizeof(serveraddrDraw));
		if (retvalDraw == SOCKET_ERROR) err_quit("connect()");
	}
	retvalDraw = send(g_sockDraw, (char*)&g_initmsg, BUFSIZE, 0);
	MessageBox(NULL, "TCP �����߽��ϴ�.", "����!", MB_ICONINFORMATION);

	// �б� & ���� ������ ����
	HANDLE hThreadDraw[2];
	hThreadDraw[0] = CreateThread(NULL, 0, ReadDrawThread, NULL, 0, NULL);
	hThreadDraw[1] = CreateThread(NULL, 0, WriteDrawThread, NULL, 0, NULL);
	if (hThreadDraw[0] == NULL || hThreadDraw[1] == NULL) {
		MessageBox(NULL, "�����带 ������ �� �����ϴ�."
			"\r\n���α׷��� �����մϴ�.",
			"����!", MB_ICONERROR);
		exit(1);
	}
	g_bStartDraw = TRUE;
	// ������ ���� ���
	retvalDraw = WaitForMultipleObjects(2, hThreadDraw, FALSE, INFINITE);
	retvalDraw -= WAIT_OBJECT_0;
	if (retvalDraw == 0)
		TerminateThread(hThreadDraw[1], 1);
	else
		TerminateThread(hThreadDraw[0], 1);
	CloseHandle(hThreadDraw[0]);
	CloseHandle(hThreadDraw[1]);

	g_bStartDraw = FALSE;
	MessageBox(NULL, "������ ������ �������ϴ�", "�˸�", MB_ICONINFORMATION);
	EnableWindow(g_hButtonSendMsg, FALSE);

	closesocket(g_sockDraw);
	return 0;
}

// ������ �ޱ�
DWORD WINAPI ReadThread(LPVOID arg)
{
	int retval;
	COMM_MSG comm_msg;
	CHAT_MSG* chat_msg;
	DRAWLINE_MSG* draw_msg;

	while (1) {
		retval = recvn(g_sock, (char*)&comm_msg, BUFSIZE, 0);
		if (retval == 0 || retval == SOCKET_ERROR || comm_msg.type == USEREXIT) {
			chat_msg == (CHAT_MSG*)&comm_msg;
			if (!strcmp(chat_msg->nickname, g_chatmsg.nickname) && chat_msg->whoSent == g_chatmsg.whoSent)
				break;
			else
				continue;
		}
		if (comm_msg.type == CHATTING) {
			chat_msg = (CHAT_MSG*)&comm_msg;


			if (chat_msg->buf[0] != workChainGame[strlen(workChainGame) - 1]) {
				DisplayText("[%s] �����ձ� ���� ! \r\n�Է��� �ܾ� : %s \r\n\r\n -------���þ� : %s------- \r\n", chat_msg->nickname, chat_msg->buf, workChainGame);
			}
			else {
				strcpy(workChainGame, chat_msg->buf);
				DisplayText("[%s] �����ձ� ���� !  \r\n�Է��� �ܾ� : %s \r\n\r\n -------���þ� : %s------- \r\n", chat_msg->nickname, chat_msg->buf, workChainGame);
			}
		}
		if (comm_msg.type == NOTICE) {
			DisplayText("\r\n");
			CHAT_MSG* notice_msg = (CHAT_MSG*)&comm_msg;
			DisplayText("\n---------[����] ���þ�� %s �Դϴ�-------\n", notice_msg->buf);
			strcpy(workChainGame, notice_msg->buf);
		}



		if (comm_msg.type == DRAWLINE) {
			draw_msg = (DRAWLINE_MSG*)&comm_msg;
			g_drawcolor = draw_msg->color;
			prevFigure = figure_type;
			figure_type = draw_msg->figure_type;
			line_thickness = draw_msg->line_thickness;
			g_drawlinemsg.x0 = draw_msg->x0;
			g_drawlinemsg.y0 = draw_msg->y0;
			g_drawlinemsg.x1 = draw_msg->x1;
			g_drawlinemsg.y1 = draw_msg->y1;
			SendMessage(g_hDrawWnd, WM_DRAWIT,
				MAKEWPARAM(draw_msg->x0, draw_msg->y0),
				MAKELPARAM(draw_msg->x1, draw_msg->y1));
			figure_type = prevFigure;
		}
	}
	return 0;
}

// ������ �ޱ�
DWORD WINAPI ReadDrawThread(LPVOID arg)
{
	int retvalDraw;
	COMM_MSG comm_msg;
	CHAT_MSG* chat_msg;
	DRAWLINE_MSG* draw_msg;
	char ss[100] = "";

	while (1) {
		retvalDraw = recvn(g_sockDraw, (char*)&comm_msg, BUFSIZE, 0);
		if (retvalDraw == 0 || retvalDraw == SOCKET_ERROR || comm_msg.type == USEREXIT) {
			chat_msg == (CHAT_MSG*)&comm_msg;
			if (!strcmp(chat_msg->nickname, g_chatmsg.nickname) && chat_msg->whoSent == g_chatmsg.whoSent)
				break;
			else
				continue;
		}
		if (comm_msg.type == CHATTING) {
			chat_msg = (CHAT_MSG*)&comm_msg;
			if (!strcmp(chat_msg->nickname, g_chatmsg.nickname) && chat_msg->whoSent == g_chatmsg.whoSent) {
				DisplayText("\n[%s] %s\r\n", chat_msg->nickname, chat_msg->buf);
			}
			else {
				DisplayText("[%s] %s\r\n", chat_msg->nickname, chat_msg->buf);
			}
		}
		if (comm_msg.type == USERWORD) {
			CHAT_MSG* userWordMsg = (CHAT_MSG*)&comm_msg;
			DisplayText("\n[���þ�] %s\n", userWordMsg->buf);
			
		}
		if (comm_msg.type == NOTICE) {
			CHAT_MSG* notice_msg = (CHAT_MSG*)&comm_msg;
			DisplayText("\n[����] %s\n", notice_msg->buf);

		}
		if (comm_msg.type == DRAWLINE) {

			draw_msg = (DRAWLINE_MSG*)&comm_msg;
			g_drawcolor = draw_msg->color;
			prevFigure = figure_type;
			figure_type = draw_msg->figure_type;
			line_thickness = draw_msg->line_thickness;
			g_drawlinemsg.x0 = draw_msg->x0;
			g_drawlinemsg.y0 = draw_msg->y0;
			g_drawlinemsg.x1 = draw_msg->x1;
			g_drawlinemsg.y1 = draw_msg->y1;
			SendMessage(g_hDrawWnd, WM_DRAWIT,
				MAKEWPARAM(draw_msg->x0, draw_msg->y0),
				MAKELPARAM(draw_msg->x1, draw_msg->y1));
			figure_type = prevFigure;
		}

	}
	return 0;
}

// ������ ������
DWORD WINAPI WriteThread(LPVOID arg)
{
	int retval;

	// ������ ������ ���
	while (1) {
		// ���� �Ϸ� ��ٸ���
		WaitForSingleObject(g_hWriteEvent, INFINITE);

		// ���ڿ� ���̰� 0�̸� ������ ����
		if (strlen(g_chatmsg.buf) == 0) {
			// '�޽��� ����' ��ư Ȱ��ȭ
			EnableWindow(g_hButtonSendMsg, TRUE);
			// �б� �Ϸ� �˸���
			SetEvent(g_hReadEvent);
			continue;
		}


		// ������ ������
		retval = send(g_sock, (char*)&g_chatmsg, BUFSIZE, 0);
		if (retval == SOCKET_ERROR) {
			break;
		}
		// '�޽��� ����' ��ư Ȱ��ȭ
		EnableWindow(g_hButtonSendMsg, TRUE);
		// �б� �Ϸ� �˸���
		SetEvent(g_hReadEvent);
	}

	return 0;
}

// ������ ������
DWORD WINAPI WriteDrawThread(LPVOID arg)
{
	int retvalDraw;

	// ������ ������ ���
	while (1) {
		// ���� �Ϸ� ��ٸ���
		WaitForSingleObject(g_hWriteDrawEvent, INFINITE);

		// ���ڿ� ���̰� 0�̸� ������ ����
		if (strlen(g_chatmsg.buf) == 0) {
			// '�޽��� ����' ��ư Ȱ��ȭ
			EnableWindow(g_hButtonSendMsg, TRUE);
			// �б� �Ϸ� �˸���
			SetEvent(g_hReadDrawEvent);
			continue;
		}
		// ������ ������
		retvalDraw = send(g_sockDraw, (char*)&g_chatmsg, BUFSIZE, 0);
		if (retvalDraw == SOCKET_ERROR) {
			break;
		}
		// '�޽��� ����' ��ư Ȱ��ȭ
		EnableWindow(g_hButtonSendMsg, TRUE);
		// �б� �Ϸ� �˸���
		SetEvent(g_hReadDrawEvent);
	}
	return 0;
}

// UDP Client ������
DWORD WINAPI ClientMainUDP(LPVOID arg) {
	int retvalUDP;
	HANDLE hThread[2];

	if (g_isIPv6 == false) {
		// socket()
		listen_sock_UDP = socket(AF_INET, SOCK_DGRAM, 0);
		if (listen_sock_UDP == INVALID_SOCKET) err_quit("socket()");

		send_sock_UDP = socket(AF_INET, SOCK_DGRAM, 0);
		if (send_sock_UDP == INVALID_SOCKET) err_quit("socket()");

		hThread[0] = CreateThread(NULL, 0, ReadThread_UDP, NULL, 0, NULL);
		hThread[1] = CreateThread(NULL, 0, WriteThread_UDP, NULL, 0, NULL);
		MessageBox(NULL, "UDP �����߽��ϴ�.", "����!", MB_ICONINFORMATION);
	}

	if (hThread[0] == NULL || hThread[1] == NULL) {
		MessageBox(NULL, "�����带 ������ �� �����ϴ�."
			"\r\n���α׷��� �����մϴ�.",
			"����!", MB_ICONERROR);
		exit(1);
	}

	g_bStart = TRUE;

	// ������ ���� ���
	retvalUDP = WaitForMultipleObjects(2, hThread, FALSE, INFINITE);
	retvalUDP -= WAIT_OBJECT_0;
	if (retvalUDP == 0)
		TerminateThread(hThread[1], 1);
	else
		TerminateThread(hThread[0], 1);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	g_bStart = FALSE;

	MessageBox(NULL, "������ ������ �������ϴ�", "�˸�", MB_ICONINFORMATION);
	EnableWindow(g_hButtonSendMsg, FALSE);

	closesocket(g_sock);
	return 0;
}

// UDP Client ������
DWORD WINAPI ClientDrawMainUDP(LPVOID arg) {
	int retvalDrawUDP;
	HANDLE hThreadDraw[2];

	if (g_isIPv6 == false) {
		// socket()
		listen_sockDraw_UDP = socket(AF_INET, SOCK_DGRAM, 0);
		if (listen_sockDraw_UDP == INVALID_SOCKET) err_quit("socket()");

		send_sockDraw_UDP = socket(AF_INET, SOCK_DGRAM, 0);
		if (send_sockDraw_UDP == INVALID_SOCKET) err_quit("socket()");

		hThreadDraw[0] = CreateThread(NULL, 0, ReadThreadDraw_UDP, NULL, 0, NULL);
		hThreadDraw[1] = CreateThread(NULL, 0, WriteThreadDraw_UDP, NULL, 0, NULL);
		MessageBox(NULL, "UDP �����߽��ϴ�.", "����!", MB_ICONINFORMATION);
	}

	if (hThreadDraw[0] == NULL || hThreadDraw[1] == NULL) {
		MessageBox(NULL, "�����带 ������ �� �����ϴ�."
			"\r\n���α׷��� �����մϴ�.",
			"����!", MB_ICONERROR);
		exit(1);
	}
	g_bStartDraw = TRUE;

	// ������ ���� ���
	retvalDrawUDP = WaitForMultipleObjects(2, hThreadDraw, FALSE, INFINITE);
	retvalDrawUDP -= WAIT_OBJECT_0;
	if (retvalDrawUDP == 0)
		TerminateThread(hThreadDraw[1], 1);
	else
		TerminateThread(hThreadDraw[0], 1);
	CloseHandle(hThreadDraw[0]);
	CloseHandle(hThreadDraw[1]);

	g_bStartDraw = FALSE;

	MessageBox(NULL, "������ ������ �������ϴ�", "�˸�", MB_ICONINFORMATION);
	EnableWindow(g_hButtonSendMsg, FALSE);

	closesocket(g_sockDraw);
	return 0;
}

DWORD WINAPI ReadThread_UDP(LPVOID arg) {
	bool optval = TRUE;
	int retvalUDP = setsockopt(listen_sock_UDP, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
	if (retvalUDP == SOCKET_ERROR) err_quit("setsockopt()");

	// bind()
	SOCKADDR_IN serveraddrUDP;
	ZeroMemory(&serveraddrUDP, sizeof(serveraddrUDP));
	serveraddrUDP.sin_family = AF_INET;
	serveraddrUDP.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddrUDP.sin_port = htons(SERVERPORTCHAT);
	retvalUDP = bind(listen_sock_UDP, (SOCKADDR*)&serveraddrUDP, sizeof(serveraddrUDP));
	if (retvalUDP == SOCKET_ERROR) err_quit("bind()");

	// ��Ƽĳ��Ʈ �׷� ����
	struct ip_mreq mreq_v4;
	mreq_v4.imr_multiaddr.s_addr = inet_addr(MULTICAST);
	mreq_v4.imr_interface.s_addr = htonl(INADDR_ANY);
	retvalUDP = setsockopt(listen_sock_UDP, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq_v4, sizeof(mreq_v4));
	if (retvalUDP == SOCKET_ERROR) {
		err_quit("setsockopt()");
	}

	SOCKADDR_IN peeraddr;
	int addrlen;
	COMM_MSG comm_msg;
	CHAT_MSG* chat_msg;
	DRAWLINE_MSG* draw_msg;

	while (1) {
		addrlen = sizeof(peeraddr);
		retvalUDP = recvfrom(listen_sock_UDP, (char*)&comm_msg, BUFSIZE, 0, (SOCKADDR*)&peeraddr, &addrlen);
		if (retvalUDP == 0 || retvalUDP == SOCKET_ERROR || comm_msg.type == USEREXIT) {
			chat_msg == (CHAT_MSG*)&comm_msg;
			DisplayText("\r\n");
			DisplayText("\n%d %d\r\n", chat_msg->whoSent, g_chatmsg.whoSent);
			if (!strcmp(chat_msg->nickname, g_chatmsg.nickname) && chat_msg->whoSent == g_chatmsg.whoSent)
				break;
			else
				continue;
		}
		if (comm_msg.type == CHATTING) {
			chat_msg = (CHAT_MSG*)&comm_msg;
			if (chat_msg->buf[0] != workChainGame[strlen(workChainGame) - 1]) {
				DisplayText("[%s] �����ձ� ���� ! \r\n�Է��� �ܾ� : %s \r\n\r\n -------���þ� : %s------- \r\n", chat_msg->nickname, chat_msg->buf, workChainGame);
			}
			else {
				strcpy(workChainGame, chat_msg->buf);
				DisplayText("[%s] �����ձ� ���� !  \r\n�Է��� �ܾ� : %s \r\n\r\n -------���þ� : %s------- \r\n", chat_msg->nickname, chat_msg->buf, workChainGame);
			}
		}
		if (comm_msg.type == NOTICE) {
			DisplayText("\r\n");
			CHAT_MSG* notice_msg = (CHAT_MSG*)&comm_msg;
			DisplayText("\n---------[����] ���þ�� %s �Դϴ�-------\n", notice_msg->buf);
			strcpy(workChainGame, notice_msg->buf);
		}
		if (comm_msg.type == DRAWLINE) {
			draw_msg = (DRAWLINE_MSG*)&comm_msg;
			g_drawcolor = draw_msg->color;
			prevFigure = figure_type;
			figure_type = draw_msg->figure_type;
			line_thickness = draw_msg->line_thickness;
			g_drawlinemsg.x0 = draw_msg->x0;
			g_drawlinemsg.y0 = draw_msg->y0;
			g_drawlinemsg.x1 = draw_msg->x1;
			g_drawlinemsg.y1 = draw_msg->y1;
			SendMessage(g_hDrawWnd, WM_DRAWIT,
				MAKEWPARAM(draw_msg->x0, draw_msg->y0),
				MAKELPARAM(draw_msg->x1, draw_msg->y1));
			figure_type = prevFigure;
		}
	}
	// ��Ƽĳ��Ʈ �׷� Ż��
	retvalUDP = setsockopt(listen_sock_UDP, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq_v4, sizeof(mreq_v4));
	if (retvalUDP == SOCKET_ERROR) err_quit("setsockopt()");

	return 0;
}

DWORD WINAPI ReadThreadDraw_UDP(LPVOID arg) {
	bool optval = TRUE;
	int retvalDraw_UDP = setsockopt(listen_sockDraw_UDP, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
	if (retvalDraw_UDP == SOCKET_ERROR) err_quit("setsockopt()");

	// bind()
	SOCKADDR_IN serveraddrDraw_UDP;
	ZeroMemory(&serveraddrDraw_UDP, sizeof(serveraddrDraw_UDP));
	serveraddrDraw_UDP.sin_family = AF_INET;
	serveraddrDraw_UDP.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddrDraw_UDP.sin_port = htons(SERVERPORTDRAW);
	retvalDraw_UDP = bind(listen_sockDraw_UDP, (SOCKADDR*)&serveraddrDraw_UDP, sizeof(serveraddrDraw_UDP));
	if (retvalDraw_UDP == SOCKET_ERROR) err_quit("bind()");

	// ��Ƽĳ��Ʈ �׷� ����
	struct ip_mreq mreq_draw;
	mreq_draw.imr_multiaddr.s_addr = inet_addr(MULTICAST);
	mreq_draw.imr_interface.s_addr = htonl(INADDR_ANY);
	retvalDraw_UDP = setsockopt(listen_sockDraw_UDP, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq_draw, sizeof(mreq_draw));
	if (retvalDraw_UDP == SOCKET_ERROR) {
		err_quit("setsockopt()");
	}

	SOCKADDR_IN peeraddr_draw;
	int addrlen_draw;
	COMM_MSG comm_msg;
	CHAT_MSG* chat_msg;
	DRAWLINE_MSG* draw_msg;

	while (1) {
		addrlen_draw = sizeof(peeraddr_draw);
		retvalDraw_UDP = recvfrom(listen_sockDraw_UDP, (char*)&comm_msg, BUFSIZE, 0, (SOCKADDR*)&peeraddr_draw, &addrlen_draw);
		//DisplayText("%d | ", comm_msg.type);
		if (retvalDraw_UDP == 0 || retvalDraw_UDP == SOCKET_ERROR || comm_msg.type == USEREXIT) {
			chat_msg == (CHAT_MSG*)&comm_msg;
			DisplayText("\r\n");
			DisplayText("%d %d\r\n", chat_msg->whoSent, g_chatmsg.whoSent);
			if (!strcmp(chat_msg->nickname, g_chatmsg.nickname) && chat_msg->whoSent == g_chatmsg.whoSent)
				break;
			else
				continue;
		}
		if (comm_msg.type == CHATTING) {
			DisplayText("\r\n");
			chat_msg = (CHAT_MSG*)&comm_msg;
			if (!strcmp(chat_msg->nickname, g_chatmsg.nickname) && chat_msg->whoSent == g_chatmsg.whoSent) {
				DisplayText("\n[%s] %s\r\n", chat_msg->nickname, chat_msg->buf);
			}
			else {
				DisplayText("\n[%s] %s\r\n", chat_msg->nickname, chat_msg->buf);
			}
		}
		if (comm_msg.type == NOTICE) {
			DisplayText("\r\n");
			CHAT_MSG* notice_msg = (CHAT_MSG*)&comm_msg;
			DisplayText("\n[����] %s\n", notice_msg->buf);

		}
		if (comm_msg.type == USERWORD) {
			CHAT_MSG* userWordMsg = (CHAT_MSG*)&comm_msg;
			DisplayText("\n[���þ�] %s\n", userWordMsg->buf);
		}
		if (comm_msg.type == DRAWLINE) {
			draw_msg = (DRAWLINE_MSG*)&comm_msg;
			g_drawcolor = draw_msg->color;
			prevFigure = figure_type;
			figure_type = draw_msg->figure_type;
			line_thickness = draw_msg->line_thickness;
			g_drawlinemsg.x0 = draw_msg->x0;
			g_drawlinemsg.y0 = draw_msg->y0;
			g_drawlinemsg.x1 = draw_msg->x1;
			g_drawlinemsg.y1 = draw_msg->y1;
			SendMessage(g_hDrawWnd, WM_DRAWIT,
				MAKEWPARAM(draw_msg->x0, draw_msg->y0),
				MAKELPARAM(draw_msg->x1, draw_msg->y1));
			figure_type = prevFigure;
		}
	}
	// ��Ƽĳ��Ʈ �׷� Ż��
	retvalDraw_UDP = setsockopt(listen_sockDraw_UDP, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq_draw, sizeof(mreq_draw));
	if (retvalDraw_UDP == SOCKET_ERROR) err_quit("setsockopt()");

	return 0;
}

DWORD WINAPI WriteThread_UDP(LPVOID arg) {
	int retvalUDP;

	DWORD ttl = 2;
	retvalUDP = setsockopt(send_sock_UDP, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));
	if (retvalUDP == SOCKET_ERROR) {
		err_quit("setsockopt()");
	}

	ZeroMemory(&remoteaddr_v4, sizeof(remoteaddr_v4));
	remoteaddr_v4.sin_family = AF_INET;
	remoteaddr_v4.sin_addr.s_addr = inet_addr(MULTICAST);
	remoteaddr_v4.sin_port = htons(REMOTEPORTCHAT);

	retvalUDP = sendto(send_sock_UDP, (char*)&g_initmsg, BUFSIZE, 0, (SOCKADDR*)&remoteaddr_v4, sizeof(remoteaddr_v4));
	while (1) {
		WaitForSingleObject(g_hWriteEvent, INFINITE);

		if (strlen(g_chatmsg.buf) == 0) {
			EnableWindow(g_hButtonSendMsg, TRUE);
			// �б� �Ϸ� �˸���
			SetEvent(g_hReadEvent);
			continue;
		}
		// ������ ������
		retvalUDP = sendto(send_sock_UDP, (char*)&g_chatmsg, BUFSIZE, 0, (SOCKADDR*)&remoteaddr_v4, sizeof(remoteaddr_v4));
		if (retvalUDP == SOCKET_ERROR) {
			DisplayText("UDP ERROR\r\n");
			break;
		}
		// '�޽��� ����' ��ư Ȱ��ȭ
		EnableWindow(g_hButtonSendMsg, TRUE);
		// �б� �Ϸ� �˸���
		SetEvent(g_hReadEvent);
	}
	return 0;
}

DWORD WINAPI WriteThreadDraw_UDP(LPVOID arg) {
	int retvalDraw_UDP;

	DWORD ttl_draw = 2;
	retvalDraw_UDP = setsockopt(send_sockDraw_UDP, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl_draw, sizeof(ttl_draw));
	if (retvalDraw_UDP == SOCKET_ERROR) {
		err_quit("setsockopt()");
	}

	ZeroMemory(&remoteaddr_Draw, sizeof(remoteaddr_Draw));
	remoteaddr_Draw.sin_family = AF_INET;
	remoteaddr_Draw.sin_addr.s_addr = inet_addr(MULTICAST);
	remoteaddr_Draw.sin_port = htons(REMOTEPORTDRAW);

	retvalDraw_UDP = sendto(send_sockDraw_UDP, (char*)&g_initmsg, BUFSIZE, 0, (SOCKADDR*)&remoteaddr_Draw, sizeof(remoteaddr_Draw));
	while (1) {
		WaitForSingleObject(g_hWriteDrawEvent, INFINITE);
		if (strlen(g_chatmsg.buf) == 0) {
			EnableWindow(g_hButtonSendMsg, TRUE);
			// �б� �Ϸ� �˸���
			SetEvent(g_hReadDrawEvent);
			continue;
		}
		// ������ ������
		retvalDraw_UDP = sendto(send_sockDraw_UDP, (char*)&g_chatmsg, BUFSIZE, 0, (SOCKADDR*)&remoteaddr_Draw, sizeof(remoteaddr_Draw));
		if (retvalDraw_UDP == SOCKET_ERROR) {
			DisplayText("UDP ERROR\r\n");
			break;
		}
		// '�޽��� ����' ��ư Ȱ��ȭ
		EnableWindow(g_hButtonSendMsg, TRUE);
		// �б� �Ϸ� �˸���
		SetEvent(g_hReadDrawEvent);
	}
	return 0;
}

// �ڽ� ������ ���ν���
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hDC;
	int cx, cy;
	PAINTSTRUCT ps;
	RECT rect;
	HPEN hPen, hOldPen;
	static HDC hDCMem;
	static int x0, y0;
	static int x1, y1;
	static int figureX0, figureY0, figureX1, figureY1;
	static double r;
	static BOOL bDrawing = FALSE;

	switch (uMsg) {
	case WM_CREATE:
		hDC = GetDC(hWnd);

		// ȭ���� ������ ��Ʈ�� ����
		cx = GetDeviceCaps(hDC, HORZRES);
		cy = GetDeviceCaps(hDC, VERTRES);
		hBitmap = CreateCompatibleBitmap(hDC, cx, cy);

		// �޸� DC ����
		hDCMem = CreateCompatibleDC(hDC);

		// ��Ʈ�� ���� �� �޸� DC ȭ���� ������� ĥ��
		SelectObject(hDCMem, hBitmap);
		SelectObject(hDCMem, GetStockObject(WHITE_BRUSH));
		SelectObject(hDCMem, GetStockObject(WHITE_PEN));
		Rectangle(hDCMem, 0, 0, cx, cy);

		// �귯�� �ʱ�ȭ
		g_brush = CreateSolidBrush(RGB(0, 0, 0)); // ���⼭ RGB(0, 0, 0)�� �������� ��Ÿ���ϴ�. ������ �������� �����ϼ���.
		ReleaseDC(hWnd, hDC);
		return 0;
	case WM_LBUTTONDOWN:
		x0 = LOWORD(lParam);
		y0 = HIWORD(lParam);
		bDrawing = TRUE;  // �׸��� ����
		return 0;
	case WM_MOUSEMOVE:
		if (bDrawing) {
			// �� �׸��� �޽��� ������
			if (figure_type == 0) {
				int newX = LOWORD(lParam);
				int newY = HIWORD(lParam);
				g_drawlinemsg.type = DRAWLINE;
				g_drawlinemsg.figure_type = figure_type;
				g_drawlinemsg.line_thickness = line_thickness;
				g_drawlinemsg.x0 = x0;
				g_drawlinemsg.y0 = y0;
				g_drawlinemsg.x1 = newX;
				g_drawlinemsg.y1 = newY;
				if (remaining > 0)
				{
					if (g_isUDP) sendto(send_sockDraw_UDP, (char*)&g_drawlinemsg, BUFSIZE, 0, (SOCKADDR*)&remoteaddr_Draw, sizeof(remoteaddr_Draw));
					else send(g_sockDraw, (char*)&g_drawlinemsg, BUFSIZE, 0);
				}
				// ���� ��ǥ�� ���� ���콺 �̺�Ʈ���� ����ϱ� ���� ������Ʈ
				x0 = newX;
				y0 = newY;
			}
		}
		return 0;
	case WM_LBUTTONUP:


		x1 = LOWORD(lParam);
		y1 = HIWORD(lParam);
		if (figure_type != 0)
		{
			g_drawlinemsg.type = DRAWLINE;
			g_drawlinemsg.figure_type = figure_type;
			g_drawlinemsg.line_thickness = line_thickness;
			g_drawlinemsg.x0 = x0;
			g_drawlinemsg.y0 = y0;
			g_drawlinemsg.x1 = x1;
			g_drawlinemsg.y1 = y1;
			if (g_isUDP) sendto(send_sockDraw_UDP, (char*)&g_drawlinemsg, BUFSIZE, 0, (SOCKADDR*)&remoteaddr_Draw, sizeof(remoteaddr_Draw));
			else send(g_sockDraw, (char*)&g_drawlinemsg, BUFSIZE, 0);
		}


		remaining--;
		printRemain(hPrintRemaining, remaining);
		bDrawing = FALSE;
		return 0;


	case WM_DRAWIT:
		hDC = GetDC(hWnd);
		hPen = CreatePen(PS_SOLID, line_thickness, g_drawcolor);

		// ���� �׸���
		if (figure_type == 0 && remaining > 0)
		{
			hOldPen = (HPEN)SelectObject(hDC, hPen);
			MoveToEx(hDC, LOWORD(wParam), HIWORD(wParam), NULL);
			LineTo(hDC, LOWORD(lParam), HIWORD(lParam));
			SelectObject(hDC, hOldPen);

			hOldPen = (HPEN)SelectObject(hDCMem, hPen);
			MoveToEx(hDCMem, LOWORD(wParam), HIWORD(wParam), NULL);
			LineTo(hDCMem, LOWORD(lParam), HIWORD(lParam));
			SelectObject(hDC, hOldPen);

			DeleteObject(hPen);
			ReleaseDC(hWnd, hDC);
		}
		//�簢�� �׸���
		else if (figure_type == 1 && remaining > 0)
		{
			hOldPen = (HPEN)SelectObject(hDC, hPen);
			SelectObject(hDC, GetStockObject(NULL_BRUSH));

			MoveToEx(hDC, g_drawlinemsg.x0, g_drawlinemsg.y0, NULL);
			LineTo(hDC, g_drawlinemsg.x1, g_drawlinemsg.y0);
			MoveToEx(hDC, g_drawlinemsg.x0, g_drawlinemsg.y0, NULL);
			LineTo(hDC, g_drawlinemsg.x0, g_drawlinemsg.y1);
			MoveToEx(hDC, g_drawlinemsg.x1, g_drawlinemsg.y0, NULL);
			LineTo(hDC, g_drawlinemsg.x1, g_drawlinemsg.y1);
			MoveToEx(hDC, g_drawlinemsg.x0, g_drawlinemsg.y1, NULL);
			LineTo(hDC, g_drawlinemsg.x1, g_drawlinemsg.y1);

			SelectObject(hDC, hOldPen);
			DeleteObject(hPen);
			ReleaseDC(hWnd, hDC);
		}

		else if (figure_type == 2 && remaining > 0)
		{
			hOldPen = (HPEN)SelectObject(hDC, hPen);
			SelectObject(hDC, GetStockObject(NULL_BRUSH));

			r = sqrt(pow(g_drawlinemsg.x0 - g_drawlinemsg.x0, 2) + pow(g_drawlinemsg.y1 - g_drawlinemsg.y0, 2));
			Ellipse(hDC, g_drawlinemsg.x0 - r, g_drawlinemsg.y0 - r, g_drawlinemsg.x0 + r, g_drawlinemsg.y0 + r);

			SelectObject(hDC, hOldPen);
			DeleteObject(hPen);
			ReleaseDC(hWnd, hDC);
		}

		//�ﰢ�� ������ ������ �Ʒ� �밢������ �׸��� ��������, �������� �߰ų� ���� �Ʒ� �� , �ߴ� ���⸶�� �޶���
		else if (figure_type == 3 && remaining > 0)
		{
			hOldPen = (HPEN)SelectObject(hDC, hPen);
			SelectObject(hDC, GetStockObject(NULL_BRUSH));

			MoveToEx(hDC, g_drawlinemsg.x0, g_drawlinemsg.y0, NULL);
			LineTo(hDC, g_drawlinemsg.x1, g_drawlinemsg.y1);
			MoveToEx(hDC, g_drawlinemsg.x0, g_drawlinemsg.y0, NULL);
			LineTo(hDC, g_drawlinemsg.x0, g_drawlinemsg.y1);
			MoveToEx(hDC, g_drawlinemsg.x0, g_drawlinemsg.y1, NULL);
			LineTo(hDC, g_drawlinemsg.x1, g_drawlinemsg.y1);

			SelectObject(hDC, hOldPen);
			DeleteObject(hPen);
			ReleaseDC(hWnd, hDC);
		}

		return 0;


	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);

		// �޸� ��Ʈ�ʿ� ����� �׸��� ȭ�鿡 ����
		GetClientRect(hWnd, &rect);
		BitBlt(hDC, 0, 0, rect.right - rect.left,
			rect.bottom - rect.top, hDCMem, 0, 0, SRCCOPY);

		EndPaint(hWnd, &ps);
		return 0;
	case WM_DESTROY:
		DeleteObject(hBitmap);
		DeleteDC(hDCMem);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


void printRemain(HWND hWnd, int remaining)
{
	// �ؽ�Ʈ�� ����� ���� Edit ��Ʈ���� ó���� ���� �����մϴ�.
	SendMessage(hWnd, EM_SETSEL, 0, -1);
	// ���õ� �ؽ�Ʈ�� ��� �����մϴ�.
	SendMessage(hWnd, EM_REPLACESEL, TRUE, (LPARAM)"");

	if (g_isGame2)
	{
		strRemain[0] = '0';
		if (remaining > 0)	strRemain[0] += remaining;
		strRemain[4] = total_number + '0';
		SendMessage(hWnd, EM_REPLACESEL, FALSE, (LPARAM)strRemain);
	}

}


void DisplayText(char* fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);

	char cbuf[1024];
	vsprintf(cbuf, fmt, arg);

	int nLength = GetWindowTextLength(g_hEditSend);
	SendMessage(g_hEditSend, EM_SETSEL, nLength, nLength);
	SendMessage(g_hEditSend, EM_REPLACESEL, FALSE, (LPARAM)cbuf);

	va_end(arg);
}

// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
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