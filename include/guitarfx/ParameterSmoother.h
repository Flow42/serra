#pragma once
#include <cmath>

class ParameterSmoother {
public:
    void reset(double sampleRate, double timeConstantSec) {
        if (sampleRate > 0.0 && timeConstantSec > 0.0) {
            alpha = 1.0f - std::exp(-1.0f / static_cast<float>(sampleRate * timeConstantSec));
        } else {
            alpha = 1.0f;
        }
        active = true;
    }
    
    void setCurrent(float val) {
        current = val;
    }
    
    float next(float target) {
        if (!active) return target;
        current = current + alpha * (target - current);
        if (std::abs(current - target) < 1e-6f) {
            current = target;
        }
        return current;
    }
    
    float getCurrent() const {
        return current;
    }
    
private:
    float current = 0.0f;
    float alpha = 1.0f;
    bool active = false;
};
