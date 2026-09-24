/*
 * Project Ambrose by Imjustchico
 * Thread-local random number helpers and a standard-compatible random engine wrapper.
 */

#ifndef AMBROSE_RANDOM_H
#define AMBROSE_RANDOM_H

#include "Types.h"

uint32 urand(uint32 min, uint32 max);
int32 irand(int32 min, int32 max);
float frand(float min, float max);
double rand_norm();
double rand_chance();
bool roll_chance_f(float chance);
bool roll_chance_i(int32 chance);

class RandomEngine
{
public:
    using result_type = uint64;

    static constexpr result_type min()
    {
        return 0;
    }

    static constexpr result_type max()
    {
        return ~result_type(0);
    }

    result_type operator()() const;

    static RandomEngine& Instance();
};

#endif
