#include <stdio.h>
#include <algorithm>
#include <vector>
#include <math.h>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "net.h"
using namespace std;
using namespace cv;

struct Bbox
{
	float score;
	int x1;
	int y1;
	int x2;
	int y2;
	float area;
	bool exist;
	float ppoint[10];
	float regreCoord[4];     
};

struct orderScore
{
	float score;
	int oriOrder;
};
bool cmpScore(orderScore lsh, orderScore rsh) {
	if (lsh.score<rsh.score)
		return true;
	else
		return false;
}

class mtcnn {
public:
	mtcnn();
	void detect(ncnn::Mat& img_, std::vector<Bbox>& finalBbox);
private:
	void generateBbox(ncnn::Mat score, ncnn::Mat location, vector<Bbox>& boundingBox_, vector<orderScore>& bboxScore_, float scale);
	void nms(vector<Bbox> &boundingBox_, std::vector<orderScore> &bboxScore_, const float overlap_threshold, string modelname = "Union");
	void refineAndSquareBbox(vector<Bbox> &vecBbox, const int &height, const int &width);

	ncnn::Net Pnet, Rnet, Onet;
	ncnn::Mat img;
	// 上面已经解释过了
	const float nms_threshold[3] = { (float)0.5, (float)0.7, (float)0.7 };
	const float threshold[3] = { (float)0.6, (float)0.6, (float)0.6 };
	const float mean_vals[3] = { (float)127.5, (float)127.5, (float)127.5 };
	const float norm_vals[3] = { (float)0.0078125, (float)0.0078125,(float) 0.0078125 };
	std::vector<Bbox> firstBbox_, secondBbox_, thirdBbox_;
	std::vector<orderScore> firstOrderScore_, secondBboxScore_, thirdBboxScore_;
	int img_w, img_h;
};

mtcnn::mtcnn() {
	Pnet.load_param("C:\\Users\\Administrator\\Desktop\\ncnn\\mtcnn\\det1.param");
	Pnet.load_model("C:\\Users\\Administrator\\Desktop\\ncnn\\mtcnn\\det1.bin");
	Rnet.load_param("C:\\Users\\Administrator\\Desktop\\ncnn\\mtcnn\\det2.param");
	Rnet.load_model("C:\\Users\\Administrator\\Desktop\\ncnn\\mtcnn\\det2.bin");
	Onet.load_param("C:\\Users\\Administrator\\Desktop\\ncnn\\mtcnn\\det3.param");
	Onet.load_model("C:\\Users\\Administrator\\Desktop\\ncnn\\mtcnn\\det3.bin");
}

// 根据Pnet的输出结果，由滑框的得分，筛选可能是人脸的滑框，并记录该框的位置、人脸坐标信息、得分以及编号
void mtcnn::generateBbox(ncnn::Mat score, ncnn::Mat location, std::vector<Bbox>& boundingBox_, std::vector<orderScore>& bboxScore_, float scale) {
	int stride = 2;
	int cellsize = 12;
	int count = 0;
	//score p 判定为人脸的概率
	float *p = score.channel(1);
	// 人脸框回归偏移量
	float *plocal = location.channel(0);
	Bbox bbox;
	orderScore order;
	for (int row = 0; row<score.h; row++) {
		for (int col = 0; col<score.w; col++) {
			if (*p>threshold[0]) {
				bbox.score = *p;
				order.score = *p;
				order.oriOrder = count;
				// 对应原图中的坐标
				bbox.x1 = round((stride*col + 1) / scale);
				bbox.y1 = round((stride*row + 1) / scale);
				bbox.x2 = round((stride*col + 1 + cellsize) / scale);
				bbox.y2 = round((stride*row + 1 + cellsize) / scale);
				bbox.exist = true;
				// 在原图中的大小
				bbox.area = (bbox.x2 - bbox.x1)*(bbox.y2 - bbox.y1);
				// 当前人脸框的回归坐标
				for (int channel = 0; channel<4; channel++)
					bbox.regreCoord[channel] = location.channel(channel)[0];
				boundingBox_.push_back(bbox);
				bboxScore_.push_back(order);
				count++;
			}
			p++;
			plocal++;
		}
	}
}
void mtcnn::nms(std::vector<Bbox> &boundingBox_, std::vector<orderScore> &bboxScore_, const float overlap_threshold, string modelname) {
	if (boundingBox_.empty()) {
		return;
	}
	std::vector<int> heros;
	//sort the score
	sort(bboxScore_.begin(), bboxScore_.end(), cmpScore);

	int order = 0;
	float IOU = 0;
	float maxX = 0;
	float maxY = 0;
	float minX = 0;
	float minY = 0;
	// 规则，站上擂台的擂台主，永远都是胜利者
	while (bboxScore_.size()>0) {
		order = bboxScore_.back().oriOrder; //取得分最高勇士的编号ID
		bboxScore_.pop_back(); // 勇士出列
		if (order<0)continue; //死的？下一个！（order在(*it).oriOrder = -1;改变）
		if (boundingBox_.at(order).exist == false) continue; //记录擂台主ID
		heros.push_back(order);
		boundingBox_.at(order).exist = false;//当前这个Bbox为擂台主，签订生死簿

		for (int num = 0; num<boundingBox_.size(); num++) {
			if (boundingBox_.at(num).exist) {// 活着的勇士
				//the iou
				maxX = (boundingBox_.at(num).x1>boundingBox_.at(order).x1) ? boundingBox_.at(num).x1 : boundingBox_.at(order).x1;
				maxY = (boundingBox_.at(num).y1>boundingBox_.at(order).y1) ? boundingBox_.at(num).y1 : boundingBox_.at(order).y1;
				minX = (boundingBox_.at(num).x2<boundingBox_.at(order).x2) ? boundingBox_.at(num).x2 : boundingBox_.at(order).x2;
				minY = (boundingBox_.at(num).y2<boundingBox_.at(order).y2) ? boundingBox_.at(num).y2 : boundingBox_.at(order).y2;
				//maxX1 and maxY1 reuse 
				maxX = ((minX - maxX + 1)>0) ? (minX - maxX + 1) : 0;
				maxY = ((minY - maxY + 1)>0) ? (minY - maxY + 1) : 0;
				//IOU reuse for the area of two bbox
				IOU = maxX * maxY;
				if (!modelname.compare("Union"))
					IOU = IOU / (boundingBox_.at(num).area + boundingBox_.at(order).area - IOU);
				else if (!modelname.compare("Min")) {
					IOU = IOU / ((boundingBox_.at(num).area<boundingBox_.at(order).area) ? boundingBox_.at(num).area : boundingBox_.at(order).area);
				}
				if (IOU>overlap_threshold) {
					boundingBox_.at(num).exist = false; //如果该对比框与擂台主的IOU够大，挑战者勇士战死
					for (vector<orderScore>::iterator it = bboxScore_.begin(); it != bboxScore_.end(); it++) {
						if ((*it).oriOrder == num) {
							(*it).oriOrder = -1;//勇士战死标志
							break;
						}
					}
				}
				//那些距离擂台主比较远迎战者幸免于难，将有机会作为擂台主出现
			}
		}
	}
	//从生死簿上剔除，擂台主活下来了
	for (int i = 0; i<heros.size(); i++)
		boundingBox_.at(heros.at(i)).exist = true;
}
void mtcnn::refineAndSquareBbox(vector<Bbox> &vecBbox, const int &height, const int &width) {
	if (vecBbox.empty()) {
		cout << "Bbox is empty!!" << endl;
		return;
	}
	float bbw = 0, bbh = 0, maxSide = 0;
	float h = 0, w = 0;
	float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	for (vector<Bbox>::iterator it = vecBbox.begin(); it != vecBbox.end(); it++) {
		if ((*it).exist) {
			bbw = (*it).x2 - (*it).x1 + 1;
			bbh = (*it).y2 - (*it).y1 + 1;
			x1 = (*it).x1 + (*it).regreCoord[0] * bbw;
			y1 = (*it).y1 + (*it).regreCoord[1] * bbh;
			x2 = (*it).x2 + (*it).regreCoord[2] * bbw;
			y2 = (*it).y2 + (*it).regreCoord[3] * bbh;

			w = x2 - x1 + 1;
			h = y2 - y1 + 1;

			maxSide = (h>w) ? h : w;
			x1 = x1 + w*0.5 - maxSide*0.5;
			y1 = y1 + h*0.5 - maxSide*0.5;
			(*it).x2 = round(x1 + maxSide - 1);
			(*it).y2 = round(y1 + maxSide - 1);
			(*it).x1 = round(x1);
			(*it).y1 = round(y1);

			//boundary check
			if ((*it).x1<0)(*it).x1 = 0;
			if ((*it).y1<0)(*it).y1 = 0;
			if ((*it).x2>width)(*it).x2 = width - 1;
			if ((*it).y2>height)(*it).y2 = height - 1;

			it->area = (it->x2 - it->x1)*(it->y2 - it->y1);
		}
	}
}
void mtcnn::detect(ncnn::Mat& img_, std::vector<Bbox>& finalBbox_) {
	firstBbox_.clear();
	firstOrderScore_.clear();
	secondBbox_.clear();
	secondBboxScore_.clear();
	thirdBbox_.clear();
	thirdBboxScore_.clear();

	img = img_;
	img_w = img.w;
	img_h = img.h;
	img.substract_mean_normalize(mean_vals, norm_vals);

	float minl = img_w<img_h ? img_w : img_h;

	// 缩放到12为止
	int MIN_DET_SIZE = 12;
	// 可以检测的最小人脸
	int minsize = 40;
	float m = (float)MIN_DET_SIZE / minsize;
	minl *= m;
	float factor = 0.709;
	int factor_count = 0;
	vector<float> scales_;
	while (minl>MIN_DET_SIZE) {
		if (factor_count>0)m = m*factor;
		scales_.push_back(m);
		minl *= factor;
		factor_count++;
	}
	orderScore order;
	int count = 0;

	for (size_t i = 0; i < scales_.size(); i++) {
		int hs = (int)ceil(img_h*scales_[i]);
		int ws = (int)ceil(img_w*scales_[i]);
		//ncnn::Mat in = ncnn::Mat::from_pixels_resize(image_data, ncnn::Mat::PIXEL_RGB2BGR, img_w, img_h, ws, hs);
		ncnn::Mat in;
		resize_bilinear(img_, in, ws, hs);
		//in.substract_mean_normalize(mean_vals, norm_vals);
		ncnn::Extractor ex = Pnet.create_extractor();
		ex.set_light_mode(true);
		ex.input("data", in);
		ncnn::Mat score_, location_;
		ex.extract("prob1", score_);
		ex.extract("conv4-2", location_);
		std::vector<Bbox> boundingBox_;
		std::vector<orderScore> bboxScore_;
		generateBbox(score_, location_, boundingBox_, bboxScore_, scales_[i]);
		nms(boundingBox_, bboxScore_, nms_threshold[0]);

		for (vector<Bbox>::iterator it = boundingBox_.begin(); it != boundingBox_.end(); it++) {
			if ((*it).exist) {
				firstBbox_.push_back(*it);
				order.score = (*it).score;
				order.oriOrder = count;
				firstOrderScore_.push_back(order);
				count++;
			}
		}
		bboxScore_.clear();
		boundingBox_.clear();
	}
	//the first stage's nms
	if (count<1)return;
	nms(firstBbox_, firstOrderScore_, nms_threshold[0]);
	refineAndSquareBbox(firstBbox_, img_h, img_w);
	printf("firstBbox_.size()=%d\n", firstBbox_.size());

	//second stage
	count = 0;
	for (vector<Bbox>::iterator it = firstBbox_.begin(); it != firstBbox_.end(); it++) {
		if ((*it).exist) {
			ncnn::Mat tempIm;
			copy_cut_border(img, tempIm, (*it).y1, img_h - (*it).y2, (*it).x1, img_w - (*it).x2);
			ncnn::Mat in;
			resize_bilinear(tempIm, in, 24, 24);
			ncnn::Extractor ex = Rnet.create_extractor();
			ex.set_light_mode(true);
			ex.input("data", in);
			ncnn::Mat score, bbox;
			ex.extract("prob1", score);
			ex.extract("conv5-2", bbox);
			if ((score[1])>threshold[1]) {
				for (int channel = 0; channel<4; channel++)
					it->regreCoord[channel] = bbox[channel];
				it->area = (it->x2 - it->x1)*(it->y2 - it->y1);
				it->score = score[1];
				secondBbox_.push_back(*it);
				order.score = it->score;
				order.oriOrder = count++;
				secondBboxScore_.push_back(order);
			}
			else {
				(*it).exist = false;
			}
		}
	}
	printf("secondBbox_.size()=%d\n", secondBbox_.size());
	if (count<1)return;
	nms(secondBbox_, secondBboxScore_, nms_threshold[1]);
	refineAndSquareBbox(secondBbox_, img_h, img_w);

	//third stage 
	count = 0;
	for (vector<Bbox>::iterator it = secondBbox_.begin(); it != secondBbox_.end(); it++) {
		if ((*it).exist) {
			ncnn::Mat tempIm;
			copy_cut_border(img, tempIm, (*it).y1, img_h - (*it).y2, (*it).x1, img_w - (*it).x2);
			ncnn::Mat in;
			resize_bilinear(tempIm, in, 48, 48);
			ncnn::Extractor ex = Onet.create_extractor();
			ex.set_light_mode(true);
			ex.input("data", in);
			ncnn::Mat score, bbox, keyPoint;
			ex.extract("prob1", score);
			ex.extract("conv6-2", bbox);
			ex.extract("conv6-3", keyPoint);
			if (score[1]>threshold[2]) {
				for (int channel = 0; channel<4; channel++)
					it->regreCoord[channel] = bbox[channel];
				it->area = (it->x2 - it->x1)*(it->y2 - it->y1);
				it->score = score[1];
				for (int num = 0; num<5; num++) {
					(it->ppoint)[num] = it->x1 + (it->x2 - it->x1)*keyPoint[num];
					(it->ppoint)[num + 5] = it->y1 + (it->y2 - it->y1)*keyPoint[num + 5];
				}

				thirdBbox_.push_back(*it);
				order.score = it->score;
				order.oriOrder = count++;
				thirdBboxScore_.push_back(order);
			}
			else
				(*it).exist = false;
		}
	}

	printf("thirdBbox_.size()=%d\n", thirdBbox_.size());
	if (count<1)return;
	refineAndSquareBbox(thirdBbox_, img_h, img_w);
	nms(thirdBbox_, thirdBboxScore_, nms_threshold[2], "Min");
	finalBbox_ = thirdBbox_;
}

int main()
{
	const char* imagepath = "F:\\3.jpg";

	cv::Mat cv_img = cv::imread(imagepath, CV_LOAD_IMAGE_COLOR);
	if (cv_img.empty())
	{
		fprintf(stderr, "cv::imread %s failed\n", imagepath);
		return -1;
	}
	std::vector<Bbox> finalBbox;
	mtcnn mm;
	ncnn::Mat ncnn_img = ncnn::Mat::from_pixels(cv_img.data, ncnn::Mat::PIXEL_BGR2RGB, cv_img.cols, cv_img.rows);
	mm.detect(ncnn_img, finalBbox);
	for (vector<Bbox>::iterator it = finalBbox.begin(); it != finalBbox.end(); it++) {
		if ((*it).exist) {
			rectangle(cv_img, Point((*it).x1, (*it).y1), Point((*it).x2, (*it).y2), Scalar(0, 0, 255), 2, 8, 0);
			for (int num = 0; num<5; num++)circle(cv_img, Point((int)*(it->ppoint + num), (int)*(it->ppoint + num + 5)), 3, Scalar(0, 255, 255), -1);
		}
	}
	imshow("result", cv_img);
	cv::imwrite("F:\\result.jpg", cv_img);
	waitKey(0);
	return 0;
}