/*
 * Project Ambrose by Imjustchico
 * Implements the random helpers on a per-thread Mersenne Twister seeded from std::random_device.
 */

#include "Random.h"

#include <random>
#include <utility>

namespace
{
    std::mt19937_64& Engine()
    {
        thread_local std::mt19937_64 engine = []
        {
            std::random_device device;
            std::seed_seq seed{ device(), device(), device(), device(), device(), device(), device(), device() };
            return std::mt19937_64(seed);
        }();
        return engine;
    }
}

uint32 urand(uint32 min, uint32 max)
{
    if (min > max)
        std::swap(min, max);
    return std::uniform_int_distribution<uint32>(min, max)(Engine());
}

int32 irand(int32 min, int32 max)
{
    if (min > max)
        std::swap(min, max);
    return std::uniform_int_distribution<int32>(min, max)(Engine());
}

float frand(float min, float max)
{
    if (min > max)
        std::swap(min, max);
    if (min == max)
        return min;
    return std::uniform_real_distribution<float>(min, max)(Engine());
}

double rand_norm()
{
    return std::uniform_real_distribution<double>(0.0, 1.0)(Engine());
}

double rand_chance()
{
    return std::uniform_real_distribution<double>(0.0, 100.0)(Engine());
}

bool roll_chance_f(float chance)
{
    return chance > rand_chance();
}

bool roll_chance_i(int32 chance)
{
    return chance > irand(0, 99);
}

RandomEngine::result_type RandomEngine::operator()() const
{
    return Engine()();
}

RandomEngine& RandomEngine::Instance()
{
    static RandomEngine instance;
    return instance;
}
