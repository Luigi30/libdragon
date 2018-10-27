/* Fixed-point math. */

#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

typedef int32_t Fixed;

#define FX_Q 16
#define FX_K (1 << (FX_Q-1))

static Fixed FX_FromFloat(float f)
{
	Fixed fx = ((int)floor(f) << 16) | (int)((f - floor(f)) * 65536);

	if(f > 32768)
		fx = 0x7FFFFFFF;
	if(f < -32768)
		fx = 0x80000000;

	return fx;
}

static Fixed FX_FromInt(int f)
{
	Fixed fx = (f << 16);

	if(f > 32768)
		fx = 0x7FFFFFFF;
	if(f < -32768)
		fx = 0x80000000;

	return fx;
}

static Fixed FX_Add(Fixed a, Fixed b)
{
	return a+b;	
}

static Fixed FX_Sub(Fixed a, Fixed b)
{
	return a-b;
}

static Fixed FX_Multiply(Fixed a, Fixed b)
{
	Fixed result;
	int64_t temp;

	temp = (int64_t)a * (int64_t)b;
	temp += FX_K;

	result = temp >> FX_Q;
	return result;
}

static Fixed FX_Divide(Fixed a, Fixed b)
{
	int64_t temp = (int64_t)a << FX_Q;

	if((temp >= 0 && b >= 0) || (temp < 0 && b < 0)) {   
        temp += b / 2;    /* OR shift 1 bit i.e. temp += (b >> 1); */
    } else {
        temp -= b / 2;    /* OR shift 1 bit i.e. temp -= (b >> 1); */
    }

	return (Fixed)(temp / b);
}