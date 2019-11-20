#ifndef __utils_h
#define __utils_h

/**
	\brief Converts a [0..1] spread to a emission("focus") exponent.

	checking the function y=cos^n(x) in wolfram alpha seems to revel that
	a 1/(x*x) mapping to n gives a nice progression of the "focus beam".
*/
float spread_to_focus( float spread )
{
	float x = clamp(spread, 0.0, 1.0);
	x *= x;

	/* I think a focus of 10000 is not a bad limit */
	if( x<1e-5)
		x=1e-5;

	return 1 / x;
}


#endif /* __utils_h */
