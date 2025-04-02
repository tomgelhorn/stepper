#include "main.h"
#include "init.h"
#include "LibL6474.h"

static void* StepLibraryMalloc( unsigned int size )
{
     return malloc(size);
}

static void StepLibraryFree( const void* const ptr )
{
     free((void*)ptr);
}

static int StepDriverSpiTransfer( void* pIO, char* pRX, const char* pTX, unsigned int length )
{
  // byte based access, so keep in mind that only single byte transfers are performed!
  for ( unsigned int i = 0; i < length; i++ )
  {
    ...
  }
  return 0;
}

static void StepDriverReset()
{
  ...
}

static void StepLibraryDelay()
{
  ...
}

static void StepTimerAsync()
{
  ...
}

static void StepTimerCancelAsync()
{
  ...
}
void init_stepper(){
	// pass all function pointers required by the stepper library
	// to a separate platform abstraction structure
	L6474x_Platform_t p;
	p.malloc     = StepLibraryMalloc;
	p.free       = StepLibraryFree;
	p.transfer   = StepDriverSpiTransfer;
	p.reset      = StepDriverReset;
	p.sleep      = StepLibraryDelay;
	p.stepAsync  = StepTimerAsync;
	p.cancelStep = StepTimerCancelAsync;

	// now create the handle
	L6474_Handle_t h = L6474_CreateInstance(&p, null, null, null);
}


