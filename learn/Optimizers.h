#pragma once

#include <math.h>

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
