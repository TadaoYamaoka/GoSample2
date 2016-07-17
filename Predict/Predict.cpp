#include <stdio.h>
#include <algorithm>
#include <vector>

#include "cuDNNWrapper.h"
#include "..\Board.h"
#include "CNNParam.h"

using namespace std;

const int minibatch_size = 2;
const int feature_num = 4;
const int filter_num = 192;

#pragma pack(push, 1)
struct Position
{
	XY xy;
	BitBoard<19*19> player_color;
	BitBoard<19*19> opponent_color;
};
#pragma pack(pop)

// 局面を読み込み
void load_input_features(const char* filename, Position* &position, int &size)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == nullptr)
	{
		fprintf(stderr, "%s read error.\n", filename);
		exit(1);
	}

	fseek(fp, 0, SEEK_END);
	int filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	size = filesize / sizeof(Position);

	position = new Position[size];

	vector<int> index;
	for (int i = 0; i < size; i++)
	{
		index.push_back(i);
	}

	for (int i = 0; i < size; i++)
	{
		fread(position + index[i], sizeof(Position), 1, fp);
	}

	fclose(fp);
}

void bitboard_to_array(BitBoard<19 * 19> bitboard, float array[19][19])
{
	for (int i = 0; i < 19 * 19; i++)
	{
		*((float*)array + i) = bitboard.bit_test(i);
	}
}

void prepare_input_data(Position* position, float input_data[feature_num][19][19])
{
	bitboard_to_array(position->player_color, input_data[0]);
	bitboard_to_array(position->opponent_color, input_data[1]);

	// empty
	BitBoard<19 * 19> empty;
	for (int i = 0; i < 6; i++)
	{
		empty.get_bitboard_part(i) = ~(position->player_color.get_bitboard_part(i) | position->opponent_color.get_bitboard_part(i));
	}
	bitboard_to_array(empty, input_data[2]);

	// filled with 1
	for (int y = 0; y < 19; y++)
	{
		for (int x = 0; x < 19; x++)
		{
			input_data[3][y][x] = 1.0f;
		}
	}
}

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

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		return 1;
	}

	// 局面データ読み込み
	Position *position;
	int size;

	load_input_features(argv[1], position, size);

	printf("size = %d\n", size);

	// CNNパラメータ読み込み
	shared_ptr<ParamMap> param_map = load_cnn_param(argv[2]);

	for (const auto pair : *param_map)
	{
		printf("%s\n", pair.first.c_str());
	}

	// CNN
	CudnnHandle cudnnHandle;
	CudnnTensorDescriptor srcTensorDesc, hdnTensorDesc, biasTensorDesc, biasTensorDesc13, dstTensorDesc;
	CudnnFilterDescriptor filterDesc1, filterDesc2, filterDesc13;
	CudnnConvolutionDescriptor convDesc1, convDesc2, convDesc13;
	CudnnActivationDescriptor  activDesc;

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
	cudnnConvolutionFwdAlgo_t algo1;
	checkCUDNN(cudnnGetConvolutionForwardAlgorithm(cudnnHandle,
		srcTensorDesc,
		filterDesc1,
		convDesc1,
		hdnTensorDesc,
		CUDNN_CONVOLUTION_FWD_PREFER_FASTEST,
		0,
		&algo1
	));

	size_t workSpaceSizeInBytes1 = 0;
	checkCUDNN(cudnnGetConvolutionForwardWorkspaceSize(cudnnHandle,
		srcTensorDesc,
		filterDesc1,
		convDesc1,
		hdnTensorDesc,
		algo1,
		&workSpaceSizeInBytes1));

	void* workSpace1 = NULL;
	if (workSpaceSizeInBytes1 != 0)
	{
		checkCudaErrors(cudaMalloc(&workSpace1, workSpaceSizeInBytes1));
	}

	// layer2～12
	cudnnConvolutionFwdAlgo_t algo2;
	checkCUDNN(cudnnGetConvolutionForwardAlgorithm(cudnnHandle,
		hdnTensorDesc,
		filterDesc2,
		convDesc2,
		hdnTensorDesc,
		CUDNN_CONVOLUTION_FWD_PREFER_FASTEST,
		0,
		&algo2
	));

	size_t workSpaceSizeInBytes2 = 0;
	checkCUDNN(cudnnGetConvolutionForwardWorkspaceSize(cudnnHandle,
		hdnTensorDesc,
		filterDesc2,
		convDesc2,
		hdnTensorDesc,
		algo2,
		&workSpaceSizeInBytes2));

	void* workSpace2 = NULL;
	if (workSpaceSizeInBytes2 != 0)
	{
		checkCudaErrors(cudaMalloc(&workSpace2, workSpaceSizeInBytes2));
	}

	// layer13
	cudnnConvolutionFwdAlgo_t algo13;
	checkCUDNN(cudnnGetConvolutionForwardAlgorithm(cudnnHandle,
		hdnTensorDesc,
		filterDesc13,
		convDesc13,
		dstTensorDesc,
		CUDNN_CONVOLUTION_FWD_PREFER_FASTEST,
		0,
		&algo13
	));

	size_t workSpaceSizeInBytes13 = 0;
	checkCUDNN(cudnnGetConvolutionForwardWorkspaceSize(cudnnHandle,
		hdnTensorDesc,
		filterDesc13,
		convDesc13,
		dstTensorDesc,
		algo13,
		&workSpaceSizeInBytes13));

	void* workSpace13 = NULL;
	if (workSpaceSizeInBytes13 != 0)
	{
		checkCudaErrors(cudaMalloc(&workSpace13, workSpaceSizeInBytes13));
	}


	// パラメータ転送
	// layer1
	float *filterData_dev1;
	const size_t filterDataSize1 = filter_num * feature_num * 5 * 5;
	//dump_param(filterDataSize1, param_map, "layer1/W.npy"); // debug
	//dump_param(filter_num, param_map, "layer1/b.npy"); // debug
	param_to_device(&filterData_dev1, filterDataSize1, param_map, "layer1/W.npy");
	float *biasData_dev1;
	param_to_device(&biasData_dev1, filter_num, param_map, "layer1/b.npy");

	// layer2
	float *filterData_dev2;
	const size_t filterDataSize2 = filter_num * filter_num * 3 * 3;
	param_to_device(&filterData_dev2, filterDataSize2, param_map, "layer2/W.npy");
	float *biasData_dev2;
	param_to_device(&biasData_dev2, filter_num, param_map, "layer2/b.npy");

	// layer3
	float *filterData_dev3;
	param_to_device(&filterData_dev3, filterDataSize2, param_map, "layer3/W.npy");
	float *biasData_dev3;
	param_to_device(&biasData_dev3, filter_num, param_map, "layer3/b.npy");

	// layer4
	float *filterData_dev4;
	param_to_device(&filterData_dev4, filterDataSize2, param_map, "layer4/W.npy");
	float *biasData_dev4;
	param_to_device(&biasData_dev4, filter_num, param_map, "layer4/b.npy");

	// layer5
	float *filterData_dev5;
	param_to_device(&filterData_dev5, filterDataSize2, param_map, "layer5/W.npy");
	float *biasData_dev5;
	param_to_device(&biasData_dev5, filter_num, param_map, "layer5/b.npy");

	// layer6
	float *filterData_dev6;
	param_to_device(&filterData_dev6, filterDataSize2, param_map, "layer6/W.npy");
	float *biasData_dev6;
	param_to_device(&biasData_dev6, filter_num, param_map, "layer6/b.npy");

	// layer7
	float *filterData_dev7;
	param_to_device(&filterData_dev7, filterDataSize2, param_map, "layer7/W.npy");
	float *biasData_dev7;
	param_to_device(&biasData_dev7, filter_num, param_map, "layer7/b.npy");

	// layer8
	float *filterData_dev8;
	param_to_device(&filterData_dev8, filterDataSize2, param_map, "layer8/W.npy");
	float *biasData_dev8;
	param_to_device(&biasData_dev8, filter_num, param_map, "layer8/b.npy");

	// layer9
	float *filterData_dev9;
	param_to_device(&filterData_dev9, filterDataSize2, param_map, "layer9/W.npy");
	float *biasData_dev9;
	param_to_device(&biasData_dev9, filter_num, param_map, "layer9/b.npy");

	// layer10
	float *filterData_dev10;
	param_to_device(&filterData_dev10, filterDataSize2, param_map, "layer10/W.npy");
	float *biasData_dev10;
	param_to_device(&biasData_dev10, filter_num, param_map, "layer10/b.npy");

	// layer11
	float *filterData_dev11;
	param_to_device(&filterData_dev11, filterDataSize2, param_map, "layer11/W.npy");
	float *biasData_dev11;
	param_to_device(&biasData_dev11, filter_num, param_map, "layer11/b.npy");

	// layer12
	float *filterData_dev12;
	param_to_device(&filterData_dev12, filterDataSize2, param_map, "layer12/W.npy");
	float *biasData_dev12;
	param_to_device(&biasData_dev12, filter_num, param_map, "layer12/b.npy");

	// layer13
	float *filterData_dev13;
	const size_t filterDataSize13 = 1 * filter_num * 1 * 1;
	param_to_device(&filterData_dev13, filterDataSize13, param_map, "layer13/W.npy");
	float *biasData_dev13;
	param_to_device(&biasData_dev13, filter_num, param_map, "layer13_2/b.npy");


	// 入力
	float srcData[minibatch_size][feature_num][19][19];
	for (int i = 0; i < minibatch_size; i++)
	{
		prepare_input_data(position + i, srcData[i]);
	}
	float *srcData_dev;
	checkCudaErrors(cudaMalloc((void**)&srcData_dev, sizeof(srcData)));
	checkCudaErrors(cudaMemcpy(srcData_dev, srcData, sizeof(srcData), cudaMemcpyHostToDevice));

	// 中間層
	float *hdnData_dev1;
	const size_t hdnDataSize = minibatch_size * filter_num * 19 * 19;
	checkCudaErrors(cudaMalloc((void**)&hdnData_dev1, hdnDataSize * sizeof(float)));

	float *hdnData_dev2;
	checkCudaErrors(cudaMalloc((void**)&hdnData_dev2, hdnDataSize * sizeof(float)));

	// 出力
	float *hdnData_dev13;
	const size_t dstDataSize = minibatch_size * 1 * 19 * 19;
	checkCudaErrors(cudaMalloc((void**)&hdnData_dev13, dstDataSize * sizeof(float)));

	float *dstData_dev;
	checkCudaErrors(cudaMalloc((void**)&dstData_dev, dstDataSize * sizeof(float)));

	// 順伝播計算
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

	// 出力表示
	float dstData[minibatch_size][1][19][19];
	checkCudaErrors(cudaMemcpy(dstData, dstData_dev, sizeof(dstData), cudaMemcpyDeviceToHost));

	for (int i = 0; i < minibatch_size; i++)
	{
		printf("minibatch : %d\n", i);
		for (int y = 0; y < 19; y++)
		{
			for (int x = 0; x < 19; x++)
			{
				printf("%f, ", dstData[i][0][y][x]);
			}
			printf("\n");
		}
	}

	return 0;
}