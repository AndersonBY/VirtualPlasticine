#include "stdafx.h" 
#include "camerads.h"
#include <stdlib.h>  
#include <cv.h>  
#include <highgui.h>  
//#include <GL/glut.h>
#include <GL/freeglut.h>
#include<iostream>
using namespace std;
#include <windows.h>
#include <math.h>


int gthreshold1[3]={0,0,0},bthreshold1[3]={0,0,0},gthreshold2[3]={0,0,0},bthreshold2[3]={0,0,0};
IplImage* final_img1;
IplImage* final_img2;
IplImage *frame1;
IplImage *frame2;

CvFont font;
double hScale=1.0;
double vScale=1.0;
int lineWidth=2;

int color_fix = 4;//阈值浮动值为7
int v_fix=50;//v通道阈值浮动值为15
int bgthershold=20;//背景阈值

IplImage* dst_img1;
IplImage* splitg_img1;
IplImage* splitb_img1;
IplImage* contourg_img1 = NULL;
IplImage* contourb_img1 = NULL;
IplImage* bg_img1;

IplImage* dst_img2;
IplImage* splitg_img2;
IplImage* splitb_img2;
IplImage* contourg_img2 = NULL;
IplImage* contourb_img2 = NULL;
IplImage* bg_img2;

CvMemStorage * storageg = cvCreateMemStorage(0);
CvMemStorage * storageb = cvCreateMemStorage(0);
CvSeq * contourg = 0;
CvSeq * contourb = 0;

int mode = CV_RETR_LIST;
double area=0;
double gareapre=0,gareanow=0,bareapre=0,bareanow=0;
bool gfindtag1=false,gfindtag2=false,bfindtag1=false,bfindtag2=false;

CvRect roi_rect_g1,roi_rect_g2,roi_rect_b1,roi_rect_b2;
int roi_size_b1=300,roi_size_b2=300,roi_size_g1=300,roi_size_g2=300;
int countlost_b1=0,countlost_g1=0,countlost_b2=0,countlost_g2=0;
int roi_size_inc=20;

CvPoint gpos[4],bpos[4],gfp,bfp;//gfp=green final postion
CvBox2D gfbox,bfbox;

CvPoint3D32f pb,pg;
CvPoint3D32f ptemp=cvPoint3D32f(0,0,0);

double dst_x0=0,dst_y0=0,dst_z0=0;
double sx,sy,sz;
double angle;//旋转角度
CvPoint3D32f nvec=cvPoint3D32f(0,0,0);//旋转轴
double tx=0,ty=0,tz=0;

CCameraDS camera1;
CCameraDS camera2;

int tkey=0;//记录按下哪个键，0为未按键，99为按下'c'，104为按下'h'

double h,s,v;//用来从RGB转到HSV时存的

int rgb2hsv[256][256][256][3];

bool camera1tag[641][481],camera2tag[641][481];
int	 camera1hsv[641][481][3],camera2hsv[641][481][3];

void rgb2hsvmap()
{
	double R,G,B;
	for(int i=0;i<=255;i++)
		for(int j=0;j<=255;j++)
			for(int k=0;k<=255;k++)
			{
				R=(double)i;G=(double)j;B=(double)k;
				double max=0;
				if (R>max) max=R;
				if (G>max) max=G;
				if (B>max) max=B;
				double min=500;
				if (R<min) min=R;
				if (G<min) min=G;
				if (B<min) min=B;
				v=max;
				if(v!=0) s=(v-min)/v;
				else s=0;
				
				if (max==min)
					h=0;
				else
				{
					if (v==R) h=(G-B)/(max-min);
					if (v==G) h=2+(B-R)/(max-min);
					if (v==B) h=4+(R-G)/(max-min);
				}
				h=h*60;

				if (h<0) h=h+360;
				h=h/2;
				s=s*255;
				rgb2hsv[(int)R][(int)G][(int)B][0]=(int)h;
				rgb2hsv[(int)R][(int)G][(int)B][1]=(int)s;
				rgb2hsv[(int)R][(int)G][(int)B][2]=(int)v;
			}
}

void opencamera()
{
	int cam_count;

	//仅仅获取摄像头数目
	cam_count = CCameraDS::CameraCount();
	printf("There are %d cameras.\n", cam_count);


	//获取所有摄像头的名称
	for(int i=0; i < cam_count; i++)
	{
		char camera_name[1024];  
		int retval = CCameraDS::CameraName(i, camera_name, sizeof(camera_name) );

		if(retval >0)
			printf("Camera #%d's Name is '%s'.\n", i, camera_name);
		else
			printf("Can not get Camera #%d's name.\n", i);
	}

	
	//打开第一个摄像头
	//if(! camera.OpenCamera(0, true)) //弹出属性选择窗口
	if(! camera1.OpenCamera(0, false, 640,480)) //不弹出属性选择窗口，用代码制定图像宽和高
	{
		fprintf(stderr, "Can not open camera.\n");
	}
	//cvNamedWindow("camera1");
	
	//if(! camera.OpenCamera(0, true)) //弹出属性选择窗口
	if(! camera2.OpenCamera(1, false, 640,480)) //不弹出属性选择窗口，用代码制定图像宽和高
	{
		fprintf(stderr, "Can not open camera.\n");
	}

	cvDestroyWindow("camera1");
}

double dst(CvPoint a,CvPoint b)//计算两点之间距离平方和
{
	return((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));
}

void Calibrate1()
{
	IplImage *temp,*tempr,*tempg,*tempb;

	cvInitFont(&font,CV_FONT_HERSHEY_SIMPLEX|CV_FONT_ITALIC, hScale,vScale,0,lineWidth);//输出字体结构初始化
	frame1=camera1.QueryFrame();
	final_img1=cvCreateImage(cvGetSize(frame1),8,3);
	tempr=cvCreateImage(cvGetSize(frame1),8,1);
	tempg=cvCreateImage(cvGetSize(frame1),8,1);
	tempb=cvCreateImage(cvGetSize(frame1),8,1);
	temp=cvCreateImage(cvGetSize(frame1),8,3);
	cvWaitKey(0);
	cvNamedWindow("Calibrate");
	while (1)
	{
		//final_img1=cvCloneImage(frame1);//这个函数原来会有内存泄露...
		cvCopy(frame1,final_img1,0);
		cvRectangle(frame1,cvPoint(295,215),cvPoint(345,265),CV_RGB(0,0,255),1);
		cvPutText( frame1,"blue",cvPoint(10,20), &font ,CV_RGB(0,255,0));
		cvShowImage("Calibrate",frame1);
		frame1=camera1.QueryFrame();
		if (cvWaitKey(2)>=0)
			break;
	}
	int n=0;
	double tempthershold1=0,tempthershold2=0;
	while (1&&n<3000000)
	{
		frame1=camera1.QueryFrame();
		cvCopy(frame1,final_img1,0);
		cvRectangle(final_img1,cvPoint(295,215),cvPoint(345,265),CV_RGB(0,0,255),1);
		cvPutText( final_img1,"blue",cvPoint(10,20), &font ,CV_RGB(0,255,0));
		/*/////////////////////////////////////////////////////灰度直方图均衡化
		cvSplit(final_img1,tempb,tempg,tempr,0);
		cvEqualizeHist(tempr,tempr);
		cvEqualizeHist(tempg,tempg);
		cvEqualizeHist(tempb,tempb);
		cvMerge(tempb,tempg,tempr,0,temp);
		/////////////////////////////////////////////////////////*/
		cvCvtColor(frame1, temp, CV_BGR2HSV );
		for(int y =215 ;y<265; y++)
        for(int x =295 ;x<345; x++)
        {
  			tempthershold1=tempthershold1/(n+1)*n;
			tempthershold1=tempthershold1+(double)((uchar*)(temp->imageData+temp->widthStep*y))[x*3]/(n+1);
			tempthershold2=tempthershold2/(n+1)*n;
			tempthershold2=tempthershold2+(double)((uchar*)(temp->imageData+temp->widthStep*y))[x*3+2]/(n+1);
			n++;
		}
		bthreshold1[0]=(int)tempthershold1;
		bthreshold1[2]=(int)tempthershold2;
			
		cvShowImage("Calibrate",final_img1);

		if (cvWaitKey(2)>=0)
			break;
	}
	cvWaitKey(0);
	while (1)
	{
		frame1=camera1.QueryFrame();
		cvCopy(frame1,final_img1,0);
		cvRectangle(final_img1,cvPoint(295,215),cvPoint(345,265),CV_RGB(0,0,255),1);
		cvPutText( final_img1,"green",cvPoint(10,20), &font ,CV_RGB(0,255,0));
		cvShowImage("Calibrate",final_img1);
		if (cvWaitKey(2)>=0)
			break;
	}
	n=0;
	tempthershold1=0;
	tempthershold2=0;
	//temp=cvCreateImage(cvGetSize(frame1),8,3);
	while (1&&n<3000000)
	{
		frame1=camera1.QueryFrame();
		cvCopy(frame1,final_img1,0);
		/*/////////////////////////////////////////////////////灰度直方图均衡化
		cvSplit(final_img1,tempb,tempg,tempr,0);
		cvEqualizeHist(tempr,tempr);
		cvEqualizeHist(tempg,tempg);
		cvEqualizeHist(tempb,tempb);
		cvMerge(tempb,tempg,tempr,0,temp);
		/////////////////////////////////////////////////////////*/
		cvCvtColor(frame1, temp, CV_BGR2HSV );
		cvRectangle(final_img1,cvPoint(295,215),cvPoint(345,265),CV_RGB(0,0,255),1);
		cvPutText( final_img1,"green",cvPoint(10,20), &font ,CV_RGB(0,255,0));
		for(int y =215 ;y<265; y++)
        for(int x =295 ;x<345; x++)
        {
			tempthershold1=tempthershold1/(n+1)*n+(double)((uchar*)(temp->imageData+temp->widthStep*y))[x*3]/(n+1);
			tempthershold2=tempthershold2/(n+1)*n+(double)((uchar*)(temp->imageData+temp->widthStep*y))[x*3+2]/(n+1);
			n++;
		}
		
		gthreshold1[0]=(int)tempthershold1;
		gthreshold1[2]=(int)tempthershold2;
		cvShowImage("Calibrate",final_img1);

		if (cvWaitKey(2)>=0)
			break;
	}
	cvDestroyWindow("Calibrate");
}

void Calibrate2()
{
	IplImage *temp,*tempr,*tempg,*tempb;

	cvInitFont(&font,CV_FONT_HERSHEY_SIMPLEX|CV_FONT_ITALIC, hScale,vScale,0,lineWidth);//输出字体结构初始化
	frame2=camera2.QueryFrame();
	final_img2=cvCreateImage(cvGetSize(frame2),8,3);
	temp=cvCreateImage(cvGetSize(frame2),8,3);
	tempr=cvCreateImage(cvGetSize(frame1),8,1);
	tempg=cvCreateImage(cvGetSize(frame1),8,1);
	tempb=cvCreateImage(cvGetSize(frame1),8,1);
	temp=cvCreateImage(cvGetSize(frame1),8,3);
	cvWaitKey(0);
	cvNamedWindow("Calibrate");

	while (1)
	{
		cvCopy(frame2,final_img2,0);
		cvRectangle(final_img2,cvPoint(295,215),cvPoint(345,265),CV_RGB(0,0,255),1);
		cvPutText( final_img2,"blue",cvPoint(10,20), &font ,CV_RGB(0,255,0));
		frame2=camera2.QueryFrame();
		cvShowImage("Calibrate",final_img2);
		if (cvWaitKey(2)>=0)
			break;
	}
	int n=0;
	double tempthershold1=0,tempthershold2=0;;
	while (1&&n<3000000)
	{
		frame2=camera2.QueryFrame();
		cvCopy(frame2,final_img2,0);
		/*/////////////////////////////////////////////////////灰度直方图均衡化
		cvSplit(final_img2,tempb,tempg,tempr,0);
		cvEqualizeHist(tempr,tempr);
		cvEqualizeHist(tempg,tempg);
		cvEqualizeHist(tempb,tempb);
		cvMerge(tempb,tempg,tempr,0,temp);
		/////////////////////////////////////////////////////////*/
		cvRectangle(final_img2,cvPoint(295,215),cvPoint(345,265),CV_RGB(0,0,255),1);
		cvPutText( final_img2,"blue",cvPoint(10,20), &font ,CV_RGB(0,255,0));
		cvCvtColor(frame2, temp, CV_BGR2HSV );
		for(int y =215 ;y<265; y++)
        for(int x =295 ;x<345; x++)
        {
  			tempthershold1=tempthershold1/(n+1)*n;
			tempthershold1=tempthershold1+(double)((uchar*)(temp->imageData+temp->widthStep*y))[x*3]/(n+1);
			tempthershold2=tempthershold2/(n+1)*n;
			tempthershold2=tempthershold2+(double)((uchar*)(temp->imageData+temp->widthStep*y))[x*3+2]/(n+1);
			n++;
		}
		bthreshold2[0]=(int)tempthershold1;	
		bthreshold2[2]=(int)tempthershold2;	
		cvShowImage("Calibrate",final_img2);

		if (cvWaitKey(2)>=0)
			break;
	}
	cvWaitKey(0);
	while (1)
	{
		frame2=camera2.QueryFrame();
		cvCopy(frame2,final_img2,0);
		cvRectangle(final_img2,cvPoint(295,215),cvPoint(345,265),CV_RGB(0,0,255),1);
		cvPutText( final_img2,"green",cvPoint(10,20), &font ,CV_RGB(0,255,0));
		cvShowImage("Calibrate",final_img2);
		if (cvWaitKey(2)>=0)
			break;
	}
	n=0;
	tempthershold1=0;
	tempthershold2=0;
	//temp=cvCreateImage(cvGetSize(frame2),8,3);
	while (1&&n<3000000)
	{
		frame2=camera2.QueryFrame();
		cvCopy(frame2,final_img2,0);
		/*/////////////////////////////////////////////////////灰度直方图均衡化
		cvSplit(final_img2,tempb,tempg,tempr,0);
		cvEqualizeHist(tempr,tempr);
		cvEqualizeHist(tempg,tempg);
		cvEqualizeHist(tempb,tempb);
		cvMerge(tempb,tempg,tempr,0,temp);
		/////////////////////////////////////////////////////////*/
		cvCvtColor(frame2, temp, CV_BGR2HSV );
		cvRectangle(final_img2,cvPoint(295,215),cvPoint(345,265),CV_RGB(0,0,255),1);
		cvPutText( final_img2,"green",cvPoint(10,20), &font ,CV_RGB(0,255,0));
		for(int y =215 ;y<265; y++)
        for(int x =295 ;x<345; x++)
        {
			tempthershold1=tempthershold1/(n+1)*n+(double)((uchar*)(temp->imageData+temp->widthStep*y))[x*3]/(n+1);
			tempthershold2=tempthershold2/(n+1)*n+(double)((uchar*)(temp->imageData+temp->widthStep*y))[x*3+2]/(n+1);
			n++;
		}
		gthreshold2[0]=(int)tempthershold1;
		gthreshold2[2]=(int)tempthershold2;

		cvShowImage("Calibrate",final_img2);

		if (cvWaitKey(2)>=0)
			break;
	}
	cvDestroyWindow("Calibrate");
}

void display(void)
{

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);  //清空颜色缓冲区
	glEnable(GL_DEPTH_TEST);
	glColor3f(1,1,1);   //重置颜色
	glLoadIdentity();   //清空矩阵
	//gluLookAt(0,0,10,0,0,0,0,2,0); //视点变换
	gluLookAt(5,5,5,0,0,0,0,2,0); //视点变换
	glTranslatef(tx,ty,tz); //将场景中的物体沿z轴负方向移动5个单位长
	glRotatef(angle,-nvec.x,nvec.y,nvec.z);
	glScalef(sx,sy,sz);  //模型变换
	glutWireCube(0.1); //绘制实心立方体和线框立方体
	//glutWireCube(2);
	glFlush();   //刷新窗口以显示当前绘制图形
} 
void init(void)
{
	glClearColor(0,0,0,0);
	glShadeModel(GL_FLAT); //选择平面明暗模式或光滑明暗模式
}

void reshape(int w,int h)
{
	glViewport(0,0,(GLsizei)w,(GLsizei)h);   //设置机口
	glMatrixMode(GL_PROJECTION);  //指定哪一个矩阵是当前矩阵
	glLoadIdentity();
    gluPerspective(45,1,1.5,20);   //创建透视投影矩阵(fovy,aspect,zNear,zFar);
	//glFrustum(-1,1,-1,1,1.5,20.0);  //用透视矩阵乘以当前矩阵(left,Right,bottom,top,near,far);
	glMatrixMode(GL_MODELVIEW);
}


void main(int argc, char *argv[])
{  

	IplImage *temph_img,*temps_img,*tempv_img,*tempb_img;
	//IplImage *finalshow_img1,*finalshow_img2;
	
	angle=0;
	CvPoint3D32f tempa=cvPoint3D32f(0,0,0);
	CvPoint3D32f tempb=cvPoint3D32f(0,0,0);//做旋转时暂存的两个向量
	CvPoint3D32f prevec=cvPoint3D32f(0,0,0);
	bool startrotate=false;
	bool starttrans=false;

 	cvInitFont(&font,CV_FONT_HERSHEY_SIMPLEX|CV_FONT_ITALIC, hScale,vScale,0,lineWidth);//输出字体结构初始化
	rgb2hsvmap();

	//cvNamedWindow("contour1",CV_WINDOW_AUTOSIZE);
	//cvNamedWindow("contour2",CV_WINDOW_AUTOSIZE);
	cvNamedWindow("blue1",CV_WINDOW_AUTOSIZE);
	cvNamedWindow("blue2",CV_WINDOW_AUTOSIZE);
	cvNamedWindow("final1",CV_WINDOW_AUTOSIZE);
	cvNamedWindow("final2",CV_WINDOW_AUTOSIZE);
	//cvNamedWindow("green",CV_WINDOW_AUTOSIZE);

	cvMoveWindow("final1",0,0);
	cvMoveWindow("final2",640,0);
	//cvMoveWindow("contour1",0,380);
	//cvMoveWindow("contour2",640,380);
	cvMoveWindow("blue1",0,380);
	cvMoveWindow("blue2",640,380);

	opencamera();
	//////////////////////////////////////////////////////////////////////////////////
	/////创建背景
	cvNamedWindow("background",CV_WINDOW_AUTOSIZE);
	while (1)
	{
		frame1=camera1.QueryFrame();
		cvPutText( frame1,"camera1 building background",cvPoint(100,50), &font ,CV_RGB(0,255,0));
		cvShowImage("background",frame1);
		if (cvWaitKey(2)>=0)
			break;
	}
	frame1=camera1.QueryFrame();
	bg_img1=cvCreateImage(cvGetSize(frame1),8,3);
	cvCopy(frame1,bg_img1);
	cvWaitKey(0);
	while (1)
	{
		frame2=camera2.QueryFrame();
		cvPutText( frame2,"camera2 building background",cvPoint(100,50), &font ,CV_RGB(0,255,0));
		cvShowImage("background",frame2);
		if (cvWaitKey(2)>=0)
			break;
	}
	frame2=camera2.QueryFrame();
	bg_img2=cvCreateImage(cvGetSize(frame1),8,3);
	cvCopy(frame2,bg_img2);
	cvDestroyWindow("background");
	///////////////////////////////////////////////////////////////////////////////////
	Calibrate1();
	Calibrate2();

	glutInit(&argc, argv); //固定格式
    glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);   //缓存模式
	glutInitWindowSize(400, 400);    //显示框的大小
    glutInitWindowPosition(400,400); //确定显示框左上角的位置
    glutCreateWindow("绘制立方体");
	init();
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
    

	sx=0,sy=0,sz=0;

	int frmnum=0; 
	while (1)
	{
		clock_t start_time=clock();
		{
		if(frmnum<=1000)frmnum++;
		else frmnum=10;
		frame1=camera1.QueryFrame();
		frame2=camera2.QueryFrame();
		if(!frame1) break;

		if(frmnum==1)//第一帧先申请内存
		{
			dst_img1=cvCreateImage(cvGetSize(frame1),8,3);
			splitg_img1=cvCreateImage(cvGetSize(frame1),8,1);
			splitb_img1=cvCreateImage(cvGetSize(frame1),8,1);
			contourg_img1=cvCreateImage(cvGetSize(frame1),8,3);
			contourb_img1=cvCreateImage(cvGetSize(frame1),8,3);
			//final_img1=cvCreateImage(cvGetSize(frame1),8,3);

			dst_img2=cvCreateImage(cvGetSize(frame2),8,3);
			splitg_img2=cvCreateImage(cvGetSize(frame2),8,1);
			splitb_img2=cvCreateImage(cvGetSize(frame2),8,1);
			contourg_img2=cvCreateImage(cvGetSize(frame2),8,3);
			contourb_img2=cvCreateImage(cvGetSize(frame2),8,3);
			//final_img2=cvCreateImage(cvGetSize(frame1),8,3);
			temph_img=cvCreateImage(cvGetSize(frame1),8,3);
			temps_img=cvCreateImage(cvGetSize(frame1),8,1);
			tempv_img=cvCreateImage(cvGetSize(frame1),8,1);
			tempb_img=cvCreateImage(cvGetSize(frame1),8,1);
		}

		cvCopy(frame1,final_img1,0);
		cvCopy(frame2,final_img2,0);
		
		//////////////////////////////////////////////////////

		cvSetImageROI(splitb_img1,roi_rect_b1);
		cvSetImageROI(final_img1,roi_rect_b1);
		//cvSetImageROI(temps_img,roi_rect_b1);
		//cvSetImageROI(tempv_img,roi_rect_b1);
		cvCvtColor(final_img1,final_img1,CV_BGR2HSV);
		//cvSplit(final_img1,splitb_img1,temps_img,tempv_img,0);
		cvSplit(final_img1,splitb_img1,0,0,0);
		cvInRangeS(splitb_img1,cvScalar(bthreshold1[0]-color_fix),cvScalar(bthreshold1[0]+color_fix),splitb_img1);
		/*cvInRangeS(temps_img,cvScalar(35),cvScalar(255),temps_img);
		cvInRangeS(tempv_img,cvScalar(75),cvScalar(255),tempv_img);
		cvAnd(splitb_img1,temps_img,splitb_img1);
		cvAnd(splitb_img1,tempv_img,splitb_img1);*/
		cvResetImageROI(splitb_img1);
		cvResetImageROI(final_img1);

		cvSetImageROI(splitg_img1,roi_rect_g1);
		cvSetImageROI(final_img1,roi_rect_g1);
		cvSetImageROI(frame1,roi_rect_g1);
		cvCopy(frame1,final_img1,0);
		cvCvtColor(final_img1,final_img1,CV_BGR2HSV);
		cvSplit(final_img1,splitg_img1,0,0,0);
		cvInRangeS(splitg_img1,cvScalar(gthreshold1[0]-color_fix),cvScalar(gthreshold1[0]+color_fix),splitg_img1);
		cvResetImageROI(splitg_img1);
		cvResetImageROI(final_img1);
		cvResetImageROI(frame1);

		cvSetImageROI(splitb_img2,roi_rect_b2);
		cvSetImageROI(final_img2,roi_rect_b2);
		cvCvtColor(final_img2,final_img2,CV_BGR2HSV);
		cvSplit(final_img2,splitb_img2,0,0,0);
		cvInRangeS(splitb_img2,cvScalar(bthreshold2[0]-color_fix),cvScalar(bthreshold2[0]+color_fix),splitb_img2);
		cvResetImageROI(splitb_img2);
		cvResetImageROI(final_img2);

		cvSetImageROI(splitg_img2,roi_rect_g2);
		cvSetImageROI(final_img2,roi_rect_g2);
		cvSetImageROI(frame2,roi_rect_g2);
		cvCopy(frame2,final_img2,0);
		cvCvtColor(final_img2,final_img2,CV_BGR2HSV);
		cvSplit(final_img2,splitg_img2,0,0,0);
		cvInRangeS(splitg_img2,cvScalar(gthreshold2[0]-color_fix),cvScalar(gthreshold2[0]+color_fix),splitg_img2);
		cvResetImageROI(splitg_img2);
		cvResetImageROI(final_img2);
		cvResetImageROI(frame2);
		
				
		//cvErode(splitb_img1,splitg_img1,0,1);//腐蚀图像
		//cvDilate(splitb_img1,splitg_img1,0,1);//膨胀图像
 		cvShowImage("blue1",splitb_img1);
		cvShowImage("blue2",splitg_img2);



		if(frmnum!=1)
		{
			cvSetImageROI(splitb_img1,roi_rect_b1);
			cvSetImageROI(splitg_img1,roi_rect_g1);
		}
		
		storageg = cvCreateMemStorage(0);
		storageb = cvCreateMemStorage(0);
		contourg=cvCreateSeq(CV_SEQ_ELTYPE_POINT,sizeof(CvSeq),sizeof(CvPoint),storageg);
		contourb=cvCreateSeq(CV_SEQ_ELTYPE_POINT,sizeof(CvSeq),sizeof(CvPoint),storageb);
		
		//检测轮廓
		CvContourScanner csg=cvStartFindContours(splitg_img1,storageg,sizeof(CvContour),mode,CV_CHAIN_APPROX_SIMPLE);
		CvContourScanner csb=cvStartFindContours(splitb_img1,storageb,sizeof(CvContour),mode,CV_CHAIN_APPROX_SIMPLE);

		cvZero(contourg_img1);
		cvZero(contourb_img1);
		CvSeq* c=contourg;
		c=cvFindNextContour(csg);//挨个寻找轮廓
		CvBox2D boxtemp;//暂存当前找到最匹配的轮廓
		double areatempnow=0,areatemp=640*480;//areatempnow为当前找到最匹配轮廓面积，areatemp为检测到的轮廓的面积与前一帧物体面积的最小差
		
  		gareanow=0;//当前检测轮廓面积
		
		while (c!=0)
		{

			CvBox2D box;

			box=cvMinAreaRect2(c,storageg);
			//gareanow=box.size.height*box.size.width;
			gareanow=fabs(cvContourArea(c));
			if (gareanow>=1000&&gareanow<=10000&&frmnum==1)
			{
				boxtemp=box;
				areatempnow=gareanow;
				gareapre=gareanow;
				gpos[0]=cvPointFrom32f(box.center);
			}
			else if (frmnum!=1)
			{
				/*double tempdst=fabs(3*dst(gpos[2],gpos[1])-dst(cvPointFrom32f(box.center),gpos[0]));
				int tdst=1;
				if (tempdst>=15000&&frmnum>=4 )
					tdst=0;						//当前点在四帧之间近似为匀速运动
				if (tdst&&gareanow>=1000&&((abs(gareanow-gareapre)<areatemp)||abs(640*480-gareanow-gareapre)<areatemp))//当前帧当前轮廓矩形面积与前一帧轮廓面积相近且在当前帧轮廓中差值最小
				{
					boxtemp=box;
					areatemp=abs(gareanow-gareapre);
					areatempnow=gareanow;
				}*/
				if (gareanow>=1200&&gareanow<=20000)
				{
					boxtemp=box;
					areatemp=abs(gareanow-gareapre);
					areatempnow=gareanow;
					//cvDrawContours( contourg_img, c, CV_RGB(0,0,255), CV_RGB(255, 0, 0),2, 2, 8 );
					//cvShowImage("contour1",contourg_img1);
				}
			}
			c=cvFindNextContour(csg);
		}
		c=cvEndFindContours(&csg);
		
		if(areatempnow!=0&&frmnum!=1)
			gareapre=areatempnow;//当前点正确检测则前一面积重新赋值
		else
			boxtemp=boxtemp;	//当前点未正确检测，则中心点暂为前一帧点

		/*if (frmnum<=3)
			gpos[frmnum]=cvPointFrom32f(boxtemp.center);
		else
		{
			gpos[0]=gpos[1];
			gpos[1]=gpos[2];
			gpos[2]=gpos[3];
			gpos[3]=cvPointFrom32f(boxtemp.center);
		}*///位置赋值

		if(areatempnow!=0)
		{
			CvPoint2D32f ptf[4];
			cvBoxPoints(boxtemp,ptf);
			CvPoint pt[4];
			pt[0]=cvPointFrom32f(ptf[0]);
			pt[2]=cvPointFrom32f(ptf[2]);	
			cvRectangle(frame1,cvPoint(pt[0].x+roi_rect_g1.x,pt[0].y+roi_rect_g1.y),
						cvPoint(pt[2].x+roi_rect_g1.x,pt[2].y+roi_rect_g1.y),CV_RGB(255,0,0),1);//满足条件的画矩形
			//SetCursorPos((int)boxtemp.center.x*2,(int)boxtemp.center.y*2);//移动鼠标位置
			gfp=cvPointFrom32f(boxtemp.center);
			gfbox=boxtemp;
			gfindtag1=true;
		}
		else
		{
			cvPutText( frame1,"green object not detected",cvPoint(100,50), &font ,CV_RGB(0,255,0));
			gfindtag1=false;
		}
		
		c=contourb;
		c=cvFindNextContour(csb);//挨个寻找轮
		areatempnow=0,areatemp=640*480;
		
 		bareanow=0;
		while (c!=0)
		{

			CvBox2D box;

			box=cvMinAreaRect2(c,storageg);
			//gareanow=box.size.height*box.size.width;
			bareanow=fabs(cvContourArea(c));
			if (bareanow>=1000&&bareanow<=20000&&frmnum==1)
			{
				boxtemp=box;
				areatempnow=bareanow;
				bareapre=bareanow;
				bpos[0]=cvPointFrom32f(box.center);
			}
			else if (frmnum!=1)
			{
				/*double tempdst=fabs(3*dst(bpos[2],bpos[1])-dst(cvPointFrom32f(box.center),bpos[0]));
				int tdst=1;
				if (tempdst>=12000 &&frmnum>=4)
					tdst=0;						//当前点在四帧之间近似为匀速运动
				if (tdst&&bareanow>=1000&&((abs(bareanow-bareapre)<areatemp)))//当前帧当前轮廓矩形面积与前一帧轮廓面积相近且在当前帧轮廓中差值最小
				{
					boxtemp=box;
					areatemp=abs(bareanow-bareapre);
					areatempnow=bareanow;
				}*/
				if (bareanow>=1200&&bareanow<=20000)
				{
					boxtemp=box;
					areatemp=abs(bareanow-bareapre);
					areatempnow=bareanow;
				}
			}
			
			/*if (bareanow>=2500&&bareanow<=20000)
			{
				//cvDrawContours( contourb_img1, c, CV_RGB(0,0,255), CV_RGB(255, 0, 0),2, 2, 8 );
				//cvShowImage("contour1",contourb_img1);
			}*/
			
			c=cvFindNextContour(csb);
		}
		c=cvEndFindContours(&csb);
		cvZero(contourb_img1);
		if(areatempnow!=0&&frmnum!=1)
			bareapre=areatempnow;//当前点正确检测则前一面积重新赋值
		else
			boxtemp=boxtemp;	//当前点未正确检测，则中心点暂为前一帧点

		/*if (frmnum<=3&&areatempnow!=0)
			bpos[frmnum]=cvPointFrom32f(boxtemp.center);
		else if(areatempnow!=0)
		{
			bpos[0]=bpos[1];
			bpos[1]=bpos[2];
			bpos[2]=bpos[3];
			bpos[3]=cvPointFrom32f(boxtemp.center);
		}*///位置赋值
		
		if (areatempnow!=0)
		{
			CvPoint2D32f ptf[4];
			cvBoxPoints(boxtemp,ptf);
			CvPoint pt[4];
			cvBoxPoints(boxtemp,ptf);
			pt[0]=cvPointFrom32f(ptf[0]);
			pt[2]=cvPointFrom32f(ptf[2]);	
			cvRectangle(frame1,cvPoint(pt[0].x+roi_rect_b1.x,pt[0].y+roi_rect_b1.y),
						cvPoint(pt[2].x+roi_rect_b1.x,pt[2].y+roi_rect_b1.y),CV_RGB(255,0,0),1);//满足条件的画矩形
			bfp=cvPointFrom32f(boxtemp.center);
			bfbox=boxtemp;
			//SetCursorPos((int)(boxtemp.center.x*65/32-10),(int)(boxtemp.center.y*41/24-10));//移动鼠标位置

			
			bfindtag1=true;
		}
		else
		{
			cvPutText( frame1,"blue object not detected",cvPoint(100,20), &font ,CV_RGB(0,255,0));
			bfindtag1=false;
		}

		bfp.x=bfp.x+roi_rect_b1.x;
		bfp.y=bfp.y+roi_rect_b1.y;
		gfp.x=gfp.x+roi_rect_g1.x;
		gfp.y=gfp.y+roi_rect_g1.y;

		pb=cvPoint3D32f((double)bfp.x,480-(double)bfp.y,0);
		pg=cvPoint3D32f((double)gfp.x,480-(double)gfp.y,0);

		if (bfindtag1&&gfindtag1)
		{
			double tdst=fabs(dst(bfp,gfp));//判断手指是否和上用的距离平方
		
			if (2*tdst<(0.5*gfbox.size.height+0.5*bfbox.size.height)*(0.5*gfbox.size.height+0.5*bfbox.size.height)+(0.5*gfbox.size.width+0.5*bfbox.size.width)*(0.5*gfbox.size.width+0.5*bfbox.size.width))
			{
			
				//mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);//鼠标左键按下
				cvPutText( frame1,"left mouse button down",cvPoint(100,100), &font ,CV_RGB(0,255,0));
			}
		}

		if (bfindtag1)
		{
			roi_size_b1=250;
			roi_rect_b1.x=bfp.x-roi_size_b1/2;
			if (roi_rect_b1.x<0) roi_rect_b1.x=0;
			roi_rect_b1.y=bfp.y-roi_size_b1/2;
			if (roi_rect_b1.y<0) roi_rect_b1.y=0;
			roi_rect_b1.height=roi_size_b1;
			if (roi_rect_b1.y>480-roi_size_b1) roi_rect_b1.height=480-roi_rect_b1.y;
			roi_rect_b1.width=roi_size_b1;
			if (roi_rect_b1.x>640-roi_size_b1) roi_rect_b1.width=640-roi_rect_b1.x;
			countlost_b1=0;
		}
		else
		{
			if(countlost_b1<=10)
			{
				roi_rect_b1.x=roi_rect_b1.x-roi_size_inc/2;
				if (roi_rect_b1.x<0) roi_rect_b1.x=0;
				roi_rect_b1.y=roi_rect_b1.y-roi_size_inc/2;
				if (roi_rect_b1.y<0) roi_rect_b1.y=0;
				roi_rect_b1.height=roi_size_b1;
				if (roi_rect_b1.y>480-roi_size_b1) roi_rect_b1.height=480-roi_rect_b1.y;
				roi_rect_b1.width=roi_size_b1;
				if (roi_rect_b1.x>640-roi_size_b1) roi_rect_b1.width=640-roi_rect_b1.x;
				roi_size_b1=roi_size_b1+roi_size_inc;
				countlost_b1++;
			}
			else
			{
				roi_rect_b1.x=0;
				roi_rect_b1.y=0;
				roi_rect_b1.height=480;
				roi_rect_b1.width=640;
				countlost_b1=0;
			}
			
		}
		if (gfindtag1)
		{
			roi_size_g1=250;
			roi_rect_g1.x=gfp.x-roi_size_g1/2;
			if (roi_rect_g1.x<0) roi_rect_g1.x=0;
			roi_rect_g1.y=gfp.y-roi_size_g1/2;
			if (roi_rect_g1.y<0) roi_rect_g1.y=0;
			roi_rect_g1.height=roi_size_g1;
			if (roi_rect_g1.y>480-roi_size_g1) roi_rect_g1.height=480-roi_rect_g1.y;
			roi_rect_g1.width=roi_size_g1;
			if (roi_rect_g1.x>640-roi_size_g1) roi_rect_g1.width=640-roi_rect_g1.x;
			countlost_g1=0;
		}
		else
		{
			if(countlost_g1<=10)
			{
				roi_rect_g1.x=roi_rect_g1.x-roi_size_inc/2;
				if (roi_rect_g1.x<0) roi_rect_g1.x=0;
				roi_rect_g1.y=roi_rect_g1.y-roi_size_inc/2;
				if (roi_rect_g1.y<0) roi_rect_g1.y=0;
				roi_rect_g1.height=roi_size_g1;
				if (roi_rect_g1.y>480-roi_size_g1) roi_rect_g1.height=480-roi_rect_g1.y;
				roi_rect_g1.width=roi_size_g1;
				if (roi_rect_g1.x>640-roi_size_g1) roi_rect_g1.width=640-roi_rect_g1.x;
				roi_size_g1=roi_size_g1+roi_size_inc;
				countlost_g1++;
			}
			else
			{
				roi_rect_g1.x=0;
				roi_rect_g1.y=0;
				roi_rect_g1.height=480;
				roi_rect_g1.width=640;
				countlost_g1=0;
			}
		}

		cvRectangle(frame1,cvPoint(roi_rect_b1.x,roi_rect_b1.y),cvPoint(roi_rect_b1.x+300,roi_rect_b1.y+300),CV_RGB(255,255,0),1);
		cvRectangle(frame1,cvPoint(roi_rect_g1.x,roi_rect_g1.y),cvPoint(roi_rect_g1.x+300,roi_rect_g1.y+300),CV_RGB(255,255,0),1);

		cvShowImage("final1",frame1);
		cvReleaseMemStorage( &storageg );
		cvReleaseMemStorage( &storageb );

		if (frmnum!=1)
		{
			cvSetImageROI(splitb_img2,roi_rect_b2);
			cvSetImageROI(splitg_img2,roi_rect_g2);
		}

		storageg = cvCreateMemStorage(0);
		storageb = cvCreateMemStorage(0);
		contourg=cvCreateSeq(CV_SEQ_ELTYPE_POINT,sizeof(CvSeq),sizeof(CvPoint),storageg);
		contourb=cvCreateSeq(CV_SEQ_ELTYPE_POINT,sizeof(CvSeq),sizeof(CvPoint),storageb);
		
		//检测轮廓
		csg=cvStartFindContours(splitg_img2,storageg,sizeof(CvContour),mode,CV_CHAIN_APPROX_SIMPLE);
		csb=cvStartFindContours(splitb_img2,storageb,sizeof(CvContour),mode,CV_CHAIN_APPROX_SIMPLE);

		cvZero(contourg_img2);
		cvZero(contourb_img2);
		c=contourg;
		c=cvFindNextContour(csg);//挨个寻找轮廓
		boxtemp;//暂存当前找到最匹配的轮廓
		areatempnow=0,areatemp=640*480;//areatempnow为当前找到最匹配轮廓面积，areatemp为检测到的轮廓的面积与前一帧物体面积的最小差
		
  		gareanow=0;//当前检测轮廓面积
		while (c!=0)
		{

			CvBox2D box;

			box=cvMinAreaRect2(c,storageg);
			//gareanow=box.size.height*box.size.width;
			gareanow=fabs(cvContourArea(c));
			if (gareanow>=1000&&gareanow<=10000&&frmnum==1)
			{
				boxtemp=box;
				areatempnow=gareanow;
				gareapre=gareanow;
				gpos[0]=cvPointFrom32f(box.center);
			}
			else if (frmnum!=1)
			{
				/*double tempdst=fabs(3*dst(gpos[2],gpos[1])-dst(cvPointFrom32f(box.center),gpos[0]));
				int tdst=1;
				if (tempdst>=12000&&frmnum>=4 )
					tdst=0;						//当前点在四帧之间近似为匀速运动
				if (tdst&&gareanow>=1000&&((abs(gareanow-gareapre)<areatemp)||abs(640*480-gareanow-gareapre)<areatemp))//当前帧当前轮廓矩形面积与前一帧轮廓面积相近且在当前帧轮廓中差值最小
				{
					boxtemp=box;
					areatemp=abs(gareanow-gareapre);
					areatempnow=gareanow;
				}*/
				if (gareanow>=1200&&gareanow<=20000)
				{
					boxtemp=box;
					areatemp=abs(gareanow-gareapre);
					areatempnow=gareanow;
					//cvDrawContours( contourg_img, c, CV_RGB(0,0,255), CV_RGB(255, 0, 0),2, 2, 8 );
					//cvShowImage("contour",contourg_img);
				}
			}
			c=cvFindNextContour(csg);
		}
		c=cvEndFindContours(&csg);
		
		if(areatempnow!=0&&frmnum!=1)
			gareapre=areatempnow;//当前点正确检测则前一面积重新赋值
		else
			boxtemp=boxtemp;	//当前点未正确检测，则中心点暂为前一帧点

		/*if (frmnum<=3)
			gpos[frmnum]=cvPointFrom32f(boxtemp.center);
		else
		{
			gpos[0]=gpos[1];
			gpos[1]=gpos[2];
			gpos[2]=gpos[3];
			gpos[3]=cvPointFrom32f(boxtemp.center);
		}*///位置赋值

		if(areatempnow!=0)
		{
			CvPoint2D32f ptf[4];
			cvBoxPoints(boxtemp,ptf);
			CvPoint pt[4];
			pt[0]=cvPointFrom32f(ptf[0]);
			pt[2]=cvPointFrom32f(ptf[2]);
			cvRectangle(frame2,cvPoint(pt[0].x+roi_rect_g2.x,pt[0].y+roi_rect_g2.y),
						cvPoint(pt[2].x+roi_rect_g2.x,pt[2].y+roi_rect_g2.y),CV_RGB(255,0,0),1);//满足条件的画矩形
			//cvRectangle(frame2,pt[0],pt[2],CV_RGB(255,0,0),1);//满足条件的画矩形
			//SetCursorPos((int)boxtemp.center.x*2,(int)boxtemp.center.y*2);//移动鼠标位置
			gfp=cvPointFrom32f(boxtemp.center);
			gfbox=boxtemp;
			gfindtag2=true;
		}
		else
		{
			cvPutText( frame2,"green object not detected",cvPoint(100,50), &font ,CV_RGB(0,255,0));
			gfindtag2=false;
		}
		
		c=contourb;
		c=cvFindNextContour(csb);//挨个寻找轮
		areatempnow=0,areatemp=640*480;
		
 		bareanow=0;
		while (c!=0)
		{

			CvBox2D box;

			box=cvMinAreaRect2(c,storageg);
			//gareanow=box.size.height*box.size.width;
			bareanow=fabs(cvContourArea(c));
			if (bareanow>=1000&&bareanow<=20000&&frmnum==1)
			{
				boxtemp=box;
				areatempnow=bareanow;
				bareapre=bareanow;
				bpos[0]=cvPointFrom32f(box.center);
			}
			else if (frmnum!=1)
			{
				/*double tempdst=fabs(3*dst(bpos[2],bpos[1])-dst(cvPointFrom32f(box.center),bpos[0]));
				int tdst=1;
				if (tempdst>=12000 &&frmnum>=4)
					tdst=0;						//当前点在四帧之间近似为匀速运动
				if (tdst&&bareanow>=1000&&((abs(bareanow-bareapre)<areatemp)))//当前帧当前轮廓矩形面积与前一帧轮廓面积相近且在当前帧轮廓中差值最小
				{
					boxtemp=box;
					areatemp=abs(bareanow-bareapre);
					areatempnow=bareanow;
				}*/
				if (bareanow>=1200&&bareanow<=20000)
				{
					boxtemp=box;
					areatemp=abs(bareanow-bareapre);
					areatempnow=bareanow;
				}
			}
			
			/*if (bareanow>=2500&&bareanow<=20000)
			{
				//cvDrawContours( contourb_img2, c, CV_RGB(0,0,255), CV_RGB(255, 0, 0),2, 2, 8 );
				//cvShowImage("contour2",contourb_img2);
			}*/
			
			c=cvFindNextContour(csb);
		}
		c=cvEndFindContours(&csb);
		cvReleaseMemStorage( &storageg );
		cvReleaseMemStorage( &storageb );

		cvZero(contourb_img2);
		if(areatempnow!=0&&frmnum!=1)
			bareapre=areatempnow;//当前点正确检测则前一面积重新赋值
		else
			boxtemp=boxtemp;	//当前点未正确检测，则中心点暂为前一帧点

		/*if (frmnum<=3&&areatempnow!=0)
			bpos[frmnum]=cvPointFrom32f(boxtemp.center);
		else if(areatempnow!=0)
		{
			bpos[0]=bpos[1];
			bpos[1]=bpos[2];
			bpos[2]=bpos[3];
			bpos[3]=cvPointFrom32f(boxtemp.center);
		}*///位置赋值
		
		if (areatempnow!=0)
		{
			CvPoint2D32f ptf[4];
			cvBoxPoints(boxtemp,ptf);
			CvPoint pt[4];
			cvBoxPoints(boxtemp,ptf);
			pt[0]=cvPointFrom32f(ptf[0]);
			pt[2]=cvPointFrom32f(ptf[2]);
			cvRectangle(frame2,cvPoint(pt[0].x+roi_rect_b2.x,pt[0].y+roi_rect_b2.y),
						cvPoint(pt[2].x+roi_rect_b2.x,pt[2].y+roi_rect_b2.y),CV_RGB(255,0,0),1);//满足条件的画矩形
			//cvRectangle(frame2,pt[0],pt[2],CV_RGB(255,0,0),1);//满足条件的画矩形
			bfp=cvPointFrom32f(boxtemp.center);
			bfbox=boxtemp;
			//SetCursorPos((int)(boxtemp.center.x*65/32-10),(int)(boxtemp.center.y*41/24-10));//移动鼠标位置

			bfindtag2=true;
		}
		else
		{
			cvPutText( frame2,"blue object not detected",cvPoint(100,20), &font ,CV_RGB(0,255,0));
			bfindtag2=false;
		}

		bfp.x=bfp.x+roi_rect_b2.x;
		bfp.y=bfp.y+roi_rect_b2.y;
		gfp.x=gfp.x+roi_rect_g2.x;
		gfp.y=gfp.y+roi_rect_g2.y;

		if(bfindtag2&&gfindtag2)
		{
			double tdst=fabs(dst(bfp,gfp));//判断手指是否按下用的距离平方
		
			if (2*tdst<(0.5*gfbox.size.height+0.5*bfbox.size.height)*(0.5*gfbox.size.height+0.5*bfbox.size.height)+(0.5*gfbox.size.width+0.5*bfbox.size.width)*(0.5*gfbox.size.width+0.5*bfbox.size.width))
			{
				//mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);//鼠标左键按下
				cvPutText( frame2,"left mouse button down",cvPoint(100,100), &font ,CV_RGB(0,255,0));
			}//判断鼠标是否按下
		}

		if (bfindtag2)
		{
			roi_size_b2=250;
			roi_rect_b2.x=bfp.x-roi_size_b2/2;
			if (roi_rect_b2.x<0) roi_rect_b2.x=0;
			roi_rect_b2.y=bfp.y-roi_size_b2/2;
			if (roi_rect_b2.y<0) roi_rect_b2.y=0;
			roi_rect_b2.height=roi_size_b2;
			if (roi_rect_b2.y>480-roi_size_b2) roi_rect_b2.height=480-roi_rect_b2.y;
			roi_rect_b2.width=roi_size_b2;
			if (roi_rect_b2.x>640-roi_size_b2) roi_rect_b2.width=640-roi_rect_b2.x;
			countlost_b2=0;
		}
		else
		{
			if(countlost_b2<=10)
			{
				roi_rect_b2.x=roi_rect_b2.x-roi_size_inc/2;
				if (roi_rect_b2.x<0) roi_rect_b2.x=0;
				roi_rect_b2.y=roi_rect_b2.y-roi_size_inc/2;
				if (roi_rect_b2.y<0) roi_rect_b2.y=0;
				roi_rect_b2.height=roi_size_b2;
				if (roi_rect_b2.y>480-roi_size_b2) roi_rect_b2.height=480-roi_rect_b2.y;
				roi_rect_b2.width=roi_size_b2;
				if (roi_rect_b2.x>640-roi_size_b2) roi_rect_b2.width=640-roi_rect_b2.x;
				roi_size_b2=roi_size_b2+roi_size_inc;
				countlost_b2++;
			}
			else
			{
				roi_rect_b2.x=0;
				roi_rect_b2.y=0;
				roi_rect_b2.height=480;
				roi_rect_b2.width=640;
				countlost_b2=0;
			}
		}
		if (gfindtag2)
		{
			roi_size_g2=250;
			roi_rect_g2.x=gfp.x-roi_size_g2/2;
			if (roi_rect_g2.x<0) roi_rect_g2.x=0;
			roi_rect_g2.y=gfp.y-roi_size_g2/2;
			if (roi_rect_g2.y<0) roi_rect_g2.y=0;
			roi_rect_g2.height=roi_size_g2;
			if (roi_rect_g2.y>480-roi_size_g2) roi_rect_g2.height=480-roi_rect_g2.y;
			roi_rect_g2.width=roi_size_g2;
			if (roi_rect_g2.x>640-roi_size_g2) roi_rect_g2.width=640-roi_rect_g2.x;
			countlost_g2=0;
		}
		else
		{
			if(countlost_g2<=10)
			{
				roi_rect_g2.x=roi_rect_g2.x-roi_size_inc/2;
				if (roi_rect_g2.x<0) roi_rect_g2.x=0;
				roi_rect_g2.y=roi_rect_g2.y-roi_size_inc/2;
				if (roi_rect_g2.y<0) roi_rect_g2.y=0;
				roi_rect_g2.height=roi_size_g2;
				if (roi_rect_g2.y>480-roi_size_g2) roi_rect_g2.height=480-roi_rect_g2.y;
				roi_rect_g2.width=roi_size_g2;
				if (roi_rect_g2.x>640-roi_size_g2) roi_rect_g2.width=640-roi_rect_g2.x;
				roi_size_g2=roi_size_g2+roi_size_inc;
				countlost_g2++;
			}
			else
			{
				roi_rect_g2.x=0;
				roi_rect_g2.y=0;
				roi_rect_g2.height=480;
				roi_rect_g2.width=640;
				countlost_g2=0;
			}
		}

		cvRectangle(frame2,cvPoint(roi_rect_b2.x,roi_rect_b2.y),cvPoint(roi_rect_b2.x+300,roi_rect_b2.y+300),CV_RGB(255,255,0),1);
		cvRectangle(frame2,cvPoint(roi_rect_g2.x,roi_rect_g2.y),cvPoint(roi_rect_g2.x+300,roi_rect_g2.y+300),CV_RGB(255,255,0),1);
		
		if (!bfindtag1)
			pb.y=480-(float)bfp.y;
		if (!gfindtag1)
			pg.y=480-(float)gfp.y;
		pb.z=640-(float)bfp.x;
		pg.z=640-(float)gfp.x;

		if (tkey==99)
		{
			//开始画立方体
			if (dst_x0==0)
				dst_x0=abs(pb.x-pg.x);
			if (dst_z0==0)
				dst_z0=abs(pb.z-pg.z);

			sx=7*abs(pb.x-pg.x)/(dst_x0-50);
			sz=7*abs(pb.z-pg.z)/(dst_z0-50);
			glutPostRedisplay();
		}

		if (tkey==104)
		{
			if(dst_y0==0)
			{
				dst_y0=1;
				ptemp=pg;
			}
			sy=0.5*abs(pg.y-ptemp.y)/dst_y0;
			glutPostRedisplay();
		}
		if (tkey==116)//tkey=116平移
		{
			if (starttrans==false)
			{
				starttrans=true;
				prevec.x=pb.x;
				prevec.y=pb.y;
				prevec.z=pb.z;
			}
			else
			{
				tx=(double)(pb.x-prevec.x)/40;
				ty=(double)(pb.y-prevec.y)/40;
				tz=(double)(pb.z-prevec.z)/40;
			}
			glutPostRedisplay();
		}

		if (tkey==114)//tkey=114为旋转
		{
			if(startrotate==false)
			{
				prevec.x=pg.x-pb.x;
				prevec.y=pg.y-pb.y;
				prevec.z=pg.z-pb.z;
				startrotate=true;
			}
			else
			{
				tempa.x=prevec.x;
				tempa.y=prevec.y;
				tempa.z=prevec.z;
				tempb.x=pg.x-pb.x;
				tempb.y=pg.y-pb.y;
				tempb.z=pg.z-pb.z;
				nvec.x=tempa.y*tempb.z-tempa.z*tempb.y;
				nvec.y=tempa.z*tempb.x-tempa.x*tempb.z;
				nvec.z=tempa.x*tempb.y-tempa.y*tempb.x;
				double asize=(double)(tempa.x*tempa.x+tempa.y*tempa.y+tempa.z*tempa.z);
				double bsize=(double)(tempb.x*tempb.x+tempb.y*tempb.y+tempb.z*tempb.z);
				angle=acos((tempa.x*tempb.x+tempa.y*tempb.y+tempa.z*tempb.z)/sqrt(asize*bsize))*57.29577951;//acos输出弧度，要转为角度
				glutPostRedisplay();
			}
		}

		cvShowImage("final2",frame2);
		
		int tkeynow=cvWaitKey(2);
		if (tkeynow>=0)
			tkey=tkeynow;
		if (tkeynow==113)
			break;
		else
			glutMainLoopEvent();
		}
		clock_t end_time=clock();
		cout<< "Running time is: "<<static_cast<double>(end_time-start_time)/CLOCKS_PER_SEC*1000<<"ms"<<endl;//输出运行时间

	}
	//glutPostRedisplay();
	//cvWaitKey(0);
	cvReleaseImage(&dst_img1);
	cvReleaseImage(&splitg_img1);
	cvReleaseImage(&splitb_img1);
	//cvReleaseImage(&contourg_img1);
	cvReleaseImage(&final_img1);

	cvReleaseImage(&dst_img2);
	cvReleaseImage(&splitg_img2);
	cvReleaseImage(&splitb_img2);
	//cvReleaseImage(&contourg_img2);
	cvReleaseImage(&final_img2);

	cvReleaseMemStorage(&storageg);
	cvReleaseMemStorage(&storageb);

	cvDestroyWindow("contour");
	cvDestroyWindow("final");
	cvDestroyWindow("blue");
}