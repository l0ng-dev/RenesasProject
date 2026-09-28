#include "da16200.h"

/* Optional test hook. It is compiled, but production code does not call it. */
bool CommunicationTest_IsReady (void)
{
    return DA16200_IsReady();
}
