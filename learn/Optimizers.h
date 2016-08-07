#pragma once

#include <math.h>

class SGD
{
private:
	static float lr;
public:
	static void set_lr(const float lr) { SGD::lr = lr; }

public:

	float operator() (const float grad) {
		return lr * grad;
	}
};

class AdaGrad
{
private:
	static float lr;
	static float eps;
public:
	static void set_lr(const float lr) { AdaGrad::lr = lr; }
	static void set_eps(const float eps) { AdaGrad::eps = eps; }

private:
	float h;

public:

	AdaGrad() : h(0) {}

	float operator() (const float grad) {
		h += grad * grad;
		return lr * grad / (sqrtf(h) + eps);
	}
};

class RMSprop
{
private:
	static float lr;
	static float alpha;
	static float eps;
public:
	static void set_lr(const float lr) { RMSprop::lr = lr; }
	static void set_alpha(const float alpha) { RMSprop::alpha = alpha; }
	static void set_eps(const float eps) { RMSprop::eps = eps; }

private:
	float ms;

public:

	RMSprop() : ms(0) {}

	float operator() (const float grad) {
		ms *= alpha;
		ms += (1 - alpha) * grad * grad;
		return lr * grad / (sqrtf(ms) + eps);
	}
};
