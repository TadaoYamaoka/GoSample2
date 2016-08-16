#include <stdio.h>
#include <algorithm>
#include <vector>

#include "..\Board.h"
#include "CNN.h"

using namespace std;

const size_t filterDataSize1 = filter_num * feature_num * 5 * 5;
const size_t filterDataSize2 = filter_num * filter_num * 3 * 3;
const size_t filterDataSize13 = 1 * filter_num * 1 * 1;
const size_t srcDataSize = minibatch_size * feature_num * 19 * 19;
const size_t hdnDataSize = minibatch_size * filter_num * 19 * 19;
const size_t dstDataSize = minibatch_size * 1 * 19 * 19;

void param_to_device(float** pfilterData_dev, const size_t filterDataSize, shared_ptr<ParamMap> param_map, const char* param_name)
{
	checkCudaErrors(cudaMalloc((void**)pfilterData_dev, filterDataSize * sizeof(float)));
	checkCudaErrors(cudaMemcpy(*pfilterData_dev, param_map->find(param_name)->second, filterDataSize * sizeof(float), cudaMemcpyHostToDevice));
}

void dump_param(const size_t filterDataSize, shared_ptr<ParamMap> param_map, const char* param_name)
{
	printf("%s:\n", param_name);
	float* data = param_map->find(param_name)->second;
	for (int i = 0; i < filterDataSize; i++)
	{
		printf("%f, ", data[i]);
	}
	printf("\n");
}

void dump_data(FILE* fp, const char* name, float* data, const size_t size)
{
	fprintf(fp, "%s:\n", name);
	for (int i = 0; i < size; i++)
	{
		fprintf(fp, "%f, ", data[i]);
	}
	fprintf(fp, "\n");
	fflush(fp);
}


void dump_device_data(const char* name, float* data_dev, const size_t size)
{
	unique_ptr<float[]> data(new float[size]);
	checkCudaErrors(cudaMemcpy(data.get(), data_dev, size * sizeof(float), cudaMemcpyDeviceToHost));

	printf("%s:\n", name);
	for (int i = 0; i < size; i++)
	{
		printf("%f, ", data[i]);
	}
	printf("\n");
}

void CNN::prepare_network(const wchar_t* filename)
{
	// CNNパラメータ読み込み
	shared_ptr<ParamMap> param_map = load_cnn_param(filename);
	/*for (const auto pair : *param_map)
	{
		printf("%s\n", pair.first.c_str());
	}*/

	checkCUDNN(cudnnSetTensor4dDescriptor(srcTensorDesc, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, minibatch_size, feature_num, 19, 19));
	checkCUDNN(cudnnSetTensor4dDescriptor(hdnTensorDesc, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, minibatch_size, filter_num, 19, 19));
	checkCUDNN(cudnnSetTensor4dDescriptor(biasTensorDesc, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, 1, filter_num, 1, 1));
	checkCUDNN(cudnnSetTensor4dDescriptor(biasTensorDesc13, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, 1, 1, 19, 19));
	checkCUDNN(cudnnSetTensor4dDescriptor(dstTensorDesc, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, minibatch_size, 1, 19, 19));
	checkCUDNN(cudnnSetFilter4dDescriptor(filterDesc1, CUDNN_DATA_FLOAT, CUDNN_TENSOR_NCHW, filter_num, feature_num, 5, 5));
	checkCUDNN(cudnnSetFilter4dDescriptor(filterDesc2, CUDNN_DATA_FLOAT, CUDNN_TENSOR_NCHW, filter_num, filter_num, 3, 3));
	checkCUDNN(cudnnSetFilter4dDescriptor(filterDesc13, CUDNN_DATA_FLOAT, CUDNN_TENSOR_NCHW, 1, filter_num, 1, 1));
	checkCUDNN(cudnnSetConvolution2dDescriptor(convDesc1, 2/*pad_h*/, 2/*pad_w*/, 1/*stride_h*/, 1/*stride_w*/, 1, 1, CUDNN_CROSS_CORRELATION));
	checkCUDNN(cudnnSetConvolution2dDescriptor(convDesc2, 1/*pad_h*/, 1/*pad_w*/, 1/*stride_h*/, 1/*stride_w*/, 1, 1, CUDNN_CROSS_CORRELATION));
	checkCUDNN(cudnnSetConvolution2dDescriptor(convDesc13, 0/*pad_h*/, 0/*pad_w*/, 1/*stride_h*/, 1/*stride_w*/, 1, 1, CUDNN_CROSS_CORRELATION));
	checkCUDNN(cudnnSetActivationDescriptor(activDesc, CUDNN_ACTIVATION_RELU, CUDNN_PROPAGATE_NAN, 0.0/*reluCeiling*/));

	// layer1
	checkCUDNN(cudnnGetConvolutionForwardAlgorithm(cudnnHandle,
		srcTensorDesc,
		filterDesc1,
		convDesc1,
		hdnTensorDesc,
		CUDNN_CONVOLUTION_FWD_PREFER_FASTEST,
		0,
		&algo1
	));

	workSpaceSizeInBytes1 = 0;
	checkCUDNN(cudnnGetConvolutionForwardWorkspaceSize(cudnnHandle,
		srcTensorDesc,
		filterDesc1,
		convDesc1,
		hdnTensorDesc,
		algo1,
		&workSpaceSizeInBytes1));

	workSpace1 = NULL;
	if (workSpaceSizeInBytes1 != 0)
	{
		checkCudaErrors(cudaMalloc(&workSpace1, workSpaceSizeInBytes1));
	}

	// layer2～12
	checkCUDNN(cudnnGetConvolutionForwardAlgorithm(cudnnHandle,
		hdnTensorDesc,
		filterDesc2,
		convDesc2,
		hdnTensorDesc,
		CUDNN_CONVOLUTION_FWD_PREFER_FASTEST,
		0,
		&algo2
	));

	workSpaceSizeInBytes2 = 0;
	checkCUDNN(cudnnGetConvolutionForwardWorkspaceSize(cudnnHandle,
		hdnTensorDesc,
		filterDesc2,
		convDesc2,
		hdnTensorDesc,
		algo2,
		&workSpaceSizeInBytes2));

	workSpace2 = NULL;
	if (workSpaceSizeInBytes2 != 0)
	{
		checkCudaErrors(cudaMalloc(&workSpace2, workSpaceSizeInBytes2));
	}

	// layer13
	checkCUDNN(cudnnGetConvolutionForwardAlgorithm(cudnnHandle,
		hdnTensorDesc,
		filterDesc13,
		convDesc13,
		dstTensorDesc,
		CUDNN_CONVOLUTION_FWD_PREFER_FASTEST,
		0,
		&algo13
	));

	workSpaceSizeInBytes13 = 0;
	checkCUDNN(cudnnGetConvolutionForwardWorkspaceSize(cudnnHandle,
		hdnTensorDesc,
		filterDesc13,
		convDesc13,
		dstTensorDesc,
		algo13,
		&workSpaceSizeInBytes13));

	workSpace13 = NULL;
	if (workSpaceSizeInBytes13 != 0)
	{
		checkCudaErrors(cudaMalloc(&workSpace13, workSpaceSizeInBytes13));
	}


	// パラメータ転送
	// layer1
	//dump_param(filterDataSize1, param_map, "layer1/W.npy"); // debug
	//dump_param(filter_num, param_map, "layer1/b.npy"); // debug
	param_to_device(&filterData_dev1, filterDataSize1, param_map, "layer1/W.npy");
	param_to_device(&biasData_dev1, filter_num, param_map, "layer1/b.npy");

	// layer2
	param_to_device(&filterData_dev2, filterDataSize2, param_map, "layer2/W.npy");
	param_to_device(&biasData_dev2, filter_num, param_map, "layer2/b.npy");

	// layer3
	param_to_device(&filterData_dev3, filterDataSize2, param_map, "layer3/W.npy");
	param_to_device(&biasData_dev3, filter_num, param_map, "layer3/b.npy");

	// layer4
	param_to_device(&filterData_dev4, filterDataSize2, param_map, "layer4/W.npy");
	param_to_device(&biasData_dev4, filter_num, param_map, "layer4/b.npy");

	// layer5
	param_to_device(&filterData_dev5, filterDataSize2, param_map, "layer5/W.npy");
	param_to_device(&biasData_dev5, filter_num, param_map, "layer5/b.npy");

	// layer6
	param_to_device(&filterData_dev6, filterDataSize2, param_map, "layer6/W.npy");
	param_to_device(&biasData_dev6, filter_num, param_map, "layer6/b.npy");

	// layer7
	param_to_device(&filterData_dev7, filterDataSize2, param_map, "layer7/W.npy");
	param_to_device(&biasData_dev7, filter_num, param_map, "layer7/b.npy");

	// layer8
	param_to_device(&filterData_dev8, filterDataSize2, param_map, "layer8/W.npy");
	param_to_device(&biasData_dev8, filter_num, param_map, "layer8/b.npy");

	// layer9
	param_to_device(&filterData_dev9, filterDataSize2, param_map, "layer9/W.npy");
	param_to_device(&biasData_dev9, filter_num, param_map, "layer9/b.npy");

	// layer10
	param_to_device(&filterData_dev10, filterDataSize2, param_map, "layer10/W.npy");
	param_to_device(&biasData_dev10, filter_num, param_map, "layer10/b.npy");

	// layer11
	param_to_device(&filterData_dev11, filterDataSize2, param_map, "layer11/W.npy");
	param_to_device(&biasData_dev11, filter_num, param_map, "layer11/b.npy");

	// layer12
	param_to_device(&filterData_dev12, filterDataSize2, param_map, "layer12/W.npy");
	param_to_device(&biasData_dev12, filter_num, param_map, "layer12/b.npy");

	// layer13
	param_to_device(&filterData_dev13, filterDataSize13, param_map, "layer13/W.npy");
	param_to_device(&biasData_dev13, filter_num, param_map, "layer13_2/b.npy");

	// 入力
	checkCudaErrors(cudaMalloc((void**)&srcData_dev, srcDataSize * sizeof(float)));

	// 中間層
	checkCudaErrors(cudaMalloc((void**)&hdnData_dev1, hdnDataSize * sizeof(float)));

	checkCudaErrors(cudaMalloc((void**)&hdnData_dev2, hdnDataSize * sizeof(float)));

	// 出力
	checkCudaErrors(cudaMalloc((void**)&hdnData_dev13, dstDataSize * sizeof(float)));

	checkCudaErrors(cudaMalloc((void**)&dstData_dev, dstDataSize * sizeof(float)));
}

void CNN::forward(float srcData[minibatch_size][feature_num][19][19], float dstData[minibatch_size][1][19][19])
{
	// 順伝播計算
	// 入力
	checkCudaErrors(cudaMemcpy(srcData_dev, srcData, srcDataSize * sizeof(float), cudaMemcpyHostToDevice));

	// layer1
	// 畳み込み
	const float alpha = 1.0f;
	const float beta = 0.0f;
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		srcTensorDesc,
		srcData_dev,
		filterDesc1,
		filterData_dev1,
		convDesc1,
		algo1,
		workSpace1,
		workSpaceSizeInBytes1,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));
	// バイアス
	const float alpha_bias = 1.0f;
	const float beta_bias = 1.0f;
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev1, &beta_bias, hdnTensorDesc, hdnData_dev1));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));
	//dump_device_data("hdnData_dev1", hdnData_dev1, hdnDataSize); // debug

	// layer2
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		filterDesc2,
		filterData_dev2,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev2, &beta_bias, hdnTensorDesc, hdnData_dev2));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));

	// layer3
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		filterDesc2,
		filterData_dev3,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev3, &beta_bias, hdnTensorDesc, hdnData_dev1));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));

	// layer4
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		filterDesc2,
		filterData_dev4,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev4, &beta_bias, hdnTensorDesc, hdnData_dev2));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));

	// layer5
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		filterDesc2,
		filterData_dev5,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev5, &beta_bias, hdnTensorDesc, hdnData_dev1));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));

	// layer6
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		filterDesc2,
		filterData_dev6,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev6, &beta_bias, hdnTensorDesc, hdnData_dev2));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));

	// layer7
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		filterDesc2,
		filterData_dev7,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev7, &beta_bias, hdnTensorDesc, hdnData_dev1));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));

	// layer8
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		filterDesc2,
		filterData_dev8,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev8, &beta_bias, hdnTensorDesc, hdnData_dev2));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));

	// layer9
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		filterDesc2,
		filterData_dev9,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev9, &beta_bias, hdnTensorDesc, hdnData_dev1));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));

	// layer10
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		filterDesc2,
		filterData_dev10,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev10, &beta_bias, hdnTensorDesc, hdnData_dev2));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));

	// layer11
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		filterDesc2,
		filterData_dev11,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev11, &beta_bias, hdnTensorDesc, hdnData_dev1));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		&beta,
		hdnTensorDesc,
		hdnData_dev1));

	// layer12
	// 畳み込み
	checkCUDNN(cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev1,
		filterDesc2,
		filterData_dev12,
		convDesc2,
		algo2,
		workSpace2,
		workSpaceSizeInBytes2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc, biasData_dev12, &beta_bias, hdnTensorDesc, hdnData_dev2));
	// 活性化関数
	checkCUDNN(cudnnActivationForward(cudnnHandle,
		activDesc,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		&beta,
		hdnTensorDesc,
		hdnData_dev2));

	// layer13
	// 畳み込み
	cudnnConvolutionForward(cudnnHandle,
		&alpha,
		hdnTensorDesc,
		hdnData_dev2,
		filterDesc13,
		filterData_dev13,
		convDesc13,
		algo13,
		workSpace13,
		workSpaceSizeInBytes13,
		&beta,
		dstTensorDesc,
		hdnData_dev13);
	// バイアス
	checkCUDNN(cudnnAddTensor(cudnnHandle, &alpha_bias, biasTensorDesc13, biasData_dev13, &beta_bias, dstTensorDesc, hdnData_dev13));
	// 活性化関数
	cudnnSoftmaxForward(cudnnHandle,
		CUDNN_SOFTMAX_FAST,
		CUDNN_SOFTMAX_MODE_INSTANCE,
		&alpha,
		dstTensorDesc,
		hdnData_dev13,
		&beta,
		dstTensorDesc,
		dstData_dev);

	// 出力
	checkCudaErrors(cudaMemcpy(dstData, dstData_dev, dstDataSize * sizeof(float), cudaMemcpyDeviceToHost));
}
