#include "GenImageVideo.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <boost/filesystem.hpp>
#include <string.h>
#include <Windows.h>
#include <ShellAPI.h>
#include <tchar.h>
#include <boost/algorithm/string.hpp>
#include <utility>
#include <algorithm>
#include "pugiconfig.hpp"
#include "pugixml.hpp"
using namespace std;
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
	double p[1024*4],p1[1024*4],num[1024*4];

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
	for(int i=0;i<1024*4;i++)
	{
		p[i]=num[i]/wMulh;
	}

	int min=0,max=0;
	double minProb=0.0,maxProb=0.0;
	while(min<1024*4&&minProb<0.02)
	{
		minProb+=p[min];
		min++;
	}
	do 
	{
		maxProb+=p[max];
		max++;
	} while (max<1024*4&&maxProb<0.98);

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

//按照screenWidth*screenHeight分辨率伸缩图像
void ReComputeBuffsize(double width,double height,int &bufWidth, int &bufHeight,
	int screenWidth,int screenHeight)
{
	if (width<screenWidth&&height<screenHeight)
	{
		bufWidth=(int)width;
		bufHeight=(int)height;
		return;
	}
	if (width/height-16/9>1e-16)
	{
		bufWidth=screenWidth;
		bufHeight=screenWidth/width*height+0.5;
	}
	else
	{
		bufHeight=screenHeight;
		bufWidth=screenHeight/height*width+0.5;
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
				Mat tmpMat=cv::Mat(bufHeight,bufWidth,CV_16UC1);
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
			if (tmpBandSize>=4)
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
	int startCol=0,startRow=0,colNum=1024,rowNum=1024,bufWidth,bufHeight;
	//根据读取的范围设置视频的分辨率，读取的范围加读取的起始位置不要超过图像的范围
	ReComputeBuffsize(colNum,rowNum,bufWidth,bufHeight);
	VideoWriter vw;  
	vw.open(videopath, CV_FOURCC('X','V','I','D'), 18, cv::Size(bufWidth,bufHeight),
		isColor/*灰度图像为false，彩色图像要设置为true*/);  

	for (int i=0; i<files.size(); i++)  
	{  
		printf("%d/%d \n", i, files.size());  
		GDAL2Mat(files[i].c_str(),img,bufWidth,bufHeight,startCol,startRow,colNum,rowNum);
		//img=imread(files[i].c_str(),0);
		if (img.empty())  
		{  
			printf("img load error, fileName: %s \n", files[i].c_str());  
			system("pause");  
			exit(-1);  
		}  
		vw<<img;
	} 
}

//c字符串转整型函数
int Cstr2Int(const char * str)
{
	istringstream iss(str);
	int a;
	iss>>a;
	return a;
}

void GenerateVideo(const char * dir,const char * videopath,bool isColor,string imageType,
	int startCol,int startRow,int colNum,int rowNum,int videoFrame,int screenWidth,int screenHeight)
{
	vector<std::string> files;
	//搜索指定目录下的遥感影像文件,所有影像的分辨率应该相同
	if(strcmp(imageType.c_str(),"tiff")==0)
	{
		ReadDirectory(dir, ".tiff", files);
	}
	else if (strcmp(imageType.c_str(),"jpg")==0)
	{
		ReadDirectory(dir, ".jpg", files);
	}
	
	Mat img;
	int bufWidth,bufHeight;
	//根据读取的范围设置视频的分辨率，读取的范围加读取的起始位置不要超过图像的范围
	ReComputeBuffsize(colNum,rowNum,bufWidth,bufHeight,screenWidth,screenHeight);
	VideoWriter vw;  
	vw.open(videopath, CV_FOURCC('X','V','I','D'), videoFrame, cv::Size(bufWidth,bufHeight),
		isColor/*灰度图像为false，彩色图像要设置为true*/);  

	for (int i=0; i<files.size(); i++)  
	{  
		printf("正在生成：%d/%d \n", i+1, files.size());  
		if(strcmp(imageType.c_str(),"tiff")==0)
		{
			GDAL2Mat(files[i].c_str(),img,bufWidth,bufHeight,startCol,startRow,colNum,rowNum);
		}
		else if (strcmp(imageType.c_str(),"jpg")==0)
		{
		
			img=imread(files[i].c_str(),isColor?1:0);
		}
		if (img.empty())  
		{  
			printf("img load error, fileName: %s \n", files[i].c_str());  
			system("pause");  
			exit(-1);  
		}  
		vw<<img;
	} 
	std::cout<<"生成成功！"<<std::endl;
}

void GenerateVideo(const vector<string> &files,const char * videopath,bool isColor,string imageType,
	int startCol,int startRow,int colNum,int rowNum,int videoFrame,int screenWidth,int screenHeight)
{
	Mat img;
	int bufWidth,bufHeight;
	//根据读取的范围设置视频的分辨率，读取的范围加读取的起始位置不要超过图像的范围
	ReComputeBuffsize(colNum,rowNum,bufWidth,bufHeight,screenWidth,screenHeight);
	VideoWriter vw;  
	vw.open(videopath, CV_FOURCC('X','V','I','D'), videoFrame, cv::Size(bufWidth,bufHeight),
		isColor/*灰度图像为false，彩色图像要设置为true*/);  

	for (int i=0; i<files.size(); i++)  
	{  
		printf("正在生成：%d/%d \n", i+1, files.size());  
		if(strcmp(imageType.c_str(),"tiff")==0)
		{
			GDAL2Mat(files[i].c_str(),img,bufWidth,bufHeight,startCol,startRow,colNum,rowNum);
		}
		else if (strcmp(imageType.c_str(),"jpg")==0)
		{

			img=imread(files[i].c_str(),isColor?1:0);
		}
		if (img.empty())  
		{  
			printf("img load error, fileName: %s \n", files[i].c_str());  
			system("pause");  
			exit(-1);  
		}  
		vw<<img;
	} 
	std::cout<<"生成成功！"<<std::endl;
}

void runMatch(const char * xmlfile)
{
	string cmd="Registration ";
	cmd.append(xmlfile);
	system(cmd.c_str());
}

BOOL MByteToWChar(LPCSTR lpcszStr, LPWSTR lpwszStr)
{
	// Get the required size of the buffer that receives the Unicode 
	// string. 
	DWORD dwMinSize;
	dwMinSize = MultiByteToWideChar (CP_ACP, 0, lpcszStr, -1, NULL, 0);

	// Convert headers from ASCII to Unicode.
	MultiByteToWideChar (CP_ACP, 0, lpcszStr, -1, lpwszStr, dwMinSize);  
	return TRUE;
}



//读取xml文件 提取时间
string GetTimeString(const char *xmlfile)
{
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(xmlfile);
	if (result)
	{
		pugi::xpath_node nodeTime = doc.select_node("//CenterTime");
		return nodeTime.node().child_value();
	}
	else
	{
		std::cout<<"读取时间失败!"<<std::endl;
		system("pause");
		exit(-1);
	}
}

string GetTimeString(const char *xmlfile,pugi::xml_document &doc,pugi::xml_parse_result &result)
{
	result = doc.load_file(xmlfile);
	if (result)
	{
		pugi::xpath_node nodeTime = doc.select_node("//CenterTime");
		return nodeTime.node().child_value();
	}
	else
	{
		std::cout<<"读取时间失败!"<<std::endl;
		system("pause");
		exit(-1);
	}
}

//将时间字符串转化为long型
string MakeTime(string timeString)
{
	int year,month,day,hour,minute,second;
	boost::replace_all(timeString,":"," ");
	boost::replace_all(timeString,"-"," ");
	istringstream iss(timeString);
	iss>>year>>month>>day>>hour>>minute>>second;
	ostringstream oss;
	oss<<year;
	if (month<10)
	{
		oss<<'0'<<month;
	}
	else
	{
		oss<<month;
	}
	if (day<10)
	{
		oss<<'0'<<day;
	}
	else
	{
		oss<<day;
	}
	if (hour<10)
	{
		oss<<'0'<<hour;
	}
	else
	{
		oss<<hour;
	}
	if (minute<10)
	{
		oss<<'0'<<minute;
	}
	else
	{
		oss<<minute;
	}
	if (second<10)
	{
		oss<<'0'<<second;
	}
	else
	{
		oss<<second;
	}
	return oss.str();
}

string MakeFilename(string filename,const string &xmldir,const string &imagedir)
{
	boost::replace_first(filename,xmldir,imagedir);
	boost::replace_first(filename,".xml","_rec.tiff");
	return filename;
}

bool compareTime(const pair<string,string> &p1,const pair<string,string> &p2)
{
	if (p1.second.compare(p2.second)>=0)
	{
		return false;
	}
	return true;
}

void GenerateVideo(const char * xmlfile)
{
	
	//20150111添加按成像时间排序功能
	string imageType,imageDir,videoPath,xmldir;
	int startCol,startRow,colNum,rowNum,screenWidth,screenHeight,videoFrame;
	bool isColor;
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(xmlfile);
	if (result)
	{
		pugi::xpath_node nodeImageType = doc.select_node("//ImageType");
		pugi::xpath_node nodeStartCol = doc.select_node("//StartCol");
		pugi::xpath_node nodeStartRow = doc.select_node("//StartRow");
		pugi::xpath_node nodeColNum = doc.select_node("//ColNum");
		pugi::xpath_node nodeRowNum = doc.select_node("//RowNum");
		pugi::xpath_node nodeImageDir = doc.select_node("//ImageDir");
		pugi::xpath_node nodeScreenWidth = doc.select_node("//ScreenWidth");
		pugi::xpath_node nodeScreenHeight = doc.select_node("//ScreenHeight");
		pugi::xpath_node nodeVideoFrame = doc.select_node("//VideoFrame");
		pugi::xpath_node nodeVideoPath = doc.select_node("//VideoPath");
		pugi::xpath_node nodeIsColor = doc.select_node("//IsColor");
		//影像文件xml目录
		pugi::xpath_node nodeXmldir = doc.select_node("//left_file_path");

		imageType=nodeImageType.node().child_value();
		startCol=Cstr2Int(nodeStartCol.node().child_value());
		startRow=Cstr2Int(nodeStartRow.node().child_value());
		colNum=Cstr2Int(nodeColNum.node().child_value());
		rowNum=Cstr2Int(nodeRowNum.node().child_value());
		imageDir=nodeImageDir.node().child_value();
		videoPath=nodeVideoPath.node().child_value();
		videoFrame=Cstr2Int(nodeVideoFrame.node().child_value());
		screenWidth=Cstr2Int(nodeScreenWidth.node().child_value());
		screenHeight=Cstr2Int(nodeScreenHeight.node().child_value());
		isColor=Cstr2Int(nodeIsColor.node().child_value());
		xmldir=nodeXmldir.node().child_value();
		std::cout<<"读取配置文件成功!"<<std::endl;
	}
	//可以抛出异常
	else
	{
		std::cout<<"读取配置文件失败!"<<std::endl;
		system("pause");  
		exit(-1);
	}
	//20150111添加按成像时间对文件排序
	vector<string> xmlfiles,sortedFiles;
	vector<pair<string,string>> imageTime;
	ReadDirectory(xmldir.c_str(),".xml",xmlfiles);
	size_t i=0;
	/*pugi::xml_document doc;
	pugi::xml_parse_result result;*/
	for (i=0;i<xmlfiles.size();i++)
	{
		cout<<xmlfiles.at(i)<<':';
		string timestring=GetTimeString(xmlfiles.at(i).c_str(),doc,result);
		imageTime.push_back(make_pair(MakeFilename(xmlfiles.at(i),xmldir,imageDir),
			MakeTime(timestring)));
		cout<<timestring<<endl;
	}
	sort(imageTime.begin(),imageTime.end(),compareTime);
	vector<pair<string,string>>::const_iterator ite;
	for (ite=imageTime.begin();ite!=imageTime.end();ite++)
	{
		sortedFiles.push_back(ite->first);
	}
	//添加的配准步骤
	/*string mkdircmd="mkdir "+videoPath;
	system(mkdircmd.c_str());*/
	if (!boost::filesystem::exists(imageDir))
	{
		boost::filesystem::create_directory(imageDir);
	}
	runMatch(xmlfile);

	/*GenerateVideo(imageDir.c_str(),videoPath.c_str(),isColor?1:0,imageType,startCol,startRow,
		colNum,rowNum,videoFrame,screenWidth,screenHeight);*/
	GenerateVideo(sortedFiles,videoPath.c_str(),isColor?1:0,imageType,startCol,startRow,
		colNum,rowNum,videoFrame,screenWidth,screenHeight);
	//打开文件 选择视频
	SHELLEXECUTEINFO shex = { 0 };
	shex.cbSize = sizeof(SHELLEXECUTEINFO);
	shex.lpFile = _T("explorer");
	/*shex.lpParameters = _T(" /select, C:\Windows\regedit.exe ");*/
	//数组应该足够大
	string para(" /select,");
	//要将路径变为反斜杠
	string path=videoPath;
	//boost::replace_all(path,"/","\\");
	para.append(path);
	wchar_t wText[500];
	MByteToWChar(para.c_str(),wText);
	shex.lpParameters = wText;
	shex.lpVerb = _T("open");
	shex.nShow = SW_SHOW;
	shex.lpDirectory = NULL;
	ShellExecuteEx(&shex);
}