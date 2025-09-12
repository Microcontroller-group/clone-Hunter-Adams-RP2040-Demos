/**
 * Fixed-point arithmetic macros and types
 * Provides 15-bit fractional precision for faster calculations
 */

#ifndef FIXED_POINT_H
#define FIXED_POINT_H

// Macros for fixed-point arithmetic (faster than floating point)
typedef signed int fix15;

#define multfix15(a,b) ((fix15)((((signed long long)(a))*((signed long long)(b)))>>15))
#define float2fix15(a) ((fix15)((a)*32768.0)) 
#define fix2float15(a) ((float)(a)/32768.0)
#define absfix15(a) abs(a) 
#define int2fix15(a) ((fix15)(a << 15))
#define fix2int15(a) ((int)(a >> 15))
#define char2fix15(a) (fix15)(((fix15)(a)) << 15)
#define divfix(a,b) (fix15)( (((signed long long)(a)) << 15) / (b))

#endif // FIXED_POINT_H
