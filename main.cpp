
#include <stdio.h>
#include <highgui.h>
#include <opencv2/opencv.hpp>
#include <cv.h>
#include "opencv2/imgproc.hpp"
#include "opencv2/ximgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "math.h"

using namespace cv;
using namespace std;
// clipHistPercent 剪枝（剪去总像素的多少百分比）
// histSize 最后将所有的灰度值归到多大的范围
// lowhist 最小的灰度值

//读取指定文件下的所有图片
vector<Mat> read_images_in_folder(cv::String pattern)
{
 vector<cv::String> fn;
 glob(pattern, fn, false);
 vector<Mat> images;

 size_t count = fn.size(); //number of png files in images folder
 for (size_t i = 0; i < count; i++)
 {
    //  cout << i;
     images.push_back(imread(fn[i])); //直读取图片并返回Mat类型
 }
 return images;
}
 
//图片写入指定文件夹下
void write_images_to_folder(vector<Mat> images_lsd, cv:: String pic_out){
    for(int i = 0; i<images_lsd.size(); i++){
        stringstream wname;
        wname << pic_out << i+1 << ".jpg";
        imwrite(wname.str(), images_lsd[i]);
        
    }
}

vector<Mat> BrightnessAndContrastAuto(vector<Mat> images, float clipHistPercent=2, int histSize = 255, int lowhist = 0)
{
    vector<Mat> images_enhance;
    double alpha_sum = 0;
    double beta_sum = 0;
    for (int i = 0; i < images.size(); i++){
        Mat src = images[i];
        Mat dst;
        CV_Assert(clipHistPercent >= 0);
        CV_Assert((src.type() == CV_8UC1) || (src.type() == CV_8UC3) || (src.type() == CV_8UC4));
        
        float alpha, beta;
        double minGray = 0, maxGray = 0;
        
        //to calculate grayscale histogram
        cv::Mat gray;
    
        if (src.type() == CV_8UC1) gray = src;

        
        else if (src.type() == CV_8UC3) cvtColor(src, gray, CV_BGR2GRAY);
        
        else if (src.type() == CV_8UC4) cvtColor(src, gray, CV_BGRA2GRAY);
        
        if (clipHistPercent == 0)

        {
            // keep full available range
            cv::minMaxLoc(gray, &minGray, &maxGray);
        }
        else
        {
            cv::Mat hist; //the grayscale histogram
            
            float range[] = { 0, 256 };
            const float* histRange = { range };
            bool uniform = true;
            bool accumulate = false;
            calcHist(&gray, 1, 0, cv::Mat (), hist, 1, &histSize, &histRange, uniform, accumulate);
            //累计直方图
            // calculate cumulative distribution from the histogram
        
            std::vector<float> accumulator(histSize);
            accumulator[0] = hist.at<float>(0);
            for (int i = 1; i < histSize; i++)
            {
                accumulator[i] = accumulator[i - 1] + hist.at<float>(i);
                // cout<<hist.at<float>(i)<<", ";
            }
            // locate points that cuts at required value
            float max = accumulator.back();//最后一个元素，也就是所有的像素点个数
            
            int clipHistPercent2;
            clipHistPercent2 = clipHistPercent * (max / 100.0); //make percent as absolute 裁剪掉百分之多少的像素点数
            
            clipHistPercent2 /= 2.0; // left and right wings 一边一半
            // cout<<clipHistPercent2<<"clp"<<endl;
            
            // locate left cut
            minGray = 0;
            while (accumulator[minGray] < clipHistPercent2)
                minGray++;
            
            // locate right cut
            maxGray = histSize - 1;

            while (accumulator[maxGray] >= (max - clipHistPercent2))
                maxGray--;

        }
        
        // current range

        float inputRange = maxGray - minGray;
        // cout<<inputRange<<endl;
        // cout<<"input range: "<<maxGray<<"  "<<minGray<<endl;
        // lowhist = minGray;
        
        alpha = (histSize - lowhist) / inputRange;   // alpha expands current range to histsize range
        
        beta = -minGray * alpha + lowhist;             // beta shifts current range so that minGray will go to 0
        cout<<"****第"<<i<<"张图片****：";
        cout<<"alpha and beta:  "<<alpha<<"  "<<beta<<endl;
        alpha_sum+=alpha;
        beta_sum+=beta;

        // Apply brightness and contrast normalization
        // convertTo operates with saurate_cast
        src.convertTo(dst, -1, alpha, beta);
        images_enhance.push_back(dst);
        // restore alpha channel from source
        if (dst.type() == CV_8UC4)
        {         
            int from_to[] = { 3, 3};
            cv::mixChannels(&src, 4, &dst,1, from_to, 1);
        }
    }
    double alpha_video = alpha_sum/images.size();
    double beta_video = beta_sum/images.size();
    cout<<"average alpha and beta： "<<alpha_video<<"  "<<beta_video<<endl;

    double contarst = alpha_video;
    double brightness = (beta_video + 16*alpha_video -16)/255;
    double saturation = alpha_video;
    cout<<"contarst_yuv: "<<contarst<<endl;
    cout<<"brightness_yuv: "<<brightness<<endl;
    cout<<"saturation_yuv: "<<saturation<<endl;
    return images_enhance;
}
void get_hsv(vector<Mat> images){
    for (int i = 0; i < images.size(); i++){
        Mat img_hsv;
        cvtColor(images[i], img_hsv, CV_BGR2HSV);
        cout<<img_hsv.size<<images[i].size;
    }

}

void get_yuv(vector<Mat> images){
    for (int i = 0; i < images.size(); i++){
        Mat img_yuv;
        cvtColor(images[i], img_yuv, CV_BGR2YUV);
        cout<<format(img_yuv.rowRange(0, 1), Formatter::FMT_PYTHON)<<endl;
       
    }

}


vector<Mat> MyGammaCorrection(vector<Mat> images, float m)  
{  

    vector<Mat> images_gamma;
    for (int i = 0; i < images.size(); i++){
        cout<<"****第"<<i<<"张图片****： ";
        Mat img_gary;
        Mat src = images[i];
        cvtColor(src, img_gary, CV_BGR2GRAY);
        cv::Scalar mean = cv::mean(img_gary);//求均值

        float fGamma = std::log10(m) / std::log10(mean[0]/255);//gamma = -0.3/log10(X)
        cout<<"gamma: "<<fGamma<<endl;
        // float gamma_input = 1.0/fGamma;
        // cout<<"gamma_input"<<gamma_input<<endl;

        CV_Assert(src.data);  //若括号中的表达式为false，则返回一个错误的信息。
    
        // accept only char type matrices  
        CV_Assert(src.depth() != sizeof(uchar));  
        // build look up table  
    
        unsigned char lut[256];  
        for( int i = 0; i < 256; i++ )  
        {  
            lut[i] = pow((float)(i/255.0), fGamma) * 255.0;  
        }  
    
        //先归一化，i/255,然后进行预补偿(i/255)^fGamma,最后进行反归一化(i/255)^fGamma*255
    
        const int channels = src.channels();  
        switch(channels)  
        {  
            case 1:  
                {  
                    //运用迭代器访问矩阵元素
                    MatIterator_<uchar> it, end;  
                    for( it = src.begin<uchar>(), end = src.end<uchar>(); it != end; it++ )  
                        //*it = pow((float)(((*it))/255.0), fGamma) * 255.0;  
                        *it = lut[(*it)];  
    
                    break;  
                }  
            case 3:   
                {  
                    MatIterator_<Vec3b> it, end;  
                    for( it = src.begin<Vec3b>(), end = src.end<Vec3b>(); it != end; it++ )  
                    {  
                        //(*it)[0] = pow((float)(((*it)[0])/255.0), fGamma) * 255.0;  
                        //(*it)[1] = pow((float)(((*it)[1])/255.0), fGamma) * 255.0;  
                        //(*it)[2] = pow((float)(((*it)[2])/255.0), fGamma) * 255.0;  
                        (*it)[0] = lut[((*it)[0])];  
                        (*it)[1] = lut[((*it)[1])];  
                        (*it)[2] = lut[((*it)[2])];  
                    }  
    
                    break;  
    
                }  
        }
        images_gamma.push_back(src);
    }  
    
    return images_gamma;
   
}  

/******************读取本地视频*******************/
vector<Mat> VideoRead()
{
    vector<Mat> images;
	//读取视频
	VideoCapture capture("/home/zhaoyx/pic_enhance/inputfile.mp4");
	/*
	VideoCapture capture;
	//参数为0时打开摄像头
	captrue.open("./video/src1.mp4");
	*/
	//循环显示每一帧
    int i = 0;
	while (1)
	{
		//frame存储每一帧图像
        if (i==30)
            break;
		Mat frame;
		//读取当前帧
		capture >> frame;
		//播放完退出
		if (frame.empty()) {
			printf("播放完成\n");
			break;
		}
		//显示当前视频
        images.push_back(frame);
		// imshow("读取视频",frame);

		//延时30ms
		waitKey(30);
        i++;
	}
    return images;
}
int main(){
    Mat img;
    Mat img_enhance;

    String in_dir = "pic_input/src/";
    

    vector<Mat> images = read_images_in_folder(in_dir);

    // vector<Mat> images = VideoRead();//输入为视频
    // get_yuv(images);
    cout<<"原图进行线性矫正……"<<endl;
    vector<Mat> images_enhance = BrightnessAndContrastAuto(images);//线性图像增强

    // get_yuv(images_enhance);
    String out_dir = "pic_input/src_balance/";
    write_images_to_folder(images_enhance, out_dir);
    

    float mean = 0.4;//均值
    cout<<"线性矫正后进行gamma……"<<endl;
    vector<Mat> images_gamma = MyGammaCorrection(images_enhance, mean);//线性增强后进行gamma矫正
    out_dir = "pic_input/src_balance_gamma/";
    write_images_to_folder(images_gamma, out_dir);
    // get_yuv(images_enhance);
    cout<<"原图进行gamma矫正……"<<endl;
    images_gamma = MyGammaCorrection(images, mean);//直接进行gamma矫正
    out_dir = "pic_input/src_gamma/";
    write_images_to_folder(images_gamma, out_dir);

    cout<<"down"<<endl;

}

/*
自适应gamma https://www.freesion.com/article/3544992195/
*/