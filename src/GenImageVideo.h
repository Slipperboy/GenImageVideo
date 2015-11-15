#ifndef GENIMAGEVIDEO_H
#define GENIMAGEVIDEO_H
#include <gdal_priv.h>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

void GenerateVideo(const char * dir,const char *videopath,bool isColor);
void GDAL2Mat(const char* fileName,cv::Mat &dstMat,int bufWidth,int bufHeight,
	int startCol,int startRow,int colNum,int rowNum);

#endif