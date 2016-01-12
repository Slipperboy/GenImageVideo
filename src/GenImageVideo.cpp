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

//����screenWidth*screenHeight�ֱ�������ͼ��
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
				//�ۻ��Ҷ�ֱ��ͼͳ��
				HistogramAccumlateMinMax16S(tmpMat,&min,&max);
				//ת��Ϊ8bit����ͼ�����2%��98%�������Сֵ���죬ʹ����ʾЧ������
				LinearStretch16S(tmpMat,dstMat,min,max);
			}
			//16bit�����ͼ������Ҫ����ϵĲ��ηֱ���д���
			if (tmpBandSize>=4)
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
	int startCol=0,startRow=0,colNum=1024,rowNum=1024,bufWidth,bufHeight;
	//���ݶ�ȡ�ķ�Χ������Ƶ�ķֱ��ʣ���ȡ�ķ�Χ�Ӷ�ȡ����ʼλ�ò�Ҫ����ͼ��ķ�Χ
	ReComputeBuffsize(colNum,rowNum,bufWidth,bufHeight);
	VideoWriter vw;  
	vw.open(videopath, CV_FOURCC('X','V','I','D'), 18, cv::Size(bufWidth,bufHeight),
		isColor/*�Ҷ�ͼ��Ϊfalse����ɫͼ��Ҫ����Ϊtrue*/);  

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

//c�ַ���ת���ͺ���
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
	//����ָ��Ŀ¼�µ�ң��Ӱ���ļ�,����Ӱ��ķֱ���Ӧ����ͬ
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
	//���ݶ�ȡ�ķ�Χ������Ƶ�ķֱ��ʣ���ȡ�ķ�Χ�Ӷ�ȡ����ʼλ�ò�Ҫ����ͼ��ķ�Χ
	ReComputeBuffsize(colNum,rowNum,bufWidth,bufHeight,screenWidth,screenHeight);
	VideoWriter vw;  
	vw.open(videopath, CV_FOURCC('X','V','I','D'), videoFrame, cv::Size(bufWidth,bufHeight),
		isColor/*�Ҷ�ͼ��Ϊfalse����ɫͼ��Ҫ����Ϊtrue*/);  

	for (int i=0; i<files.size(); i++)  
	{  
		printf("�������ɣ�%d/%d \n", i+1, files.size());  
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
	std::cout<<"���ɳɹ���"<<std::endl;
}

void GenerateVideo(const vector<string> &files,const char * videopath,bool isColor,string imageType,
	int startCol,int startRow,int colNum,int rowNum,int videoFrame,int screenWidth,int screenHeight)
{
	Mat img;
	int bufWidth,bufHeight;
	//���ݶ�ȡ�ķ�Χ������Ƶ�ķֱ��ʣ���ȡ�ķ�Χ�Ӷ�ȡ����ʼλ�ò�Ҫ����ͼ��ķ�Χ
	ReComputeBuffsize(colNum,rowNum,bufWidth,bufHeight,screenWidth,screenHeight);
	VideoWriter vw;  
	vw.open(videopath, CV_FOURCC('X','V','I','D'), videoFrame, cv::Size(bufWidth,bufHeight),
		isColor/*�Ҷ�ͼ��Ϊfalse����ɫͼ��Ҫ����Ϊtrue*/);  

	for (int i=0; i<files.size(); i++)  
	{  
		printf("�������ɣ�%d/%d \n", i+1, files.size());  
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
	std::cout<<"���ɳɹ���"<<std::endl;
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



//��ȡxml�ļ� ��ȡʱ��
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
		std::cout<<"��ȡʱ��ʧ��!"<<std::endl;
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
		std::cout<<"��ȡʱ��ʧ��!"<<std::endl;
		system("pause");
		exit(-1);
	}
}

//��ʱ���ַ���ת��Ϊlong��
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
	
	//20150111��Ӱ�����ʱ��������
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
		//Ӱ���ļ�xmlĿ¼
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
		std::cout<<"��ȡ�����ļ��ɹ�!"<<std::endl;
	}
	//�����׳��쳣
	else
	{
		std::cout<<"��ȡ�����ļ�ʧ��!"<<std::endl;
		system("pause");  
		exit(-1);
	}
	//20150111��Ӱ�����ʱ����ļ�����
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
	//��ӵ���׼����
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
	//���ļ� ѡ����Ƶ
	SHELLEXECUTEINFO shex = { 0 };
	shex.cbSize = sizeof(SHELLEXECUTEINFO);
	shex.lpFile = _T("explorer");
	/*shex.lpParameters = _T(" /select, C:\Windows\regedit.exe ");*/
	//����Ӧ���㹻��
	string para(" /select,");
	//Ҫ��·����Ϊ��б��
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