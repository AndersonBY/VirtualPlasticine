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

int color_fix = 4;//��ֵ����ֵΪ7
int v_fix=50;//vͨ����ֵ����ֵΪ15
int bgthershold=20;//������ֵ

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
double angle;//��ת�Ƕ�
CvPoint3D32f nvec=cvPoint3D32f(0,0,0);//��ת��
double tx=0,ty=0,tz=0;

CCameraDS camera1;
CCameraDS camera2;

int tkey=0;//��¼�����ĸ�����0Ϊδ������99Ϊ����'c'��104Ϊ����'h'

double h,s,v;//������RGBת��HSVʱ���

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

	//������ȡ����ͷ��Ŀ
	cam_count = CCameraDS::CameraCount();
	printf("There are %d cameras.\n", cam_count);


	//��ȡ��������ͷ������
	for(int i=0; i < cam_count; i++)
	{
		char camera_name[1024];  
		int retval = CCameraDS::CameraName(i, camera_name, sizeof(camera_name) );

		if(retval >0)
			printf("Camera #%d's Name is '%s'.\n", i, camera_name);
		else
			printf("Can not get Camera #%d's name.\n", i);
	}

	
	//�򿪵�һ������ͷ
	//if(! camera.OpenCamera(0, true)) //��������ѡ�񴰿�
	if(! camera1.OpenCamera(0, false, 640,480)) //����������ѡ�񴰿ڣ��ô����ƶ�ͼ���͸�
	{
		fprintf(stderr, "Can not open camera.\n");
	}
	//cvNamedWindow("camera1");
	
	//if(! camera.OpenCamera(0, true)) //��������ѡ�񴰿�
	if(! camera2.OpenCamera(1, false, 640,480)) //����������ѡ�񴰿ڣ��ô����ƶ�ͼ���͸�
	{
		fprintf(stderr, "Can not open camera.\n");
	}

	cvDestroyWindow("camera1");
}

double dst(CvPoint a,CvPoint b)//��������֮�����ƽ����
{
	return((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));
}

void Calibrate1()
{
	IplImage *temp,*tempr,*tempg,*tempb;

	cvInitFont(&font,CV_FONT_HERSHEY_SIMPLEX|CV_FONT_ITALIC, hScale,vScale,0,lineWidth);//�������ṹ��ʼ��
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
		//final_img1=cvCloneImage(frame1);//�������ԭ�������ڴ�й¶...
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
		/*/////////////////////////////////////////////////////�Ҷ�ֱ��ͼ���⻯
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
		/*/////////////////////////////////////////////////////�Ҷ�ֱ��ͼ���⻯
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

	cvInitFont(&font,CV_FONT_HERSHEY_SIMPLEX|CV_FONT_ITALIC, hScale,vScale,0,lineWidth);//�������ṹ��ʼ��
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
		/*/////////////////////////////////////////////////////�Ҷ�ֱ��ͼ���⻯
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
		/*/////////////////////////////////////////////////////�Ҷ�ֱ��ͼ���⻯
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

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);  //�����ɫ������
	glEnable(GL_DEPTH_TEST);
	glColor3f(1,1,1);   //������ɫ
	glLoadIdentity();   //��վ���
	//gluLookAt(0,0,10,0,0,0,0,2,0); //�ӵ�任
	gluLookAt(5,5,5,0,0,0,0,2,0); //�ӵ�任
	glTranslatef(tx,ty,tz); //�������е�������z�Ḻ�����ƶ�5����λ��
	glRotatef(angle,-nvec.x,nvec.y,nvec.z);
	glScalef(sx,sy,sz);  //ģ�ͱ任
	glutWireCube(0.1); //����ʵ����������߿�������
	//glutWireCube(2);
	glFlush();   //ˢ�´�������ʾ��ǰ����ͼ��
} 
void init(void)
{
	glClearColor(0,0,0,0);
	glShadeModel(GL_FLAT); //ѡ��ƽ������ģʽ��⻬����ģʽ
}

void reshape(int w,int h)
{
	glViewport(0,0,(GLsizei)w,(GLsizei)h);   //���û���
	glMatrixMode(GL_PROJECTION);  //ָ����һ�������ǵ�ǰ����
	glLoadIdentity();
    gluPerspective(45,1,1.5,20);   //����͸��ͶӰ����(fovy,aspect,zNear,zFar);
	//glFrustum(-1,1,-1,1,1.5,20.0);  //��͸�Ӿ�����Ե�ǰ����(left,Right,bottom,top,near,far);
	glMatrixMode(GL_MODELVIEW);
}


void main(int argc, char *argv[])
{  

	IplImage *temph_img,*temps_img,*tempv_img,*tempb_img;
	//IplImage *finalshow_img1,*finalshow_img2;
	
	angle=0;
	CvPoint3D32f tempa=cvPoint3D32f(0,0,0);
	CvPoint3D32f tempb=cvPoint3D32f(0,0,0);//����תʱ�ݴ����������
	CvPoint3D32f prevec=cvPoint3D32f(0,0,0);
	bool startrotate=false;
	bool starttrans=false;

 	cvInitFont(&font,CV_FONT_HERSHEY_SIMPLEX|CV_FONT_ITALIC, hScale,vScale,0,lineWidth);//�������ṹ��ʼ��
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
	/////��������
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

	glutInit(&argc, argv); //�̶���ʽ
    glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);   //����ģʽ
	glutInitWindowSize(400, 400);    //��ʾ��Ĵ�С
    glutInitWindowPosition(400,400); //ȷ����ʾ�����Ͻǵ�λ��
    glutCreateWindow("����������");
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

		if(frmnum==1)//��һ֡�������ڴ�
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
		
				
		//cvErode(splitb_img1,splitg_img1,0,1);//��ʴͼ��
		//cvDilate(splitb_img1,splitg_img1,0,1);//����ͼ��
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
		
		//�������
		CvContourScanner csg=cvStartFindContours(splitg_img1,storageg,sizeof(CvContour),mode,CV_CHAIN_APPROX_SIMPLE);
		CvContourScanner csb=cvStartFindContours(splitb_img1,storageb,sizeof(CvContour),mode,CV_CHAIN_APPROX_SIMPLE);

		cvZero(contourg_img1);
		cvZero(contourb_img1);
		CvSeq* c=contourg;
		c=cvFindNextContour(csg);//����Ѱ������
		CvBox2D boxtemp;//�ݴ浱ǰ�ҵ���ƥ�������
		double areatempnow=0,areatemp=640*480;//areatempnowΪ��ǰ�ҵ���ƥ�����������areatempΪ��⵽�������������ǰһ֡�����������С��
		
  		gareanow=0;//��ǰ����������
		
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
					tdst=0;						//��ǰ������֮֡�����Ϊ�����˶�
				if (tdst&&gareanow>=1000&&((abs(gareanow-gareapre)<areatemp)||abs(640*480-gareanow-gareapre)<areatemp))//��ǰ֡��ǰ�������������ǰһ֡�������������ڵ�ǰ֡�����в�ֵ��С
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
			gareapre=areatempnow;//��ǰ����ȷ�����ǰһ������¸�ֵ
		else
			boxtemp=boxtemp;	//��ǰ��δ��ȷ��⣬�����ĵ���Ϊǰһ֡��

		/*if (frmnum<=3)
			gpos[frmnum]=cvPointFrom32f(boxtemp.center);
		else
		{
			gpos[0]=gpos[1];
			gpos[1]=gpos[2];
			gpos[2]=gpos[3];
			gpos[3]=cvPointFrom32f(boxtemp.center);
		}*///λ�ø�ֵ

		if(areatempnow!=0)
		{
			CvPoint2D32f ptf[4];
			cvBoxPoints(boxtemp,ptf);
			CvPoint pt[4];
			pt[0]=cvPointFrom32f(ptf[0]);
			pt[2]=cvPointFrom32f(ptf[2]);	
			cvRectangle(frame1,cvPoint(pt[0].x+roi_rect_g1.x,pt[0].y+roi_rect_g1.y),
						cvPoint(pt[2].x+roi_rect_g1.x,pt[2].y+roi_rect_g1.y),CV_RGB(255,0,0),1);//���������Ļ�����
			//SetCursorPos((int)boxtemp.center.x*2,(int)boxtemp.center.y*2);//�ƶ����λ��
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
		c=cvFindNextContour(csb);//����Ѱ����
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
					tdst=0;						//��ǰ������֮֡�����Ϊ�����˶�
				if (tdst&&bareanow>=1000&&((abs(bareanow-bareapre)<areatemp)))//��ǰ֡��ǰ�������������ǰһ֡�������������ڵ�ǰ֡�����в�ֵ��С
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
			bareapre=areatempnow;//��ǰ����ȷ�����ǰһ������¸�ֵ
		else
			boxtemp=boxtemp;	//��ǰ��δ��ȷ��⣬�����ĵ���Ϊǰһ֡��

		/*if (frmnum<=3&&areatempnow!=0)
			bpos[frmnum]=cvPointFrom32f(boxtemp.center);
		else if(areatempnow!=0)
		{
			bpos[0]=bpos[1];
			bpos[1]=bpos[2];
			bpos[2]=bpos[3];
			bpos[3]=cvPointFrom32f(boxtemp.center);
		}*///λ�ø�ֵ
		
		if (areatempnow!=0)
		{
			CvPoint2D32f ptf[4];
			cvBoxPoints(boxtemp,ptf);
			CvPoint pt[4];
			cvBoxPoints(boxtemp,ptf);
			pt[0]=cvPointFrom32f(ptf[0]);
			pt[2]=cvPointFrom32f(ptf[2]);	
			cvRectangle(frame1,cvPoint(pt[0].x+roi_rect_b1.x,pt[0].y+roi_rect_b1.y),
						cvPoint(pt[2].x+roi_rect_b1.x,pt[2].y+roi_rect_b1.y),CV_RGB(255,0,0),1);//���������Ļ�����
			bfp=cvPointFrom32f(boxtemp.center);
			bfbox=boxtemp;
			//SetCursorPos((int)(boxtemp.center.x*65/32-10),(int)(boxtemp.center.y*41/24-10));//�ƶ����λ��

			
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
			double tdst=fabs(dst(bfp,gfp));//�ж���ָ�Ƿ�����õľ���ƽ��
		
			if (2*tdst<(0.5*gfbox.size.height+0.5*bfbox.size.height)*(0.5*gfbox.size.height+0.5*bfbox.size.height)+(0.5*gfbox.size.width+0.5*bfbox.size.width)*(0.5*gfbox.size.width+0.5*bfbox.size.width))
			{
			
				//mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);//����������
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
		
		//�������
		csg=cvStartFindContours(splitg_img2,storageg,sizeof(CvContour),mode,CV_CHAIN_APPROX_SIMPLE);
		csb=cvStartFindContours(splitb_img2,storageb,sizeof(CvContour),mode,CV_CHAIN_APPROX_SIMPLE);

		cvZero(contourg_img2);
		cvZero(contourb_img2);
		c=contourg;
		c=cvFindNextContour(csg);//����Ѱ������
		boxtemp;//�ݴ浱ǰ�ҵ���ƥ�������
		areatempnow=0,areatemp=640*480;//areatempnowΪ��ǰ�ҵ���ƥ�����������areatempΪ��⵽�������������ǰһ֡�����������С��
		
  		gareanow=0;//��ǰ����������
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
					tdst=0;						//��ǰ������֮֡�����Ϊ�����˶�
				if (tdst&&gareanow>=1000&&((abs(gareanow-gareapre)<areatemp)||abs(640*480-gareanow-gareapre)<areatemp))//��ǰ֡��ǰ�������������ǰһ֡�������������ڵ�ǰ֡�����в�ֵ��С
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
			gareapre=areatempnow;//��ǰ����ȷ�����ǰһ������¸�ֵ
		else
			boxtemp=boxtemp;	//��ǰ��δ��ȷ��⣬�����ĵ���Ϊǰһ֡��

		/*if (frmnum<=3)
			gpos[frmnum]=cvPointFrom32f(boxtemp.center);
		else
		{
			gpos[0]=gpos[1];
			gpos[1]=gpos[2];
			gpos[2]=gpos[3];
			gpos[3]=cvPointFrom32f(boxtemp.center);
		}*///λ�ø�ֵ

		if(areatempnow!=0)
		{
			CvPoint2D32f ptf[4];
			cvBoxPoints(boxtemp,ptf);
			CvPoint pt[4];
			pt[0]=cvPointFrom32f(ptf[0]);
			pt[2]=cvPointFrom32f(ptf[2]);
			cvRectangle(frame2,cvPoint(pt[0].x+roi_rect_g2.x,pt[0].y+roi_rect_g2.y),
						cvPoint(pt[2].x+roi_rect_g2.x,pt[2].y+roi_rect_g2.y),CV_RGB(255,0,0),1);//���������Ļ�����
			//cvRectangle(frame2,pt[0],pt[2],CV_RGB(255,0,0),1);//���������Ļ�����
			//SetCursorPos((int)boxtemp.center.x*2,(int)boxtemp.center.y*2);//�ƶ����λ��
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
		c=cvFindNextContour(csb);//����Ѱ����
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
					tdst=0;						//��ǰ������֮֡�����Ϊ�����˶�
				if (tdst&&bareanow>=1000&&((abs(bareanow-bareapre)<areatemp)))//��ǰ֡��ǰ�������������ǰһ֡�������������ڵ�ǰ֡�����в�ֵ��С
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
			bareapre=areatempnow;//��ǰ����ȷ�����ǰһ������¸�ֵ
		else
			boxtemp=boxtemp;	//��ǰ��δ��ȷ��⣬�����ĵ���Ϊǰһ֡��

		/*if (frmnum<=3&&areatempnow!=0)
			bpos[frmnum]=cvPointFrom32f(boxtemp.center);
		else if(areatempnow!=0)
		{
			bpos[0]=bpos[1];
			bpos[1]=bpos[2];
			bpos[2]=bpos[3];
			bpos[3]=cvPointFrom32f(boxtemp.center);
		}*///λ�ø�ֵ
		
		if (areatempnow!=0)
		{
			CvPoint2D32f ptf[4];
			cvBoxPoints(boxtemp,ptf);
			CvPoint pt[4];
			cvBoxPoints(boxtemp,ptf);
			pt[0]=cvPointFrom32f(ptf[0]);
			pt[2]=cvPointFrom32f(ptf[2]);
			cvRectangle(frame2,cvPoint(pt[0].x+roi_rect_b2.x,pt[0].y+roi_rect_b2.y),
						cvPoint(pt[2].x+roi_rect_b2.x,pt[2].y+roi_rect_b2.y),CV_RGB(255,0,0),1);//���������Ļ�����
			//cvRectangle(frame2,pt[0],pt[2],CV_RGB(255,0,0),1);//���������Ļ�����
			bfp=cvPointFrom32f(boxtemp.center);
			bfbox=boxtemp;
			//SetCursorPos((int)(boxtemp.center.x*65/32-10),(int)(boxtemp.center.y*41/24-10));//�ƶ����λ��

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
			double tdst=fabs(dst(bfp,gfp));//�ж���ָ�Ƿ����õľ���ƽ��
		
			if (2*tdst<(0.5*gfbox.size.height+0.5*bfbox.size.height)*(0.5*gfbox.size.height+0.5*bfbox.size.height)+(0.5*gfbox.size.width+0.5*bfbox.size.width)*(0.5*gfbox.size.width+0.5*bfbox.size.width))
			{
				//mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);//����������
				cvPutText( frame2,"left mouse button down",cvPoint(100,100), &font ,CV_RGB(0,255,0));
			}//�ж�����Ƿ���
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
			//��ʼ��������
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
		if (tkey==116)//tkey=116ƽ��
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

		if (tkey==114)//tkey=114Ϊ��ת
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
				angle=acos((tempa.x*tempb.x+tempa.y*tempb.y+tempa.z*tempb.z)/sqrt(asize*bsize))*57.29577951;//acos������ȣ�ҪתΪ�Ƕ�
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
		cout<< "Running time is: "<<static_cast<double>(end_time-start_time)/CLOCKS_PER_SEC*1000<<"ms"<<endl;//�������ʱ��

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