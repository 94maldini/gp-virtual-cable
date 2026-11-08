// Memastikan header juga dapat di-include setelah <windows.h> (tanpa redefinisi tipe).
#include <windows.h>
#include "gpring.h"
#include "gpmix.h"

int main()
{
    GpRing r;
    BYTE b[4];
    GpRingInit(&r, b, 4);
    return (int)GpRingFill(&r);
}
