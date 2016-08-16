#pragma once

#include "CNNParam.h"
#include "cuDNNWrapper.h"

#ifndef _DEBUG
const int minibatch_size = 4;
#else
const int minibatch_size = 1;
#endif // !_DEBUG

const int feature_num = 4;
const int filter_num = 192;

void param_to_device(float** pfilterData_dev, const size_t filterDataSize, shared_ptr<ParamMap> param_map, const char* param_name);
void dump_param(const size_t filterDataSize, shared_ptr<ParamMap> param_map, const char* param_name);
void dump_data(FILE* fp, const char* name, float* data, const size_t size);
void dump_device_data(const char* name, float* data_dev, const size_t size);

class CNN
{
private:
	CudnnHandle cudnnHandle;
	CudnnTensorDescriptor srcTensorDesc, hdnTensorDesc, biasTensorDesc, biasTensorDesc13, dstTensorDesc;
	CudnnFilterDescriptor filterDesc1, filterDesc2, filterDesc13;
	CudnnConvolutionDescriptor convDesc1, convDesc2, convDesc13;
	CudnnActivationDescriptor  activDesc;

	cudnnConvolutionFwdAlgo_t algo1;
	size_t workSpaceSizeInBytes1;
	void* workSpace1;

	cudnnConvolutionFwdAlgo_t algo2;
	size_t workSpaceSizeInBytes2;
	void* workSpace2;

	cudnnConvolutionFwdAlgo_t algo13;
	size_t workSpaceSizeInBytes13;
	void* workSpace13;

	float *filterData_dev1;
	float *biasData_dev1;
	float *filterData_dev2;
	float *biasData_dev2;
	float *filterData_dev3;
	float *biasData_dev3;
	float *filterData_dev4;
	float *biasData_dev4;
	float *filterData_dev5;
	float *biasData_dev5;
	float *filterData_dev6;
	float *biasData_dev6;
	float *filterData_dev7;
	float *biasData_dev7;
	float *filterData_dev8;
	float *biasData_dev8;
	float *filterData_dev9;
	float *biasData_dev9;
	float *filterData_dev10;
	float *biasData_dev10;
	float *filterData_dev11;
	float *biasData_dev11;
	float *filterData_dev12;
	float *biasData_dev12;
	float *filterData_dev13;
	float *biasData_dev13;
	float *srcData_dev;
	float *hdnData_dev1;
	float *hdnData_dev2;
	float *hdnData_dev13;
	float *dstData_dev;

public:
	void prepare_network(const wchar_t* filename);
	void forward(float srcData[minibatch_size][feature_num][19][19], float dstData[minibatch_size][1][19][19]);
};