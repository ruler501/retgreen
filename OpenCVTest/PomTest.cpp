#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;
using namespace std;

short threshh_min=1, threshh_range=10, threshs_min=127, threshs_range=128, threshv_min=127, threshv_range=128;
Mat source;
vector<Mat> hueChan(3);

bool greaterArea(const vector<int> &a, const vector<int> &b)
{
   return a[0] > b[0];
}

void findPoms()
{
    //int largestSize=0, tmpInt=0, largestLength=0, tmpLength=0;
    Mat singleChan, tmpMatA, tmpMatB;
    vector< vector<Point> > contours;
    vector< vector<int> > goodContours;
    vector<int> tmpCont(3);
    int tmpInt;
    //Channel One
    if(threshh_min+threshh_range<180) inRange(hueChan[0], threshh_min, threshh_min+threshh_range, singleChan);
    else
    {
        compare(hueChan[0], (threshh_min+threshh_range)%180, tmpMatB, CMP_LE);
        compare(hueChan[0], threshh_min, tmpMatA, CMP_GE);
        bitwise_or(tmpMatA, tmpMatB, singleChan, Mat());
    }
    compare(singleChan, 0, singleChan, CMP_GT);
    inRange(hueChan[1], threshs_min, threshs_min+threshs_range, tmpMatA);
    bitwise_and(singleChan, tmpMatA, singleChan, Mat());
    inRange(hueChan[2], threshv_min, threshv_min+threshv_range, tmpMatA);
    bitwise_and(singleChan, tmpMatA, singleChan, Mat());

    namedWindow("Drawing", CV_WINDOW_AUTOSIZE);
    imshow("Drawing", singleChan);

    findContours(singleChan, contours, CV_RETR_LIST, CV_CHAIN_APPROX_NONE, Point(0, 0));

    tmpCont[1]=5;
    for(unsigned int i=0; i < contours.size(); i++)
    {
        tmpInt= contourArea(contours[i]);
        //if (tmpInt < 500) continue;
        tmpCont[0]= tmpInt;
        //tmpCont[1]=arcLength(contours[i],0);
        tmpCont[2]=i;
        goodContours.push_back(tmpCont);
    }
    sort(goodContours.begin(), goodContours.end(), greaterArea);
    //sort(goodContours.begin(), goodContours.end(), greaterArea);
    Mat drawing = Mat::zeros( singleChan.size(), CV_8UC3 );
    RNG rng(12345);
    unsigned int maxDepth = 1;//goodContours.size();
    if (goodContours.size() < maxDepth) maxDepth=goodContours.size();
    Rect centerCalc;
    for(unsigned int i=0; i < maxDepth; i++)
    {
        Scalar color = Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) );
        drawContours( drawing, contours, goodContours[i][2], color, 1, 8, vector<Vec4i>(), 0, Point() );
        centerCalc = boundingRect(contours[goodContours[i][2]]);
        circle(drawing, Point(centerCalc.x+centerCalc.width/2, centerCalc.y+centerCalc.height/2),1,Scalar(255,0,0),1,8,0);
    }
    namedWindow("Contours", CV_WINDOW_AUTOSIZE);
    imshow("Contours", drawing);
    if (goodContours.size()>0) cout << goodContours[0][0] << " Area, " << goodContours[0][1] << " Arc Length" << endl;
}

void ChangeHLevel(int h_min,void*)
{
    threshh_min=h_min;
    findPoms();
}

void ChangeSLevel(int s_min,void*)
{
    threshs_min=s_min;
    findPoms();
}

void ChangeVLevel(int v_min,void*)
{
    threshv_min=v_min;
    findPoms();
}

void ChangeHRange(int h_range,void*)
{
    threshh_range=h_range;
    findPoms();
}

void ChangeSRange(int s_range,void*)
{
    threshs_range=s_range;
    findPoms();
}

void ChangeVRange(int v_range,void*)
{
    threshv_range=v_range;
    findPoms();
}

int main(int argc, char* argv[])
{
    if (argc != 2) return -1;
    int *quickTestH=new int, *quickTestS=new int, *quickTestV=new int;
    int *quickTestHR=new int, *quickTestSR=new int, *quickTestVR=new int;
    Mat chans;
    source = imread(argv[1], 1);
    if (source.data==NULL) return 666;
    namedWindow("Source", CV_WINDOW_AUTOSIZE);
    createTrackbar("Hue Min:", "Source", quickTestH, 180, ChangeHLevel);
    createTrackbar("Saturation Min:", "Source", quickTestS, 255, ChangeSLevel);
    createTrackbar("Value Min:", "Source", quickTestV, 255, ChangeVLevel);
    createTrackbar("Hue Range:", "Source", quickTestHR, 179, ChangeHRange);
    createTrackbar("Saturation Range:", "Source", quickTestSR, 254, ChangeSRange);
    createTrackbar("Value Range:", "Source", quickTestVR, 254, ChangeVRange);
    imshow("Source", source);
    blur(source, source, Size(5, 5));
    cvtColor(source, chans, CV_BGR2HSV);

    namedWindow("HSV", CV_WINDOW_AUTOSIZE);
    imshow("HSV", chans);
    //blur(chans, chans, Size(5, 5));
    split(chans, hueChan);

    findPoms();
    waitKey(0);
    /*findPoms("Test2.jpg");
    waitKey(2500);

    findPoms("Test3.jpg");
    waitKey(2500);
    findPoms("Test4.jpg");
    waitKey(2500);*/
    return 0;
}
