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

//����1920X1080�ֱ�������ͼ��
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
��ȡһ����Χ��tiffӰ�񣬽����ݱ�����Mat�ṹ��
@param filename ң��Ӱ���·�� 
@param dstMat Ҫ�������ݵ�Mat
@param bufWidth ����������Mat������ 
@param bufHeight ��������Mat������
@param startCol ��ȡ����ʼ��
@param startRow ��ȡ����ʼ��
@param startCol ��ȡ������
@param startRow ��ȡ������

*/
void GDAL2Mat(const char* fileName,Mat &dstMat,int bufWidth,int bufHeight,
	int startCol,int startRow,int colNum,int rowNum)
{
	GDALAllRegister();  
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO");   
	GDALDataset *poDataset = (GDALDataset *)GDALOpen(fileName,GA_ReadOnly);   // GDAL���ݼ�
	int imgWidth=poDataset->GetRasterXSize();
	int imgHeight=poDataset->GetRasterYSize();

	int tmpBandSize = poDataset->GetRasterCount();
	GDALDataType dataType = poDataset->GetRasterBand(1)->GetRasterDataType();
	GDALRasterBand *pBand;
	//��Ҫ�����������ͣ��������Լ�����ͶӰ
	switch(dataType)
	{
	case GDT_Byte:
		{
			uchar *pafScan= new uchar[bufWidth*bufHeight]; // �洢����
			//8bit�Ҷ�ͼ����
			if (tmpBandSize==1)
			{
				dstMat = cv::Mat(bufHeight,bufWidth,CV_8UC1);
				pBand = poDataset->GetRasterBand(1);
				//��ָ����Χ�����ݶ���������
				pBand->RasterIO(GF_Read,startCol,startRow,colNum,rowNum,pafScan,bufWidth,bufHeight,GDT_Byte,0,0);
				for(int i=0;i<dstMat.rows;i++)
				{
					for (int j=0;j<dstMat.cols;j++)
					{
						//��dstMat�������
						dstMat.at<uchar>(i,j)=*(pafScan+i*dstMat.cols+j);
					}
				}
			}
			
			//8bit�����ͼ����ע�Ⲩ������һ����3��
			if (tmpBandSize>=3)
			{
				//����ѡȡ��ǰ�����������
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
			//16bit�Ҷ�ͼ����
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
				//�ۻ��Ҷ�ֱ��ͼͳ��
				HistogramAccumlateMinMax16S(tmpMat,&min,&max);
				//ת��Ϊ8bit����ͼ�����2%��98%�������Сֵ���죬ʹ����ʾЧ������
				LinearStretch16S(tmpMat,dstMat,min,max);
			}
			//16bit�����ͼ������Ҫ����ϵĲ��ηֱ���д���
			if (tmpBandSize==4)
			{
				//���������4,3,2���ν������
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
				//ͨ���Ĳ��  
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
		if (!boost::filesystem::is_directory(*file_itr) && (boost::filesystem::extension(*file_itr)==ext))        // �ļ���׺
		{
			files.push_back(file_itr->path().string());    //��ȡ�ļ���
		}
	}
}

/** 
��ָ��Ŀ¼�µ�ң��Ӱ��������Ƶ
@param dir ң��Ӱ��Ŀ¼ 
@param dstMat ���ɵ���Ƶ���ļ���
@param isColor �Ҷ�ͼ��Ϊfalse����ɫͼ��Ҫ����Ϊtrue
@return
*/
void GenerateVideo(const char * dir,const char * videopath,bool isColor)
{
	vector<std::string> files;
	//����ָ��Ŀ¼�µ�ң��Ӱ���ļ�,����Ӱ��ķֱ���Ӧ����ͬ
	ReadDirectory(dir, ".tiff", files);
	Mat img;
	int startCol=0,startRow=0,colNum=512,rowNum=512,bufWidth,bufHeight;
	//���ݶ�ȡ�ķ�Χ������Ƶ�ķֱ��ʣ���ȡ�ķ�Χ�Ӷ�ȡ����ʼλ�ò�Ҫ����ͼ��ķ�Χ
	ReComputeBuffsize(colNum,rowNum,bufWidth,bufHeight);
	VideoWriter vw;  
	vw.open(videopath, CV_FOURCC('X','V','I','D'), 25, cv::Size(bufWidth,bufHeight),
		isColor/*�Ҷ�ͼ��Ϊfalse����ɫͼ��Ҫ����Ϊtrue*/);  

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