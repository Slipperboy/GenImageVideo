#include "GenImageVideo.h"

int main(int argc,char** argv)
{
	/*const char *dir="C:\\Users\\Dell\\Desktop\\assets\\16bit512X512Mux";
	const char *videoname="C:\\Users\\Dell\\Desktop\\assets\\16bit512X512Mux\\result.avi";*/
	const char *dir="C:\\Users\\Dell\\Desktop\\assets\\16bit512X512Gray";
	const char *videoname="C:\\Users\\Dell\\Desktop\\assets\\16bit512X512Gray\\result.avi";
	GenerateVideo(dir,videoname,false);
}