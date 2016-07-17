#pragma once

#include <cuda.h>
#include <cudnn.h>

#include "error_util.h"

class CudnnHandle
{
private:
	cudnnHandle_t cudnnHandle;

public:
	CudnnHandle() {
		checkCUDNN(cudnnCreate(&cudnnHandle));
	}
	~CudnnHandle() {
		checkCUDNN(cudnnDestroy(cudnnHandle));
	}

	cudnnHandle_t* operator &() {
		return &cudnnHandle;
	}

	operator cudnnHandle_t() {
		return cudnnHandle;
	}
};

class CudnnTensorDescriptor
{
private:
	cudnnTensorDescriptor_t cudnnTensorDescriptor;

public:
	CudnnTensorDescriptor() {
		checkCUDNN(cudnnCreateTensorDescriptor(&cudnnTensorDescriptor));
	}
	~CudnnTensorDescriptor() {
		checkCUDNN(cudnnDestroyTensorDescriptor(cudnnTensorDescriptor));
	}

	cudnnTensorDescriptor_t* operator &() {
		return &cudnnTensorDescriptor;
	}

	operator cudnnTensorDescriptor_t() {
		return cudnnTensorDescriptor;
	}
};

class CudnnFilterDescriptor
{
private:
	cudnnFilterDescriptor_t cudnnFilterDescriptor;

public:
	CudnnFilterDescriptor() {
		checkCUDNN(cudnnCreateFilterDescriptor(&cudnnFilterDescriptor));
	}
	~CudnnFilterDescriptor() {
		checkCUDNN(cudnnDestroyFilterDescriptor(cudnnFilterDescriptor));
	}

	cudnnFilterDescriptor_t* operator &() {
		return &cudnnFilterDescriptor;
	}

	operator cudnnFilterDescriptor_t() {
		return cudnnFilterDescriptor;
	}
};

class CudnnConvolutionDescriptor
{
private:
	cudnnConvolutionDescriptor_t cudnnConvolutionDescriptor;

public:
	CudnnConvolutionDescriptor() {
		checkCUDNN(cudnnCreateConvolutionDescriptor(&cudnnConvolutionDescriptor));
	}
	~CudnnConvolutionDescriptor() {
		checkCUDNN(cudnnDestroyConvolutionDescriptor(cudnnConvolutionDescriptor));
	}

	cudnnConvolutionDescriptor_t* operator &() {
		return &cudnnConvolutionDescriptor;
	}

	operator cudnnConvolutionDescriptor_t() {
		return cudnnConvolutionDescriptor;
	}
};

class CudnnActivationDescriptor
{
private:
	cudnnActivationDescriptor_t cudnnActivationDescriptor;

public:
	CudnnActivationDescriptor() {
		checkCUDNN(cudnnCreateActivationDescriptor(&cudnnActivationDescriptor));
	}
	~CudnnActivationDescriptor() {
		checkCUDNN(cudnnDestroyActivationDescriptor(cudnnActivationDescriptor));
	}

	cudnnActivationDescriptor_t* operator &() {
		return &cudnnActivationDescriptor;
	}

	operator cudnnActivationDescriptor_t() {
		return cudnnActivationDescriptor;
	}
};