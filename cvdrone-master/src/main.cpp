#include "ardrone/ardrone.h"
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <thread>
#include <time.h>
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <process.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include "ardrone/SpeechRecord.h"
#include "Leap.h"
#include <pthread.h>

#pragma comment(lib,"Winmm.lib")
#pragma comment(linker, "/subsystem:console /entry:WinMainCRTStartup")
#pragma comment(lib,"ws2_32.lib") //Winsock Library


using namespace std;
using namespace cv;
using namespace Leap;

static TCHAR szWindowClass[] = _T("win32app");
// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("AR Drone Application");
// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void detectAndDraw( Mat& img, CascadeClassifier& cascade, double scale);
void *recordSound(void *);
string intToString(int num);
void tracking(vector<Rect> faces);
void recieveSocketData();
void sendSocketData(char *msg);
void *saveSoundFile(void *);
void *sendSound(void *);
void releaseRecordSource();
void *leap(void *);
void *showVideo(void *);
void *navData(void *);
void *gpsLocate(void *);
void updateMhi(const IplImage* src_image,IplImage* binary_mhi);
void CheckFireColor2(IplImage *RGBimg, IplImage *plmgFire) ; 
void CheckFireMove(IplImage *pImgFrame, IplImage* pInitBackground, IplImage *pImgMotion)  ;
int iniTCP();

WSADATA wsa;
SOCKET s;
ARDrone ardrone;
TCPSocket client;
struct sockaddr_in server;
char *message , server_reply[2000], gps_reply[2000];
int recv_size, gps_size;
char * num[5] = {"1","2","3","4","5"};
int soundFileNum = 0;
int recordFileNum = 0;
bool isRecord = false;
bool isTracking = false;
bool isSendSound = false;
bool isSaving = false;
bool isLeapStart = false;
bool isConnect = false;
bool isGPS = false;
int controlMode = 1; // 0: Leap 1: Keyboard 3: Open CV tracking
float originalHandPositionZ = 0.0;
float originalHandPositionY = 0.0;
float originalHandPositionX = 0.0;
double angle = -3.1415926/180;
double longitude[50], latitude[50];
//运动像素维持的最大时间，单位与timestamp的单位一致  
    #define MHI_DURATION 1  
    //时间标记  
    
      
    //当前帧的上一帧图像  
    static IplImage* last_image = NULL;  
    static IplImage* color_silh = NULL;  
    static IplImage* gray_silh = NULL;  
    //运动历史图像  
    static IplImage* mhi = NULL;  
	CvSize size = cvSize(640, 360);
	IplImage *test = cvCreateImage(size, 8 , 3);
	IplImage *omhi = cvCreateImage(size, 8 , 1);
	IplImage* Fire = cvCreateImage(size, 8 , 3) ;  
	IplImage *image;
WAVEFORMATEX wf;
HWAVEIN hWaveIn[5];
WAVEHDR waveHdr[5];
DWORD dataSize = 240000L;
MMTIME mmt;
pthread_t leapThread;
pthread_t videoThread;
pthread_t navDataThread;
pthread_t recordThread;
pthread_t saveSoundThread;
pthread_t sendSoundThread;
pthread_t gpsLocateThread;
String cascadeName = "../../haarcascade_frontalface_alt.xml";
HINSTANCE hInst; 
HWND hWnd_record_button, hWnd_stop_button, hWnd_battery, hWnd_connect_drone_button;
HWND hWnd_leap_control_button, hWnd_keyboard_control_button, hWnd_tracking_button;
HWND hWnd_gps_button;
HWND hWnd_longitude, hWnd_latitude;

class SampleListener : public Listener {
  public:
    virtual void onInit(const Controller&);
    virtual void onConnect(const Controller&);
    virtual void onDisconnect(const Controller&);
    virtual void onExit(const Controller&);
    virtual void onFrame(const Controller&);
    virtual void onFocusGained(const Controller&);
    virtual void onFocusLost(const Controller&);
};

void SampleListener::onInit(const Controller& controller) {
  std::cout << "Initialized" << std::endl;
}

void SampleListener::onConnect(const Controller& controller) {
  std::cout << "Connected" << std::endl;
  controller.enableGesture(Gesture::TYPE_CIRCLE);
  controller.enableGesture(Gesture::TYPE_KEY_TAP);
  controller.enableGesture(Gesture::TYPE_SCREEN_TAP);
  controller.enableGesture(Gesture::TYPE_SWIPE);
}

void SampleListener::onDisconnect(const Controller& controller) {
  //Note: not dispatched when running in a debugger.
  std::cout << "Disconnected" << std::endl;
}

void SampleListener::onExit(const Controller& controller) {
  std::cout << "Exited" << std::endl;
}

void SampleListener::onFrame(const Controller& controller) {
  // Get the most recent frame and report some basic information
  const Frame frame = controller.frame();
  if (!frame.hands().empty()) {
    // Get the first hand
    const Hand hand = frame.hands()[0];

    // Check if the hand has any fingers
	if(isLeapStart)
	{
		originalHandPositionZ = hand.palmPosition().y;
		originalHandPositionY = hand.palmPosition().x;
		originalHandPositionX = hand.palmPosition().z;
		isLeapStart = false;
	}
	if(controlMode == 0)
	{
		double z  = 0;
		double y = 0;
		double x = 0;
		//cout << hand.palmVelocity() << endl;
		if(!ardrone.onGround())
		{
			clock_t endwait;
			endwait = clock() + 0.1 * CLOCKS_PER_SEC ;
			while (clock() < endwait) {}
			if ( -(originalHandPositionZ - hand.palmPosition().y) > 0)
			{
				 z  = 1;
			}
			else if(-(originalHandPositionZ - hand.palmPosition().y) < 0)
			{
				z = -1;
			}
			if ( originalHandPositionX - hand.palmPosition().z > 0)
			{
				x = 1;
			}
			else if(originalHandPositionX - hand.palmPosition().z < 0)
			{
				x = -1;
			}
			if (-(originalHandPositionY - hand.palmPosition().x) > 0)
			{
				y = 1.0;
			}
			else if(-(originalHandPositionY - hand.palmPosition().x) < 0)
			{
				y = -1.0;
			}
			cout << y  << endl;
			//ardrone.move3D(x, y, z, 0);
		}
	}
  }
  if (!frame.hands().empty()) {
    std::cout << std::endl;					
  }
}

void SampleListener::onFocusGained(const Controller& controller) {
  std::cout << "Focus Gained" << std::endl;
}

void SampleListener::onFocusLost(const Controller& controller) {
  std::cout << "Focus Lost" << std::endl;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL,
            _T("Call to RegisterClassEx failed!"),
            _T("Win32 Guided Tour"),
            NULL);

        return 1;
    }

    hInst = hInstance; // Store instance handle in our global variable

    // The parameters to CreateWindow explained:
    // szWindowClass: the name of the application
    // szTitle: the text that appears in the title bar
    // WS_OVERLAPPEDWINDOW: the type of window to create
    // CW_USEDEFAULT, CW_USEDEFAULT: initial position (x, y)
    // 500, 100: initial size (width, length)
    // NULL: the parent of this window
    // NULL: this application dows not have a menu bar
    // hInstance: the first parameter from WinMain
    // NULL: not used in this application
    HWND hWnd = CreateWindow(
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        0, 0,
        500, 500,
        NULL,
        NULL,
        hInstance,
        NULL
    );

	hWnd_longitude = CreateWindow("static", "ST_U", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 60, 100, 30, hWnd, (HMENU)(502), (HINSTANCE) GetWindowLong (hWnd, GWL_HINSTANCE), NULL);
	hWnd_latitude = CreateWindow("static", "ST_U", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 100, 100, 30, hWnd, (HMENU)(503), (HINSTANCE) GetWindowLong (hWnd, GWL_HINSTANCE), NULL);
	hWnd_battery = CreateWindow("static", "ST_U", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 20, 100, 30, hWnd, (HMENU)(501), (HINSTANCE) GetWindowLong (hWnd, GWL_HINSTANCE), NULL);
	hWnd_leap_control_button = CreateWindow(TEXT("button"), TEXT("Leap Control"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, 350, 100, 30, hWnd, (HMENU)15000, hInstance, NULL);
	hWnd_stop_button = CreateWindow(TEXT("button"), TEXT("Stop"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 300, 300, 80, 30, hWnd, (HMENU)12000, hInstance, NULL);
	hWnd_record_button = CreateWindow(TEXT("button"), TEXT("Record"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 200, 300, 80, 30, hWnd, (HMENU)10000, hInstance, NULL);
	hWnd_connect_drone_button = CreateWindow(TEXT("button"), TEXT("Connect"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, 300, 80, 30, hWnd, (HMENU)9000, hInstance, NULL);
	hWnd_keyboard_control_button = CreateWindow(TEXT("button"), TEXT("Key Control"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 220, 350, 100, 30, hWnd, (HMENU)16000, hInstance, NULL);
	hWnd_gps_button = CreateWindow(TEXT("button"), TEXT("GPS"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, 390, 100, 30, hWnd, (HMENU)17000, hInstance, NULL);


	if (!hWnd)
    {
        MessageBox(NULL, _T("Call to CreateWindow failed!"), _T("Win32 Guided Tour"), NULL);
        return 1;
    }

	pthread_create(&recordThread, NULL, recordSound, NULL);
	pthread_create(&saveSoundThread, NULL, saveSoundFile, NULL);
	pthread_create(&sendSoundThread, NULL, sendSound, NULL);
    // The parameters to ShowWindow explained:
    // hWnd: the value returned from CreateWindow
    // nCmdShow: the fourth parameter from WinMain
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int) msg.wParam;
}

void *leap(void *alg)
{
	SampleListener listener;
	Controller controller;
	controller.addListener(listener);
	isLeapStart = true;
	puts("Leap motion start.");
	cout << "Press Enter to quit..." << endl;
	cin.get();
	return NULL;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    switch (message)
    {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
	case WM_COMMAND:
    if(LOWORD(wParam)==10000)//record
    {
        //MessageBox(hWnd, TEXT("Record!"), TEXT(""), 0);
		isRecord = true;
		
		EnableWindow( GetDlgItem( hWnd, 10000 ), FALSE); 
		EnableWindow( GetDlgItem( hWnd, 12000 ), TRUE); 
    }
	if(LOWORD(wParam)==12000)// stop record
    {
		isSaving = false;
		isRecord = false;
		//releaseRecordSource();
		EnableWindow( GetDlgItem( hWnd, 12000 ), FALSE);
		EnableWindow( GetDlgItem( hWnd, 10000 ), TRUE); 
    }
	if(LOWORD(wParam)==9000)// connect drone
    {
		pthread_create(&videoThread, NULL, showVideo, NULL);
		//pthread_create(&navDataThread, NULL, navData, NULL);
		isConnect = true;
		EnableWindow( GetDlgItem( hWnd, 9000 ), FALSE); 
    }
	if(LOWORD(wParam)==16000)// Key control mode
    {
		if (isConnect)
		{
			controlMode = 1;
		}
    }
	if(LOWORD(wParam)==15000)// Leap control mode
    {
		if (isConnect)
		{
			controlMode = 0;
			pthread_create(&leapThread, NULL, leap, NULL);
		}
    }
	if(LOWORD(wParam)==17000)// GPS 
    {
		isGPS = !isGPS;
		pthread_create(&gpsLocateThread, NULL, gpsLocate, NULL);
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }
    return 0;
}

int iniTCP()
{
    printf("\nInitialising Winsock...");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed. Error Code : %d",WSAGetLastError());
        return 1;
    }
     
    printf("Initialised.\n");
     
    //Create a socket
    if((s = socket(AF_INET , SOCK_STREAM , 0 )) == INVALID_SOCKET)
    {
        printf("Could not create socket : %d" , WSAGetLastError());
    }
 
    printf("Socket created.\n");
     
     
    server.sin_addr.s_addr = inet_addr("192.168.1.2");
    server.sin_family = AF_INET;
    server.sin_port = htons( 1234 );
 
    //Connect to remote server
    if (connect(s , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        puts("connect error");
        return 1;
    }
     
    puts("Connected");
}

void *showVideo(void  *arg)
{
	unsigned char data[2048] = {};
	string sbuffer;
	Mat frame, frameCopy;
	CascadeClassifier cascade;
	if( !cascade.load( cascadeName ) )   
	{
		 return NULL;
	}
	double scale = 2;
	char *data1[1024];
	int size =1024;
	if (!ardrone.open()) 
	{
        printf("Failed to initialize.\n");
    }
	else
	{
		while (1)
		{
			int key = cvWaitKey(30);
			if (key == 0x1b) break;
			if (!ardrone.update()) break;
			image = ardrone.getImage();
			//IplImage *h = cvCreateImage(size, 8 , 1);
			//IplImage *s = cvCreateImage(size, 8 , 1);
			//IplImage *v = cvCreateImage(size, 8 , 1);
			
			frame = image;
			if( image->origin == IPL_ORIGIN_TL )
				frame.copyTo( frameCopy );
			else
				flip( frame, frameCopy, 0 );
			detectAndDraw(frameCopy, cascade, scale);
			//cvCvtColor( image, test, CV_BGR2HSV );
			//cvSplit(test, h , s , v , NULL);
			//cvShowImage("hsv",test);
			
			CheckFireColor2(image, Fire);
			updateMhi(Fire, omhi);
			//cvShowImage("img", Fire);
			cvShowImage("mhi",omhi);
			if (key == ' ') 
			{
				if (ardrone.onGround()) ardrone.takeoff();
				else                    ardrone.landing();
			}
			string battery = "Battery: " + intToString(ardrone.getBatteryPercentage()) + "%";
			SetWindowText(hWnd_battery,  battery.c_str());
			// Move
			if (controlMode == 1)
			{
				double vx = 0.0, vy = 0.0, vz = 0.0, vr = 0.0;
				if (key == 0x260000) vx =  1.0;
				if (key == 0x280000) vx = -1.0;
				if (key == 0x250000) vr =  1.0;
				if (key == 0x270000) vr = -1.0;
				if (key == 'q')      vz =  1.0;
				if (key == 'a')      vz = -1.0;
				ardrone.move3D(vx, vy, vz, vr);
			}
			// Change camera
			static int mode = 0;
			if (key == 'c') ardrone.setCamera(++mode%4);
		}
		//cvDestroyWindow("camera");
		ardrone.close();
	}
	return NULL;
}

void recieveGPSData()
{
	if((gps_size = recv(s , gps_reply , 2000 , 0)) == SOCKET_ERROR)
		{
		 puts("recv failed");
		}
		puts("GPS received\n");
		//Add a NULL terminating character to make it a proper string before printing
		gps_reply[gps_size] = '\0';
		//puts(gps_reply);
}

void recieveSocketData()
{
	if((recv_size = recv(s , server_reply , 2000 , 0)) == SOCKET_ERROR)
		{
		 puts("recv failed");
		}
		puts("Reply received\n");
		//Add a NULL terminating character to make it a proper string before printing
		server_reply[recv_size] = '\0';
		puts(server_reply);
}

void sendSocketData(char *msg)
{
	message = msg;
	if( send(s , message , strlen(message) , 0) < 0)
	{
		puts("Send failed");
	}
	else
	{
		puts("Data Send\n");
	}
}

void tracking(vector<Rect> faces)
{
	if(isTracking)
	{
		cout << "x:" << faces[0].x + faces[0].width*0.5 << endl;
		cout << "y:" << faces[0].y + faces[0].height*0.5 << endl;
		double centerX, centerY;
		centerX = faces[0].x + faces[0].width*0.5;
		centerY = faces[0].y + faces[0].height*0.5;
		if(ardrone.onGround() == 0)
		{
			cout << "tracking" << endl;
			ardrone.move3D((320 - centerX)/5, (160 - centerY)/5, 0, 0);
		}
	}
}

string intToString(int num)
{
	string s;
	stringstream ss(s);
	ss << num;
	return ss.str();
}

void *navData(void *)
{
	/*IplImage *nav;
	while (true)
	{
		
		cvShowImage("NavData", nav);
	}*/
}

void *gpsLocate(void *)
{
	while (isGPS)
	{
		bool isValid = false;
		iniTCP();
		sendSocketData("gps");
		recieveGPSData();
		clock_t endwait;
		endwait = clock() + 1 * CLOCKS_PER_SEC ;
		while (clock() < endwait) {}
		stringstream ss(gps_reply);
        string sub_str;
		for (int i = 0; i < 3;i++)
		{
			getline(ss,sub_str,',');
		}
		getline(ss,sub_str,',');
		if (sub_str == "A")
		{
			getline(ss,sub_str,',');
			SetWindowText(hWnd_latitude, sub_str.c_str());
			getline(ss,sub_str,',');
			SetWindowText(hWnd_longitude, sub_str.c_str());
		}
		
        /*while(getline(ss,sub_str,',')) 
                cout << sub_str << endl;	*/
	}
	return NULL;
}

void detectAndDraw( Mat& img, CascadeClassifier& cascade, double scale)
{
   vector<Rect> faces;
   Mat frame_gray;
   
   cvtColor( img, frame_gray, CV_BGR2GRAY );
   equalizeHist( frame_gray, frame_gray );
   //-- Detect faces
   cascade.detectMultiScale( frame_gray, faces, 1.1, 2, 0|CV_HAAR_SCALE_IMAGE, Size(50, 50) );
   
   for( int i = 0; i < faces.size(); i++ )
    {
      Point center( faces[i].x + faces[i].width*0.5, faces[i].y + faces[i].height*0.5 );
      ellipse( img, center, Size( faces[i].width*0.5, faces[i].height*0.5), 0, 0, 360, Scalar( 0, 255, 0 ), 2, 8, 0 );
	  //tracking(faces);
      Mat faceROI = frame_gray( faces[i] );
    }
   cvResizeWindow("result",640,500);
   imshow( "result", img );
}

void *sendSound(void *)
{
	while (1)
	{
		if (isSendSound)
		{
			iniTCP();
			if(soundFileNum != 0)
			{
				sendSocketData(num[soundFileNum - 1]);
				puts(num[soundFileNum - 1]);
			}
			else
			{
				sendSocketData(num[4]);
				puts(num[4]);
			}
			clock_t endwait;
			endwait = clock() + 5 * CLOCKS_PER_SEC ;//錄製三秒
			while (clock() < endwait) {}
		}
	}
}

void *recordSound(void *)
{
	cout << "OpenCV Version: " << CV_VERSION << endl;
	SetWaveFormat(&wf,1,1,12000,1,8,0);
	while(1)
	{
		if(isRecord)
		{
			if(recordFileNum == 5)
			{
				recordFileNum = 0;
			}
			
			OpenWaveIn(&hWaveIn[recordFileNum],&wf);
			PrepareWaveIn(&hWaveIn[recordFileNum], &waveHdr[recordFileNum], dataSize);
			StartRecord(&hWaveIn[recordFileNum]);
			clock_t endwait;
			endwait = clock() + 5 * CLOCKS_PER_SEC ;//錄製三秒
			while (clock() < endwait) {}
			StopRecord(&hWaveIn[recordFileNum], &mmt);
			recordFileNum++;
			isSaving = true;
		}
	}
}

void releaseRecordSource()
{
	int i;
	for (int i = 0; i < 5; i++)
	{
		ReleaseWaveIn(&hWaveIn[i], &waveHdr[i]);
		CloseWaveIn(&hWaveIn[i]);
	}
}

void *saveSoundFile(void *)
{
	while(1)
	{
		if(isSaving)
		{
			if(soundFileNum == 5)
			{
				soundFileNum = 0;
			}
			if(soundFileNum == 1)
			{
				isSendSound = true;
			}
			string path = "D://xampp//htdocs//" + intToString(soundFileNum + 1) + ".wav";
			SaveRecordtoFile(path.c_str(),&wf,&hWaveIn[soundFileNum],&waveHdr[soundFileNum],&mmt);
			soundFileNum ++;
			isSaving = false;
		}
	}
}


void updateMhi(const IplImage* src_image,IplImage* binary_mhi)  
{  
	double timestamp = clock()/100.;  
    if(NULL==last_image && NULL==color_silh && NULL==gray_silh && NULL==mhi)  
    {  
        CvSize size = cvGetSize(src_image);  
        //初始分配内存，在该程序中没有释放如下图像的内存空间  
        last_image = cvCreateImage(size,8,3);  
        color_silh = cvCreateImage(size,8,3);  
        gray_silh = cvCreateImage(size,8,1);  
        mhi = cvCreateImage(size,IPL_DEPTH_32F,1);  
        //初始情况下令当前帧与上一帧图像相同  
        cvCopy(src_image,last_image);  
        //置零初始运动历史图像  
        cvZero(binary_mhi);  
    }  
    else  
    {  
        //获得帧差图像color_silh  
        cvAbsDiff(src_image,last_image,color_silh);  
        cvCopy(src_image,last_image);  
       cvCvtColor(color_silh,gray_silh,CV_BGR2GRAY);  
  
		cvThreshold( gray_silh, gray_silh, 30, 255, CV_THRESH_BINARY);  
        //更新运动历史图像  
		cvUpdateMotionHistory(gray_silh,mhi,timestamp,MHI_DURATION);  
        //将浮点型的运动历史图像转换成图像binary_mhi  
       cvCvtScale( mhi, binary_mhi, 255./MHI_DURATION,  
                  (MHI_DURATION - timestamp)*255./MHI_DURATION );  
        //二值化  
        cvThreshold( binary_mhi, binary_mhi, 0, 255, CV_THRESH_BINARY );  
		//CheckFireColor2(binary_mhi);
		}  
}  


void CheckFireColor2(IplImage *RGBimg, IplImage *pImgFire)  
{  
    int RedThreshold=135;  //115~135   
    int SaturationThreshold=45;  //55~65  
    cvSmooth(RGBimg, RGBimg, CV_GAUSSIAN, 3, 0, 0);  
	cvErode(RGBimg, RGBimg, 0, 1);  
    cvDilate(RGBimg, RGBimg, 0, 1);   
    for(int j = 0;j < RGBimg->height;j++)  
    {  
        for (int i = 0;i < RGBimg->widthStep;i+=3)  
        {  
            uchar B = (uchar)RGBimg->imageData[j*RGBimg->widthStep+i];  
            uchar G = (uchar)RGBimg->imageData[j*RGBimg->widthStep+i+1];  
            uchar R = (uchar)RGBimg->imageData[j*RGBimg->widthStep+i+2];  
            uchar maxv=max(max(R,G),B);   
            uchar minv=min(min(R,G),B);   
            double S = (1 - 3.0*minv/(R+G+B));  
              
            //火焰像素满足颜色特征  
            //(1)R>RT   (2)R>=G>=B   (3)S>=( (255-R) * ST / RT )  
           if(R>RedThreshold&&R>=G&&G>=B&&S>0.20 /*&& S>(255-R)/20&&S>=((255-R)*SaturationThreshold/RedThreshold)*/)  
		   {
				pImgFire->imageData[j*RGBimg->widthStep+i] = RGBimg->imageData[j*RGBimg->widthStep+i];  
				pImgFire->imageData[j*RGBimg->widthStep+i + 1] = RGBimg->imageData[j*RGBimg->widthStep+i+1]; 
				pImgFire->imageData[j*RGBimg->widthStep+i + 2] = RGBimg->imageData[j*RGBimg->widthStep+i+2]; 
		   }
            else  
			{
				pImgFire->imageData[j*RGBimg->widthStep+i] = 0; 
			}
              
        }  
    }  
}  

