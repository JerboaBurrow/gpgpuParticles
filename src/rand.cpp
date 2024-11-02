#include <rand.h>

std::uniform_real_distribution<float> RNG::floatU;
std::normal_distribution<float> RNG::floatN(0.0, 1.0);
std::random_device RNG::device;
std::mt19937 RNG::engine(device());
