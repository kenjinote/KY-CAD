#pragma once

struct KYPOINT {
    double x;
    double y;
};

template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = 0;
    }
}

inline double distance(KYPOINT p1, KYPOINT p2);
inline double distance(KYPOINT p1, KYPOINT p2, KYPOINT p3); //ü•ªp1-p2‚Æp3‚Æ‚Ì‹——£