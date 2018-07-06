// 'Pong' as vector game sample 
// compile: gcc -Wall -o pong pong.cpp -lwiringPi -lm
//
#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <math.h>
#include <time.h>
#include <sys/time.h> 
#include <mcp3004.h>

#define WORD unsigned short
#define INT short
const int GPIO25 = 25;
const int ScreenWidth = 4096;
const int ScreenHeight = 4096;
const double pi = 3.1415926535897932384626433832795;
int fd; //global
static sig_atomic_t end = 0;

static void sighandler(int signo){
	end = 1;
}

void SPIWrite(int fd, unsigned char Channel, int Gain, unsigned short wValue) {
	unsigned char SPIData[2];

	if (wValue>0x0FFF)
		wValue=0x0FFF;
	SPIData[1] = wValue & 0xFF;
	SPIData[0] = ((wValue >> 8) & 0x0F) | 0x10;
	if (1==Channel) {
		SPIData[0] |= 0x80; 
	} 
	if(Gain <= 1 ){ // max Vdd
		SPIData[0] |= 0x20; 
	}
	
	if (write(fd, &SPIData, sizeof(SPIData)) != sizeof(SPIData)) {
		fprintf(stderr, 
		"Failed to write to spi bus (%s)\n", strerror(errno));
		return;
	}
	//wiringPiSPIDataRW (0, SPIData, 2) ; //10% slower than write!
 }

void DualDACWrite(int fd, WORD wValue0, WORD wValue1) {
	digitalWrite(GPIO25, HIGH);
	SPIWrite(fd, 0, 1, wValue0);
	SPIWrite(fd, 1, 1, wValue1);
	digitalWrite(GPIO25, LOW);
}

class CObject{
public:
	float PosX, PosY;
	float Width, Height;
	int Step;
	float XDirection;
	float YDirection;
	INT VBufferX[100];
	INT VBufferY[100];
	int m_nDotPerObject;

	CObject(){
		PosX = 0;
		PosY = 0;
		Width = 0;
		Height = 0;
		Step = 15;
		XDirection = 0;
		YDirection = 0;
		m_nDotPerObject = 100;
	}
	void CreateObjectAsCycle(int nSize, int nDotPerObject) {
		double sinus;
		double cosinus;
		
		if (nDotPerObject<=100) { 
			m_nDotPerObject = nDotPerObject;
		} else {
			m_nDotPerObject = 100;
		}
		Width = nSize;
		Height = nSize;
		for (int nDotNo = 0; nDotNo < m_nDotPerObject; nDotNo++) { 
			sinus = sin(2*pi*nDotNo/m_nDotPerObject);
			VBufferX[nDotNo] = sinus * nSize;
			cosinus = cos(2*pi*nDotNo/nDotPerObject);
			VBufferY[nDotNo] = cosinus * nSize;
		}
	}
	void CreateObjectAsLine(int nLenght, int nWidth, int nDotPerObject) {
		if (nDotPerObject<=100) { 
			m_nDotPerObject = nDotPerObject;
		} else {
			m_nDotPerObject = 100;
		}	
		Width = nWidth;
		Height = nLenght;		
		for (int nDotNo = 0; nDotNo < m_nDotPerObject; nDotNo++) { 
			VBufferX[nDotNo] = -nWidth/2+nDotNo%nWidth;
			VBufferY[nDotNo] = -nLenght/2+nLenght * nDotNo/nDotPerObject;
		}
	}
	
	void Draw(){
		for (int nDotNo = 0; nDotNo < m_nDotPerObject; nDotNo++) { 
			DualDACWrite(fd, PosX+VBufferX[nDotNo], PosY+VBufferY[nDotNo]);	
		}
	}
	void SetRelativePosY(int nPercent) {
		if(nPercent>100) nPercent = 100;
		if(nPercent<0) nPercent = 0;
		
		PosY = Height/2 + ((ScreenHeight-Height) * nPercent / 100); 
	}
	
	void ReverseDirX() {
		XDirection *= -1;
	}
	void ReverseDirY() {
		YDirection *= -1;
	}

	void SetYDirection(float YDir) {
		YDirection = YDir;
	}
	void SetXYDirection(float XDir, float YDir) {
		XDirection = XDir;
		YDirection = YDir;
	}
	void MoveXStep(){
		PosX += XDirection * Step;
	}
	void MoveYStep(){
		PosY += YDirection * Step;
	}
	void XStepUp(){ 
		if (PosX+Step+Width<=ScreenWidth) {
			PosX += Step; 
		}
	};
	void XStepDown(){ 
		if (PosX-Step>=0) {
			PosX -= Step;
		}
	};
	void YStepUp(){ 
		if (PosY+Step+Height<=ScreenHeight) {
			PosY += Step; 
		}
	};
	void YStepDown(){ 
		if (PosY-Step>=0) {
			PosY -= Step;
		}
	};	

	bool Collision(CObject &Object) {
		return (Collision(Object.PosX, Object.PosY, Object.Width, Object.Height));
	}
	bool Collision(int ObjectPosX, int ObjectPosY, int nObjectWidth, int nObjectHeight) {
		int X1 = PosX-Width/2;
		int X2 = X1+Width;
		int X3 = ObjectPosX-nObjectWidth/2;
		int X4 = X3 + nObjectWidth;
		int Y1 = PosY-Height/2;
		int Y2 = Y1+Height;
		int Y3 = ObjectPosY-nObjectHeight/2;
		int Y4 = Y3 + nObjectHeight;
		
		if (X3>X2 || X4<X1) {
			return false;
		} 
		if (Y3>Y2 || Y4<Y1) {
			return false;
		} 
		
		return true;
	}
};

class CRacket : public CObject {
public:
	CRacket(){
		CreateObjectAsLine(500, 10, 75); 	
	}
};

class CLeftRacket : public CRacket {
public:
	CLeftRacket(){
		PosX = 100;
		PosY = ScreenHeight / 2;
	}
};

class CRightRacket : public CRacket {
public:
	CRightRacket(){
		PosX = ScreenWidth - Width - 100;
		PosY = ScreenHeight / 2;
	}
};

class CBall : public CObject {
public:
	CBall(){
		XDirection = -1.0f;
		YDirection = 0.0f;
		MoveCenter();
		Step = 20;
		CreateObjectAsCycle(150, 50); 
	}
	void MoveCenter(){
		PosX = ScreenWidth / 2.0f;
		PosY = ScreenHeight / 2.0f;
	}

	void MoveXYStep(){
		MoveXStep();
		MoveYStep();

		// hit left wall?
		if (PosX < 0) {
				//++score_right;
				MoveCenter();
				ReverseDirX();
				YDirection = 0;
		}

		// hit right wall?
		if (PosX > ScreenWidth) {
				//++score_left;
				MoveCenter();
				ReverseDirX();
				YDirection = 0;
		}

		// hit top wall?
		if (PosY > ScreenHeight) {
				ReverseDirY();
		}

		// hit bottom wall?
		if (PosY < 0) {
		   ReverseDirY();
		}

		float VectorLength = sqrt((XDirection * XDirection) + (YDirection * YDirection));
		if (VectorLength != 0.0f) {
			VectorLength = 1.0f / VectorLength;
			XDirection *= VectorLength;
			YDirection *= VectorLength;
		}
	}
};

unsigned short DiscObjectX[1024], DiscObjectY[1024];
unsigned short Paddle1ObjectX[1024], Paddle1ObjectY[1024];
unsigned short Paddle1Object2X[1024], Paddle1Object2Y[1024];
int XDir = 1;
int YDir = -1;
int step = 20;
unsigned short woffset = 4096/2;
int area;
int nDotPerObject = 50;
int XPos;
int YPos;	

int PongSample(int fd){
	int Pos;		
	int loop;
	int nDots = 0;
	int Poti1 = 0;
	int Poti2 = 0;
	
		// read poti
	Poti1 = analogRead(200);
	Poti2 = analogRead(201);
	
	//Disc
	for (loop = 0; loop < nDotPerObject; loop++) { 
		DualDACWrite(fd, XPos + DiscObjectX[loop], YPos + DiscObjectY[loop]);
	}
	nDots += nDotPerObject;

	//paddle right
	for (loop = 0; loop < nDotPerObject; loop++) { 
		DualDACWrite(fd, 2000+Paddle1ObjectX[loop], /*YPos +*/ Poti2*-2 + Paddle1ObjectY[loop]);
	}
	nDots += nDotPerObject;

	//paddle left 
	for (loop = 0; loop < nDotPerObject; loop++) { 
		DualDACWrite(fd, Paddle1ObjectX[loop]-2000, Poti1*-2 /*YPos*/ + Paddle1ObjectY[loop]);		
	}
	nDots += nDotPerObject;

	Pos = XPos+(XDir*step );
	if (Pos < (area) && Pos>(area*-1)) {
		XPos=Pos;
	} else {
//		printf("xPos=%d\n", XPos);
		XDir = XDir * -1;
	}
	Pos = YPos+(YDir*step );
	if(Pos < area && Pos>(area*-1)) {
		YPos=Pos;
	} else {
//		printf("yPos=%d\n", YPos);
		YDir = YDir * -1;
	}

	return(nDots);
}


int main (void) {
	char device[20];
	int loop, loop2;
	struct timeval t1, t2;
	double elapsedTime, fTimePerOperation, fFPS;
  struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = sighandler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT,&sa, NULL);
	sigaction(SIGTERM,&sa, NULL);

	if (wiringPiSPISetup(0, 4000000) == -1){
		printf("wiringPiSPISetup failed\n\n");
		exit(EXIT_FAILURE);
	}
	if (wiringPiSPISetup(1, 4000000) == -1){
		printf("wiringPiSPISetup failed\n\n");
		exit(EXIT_FAILURE);
	}	
	if ( mcp3004Setup(200, 1) == -1){
		printf("mcp3004Setup failed\n\n");
		exit(EXIT_FAILURE);
	}
	if ( wiringPiSetupGpio () == -1){
		printf("wiringPiSetup failed \n\n");
		exit(EXIT_FAILURE) ;
	}
	pinMode(GPIO25, OUTPUT);
	digitalWrite(GPIO25, LOW);

	sprintf(device, "/dev/spidev0.0");
	printf("open device '%s'...\n", device);
	if ((fd = open(device, O_RDWR)) < 0) {
		fprintf(stderr,"Failed to open spi 0.0 bus '%s'\n", device);
		exit(EXIT_FAILURE);
	}

	unsigned char *pBuffer0, *pBuffer1;
	int nSetPointSize = 3*sizeof(unsigned char);
	int nSetPointCount = 6;
	int nBufferSize = nSetPointSize*nSetPointCount;
	
	pBuffer0 = (unsigned char*)malloc(nBufferSize);
	pBuffer1 = (unsigned char*)malloc(nBufferSize);
	if(!pBuffer0 || !pBuffer1) {
		fprintf(stderr, "allocate buffer with %d Byte failed\n", nBufferSize);
		close(fd);
		free(pBuffer0);
		free(pBuffer1);
		exit(EXIT_FAILURE);		
	}
	
	const double pi = 3.1415926535897932384626433832795;
	double sinus ;
	double cosinus ;
	
	printf("write 50 %% ...\n");
 	
	int nSize = 150;	
	for (loop = 0; loop < nDotPerObject; loop++) { 
		sinus = sin(2*pi*loop/nDotPerObject);
		DiscObjectX[loop] = (unsigned short) woffset + sinus * nSize;
		cosinus = cos(2*pi*loop/nDotPerObject);
		DiscObjectY[loop] = (unsigned short)woffset + cosinus * nSize;
		
		
		Paddle1ObjectX[loop] = woffset + nDotPerObject%10;
		Paddle1ObjectY[loop] = (unsigned short)woffset + nSize *2 * loop/nDotPerObject;

		Paddle1Object2X[loop] = (unsigned short)woffset + nSize *2 * loop/nDotPerObject;
		Paddle1Object2Y[loop] = woffset + nDotPerObject%10;
	}
	
	area = woffset-300;
	XPos = area/2;
	YPos = area/2;
	int nDots = 0;
	int nDotsPerFrame = 0;
	int Frames = 1000; 

	printf("Pong test:\n");
	gettimeofday(&t1, 0);
	for (loop2 = 1; loop2 < Frames; loop2++) {
		nDotsPerFrame = PongSample(fd);
		nDots += nDotsPerFrame;
	}
	gettimeofday(&t2, 0);
	elapsedTime = (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec)/1000000.0;	
	fTimePerOperation = elapsedTime*1000*1000 / nDots;
	fFPS = Frames / elapsedTime;
	printf("  %d frames took %.3f s, dots per frame %d, Time per dot %.0f us, fps %.0f \n", Frames, elapsedTime, nDotsPerFrame, fTimePerOperation, fFPS);


	area = woffset-300;
	XPos = area/2;
	YPos = area/2;
	nDots = 0;

	printf("Real pong game:\n");	
	CBall	Ball;
	CLeftRacket	LeftRacket;
	CRightRacket RightRacket;
	int Poti1;
	int Poti2;

	while(1){
		Ball.Draw();
		LeftRacket.Draw();
		RightRacket.Draw();
		
		Poti1 = analogRead(200)*100/1024;
		Poti2 = analogRead(201)*100/1024;
		RightRacket.SetRelativePosY(Poti2);
		LeftRacket.SetRelativePosY(Poti1);
		Ball.MoveXYStep();
		if (LeftRacket.Collision(Ball)){
			// set fly direction depending on where it hit the racket
			// (t is 0.5 if hit at top, 0 at center, -0.5 at bottom)
			float t = ((Ball.PosY - LeftRacket.PosY) / LeftRacket.Height) - 0.5f;
			Ball.ReverseDirX();
			Ball.SetYDirection(t);
		}
		if (RightRacket.Collision(Ball)) {
			float t = ((Ball.PosY - RightRacket.PosY) / RightRacket.Height) - 0.5f;
			Ball.ReverseDirX(); 
			Ball.SetYDirection(t);
		}
	}
	free(pBuffer0);
	free(pBuffer1);
	close(fd);
	exit(EXIT_SUCCESS);
}

