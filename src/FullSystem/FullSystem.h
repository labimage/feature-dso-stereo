/**
* This file is part of DSO.
*
* Copyright 2016 Technical University of Munich and Intel.
* Developed by Jakob Engel <engelj at in dot tum dot de>,
* for more information see <http://vision.in.tum.de/dso>.
* If you use this code, please cite the respective publications as
* listed on the above website.
*
* DSO is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* DSO is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with DSO. If not, see <http://www.gnu.org/licenses/>.
*/


#pragma once
#define MAX_ACTIVE_FRAMES 100

#include "util/NumType.h"
#include "util/globalCalib.h"
#include "util/tic_toc.h"
#include "vector"

#include <iostream>
#include <fstream>
#include "util/NumType.h"
#include "FullSystem/Residuals.h"
#include "FullSystem/FrameHessian.h"
#include "FullSystem/PointHessian.h"
#include "FullSystem/CalibHessian.h"
#include "FullSystem/FrameShell.h"
#include "FullSystem/LoopClosing.h"
#include "util/IndexThreadReduce.h"
#include "OptimizationBackend/EnergyFunctional.h"

#include "FullSystem/PixelSelector2.h"

#include "FullSystem/FeatureDetector.h"
#include "FullSystem/FeatureMatcher.h"

#include "FullSystem/OptimizerPnP.h"

#include "FullSystem/Map.h"

#include <math.h>
#include <boost/timer.hpp>

namespace fdso
{
namespace IOWrap
{
class Output3DWrapper;
}

class PixelSelector;
class PCSyntheticPoint;
class CoarseTracker;
struct FrameHessian;
struct PointHessian;
class CoarseInitializer;
struct ImmaturePointTemporaryResidual;
class ImageAndExposure;
class CoarseDistanceMap;

class EnergyFunctional;

class FeatureMatcher;
class LoopClosing;


// template<typename T> inline void deleteOut(std::vector<std::shared_ptr<T>> &v, const int i)
// {
// 	// delete v[i];

// 	v[i] = v.back();
// 	v.pop_back();
// }
// template<typename T> inline void deleteOutPt(std::vector<std::shared_ptr<T>> &v, const std::shared_ptr<T> i)
// {
// 	// delete i;

// 	for (unsigned int k = 0; k < v.size(); k++)
// 		if (v[k] == i)
// 		{
// 			v[k] = v.back();
// 			v.pop_back();
// 		}
// }
// template<typename T> inline void deleteOutOrder(std::vector<std::shared_ptr<T>> &v, const int i)
// {
// 	// delete v[i];
// 	for (unsigned int k = i + 1; k < v.size(); k++)
// 		v[k - 1] = v[k];
// 	v.pop_back();
// }
// template<typename T> inline void deleteOutOrder(std::vector<std::shared_ptr<T>> &v, const std::shared_ptr<T> element)
// {
// 	int i = -1;
// 	for (unsigned int k = 0; k < v.size(); k++)
// 	{
// 		if (v[k] == element)
// 		{
// 			i = k;
// 			break;
// 		}
// 	}
// 	assert(i != -1);

// 	for (unsigned int k = i + 1; k < v.size(); k++)
// 		v[k - 1] = v[k];
// 	v.pop_back();

// 	// delete element;
// }

// // delete an element from a vector and keep its order
// template<typename T>
// inline void deleteOutOrder(std::vector<T> &v, const T element)
// {
// 	int i = -1;
// 	for (unsigned int k = 0; k < v.size(); k++)
// 	{
// 		if (v[k] == element) {
// 			i = k;
// 			break;
// 		}
// 	}
// 	assert(i != -1);

// 	for (unsigned int k = i + 1; k < v.size(); k++)
// 		v[k - 1] = v[k];
// 	v.pop_back();
// }


template<typename T> inline void deleteOut(std::vector<T*> &v, const int i)
{
	delete v[i];
	v[i] = v.back();
	v.pop_back();
}
template<typename T> inline void deleteOutPt(std::vector<T*> &v, const T* i)
{
	delete i;

	for (unsigned int k = 0; k < v.size(); k++)
		if (v[k] == i)
		{
			v[k] = v.back();
			v.pop_back();
		}
}
template<typename T> inline void deleteOutOrder(std::vector<T*> &v, const int i)
{
	delete v[i];
	for (unsigned int k = i + 1; k < v.size(); k++)
		v[k - 1] = v[k];
	v.pop_back();
}
template<typename T> inline void deleteOutOrder(std::vector<T*> &v, const T* element)
{
	int i = -1;
	for (unsigned int k = 0; k < v.size(); k++)
	{
		if (v[k] == element)
		{
			i = k;
			break;
		}
	}
	assert(i != -1);

	for (unsigned int k = i + 1; k < v.size(); k++)
		v[k - 1] = v[k];
	v.pop_back();

	delete element;
}


inline bool eigenTestNan(MatXX m, std::string msg)
{
	bool foundNan = false;
	for (int y = 0; y < m.rows(); y++)
		for (int x = 0; x < m.cols(); x++)
		{
			if (!std::isfinite((double)m(y, x))) foundNan = true;
		}

	if (foundNan)
	{
		printf("NAN in %s:\n", msg.c_str());
		std::cout << m << "\n\n";
	}


	return foundNan;
}

class FullSystem {
public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW;
	FullSystem(std::shared_ptr<ORBVocabulary> voc);
	virtual ~FullSystem();

	// adds a new frame, and creates point & residual structs.
	void addActiveFrame(ImageAndExposure* image, ImageAndExposure* image_right, int id);

	// marginalizes a frame. drops / marginalizes points & residuals.
	void marginalizeFrame(FrameHessian* frame);
	void blockUntilMappingIsFinished();

	float optimize(int mnumOptIts);

	void makeCurrentDepth(FrameHessian* fh, FrameHessian* fhRight);

	//compute stereo idepth
	void stereoMatch(ImageAndExposure* image, ImageAndExposure* image_right, int id, cv::Mat &idepthMap);
	void stereoMatch(FrameHessian* fh, FrameHessian* fhRight);

	void printResult(std::string file);

	void debugPlot(std::string name);

	void printFrameLifetimes();
	// contains pointers to active frames

	std::vector<IOWrap::Output3DWrapper*> outputWrapper;

	bool isLost;
	bool initFailed;
	bool initialized;
	bool linearizeOperation;

	std::shared_ptr<ORBVocabulary> _vocab;
	//矫正类
	CalibHessian Hcalib;

	//全部的关键帧
	Map* globalMap = nullptr;

	//闭环检测类
	LoopClosing* loopClosing = nullptr;

	void setGammaFunction(float* BInv);
	void setOriginalCalib(VecXf originalCalib, int originalW, int originalH);

private:

	// opt single point
	//　优化一个点
	int optimizePoint(PointHessian* point, int minObs, bool flagOOB);

	//优化一个未成熟点
	PointHessian* optimizeImmaturePoint(ImmaturePoint* point, int minObs, ImmaturePointTemporaryResidual* residuals);

	//
	double linAllPointSinle(PointHessian* point, float outlierTHSlack, bool plot);

	//非关键帧的跟踪
	void traceNewCoarseNonKey(FrameHessian* fh, FrameHessian* fhRight);


	// mainPipelineFunctions
	//主跟踪函数
	Vec4 trackNewCoarse(FrameHessian* fh, FrameHessian* fhRight, SE3 initT,bool usePnP);
	//关键帧的更新
	void traceNewCoarseKey(FrameHessian* fh, FrameHessian* fhRight);
	//更新一个点
	void activatePoints();
	void activatePointsMT();
	void activatePointsOldFirst();
	void flagPointsForRemoval();
	void makeNewTraces(FrameHessian* newFrame, FrameHessian* newFrameRight, float* gtDepth);
	void initializeFromInitializer(FrameHessian* newFrame);
	// void initializeFromInitializer(FrameHessian* newFrame, FrameHessian* newFrame_right);

	void initializeFromInitializerStereo(FrameHessian* newFrame);
	void flagFramesForMarginalization(FrameHessian* newFH);

	void removeOutliers();

	// set precalc values.
	void setPrecalcValues();

	// solce. eventually migrate to ef.
	void solveSystem(int iteration, double lambda);
	Vec3 linearizeAll(bool fixLinearization);
	bool doStepFromBackup(float stepfacC, float stepfacT, float stepfacR, float stepfacA, float stepfacD);
	void backupState(bool backupLastStep);
	void loadSateBackup();
	double calcLEnergy();
	double calcMEnergy();
	void linearizeAll_Reductor(bool fixLinearization, std::vector<PointFrameResidual*>* toRemove, int min, int max, Vec10* stats, int tid);
	void activatePointsMT_Reductor(std::vector<PointHessian*>* optimized, std::vector<ImmaturePoint*>* toOptimize, int min, int max, Vec10* stats, int tid);
	void applyRes_Reductor(bool copyJacobians, int min, int max, Vec10* stats, int tid);

	void printOptRes(Vec3 res, double resL, double resM, double resPrior, double LExact, float a, float b);

	void debugPlotTracking();

	std::vector<VecX> getNullspaces(
	  std::vector<VecX> &nullspaces_pose,
	  std::vector<VecX> &nullspaces_scale,
	  std::vector<VecX> &nullspaces_affA,
	  std::vector<VecX> &nullspaces_affB);

	void setNewFrameEnergyTH();


	void printLogLine();
	void printEvalLine();
	void printEigenValLine();
	std::ofstream* calibLog;
	std::ofstream* numsLog;
	std::ofstream* errorsLog;
	std::ofstream* eigenAllLog;
	std::ofstream* eigenPLog;
	std::ofstream* eigenALog;
	std::ofstream* DiagonalLog;
	std::ofstream* variancesLog;
	std::ofstream* nullspacesLog;

	std::ofstream* coarseTrackingLog;

	// statistics
	long int statistics_lastNumOptIts;
	long int statistics_numDroppedPoints;
	long int statistics_numActivatedPoints;
	long int statistics_numCreatedPoints;
	long int statistics_numForceDroppedResBwd;
	long int statistics_numForceDroppedResFwd;
	long int statistics_numMargResFwd;
	long int statistics_numMargResBwd;
	float statistics_lastFineTrackRMSE;


	// =================== changed by tracker-thread. protected by trackMutex ============
	//跟踪互斥锁
	boost::mutex trackMutex;
	//全部帧的信息
	std::vector<FrameShell*> allFrameHistory;

	//初始化类
	CoarseInitializer* coarseInitializer;
	//上一时刻的残差
	Vec5 lastCoarseRMSE;


	// ================== changed by mapper-thread. protected by mapMutex ===============
	//后端优化互斥锁
	boost::mutex mapMutex;
	//全部的关键帧
	std::vector<FrameShell*> allKeyFramesHistory;

	//误差能量函数
	EnergyFunctional* ef;
	//安全线程
	IndexThreadReduce<Vec10> treadReduce;

	//点选择图
	float* selectionMap;
	//点选择类
	PixelSelector* pixelSelector;
	//距离图
	CoarseDistanceMap* coarseDistanceMap;

	//窗口中的每一帧的Hessian信息
	std::vector<FrameHessian*> frameHessians;	// ONLY changed in marginalizeFrame and addFrame.
	std::vector<FrameHessian *> frameHessiansRight;

	//点和帧的残差
	std::vector<PointFrameResidual*> activeResiduals;
	float currentMinActDist;

	// //右图的
	// std::vector<FrameHessian*> frameHessiansRight;

	//全部的残差
	std::vector<float> allResVec;

	// mutex etc. for tracker exchange.
	//用于跟新跟踪器的
	boost::mutex coarseTrackerSwapMutex;			// if tracker sees that there is a new reference, tracker locks [coarseTrackerSwapMutex] and swaps the two.
	CoarseTracker* coarseTracker_forNewKF;			// set as as reference. protected by [coarseTrackerSwapMutex].
	CoarseTracker* coarseTracker;					// always used to track new frames. protected by [trackMutex].
	float minIdJetVisTracker, maxIdJetVisTracker;
	float minIdJetVisDebug, maxIdJetVisDebug;


	// mutex for camToWorl's in shells (these are always in a good configuration).
	//位姿互斥锁
	boost::mutex shellPoseMutex;

	/*
	 * tracking always uses the newest KF as reference.
	 *
	 */
	//创建关键帧
	void makeKeyFrame( FrameHessian* fh, FrameHessian* fhRight);
	//创建非关键帧
	void makeNonKeyFrame( FrameHessian* fh, FrameHessian* fhRight);

	//跟踪和后端优化的连接传递函数
	void deliverTrackedFrame(FrameHessian* fh, FrameHessian* fhRight, bool needKF);

	//后端优化
	void mappingLoop();


	// tracking / mapping synchronization. All protected by [trackMapSyncMutex].
	//非同步的跟踪和后端优化的一些变量
	//互斥锁
	boost::mutex trackMapSyncMutex;

	//条件变量
	boost::condition_variable trackedFrameSignal;
	boost::condition_variable mappedFrameSignal;

	//非同步优化的时候的队列
	std::deque<FrameHessian*> unmappedTrackedFrames;
	std::deque<FrameHessian*> unmappedTrackedFrames_right;

	//是否是关键帧
	int needNewKFAfter;	// Otherwise, a new KF is *needed that has ID bigger than [needNewKFAfter]*.

	//后端优化线程
	boost::thread mappingThread;

	//是否运行后端优化
	bool runMapping;

	bool needToKetchupMapping;

	//上一帧的关键帧的ID
	int lastRefStopID;

	/*---ORB---*/
	FeatureDetector* detectorLeft, *detectorRight;

	FeatureMatcher* matcher;

	// void ExtractORB(int flag, const cv::Mat &im);
	bool find_feature_matches (const cv::Mat& descriptorsLast, const cv::Mat& descriptorsCur, std::vector<cv::DMatch>& feature_matches_);
};
}

