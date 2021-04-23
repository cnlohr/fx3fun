#include <cyu3gpif.h> //XXX CNL

#define CyFxGpifConfig		CyFxGpifConfig_Complex
#define CyFxGpifRegValue 	CyFxGpifRegValue_Complex
#define CyFxGpifWavedataPosition CyFxGpifWavedataPosition_Complex
#define CyFxGpifWavedata CyFxGpifWavedata_Complex
#define CyFxGpifTransition CyFxGpifTransition_Complex

#include "complex_gpif2.cydsn/cyfxgpif2config_complex.h"

int ComplexGPIFLoad()
{
	return CyU3PGpifLoad(&CyFxGpifConfig_Complex);
}
