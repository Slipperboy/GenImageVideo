#include "GenImageVideo.h"
#include <iostream>
#include <vector>
#include <boost/filesystem.hpp>
using std::string;
using std::vector;
using namespace cv;

void LinearStretch16S(Mat &src,Mat &dst,double minVal,double maxVal)
{
	ushort data;
	ushort result;
	for (int x=0;x<src.cols;x++)
	{
		for (int y=0;y<src.rows;y++)
		{
			data=src.at<ushort>(y,x);
			if (data>maxVal)
			{
				result=255;
			}
			else if (data<minVal)
			{
				result=0;
			}
			else
			{
				result=(data-minVal)/(maxVal-minVal)*255;
			}
			dst.at<uchar>(y,x)=result;
		}
	}
}

void HistogramAccumlateMinMax16S(const Mat &mat,double *minVal,double *maxVal)
{
	double p[1024],p1[1024],num[1024];

	memset(p,0,sizeof(p));
	memset(p1,0,sizeof(p1));
	memset(num,0,sizeof(num));

	int height=mat.rows;
	int width=mat.cols;
	long wMulh = height * width;

	//statistics
	for(int x=0;x<width;x++)
	{
		for(int y=0;y<height;y++){
			ushort v=mat.at<ushort>(y,x);
			num[v]++;
		}
	}

	//calculate probability
	for(int i=0;i<1024;i++)
	{
		p[i]=num[i]/wMulh;
	}

	int min=0,max=0;
	double minProb=0.0,maxProb=0.0;
	while(min<1024&&minProb<0.02)
	{
		minProb+=p[min];
		min++;
	}
	do 
	{
		maxProb+=p[max];
		max++;
	} while (max<1024&&maxProb<0.98);

	*minVal=min;
	*maxVal=max;
}

//按照1920X1080分辨率伸缩图像
void ReComputeBuffsize(double width,double height,int &bufWidth, int &bufHeight)
{
	if (width<1920&&height<1080)
	{
		bufWidth=(int)width;
		bufHeight=(int)height;
		return;
	}
	if (width/height-16/9>1e-16)
	{
		bufWidth=1920;
		bufHeight=1920/width*height+0.5;
	}
	else
	{
		bufHeight=1080;
		bufWidth=1080/height*width+0.5;
	}
}

/** 
读取一定范围的tiff影像，将数据保存在Mat结构中
@param filename 遥感影像的路径 
@param dstMat 要保存数据的Mat
@param bufWidth 保存数据数Mat的列数 
@param bufHeight 保存数组Mat的行数
@param startCol 读取的起始列
@param startRow 读取的起始行
@param startCol 读取的列数
@param startRow 读取的行数

*/
void GDAL2Mat(const char* fileName,Mat &dstMat,int bufWidth,int bufHeight,
	int startCol,int startRow,int colNum,int rowNum)
{
	GDALAllRegister();  
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO");   
	GDALDataset *poDataset = (GDALDataset *)GDALOpen(fileName,GA_ReadOnly);   // GDAL数据集
	int imgWidth=poDataset->GetRasterXSize();
	int imgHeight=poDataset->GetRasterYSize();

	int tmpBandSize = poDataset->GetRasterCount();
	GDALDataType dataType = poDataset->GetRasterBand(1)->GetRasterDataType();
	GDALRasterBand *pBand;
	//需要考虑数据类型，波段数以及有无投影
	switch(dataType)
	{
	case GDT_Byte:
		{
			uchar *pafScan= new uchar[bufWidth*bufHeight]; // 存储数据
			//8bit灰度图像处理
			if (tmpBandSize==1)
			{
				dstMat = cv::Mat(bufHeight,bufWidth,CV_8UC1);
				pBand = poDataset->GetRasterBand(1);
				//将指定范围的数据读到数组中
				pBand->RasterIO(GF_Read,startCol,startRow,colNum,rowNum,pafScan,bufWidth,bufHeight,GDT_Byte,0,0);
				for(int i=0;i<dstMat.rows;i++)
				{
					for (int j=0;j<dstMat.cols;j++)
					{
						//对dstMat进行填充
						dstMat.at<uchar>(i,j)=*(pafScan+i*dstMat.cols+j);
					}
				}
			}
			
			//8bit多光谱图像处理，注意波段数不一定是3个
			if (tmpBandSize>=3)
			{
				//这里选取的前三个波段组合
				dstMat = cv::Mat(bufHeight,bufWidth,CV_8UC3);
				for (int bgr=3;bgr>=1;bgr--)
				{
					pBand = poDataset->GetRasterBand(bgr);
					pBand->RasterIO(GF_Read,startCol,startRow,colNum,rowNum,pafScan,bufWidth,bufHeight,GDT_Byte,0,0);
					for(int i=0;i<dstMat.rows;i++)
					{
						for (int j=0;j<dstMat.cols;j++)
						{
							dstMat.at<cv::Vec3b>(i,j)[3-bgr]=*(pafScan+i*dstMat.cols+j);
						}
					}
				}
			}
			delete pafScan;
			break;
		}
	case GDT_UInt16:
		{
			ushort *pafScanS= new ushort[bufWidth*bufHeight];
			//16bit灰度图像处理
			if (tmpBandSize==1)
			{
				Mat tmpMat=cv::Mat(bufHeight,bufWidth,CV_16SC1);
				dstMat = cv::Mat(bufHeight,bufWidth,CV_8UC1);
				pBand = poDataset->GetRasterBand(1);
				pBand->RasterIO(GF_Read,startCol,startRow,colNum,rowNum,pafScanS,
					bufWidth,bufHeight,GDT_UInt16,0,0);
				for(int i=0;i<tmpMat.rows;i++)
				{
					for (int j=0;j<tmpMat.cols;j++)
					{
						tmpMat.at<ushort>(i,j)=*(pafScanS+i*tmpMat.cols+j);
					}
				}
				double min,max;
				//累积灰度直方图统计
				HistogramAccumlateMinMax16S(tmpMat,&min,&max);
				//转化为8bit，对图像进行2%，98%的最大最小值拉伸，使得显示效果更好
				LinearStretch16S(tmpMat,dstMat,min,max);
			}
			//16bit多光谱图像处理，需要对组合的波段分别进行处理
			if (tmpBandSize==4)
			{
				//这里采用了4,3,2波段进行组合
				Mat tmpMat=cv::Mat(bufHeight,bufWidth,CV_16SC3);
				vector<Mat> channels;
				vector<Mat> channels8U; 
				for (int bgr=4;bgr>=2;bgr--)
				{
					pBand = poDataset->GetRasterBand(bgr);
					pBand->RasterIO(GF_Read,startCol,startRow,colNum,rowNum,pafScanS,
						bufWidth,bufHeight,GDT_UInt16,0,0);
					for(int i=0;i<tmpMat.rows;i++)
					{
						for (int j=0;j<tmpMat.cols;j++)
						{
							tmpMat.at<cv::Vec3w>(i,j)[4-bgr]=*(pafScanS+i*tmpMat.cols+j);
						}
					}
				}
				Mat imageBlue,imageGreen,imageRed,imgBlue8U,imgGreen8U,imgRed8U;
				//通道的拆分  
				split(tmpMat,channels);
				imageBlue = channels.at(0);  
				imageGreen = channels.at(1);  
				imageRed = channels.at(2);

				double min,max;
				imgBlue8U=cv::Mat(bufHeight,bufWidth,CV_8UC1);
				imgGreen8U=cv::Mat(bufHeight,bufWidth,CV_8UC1);
				imgRed8U=cv::Mat(bufHeight,bufWidth,CV_8UC1);

				
				HistogramAccumlateMinMax16S(imageBlue,&min,&max);
				LinearStretch16S(imageBlue,imgBlue8U,min,max);
				HistogramAccumlateMinMax16S(imageGreen,&min,&max);
				LinearStretch16S(imageGreen,imgGreen8U,min,max);
				HistogramAccumlateMinMax16S(imageRed,&min,&max);
				LinearStretch16S(imageRed,imgRed8U,min,max);
				channels8U.push_back(imgRed8U);
				channels8U.push_back(imgGreen8U);
				channels8U.push_back(imgBlue8U);
				merge(channels8U,dstMat);

			}
			delete []pafScanS;
			break;
		}
	}

	GDALClose(poDataset);
}


void ReadDirectory(const char *dir, const char * ext, vector<string> &files)
{
	boost::filesystem::directory_iterator end_iter;
	for (boost::filesystem::directory_iterator file_itr(dir); file_itr != end_iter; ++file_itr)
	{
		if (!boost::filesystem::is_directory(*file_itr) && (boost::filesystem::extension(*file_itr)==ext))        // 文件后缀
		{
			files.push_back(file_itr->path().string());    //获取文件名
		}
	}
}

/** 
将指定目录下的遥感影像生成视频
@param dir 遥感影像目录 
@param dstMat 生成的视频的文件名
@param isColor 灰度图像为false，彩色图像要设置为true
@return
*/
void GenerateVideo(const char * dir,const char * videopath,bool isColor)
{
	vector<std::string> files;
	//搜索指定目录下的遥感影像文件,所有影像的分辨率应该相同
	ReadDirectory(dir, ".tiff", files);
	Mat img;
	int startCol=0,startRow=0,colNum=512,rowNum=512,bufWidth,bufHeight;
	//根据读取的范围设置视频的分辨率，读取的范围加读取的起始位置不要超过图像的范围
	ReComputeBuffsize(colNum,rowNum,bufWidth,bufHeight);
	VideoWriter vw;  
	vw.open(videopath, CV_FOURCC('X','V','I','D'), 25, cv::Size(bufWidth,bufHeight),
		isColor/*灰度图像为false，彩色图像要设置为true*/);  

	for (int i=0; i<files.size(); i++)  
	{  
		printf("%d/%d \n", i, files.size());  
		GDAL2Mat(files[i].c_str(),img,bufWidth,bufHeight,startCol,startRow,colNum,rowNum);
		if (img.empty())  
		{  
			printf("img load error, fileName: %s \n", files[i].c_str());  
			system("pause");  
			exit(-1);  
		}  
		vw<<img;
	} 
}