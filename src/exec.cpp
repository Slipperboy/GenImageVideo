#include "GenImageVideo.h"

int main(int argc,char** argv)
{
	/*const char *dir="C:\\Users\\Dell\\Desktop\\assets\\16bit512X512Mux";
	const char *videoname="C:\\Users\\Dell\\Desktop\\assets\\16bit512X512Mux\\result.avi";*/
	//const char *dir="D:\\assets\\16bit512X512Gray";
	//const char *dir="H:\\GF4-test\\2\\jpg";
	//const char *dir="H:\\GF4-test\\2\\tiff";
	//const char *dir="H:\\GF4_result";
	//const char *dir="H:\\GF_RESULT_PAN_1";
	//const char *dir="H:\\GF4-test\\pi\\IRS";
	//const char *dir="H:\\GF4-test\\B1";
	//const char *videoname="D:\\assets\\16bit512X512Gray\\result.avi";
	//const char *videoname="H:\\GF4-test\\2\\jpg\\result.avi";
	//const char *videoname="H:\\GF4-test\\2\\tiff\\result.avi";
	//const char *videoname="H:\\GF4_result\\result.avi";
	//const char *videoname="H:\\GF_RESULT_PAN_1\\result.avi";
	//const char *videoname="H:\\GF4-test\\pi\\IRS\\result.avi";
	//const char *videoname="H:\\GF4-test\\B1\\result.avi";
	//GenerateVideo(dir,videoname,false);
	//const char * filename="C:\\Users\\wangmi\\Desktop\\example.xml";
	if ( argc!=2)
	{
		std::cout<<"usage: GenImageVideo.exe pan.xml"<<std::endl;
	}
	GenerateVideo(argv[1]);
}