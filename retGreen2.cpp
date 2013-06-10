#include <iostream>
#include <cstdio>
#include <stdlib.h>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

//!Simple helper function to calculate the absolute value
#define ABS(x) ((x)>0 ? (x) : -1*(x))

//! Defined if we want to print out debug info
#define DEBUG_RETGREEN 1
#define DEBUG_POMS 1
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

#define RBIAS     5
#define YBARRIER 75
#define CENTERX  120

using namespace cv;
using namespace std;

#define BASKETPORT 1
#define CLAWPORT   3
#define ARMPORT    2
enum { CLAW_OPEN, CLAW_POPEN, CLAW_CLOSED };
enum { ARM_UP, ARM_DOWN, ARM_BASKET};
enum { BASKET_UP, BASKET_DOWN, BASKET_DUMP };
VideoCapture cap(0);
int ticksLost=0, pic=0;
const float errorX=5;

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
        if(hmin < 0 || hmin >= 180) return false;
        hueMin = hmin;
        return true;
    }
    bool setSatMin(unsigned short smin)
    {
        if(smin < 0 || smin+satRange > 255) return false;
        satMin = smin;
        return true;
    }
    bool setValMin(unsigned short vmin)
    {
        if(vmin < 0 || vmin+valRange > 255) return false;
        valMin = vmin;
        return true;
    }

    bool setHueRange(unsigned short hrange)
    {
        if(hrange < 0 || hrange >= 180) return false;
        hueRange = hrange;
        return true;
    }
    bool setSatRange(unsigned short srange)
    {
        if(srange < 0 || srange+satMin > 255) return false;
        satRange = srange;
        return true;
    }
    bool setValRange(unsigned short vrange)
    {
        if(vrange < 0 || vrange+valMin > 255) return false;
        valRange = vrange;
        return true;
    }
};

/*
colorRange orangeRange()
{
    colorRange orange;
    orange.setHueMin(6);
    orange.setHueRange(13);
    orange.setSatMin(10);
    orange.setSatRange(80);
    orange.setValMin(196);
    orange.setValRange(59);
    return orange;
}
*/

colorRange orangeRange()
{
    colorRange orange;
    orange.setHueMin(6);
    orange.setHueRange(8);
    orange.setSatMin(170);
    orange.setSatRange(75);
    orange.setValMin(145);
    orange.setValRange(75);
    return orange;
}

/*
colorRange greenRange()
{
    colorRange green;
    green.setHueMin(37);
    green.setHueRange(33);
    green.setSatMin(95);
    green.setSatRange(106);
    green.setValMin(104);
    green.setValRange(60);
    return green;
}
*/
//119+-10
colorRange greenRange()
{
    colorRange green;
    green.setHueMin(25);
    green.setHueRange(80);
    green.setSatMin(135);
    green.setSatRange(120);
    green.setValMin(76);
    green.setValRange(75);
    return green;
}

void moveClaw(int position)
{
    switch (position)
    {
        case CLAW_CLOSED:
            set_servo_position(CLAWPORT, 930);
            break;
		case CLAW_POPEN:
			set_servo_position(CLAWPORT, 1300);
            break;
        case CLAW_OPEN:
            set_servo_position(CLAWPORT, 1600);
            break;
    }
    msleep(100);
}

void moveArm(int position)
{
    switch (position)
    {
    	case ARM_BASKET:
			set_servo_position(ARMPORT, 1500);
            break;
        case ARM_UP:
            set_servo_position(ARMPORT, 1000);
            break;
        case ARM_DOWN:
            set_servo_position(ARMPORT, 0);
    }
    msleep(100);
}

void moveBasket(int position)
{
    switch (position)
    {
        case BASKET_UP:
            set_servo_position(BASKETPORT, 550);
            break;
        case BASKET_DOWN:
            set_servo_position(BASKETPORT, 500);
            break;
		case BASKET_DUMP:
			set_servo_position(BASKETPORT, 1250);
			break;
    }
    msleep(0);
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

//! Finds Poms and goes to them based on hsv values
bool goToPom(colorRange range, void* ourBot)
{
#ifdef LOG
    Mat drawinga;
    char dest[150], picCurrent[4];
    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(0);
#endif// LOG
    if (!cap.isOpened()) return false;
    //int width=cap.get(CV_CAP_PROP_FRAME_WIDTH), height=cap.get(CV_CAP_PROP_FRAME_HEIGHT);
    vector<vector<int> > orderedContours;
    Mat source, chans, singleChan, tmpMatA, tmpMatB;
    vector< vector<Point> > contours;
    vector<Mat> hueChan(3);
    vector<int> tmpCont(3);
    int goodContour=-1, tmpInt;
    Point2f center;
    float radius;
    for(int i=0; i < 10; i++)
    {
        cap >> source;
#ifdef LOG
        strcpy(dest, "pics/");
        pic++;
        cout << "Pic" << pic << endl;
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
        strcpy(dest, "pics/");
        pic++;
        cout << "Pic" << pic << endl;
        itoa(pic,picCurrent,10);
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
        goodContour = 0;
        break;
#ifdef DEBUG_POMS
        cout << "Center x:" << center.x << " y:" << center.y << " with radius " << radius << " and area " << orderedContours[0][0] << endl;
#endif// DEBUG_POMS
    }
    while (ABS(center.x - CENTERX) > errorX || center.y < YBARRIER)
    {
        orderedContours.clear();
        cap >> source;
#ifdef LOG
        strcpy(dest, "pics/");
        pic++;
        cout << "Pic" << pic << endl;
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
        strcpy(dest, "pics/");
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
        if (orderedContours.size() < 1)
        {
#ifdef DEBUG_POMS
            cout << "We lost the pom" << endl;
#endif
            mav(0, -150);
            mav(2,  150);
            continue;
        }
        sort(orderedContours.begin(), orderedContours.end(), greaterArea);
//Find radius and center
        minEnclosingCircle((Mat)contours[orderedContours[0][2]], center, radius);
        goodContour = 0;
#ifdef DEBUG_POMS
        cout << "Center x:" << center.x << " y:" << center.y << " with radius " << radius << " and area " << orderedContours[0][0] << endl;
        cout << "Turning l:" << 10*(YBARRIER-center.y) - 2*(CENTERX-center.x) << " r:" << 10*(YBARRIER-center.y) + 2*(CENTERX-center.x) << endl;
#endif// DEBUG_POMS
        mav(0, 15*(YBARRIER-center.y) - 3*(CENTERX-center.x));
        mav(2, 15*(YBARRIER-center.y) + 3*(CENTERX-center.x));
    }
#ifdef DEBUG_POMS
    cout << "We has da gots em" << endl;
#endif
    //mav(0,  400);
    //mav(2, -400);
    //msleep(100);
    off(0);
    off(2);
    return true;
}

bool moveOrangeBack(colorRange rangeA, void* ourBot)
{
    bool retval = goToPom(rangeA, 0);
    if(!retval) return false;
    moveArm(ARM_DOWN);
    moveBasket(BASKET_UP);
    moveClaw(CLAW_OPEN);
    enable_servos();
//Grab it, move it back and turn to look at it
    mav(0, 900);
    mav(2, 900);
    msleep(1650);
    moveClaw(CLAW_CLOSED);
    mav(0, -900);
    mav(2, -900);
    msleep(750);
    moveClaw(CLAW_OPEN);
    mav(0, -900);
    mav(2, -900);
    msleep(900);
    mav(0, -200);
    mav(2,  200);
    msleep(150);
    off(0);
    off(2);
    return retval;
}

//! Attempts to maneuver the green into the claw without orange
/**
 \param [in] rangeA the range for the color we want to find
 \param [in] rangeB the range of the color we want to avoid
 */
bool retrieveGreen(colorRange rangeA, colorRange rangeB, void* ourBot)
{
#ifdef LOG
    Mat drawinga;
    char dest[150], picCurrent[4];
//    int pic=0;
    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(0);
#endif// LOG
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
    vector< vector<Point> > contoursA, contoursB;
    vector<Mat> hueChan(3);
    vector<int> tmpCont(3);
    int tmpInt;
    Point2f centerA, centerB;
    float radiusA, radiusB;
    bool seperated=true;
    for(int i=0; i<5; i++)
    {
        moveOrangeBack(rangeB,0);
//Figure out if it worked
        cap >> source;
//clear out contours
        orderedContoursA.clear();
        orderedContoursB.clear();
//Find contours for what we want to grab to start
        cap >> source;
#ifdef LOG
        strcpy(dest, "pics/");
        pic++;
        cout << "Pic" << pic << endl;
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
        strcpy(dest, "pics/");
        itoa(pic,picCurrent,10);
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
        if (orderedContoursA.size() < 1) //We couldn't find anything to look for
        {
#ifdef DEBUG_RETGREEN
            cout << "We were unable find what we wanted" << endl;
#endif
            return false;
        }
        sort(orderedContoursA.begin(), orderedContoursA.end(), greaterArea);
//Find the min enclosing circle for center
        minEnclosingCircle((Mat)contoursA[orderedContoursA[0][2]], centerA, radiusA);
#ifdef DEBUG_RETGREEN
        cout << "Good Center x:" << centerA.x << " y:" << centerA.y << " With radius " << radiusA << " and area " << orderedContoursA[0][0] << endl;
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
        strcpy(dest, "pics/");
        itoa(pic,picCurrent,10);
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
    //       if (orderedContoursB.size() > 0) sort(orderedContoursA.begin(), orderedContoursB.end(), greaterArea);

#ifdef DEBUG_RETGREEN
        cout << "We found it to start" << endl;
#endif


        for(unsigned int j=0; j < orderedContoursB.size() && seperated; j++)
        {
            minEnclosingCircle((Mat)contoursB[orderedContoursB[j][2]], centerB, radiusB);
            seperated = ((centerA.x-centerB.x)*(centerA.x-centerB.x) + (centerA.y-centerB.y)*(centerA.y-centerB.y) >= (25+2*radiusB)*(25+2*radiusB)) || ((centerA.y-5 <= centerB.y) && (ABS(centerA.x-centerB.x) <= 15+2*radiusB));
#ifdef DEBUG_RETGREEN
            cout << "Bad Center x:" << centerB.x << " y:" << centerB.y << " With radius " << radiusB << " and Area " << orderedContoursB[j][0] << endl;
            if(!seperated) cout << "It is too close to the orange" << endl;
#endif
        }

        if(seperated)
        {
            goToPom(rangeA, 0);
            mav(0, 900);
            mav(2, 900);
            msleep(1500);
            moveClaw(CLAW_CLOSED);
            cout << "We got it" << endl;
            disable_servos();
            return true;
        }
        else
        {
            if(centerA.y < YBARRIER)
#ifdef DEBUG_RETGREEN
            cout << "It didn't work. Implement backup" << endl;;
#endif
        }
    }
    disable_servos();
    return false;
}

#ifdef TESTCASES_POMS
int main(int argc, char* argv[])
{
    goToPom(orangeRange(), 0);
    off(0);
    off(2);
    return 0;
}
#else
#ifdef TESTCASES_RETGREEN
int main(int argc, char* argv[])
{
	cap.set(CV_CAP_PROP_FRAME_WIDTH, 160);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 120);
    Mat sourcetmp;
    for(int i=0; i<15; i++) cap >> sourcetmp;
    retrieveGreen(greenRange(), orangeRange(), 0);
    off(0);
    off(2);
    return 0;
}
#endif// TESTCASES_RETGREEN
#endif// TESTCASES_POMS
