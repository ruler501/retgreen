#include <iostream>
#include <cstdio>
#include <stdlib.h>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "kovan/kovan.hpp"
#include "kovan/kovan.h"

//!Simple helper function to calculate the absolute value
#define ABS(x) ((x)>0 ? (x) : -1*(x))

//! Define if we want copies of the pictures saved.
#define LOG 1

using namespace cv;
using namespace std;

VideoCapture cap(0);

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

int main(int argc, char* argv[])
{
#ifdef LOG
    Mat drawinga;
    char dest[150], picCurrent[4];
    int pic=0;
    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(0);
#endif// LOG
    if (!cap.isOpened()) return false;
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 160);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 120);
    Mat source;
    for(int i=0; i<15; i++){ cap >> source; }
#ifdef LOG
    for (int i=0; i<15; i++)
    {
        strcpy(dest, "pics/");
        pic++;
        cout << "Pic" << pic << endl;
        itoa(pic,picCurrent,10);
        strcat(dest, picCurrent);
        strcat(dest, ".png");
        imwrite(dest, source, compression_params);
    }
#endif
    return 0;
}
