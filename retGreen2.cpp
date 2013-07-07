#include <iostream>
#include <vector>
#include <cstdio>
#include <stdlib.h>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

//! Simple helper function to calculate the absolute value
#define ABS(x) ((x)>0 ? (x) : -1*(x))
//! Additional helper function to compute the sign of a number
#define SIGN(x) (x/ABS(x))

//! Defined if we want to print out debug info
#define DEBUG_RETGREEN 1
#define DEBUG_POMS 1
//! Define for Satan
//#define SATAN 1
//! Define for Anti-ADD meds
//#define RITALIN 1
//! Define if you want to run test cases.
#define TESTCASES_RETGREEN 1
//! Define if we want copies of the pictures saved.
#define LOG 1

#ifdef ONCOMP
char* filename;
#else
#include "kovan/kovan.hpp"
#include "kovan/kovan.h"
//#include "Robot.h"
#endif// ONCOMP

#define RBIAS		5
#define YBARRIER	115
#define CENTERX		100
#define MAXLOST		20
#define MAXCORRECT	10
#define MINVEL		200
#define GAMETIME	110

using namespace cv;
using namespace std;

#define BASKETPORT	1
#define CLAWPORT	3
#define ARMPORT		2
#define LMOTOR		0
#define RMOTOR		2
enum { CLAW_OPEN, CLAW_POPEN, CLAW_CLOSED };
enum { ARM_UP, ARM_DOWN, ARM_BASKET};
enum { BASKET_UP, BASKET_DOWN, BASKET_DUMP };
VideoCapture cap(0);
int ticksLost=0, lastY=-1;
const float errorX=10, errorSep=5;
short lastVel[]={0,0,0,0};
unsigned short lastPos[]={0,0,0,0};
#ifdef LOG
Mat drawinga;
int pic=0;
char dest[150], picCurrent[4];
vector<int> compression_params;
#endif// LOG
#ifdef RITALIN
Point lastCenter=Point(-1, -1);
#endif// RITALIN

class colorRange
{
private:
    unsigned short hueMin, hueRange, satMin, satRange, valMin, valRange;

public:
    colorRange()
    : hueMin(0), hueRange(0), satMin(0), satRange(0), valMin(0), valRange(0)
    { }

    unsigned short getHueMin() {return hueMin;}
    unsigned short getSatMin() {return satMin;}
    unsigned short getValMin() {return valMin;}

    unsigned short getHueRange() {return hueRange;}
    unsigned short getSatRange() {return satRange;}
    unsigned short getValRange() {return valRange;}

    bool setHueMin(unsigned short hmin)
    {
        if(hmin >= 180) return false;
        hueMin = hmin;
        return true;
    }
    bool setSatMin(unsigned short smin)
    {
        if(smin+satRange > 255) return false;
        satMin = smin;
        return true;
    }
    bool setValMin(unsigned short vmin)
    {
        if( vmin+valRange > 255) return false;
        valMin = vmin;
        return true;
    }

    bool setHueRange(unsigned short hrange)
    {
        if(hrange >= 180) return false;
        hueRange = hrange;
        return true;
    }
    bool setSatRange(unsigned short srange)
    {
        if(srange+satMin > 255) return false;
        satRange = srange;
        return true;
    }
    bool setValRange(unsigned short vrange)
    {
        if(vrange+valMin > 255) return false;
        valRange = vrange;
        return true;
    }
};

colorRange orangeRange()
{
    colorRange orange;
    orange.setHueMin(0);
    orange.setHueRange(14);
    orange.setSatMin(105);
    orange.setSatRange(115);
    orange.setValMin(168);
    orange.setValRange(95);
    return orange;
}

colorRange greenRange()
{
    colorRange green;
    green.setHueMin(40);
    green.setHueRange(35);
    green.setSatMin(120);
    green.setSatRange(135);
    green.setValMin(76);
    green.setValRange(154);
    return green;
}

void controlledServo(unsigned char port, unsigned short position, unsigned short milliseconds)
{
	unsigned short intervals=milliseconds/10, delta=(position - lastPos[port])/intervals;
	for(int i=0; i<intervals; i++)
	{
		lastPos[port] += delta;
		set_servo_position(port, lastPos[port]);
		msleep(9);
	}
	set_servo_position(port, position);
	lastPos[port] = position;
}

void moveClaw(int position)
{
    switch (position)
    {
        case CLAW_CLOSED:
            controlledServo(CLAWPORT, 1330, 500);
            break;
		case CLAW_POPEN:
			controlledServo(CLAWPORT, 1710, 500);
            break;
        case CLAW_OPEN:
            controlledServo(CLAWPORT, 2047, 500);
            break;
    }
}

void moveArm(int position)
{
    switch (position)
    {
    	case ARM_BASKET:
			controlledServo(ARMPORT, 1400, 1500);
            break;
        case ARM_UP:
            controlledServo(ARMPORT, 1000, 1000);
            break;
        case ARM_DOWN:
            controlledServo(ARMPORT, 100, 1000);
    }
}

void moveBasket(int position)
{
    switch (position)
    {
        case BASKET_UP:
            controlledServo(BASKETPORT, 920, 600);
            break;
        case BASKET_DOWN:
            controlledServo(BASKETPORT, 500, 600);
            break;
		case BASKET_DUMP:
			controlledServo(BASKETPORT, 1250, 600);
			break;
    }
}

//! Evaluates my custom data type to see whether the first's area is larger than the second's
bool greaterArea(const vector<int> &a, const vector<int> &b)
{
   return a[0] > b[0];
}

/**
 * C++ version 0.4 char* style "itoa":
 * Written by Lukás Chmela
 * Released under GPLv3.
 */
char* itoa(int value, char* result, int base) {
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return result; }

    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

#ifdef RITALIN
int checkContours(const vector< vector<Point> > &contours, const vector<vector<int> > &orderedContours)
{
	if( contours.size() < 1 || orderedContours.size() < 1) return -1;
	int good=0;
	Point2f center;
	float radius;
	//Find radius and center
	if(lastCenter.x > 0 || lastCenter.y > 0)
	{
		for(int i=0; i!=orderedContours.size(); i++)
		{
			minEnclosingCircle((Mat)contours[orderedContours[i][2]], center, radius);
			if( (lastCenter.x-center.x)*(lastCenter.x-center.x)+(lastCenter.y-center.y)*(lastCenter.y-center.y) < ((lastVel[0]*lastVel[0])+lastVel[1]*lastVel[1])/3500)
			{
				good=i;
				break;
			}
		}
	}
	minEnclosingCircle((Mat)contours[orderedContours[good][2]], center, radius);
	lastCenter.x = center.x;
	lastCenter.y = center.y;
	return good;
}
#endif// RITALIN

//! Finds Poms and goes to them based on HSV values
bool goToPom(colorRange range, void* ourBot)
{
    if (!cap.isOpened()) return false;
    vector<vector<int> > orderedContours;
    Mat source, chans, singleChan, tmpMatA, tmpMatB;
    vector< vector<Point> > contours;
    vector<Mat> hueChan(3);
    vector<int> tmpCont(3);
    int tmpInt;
    Point2f center;
    float radius;
    for(int i=0; i < 10; i++)
    {
        cap >> source;
#ifdef LOG
        strcpy(dest, "/kovan/vision/pics/");
        cout << "Pic" << ++pic << endl;
        itoa(pic,picCurrent,10);
        strcat(dest, picCurrent);
        strcat(dest, ".png");
        imwrite(dest, source, compression_params);
#endif
        blur(source, source, Size(5, 5));
        cvtColor(source, chans, CV_BGR2HSV);
        split(chans, hueChan);
//Seperate the channels
//Channel 1(Hue)
        if(range.getHueMin()+range.getHueRange()<180) inRange(hueChan[0], range.getHueMin(), range.getHueMin()+range.getHueRange(), singleChan);
        else
        {
            compare(hueChan[0], range.getHueMin()+range.getHueRange()-180, tmpMatB, CMP_LE);
            compare(hueChan[0], range.getHueMin(), tmpMatA, CMP_GE);
            bitwise_or(tmpMatA, tmpMatB, singleChan, Mat());
        }
        compare(singleChan, 0, singleChan, CMP_GT);
//Channel 2(Saturation)
        inRange(hueChan[1], range.getSatMin(), range.getSatMin()+range.getSatRange(), tmpMatA);
        bitwise_and(singleChan, tmpMatA, singleChan, Mat());
//Channel 3(Value)
        inRange(hueChan[2], range.getValMin(), range.getValMin()+range.getValRange(), tmpMatA);
        bitwise_and(singleChan, tmpMatA, singleChan, Mat());
#ifdef LOG
        strcpy(dest, "/kovan/vision/pics/");
        cout << "Pic" << pic << endl;
        strcat(dest, picCurrent);
        strcat(dest, "A.png");
        imwrite(dest, singleChan, compression_params);
#endif
//Find and sort the contours
        findContours(singleChan, contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
        for(unsigned int i=0; i < contours.size(); i++)
        {
            tmpInt = contourArea(contours[i]);
            //if (tmpInt < 500) continue;
            tmpCont[0]=tmpInt;
            tmpCont[2]=i;
            orderedContours.push_back(tmpCont);
        }
        if (orderedContours.size() < 1) continue;
        sort(orderedContours.begin(), orderedContours.end(), greaterArea);
//Find radius and center
        minEnclosingCircle((Mat)contours[orderedContours[0][2]], center, radius);
#ifdef DEBUG_POMS
        cout << "Center x:" << center.x << " y:" << center.y << " with radius " << radius << " and area " << orderedContours[0][0] << endl;
#endif// DEBUG_POMS
        break;
    }
    while (ABS(center.x - CENTERX) > errorX || center.y < YBARRIER)
    {
#ifdef SATAN
		cout << "Hail Satan meaningless beings" << endl;
#endif
        orderedContours.clear();
        cap >> source;
#ifdef LOG
        strcpy(dest, "/kovan/vision/pics/");
        cout << "Pic" << ++pic << endl;
        itoa(pic,picCurrent,10);
        strcat(dest, picCurrent);
        strcat(dest, ".png");
        imwrite(dest, source, compression_params);
#endif
        blur(source, source, Size(5, 5));
        cvtColor(source, chans, CV_BGR2HSV);
        split(chans, hueChan);
//Seperate the channels
        if(range.getHueMin()+range.getHueRange()<180) inRange(hueChan[0], range.getHueMin(), range.getHueMin()+range.getHueRange(), singleChan);
        else
        {
            compare(hueChan[0], (range.getHueMin()+range.getHueRange())%180, tmpMatB, CMP_LE);
            compare(hueChan[0], range.getHueMin(), tmpMatA, CMP_GE);
            bitwise_or(tmpMatA, tmpMatB, singleChan, Mat());
        }
        compare(singleChan, 0, singleChan, CMP_GT);
//Channel 2(Saturation)
        inRange(hueChan[1], range.getSatMin(), range.getSatMin()+range.getSatRange(), tmpMatA);
        bitwise_and(singleChan, tmpMatA, singleChan, Mat());
//Channel 3(Value)
        inRange(hueChan[2], range.getValMin(), range.getValMin()+range.getValRange(), tmpMatA);
        bitwise_and(singleChan, tmpMatA, singleChan, Mat());
#ifdef LOG
        strcpy(dest, "/kovan/vision/pics/");
        strcat(dest, picCurrent);
        strcat(dest, "A.png");
        imwrite(dest, singleChan, compression_params);
#endif
//Find and sort the contours
        findContours(singleChan, contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
        for(unsigned int i=0; i < contours.size(); i++)
        {
            tmpInt = contourArea(contours[i]);
            //if (tmpInt < 500) continue;
            tmpCont[0]=tmpInt;
            tmpCont[2]=i;
            orderedContours.push_back(tmpCont);
        }
		sort(orderedContours.begin(), orderedContours.end(), greaterArea);
#ifdef RITALIN
		if (orderedContours.size() > 0) tmpInt = checkContours(contours, orderedContours);
		else if (orderedContours.size() < 1)
		{
			lastCenter=Point(-1,-1);
#ifdef DEBUG_POMS
			cout << ((tmpInt < 0) ? "Ridalin failed" : "Can't see nuttin") << endl;
#endif
#else
		if (orderedContours.size() < 1)
		{
#endif
#ifdef DEBUG_POMS
            cout << "We lost da dad gum pom" << endl;
#endif
			if (ticksLost < MAXCORRECT && (lastVel[LMOTOR] != 0  || lastVel[RMOTOR] != 0))
			{
				mav(LMOTOR, -2*lastVel[LMOTOR]);
				mav(RMOTOR, -2*lastVel[RMOTOR]);
			}
			else if (ticksLost < MAXCORRECT+MAXLOST)
			{
				mav(LMOTOR, -200);
				mav(RMOTOR,  200);
			}
			else
			{
				mav(LMOTOR,  200);
				mav(RMOTOR, -200);
			}
			if(ticksLost++ > MAXCORRECT+MAXLOST*3) { ticksLost=0; return false; }
            continue;
        }
//Find radius and center
#ifdef RITALIN
		minEnclosingCircle((Mat)contours[orderedContours[tmpInt][2]], center, radius);
#else
        minEnclosingCircle((Mat)contours[orderedContours[0][2]], center, radius);
#endif// RITALIN
#ifdef DEBUG_POMS
        cout << "Center x:" << center.x << " y:" << center.y << " with radius " << radius << " and area " << orderedContours[0][0] << endl;
#endif// DEBUG_POMS
		lastVel[LMOTOR] = 10*(YBARRIER-center.y) - 4*(CENTERX-center.x);
		lastVel[RMOTOR] = 10*(YBARRIER-center.y) + 4*(CENTERX-center.x);
		tmpInt = (YBARRIER-center.y) != 0 ? SIGN((YBARRIER-center.y)) : -1;
		while (ABS(lastVel[LMOTOR]) < MINVEL || ABS(lastVel[RMOTOR]) < MINVEL)
		{
			lastVel[LMOTOR] += tmpInt;
			lastVel[RMOTOR] += tmpInt;
		}
#ifdef DEBUG_POMS
		cout << "Turning l:" << lastVel[LMOTOR] << " r:" << lastVel[RMOTOR] << endl;
#endif// DEBUG_POMS
		mav(LMOTOR, lastVel[LMOTOR]);
		mav(RMOTOR, lastVel[RMOTOR]);
    }
#ifdef DEBUG_POMS
    cout << "We has da gone ta it" << endl;
#endif
    off(LMOTOR);
    off(RMOTOR);
    return true;
}

//! Tries to move orange away from green(Tracks green)
bool moveOrangeBack(colorRange rangeA, void* ourBot)
{
    bool retval = goToPom(rangeA, 0);
    if(!retval) return false;
    moveClaw(CLAW_OPEN);
//Grab it, move it back and turn to look at it
    mav(LMOTOR, 1500);
    mav(RMOTOR, 1500);
    msleep(250);
    moveClaw(CLAW_CLOSED);
    mav(LMOTOR, -1500);
    mav(RMOTOR, -1500);
    msleep(750);
    mav(LMOTOR,  500);
    mav(RMOTOR, -500);
    moveClaw(CLAW_OPEN);
    mav(LMOTOR, -500);
    mav(RMOTOR,  500);
    msleep(750);
    mav(LMOTOR, -1000);
    mav(RMOTOR, -1000);
    msleep(1250);
    mav(LMOTOR,  900);
    mav(RMOTOR, -900);
    msleep(100);
    off(LMOTOR);
    off(RMOTOR);
#ifdef RITALIN
	//invalidate the sensor after we screw with it
	lastCenter=Point(-1, -1);
#endif
    return true;
}

//! Attempts to maneuver the green into the claw without orange
/**
 \param [in] rangeA the range for the color we want to find
 \param [in] rangeB the range of the color we want to avoid
 */
bool retrieveGreen(colorRange rangeA, colorRange rangeB, void* ourBot)
{
#ifdef ONCOMP
    Mat drawing;
    int width=160, height=120;
#else
//Create our camera
    if (!cap.isOpened()) return false;
#endif// ONCOMP
//Define misc variables
    vector<vector<int> > orderedContoursA, orderedContoursB;
    Mat source, chans, singleChan, tmpMatA, tmpMatB;
    vector<vector<Point> > contoursA, contoursB;
    vector<Mat> hueChan(3);
    vector<int> tmpCont(3);
    int tmpInt;
    Point2f centerA, centerB;
    float radiusA, radiusB;
    for(int i=0; i<8; i++)
    {
        moveOrangeBack(rangeA,0);//) return false;
//clear out contours
        orderedContoursA.clear();
        orderedContoursB.clear();
//Find contours for what we want to grab to start
        cap >> source;
#ifdef LOG
        strcpy(dest, "/kovan/vision/pics/");
        cout << "Pic" << ++pic << endl;
        itoa(pic,picCurrent,10);
        strcat(dest, picCurrent);
        strcat(dest, ".png");
        imwrite(dest, source, compression_params);
#endif
//Setup the image for parsing
        blur(source, source, Size(5, 5));
        cvtColor(source, chans, CV_BGR2HSV);
        split(chans, hueChan);
//Filter the image
        if(rangeA.getHueMin()+rangeA.getHueRange()<180) inRange(hueChan[0], rangeA.getHueMin(), rangeA.getHueMin()+rangeA.getHueRange(), singleChan);
        else
        {
            compare(hueChan[0], (rangeA.getHueMin()+rangeA.getHueRange())%180, tmpMatB, CMP_LE);
            compare(hueChan[0], rangeA.getHueMin(), tmpMatA, CMP_GE);
            bitwise_or(tmpMatA, tmpMatB, singleChan, Mat());
        }
        compare(singleChan, 0, singleChan, CMP_GT);
//Channel 2(Saturation)
        inRange(hueChan[1], rangeA.getSatMin(), rangeA.getSatMin()+rangeA.getSatRange(), tmpMatA);
        bitwise_and(singleChan, tmpMatA, singleChan, Mat());
//Channel 3(Value)
        inRange(hueChan[2], rangeA.getValMin(), rangeA.getValMin()+rangeA.getValRange(), tmpMatA);
        bitwise_and(singleChan, tmpMatA, singleChan, Mat());
#ifdef LOG
        strcpy(dest, "/kovan/vision/pics/");
        strcat(dest, picCurrent);
        strcat(dest, "A.png");
        imwrite(dest, singleChan, compression_params);
#endif// LOG
//Find the contours and sort them by area
        findContours(singleChan, contoursA, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
        for(unsigned int j=0; j < contoursA.size(); j++)
        {
            tmpInt = contourArea(contoursA[j]);
            //if (tmpInt < 500) continue;
            tmpCont[0]=tmpInt;
            tmpCont[2]=j;
            orderedContoursA.push_back(tmpCont);
        }
        sort(orderedContoursA.begin(), orderedContoursA.end(), greaterArea);
#ifdef RITALIN

		if (orderedContoursA.size() > 0) tmpInt=checkContours(contoursA, orderedContoursA);
        if(orderedContoursA.size() < 1)
        {
        	lastCenter=Point(-1,-1);
#else
        if (orderedContoursA.size() < 1) //We couldn't find anything to look for
        {
#endif
#ifdef DEBUG_RETGREEN
            cout << "We were unable find what we wanted" << endl;
#endif
            return false;
        }
#ifdef RITALIN
	minEnclosingCircle((Mat)contoursA[orderedContoursA[tmpInt][2]], centerA, radiusA);
#else
	minEnclosingCircle((Mat)contoursA[orderedContoursA[0][2]], centerA, radiusA);
#endif
//Find the contours for what we want to avoid next
        if(rangeB.getHueMin()+rangeB.getHueRange()<180) inRange(hueChan[0], rangeB.getHueMin(), rangeB.getHueMin()+rangeB.getHueRange(), singleChan);
        else
        {
            compare(hueChan[0], (rangeB.getHueMin()+rangeB.getHueRange())%180, tmpMatB, CMP_LE);
            compare(hueChan[0], rangeB.getHueMin(), tmpMatA, CMP_GE);
            bitwise_or(tmpMatA, tmpMatB, singleChan, Mat());
        }
        compare(singleChan, 0, singleChan, CMP_GT);
//Channel 2(Saturation
        inRange(hueChan[1], rangeB.getSatMin(), rangeB.getSatMin()+rangeB.getSatRange(), tmpMatA);
        bitwise_and(singleChan, tmpMatA, singleChan, Mat());
//Channel 3(Value)
        inRange(hueChan[2], rangeB.getValMin(), rangeB.getValMin()+rangeB.getValRange(), tmpMatA);
        bitwise_and(singleChan, tmpMatA, singleChan, Mat());
#ifdef LOG
        strcpy(dest, "/kovan/vision/pics/");
        strcat(dest, picCurrent);
        strcat(dest, "B.png");
        imwrite(dest, singleChan, compression_params);
#endif// LOG
//Find the contours from the filtered image and sort by size
        findContours(singleChan, contoursB, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
        for(unsigned int j=0; j < contoursB.size(); j++)
        {
            tmpInt = contourArea(contoursB[j]);
            if (tmpInt < 10) continue;
            tmpCont[0]=tmpInt;
            tmpCont[2]=j;
            orderedContoursB.push_back(tmpCont);
        }

#ifdef DEBUG_RETGREEN
        cout << "We found it to start" << endl;
#endif

		cout << "Good Center x:" << centerA.x << " y:" << centerA.y << " With radius " << radiusA << " and Area " << orderedContoursA[0][0] << endl;
        for(unsigned int j=0; j < orderedContoursB.size(); j++)
        {
            minEnclosingCircle((Mat)contoursB[orderedContoursB[j][2]], centerB, radiusB);
            if(((centerA.x-centerB.x)*(centerA.x-centerB.x) + (centerA.y-centerB.y)*(centerA.y-centerB.y) >= (errorSep+radiusB)*(errorSep+radiusB)))// || ((centerA.y-radiusA <= centerB.y) && (ABS(centerA.x-centerB.x) <= 15+2*radiusB)))
            {
            	moveClaw(CLAW_CLOSED);
            	goToPom(rangeA, 0);
            	mav(LMOTOR, -900);
            	mav(RMOTOR, -900);
            	msleep(200);
            	off(LMOTOR);
				off(RMOTOR);
				moveClaw(CLAW_OPEN);
				mav(LMOTOR, 900);
				mav(RMOTOR, 900);
				msleep(750);
				off(LMOTOR);
				off(RMOTOR);
				moveClaw(CLAW_CLOSED);
				cout << "We got it" << endl;
				return true;
            }
            else
            {
				cout << "Bad Center x:" << centerB.x << " y:" << centerB.y << " With radius " << radiusB << " and Area " << orderedContoursB[j][0] << endl;
				cout << "It is too close to the orange" << endl;
				break;
            }
        }
    }
    disable_servos();
    cout << "We couldn't find nuttin" << endl;
    return false;
}

#ifdef TESTCASES_POMS
int main(int argc, char* argv[])
{
#ifdef LOG
	compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
	compression_params.push_back(0);
#endif
    goToPom(orangeRange(), 0);
    off(LMOTOR);
    off(RMOTOR);
    return 0;
}
#else
#ifdef TESTCASES_RETGREEN
int main(int argc, char* argv[])
{
	system("cd /kovan/vision");
#ifdef LOG
	compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
	compression_params.push_back(0);
#endif
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 160);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 120);
    Mat sourcetmp;
    for(int i=0; i<15; i++) cap >> sourcetmp;
    moveArm(ARM_DOWN);
    moveBasket(BASKET_DOWN);
    moveClaw(CLAW_POPEN);
    enable_servos();
    wait_for_light(0);
    shut_down_in(GAMETIME);
    mav(LMOTOR, 1000);
    mav(RMOTOR, 1000);
    msleep(3000);
    mav(LMOTOR,  1000);
    mav(RMOTOR, -1000);
    msleep(800);
    off(LMOTOR);
    off(RMOTOR);
    for (int i=0; i<4; i++)
    {
		if(retrieveGreen(greenRange(), orangeRange(), 0))
		{
			moveBasket(BASKET_UP);
			moveArm(ARM_BASKET);
			moveClaw(CLAW_OPEN);
			moveArm(ARM_DOWN);
			moveClaw(CLAW_CLOSED);
			moveBasket(BASKET_DOWN);
		}
		else
		{
#ifdef DEBUG
			cout << "We lost everything good and wonderful" << endl;
#endif// DEBUG
			break;
		}
		mav(LMOTOR, -1000);
		mav(RMOTOR, -1000);
		msleep(2500);
		off(LMOTOR);
		off(RMOTOR);
    }
    mav(LMOTOR, -1000);
    mav(RMOTOR, -1000);
    msleep(5000);
    mav(LMOTOR, 1000);
    mav(RMOTOR,  750);
    msleep(1500);
    moveArm(ARM_UP);
    mav(LMOTOR, -1000);
    mav(RMOTOR, 1000);
    msleep(2500);
    ao();
    moveBasket(BASKET_DUMP);
    disable_servos();
    cout << "We are done executing" << endl;
    return 0;
}
#endif// TESTCASES_RETGREEN
#endif// TESTCASES_POMS
