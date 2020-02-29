#include "util.h"
#include <math.h>

inline double distance(KYPOINT p1, KYPOINT p2)
{
	return sqrt((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y));
}

inline double distance(KYPOINT p1, KYPOINT p2, KYPOINT p3)
{
    const double x0 = p3.x, y0 = p3.y;
    const double x1 = p1.x, y1 = p1.y;
    const double x2 = p2.x, y2 = p2.y;

    const double a = x2 - x1;
    const double b = y2 - y1;
    const double a2 = a * a;
    const double b2 = b * b;
    const double r2 = a2 + b2;
    const double tt = -(a * (x1 - x0) + b * (y1 - y0));

    if (tt < 0)
        return distance(p1, p3);
    else if (tt > r2)
        return distance(p2, p3);

    double f1 = a * (y1 - y0) - b * (x1 - x0);
    return sqrt(f1 * f1 / r2);
}