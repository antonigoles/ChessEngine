#pragma once
#include <random>

class MathUtil
{
public:
    static float random()
    {
        // just in case I want to move to multithreaded
        thread_local std::random_device rd;
        thread_local std::mt19937 generator(rd());
        std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
        return distribution(generator);
    }

    static inline float clamp(float v, float min, float max)
    {
        return v > max ? max : ((v < min) ? min : v);
    }
};