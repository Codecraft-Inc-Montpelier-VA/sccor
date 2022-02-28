/*******************************************************************************
**                                                                            **
**       FILE:  mt.cpp                                                        **
**                                                                            **
**   SYNOPSIS:  Coroutines as lightweight cooperative multitasking for Clang. **
**              The coroutines run non-preemptively in a single macOS thread. **
**                                                                            **
** TARGET O/S:  Mac macOS Big Sur (11.0), or later:                           **
**                Compile with Clang:                                         **
**                  Apple Clang version 12.0.0 (Clang-1200.0.32.27)           **
**                  Target: x86_64-apple-darwin20.1.0                         **
**                  Thread model: posix                                       **
**              Windows Cygwin 2.897:                                         **
**                Compile with Clang:                                         **
**                  Clang version 8.0.1 (tags/RELEASE_801/final)              **
**                  Target: x86_64-unknown-windows-cygnus                     **
**                  Thread model: posix                                       **
**                                                                            **
**       N.B.:  - This implementation supports 64-bit architectures only.     **
**              - There is no maximum to number of longs as parameters        **
**                per coroutine start in invoke() and cobegin().              **
**              - The CSA is static and has a fixed size of 720,000 bytes.    **
**                This will be made dynamic in a future release.              **
**              - Some magic numbers require checking with changes to mt.cpp. **
**                This will be automated in a future release.                 **
**                                                                            **
**     AUTHOR:  Cary WR Campbell                                              **
**                                                                            **
** Copyright 2007 - 2021 Codecraft, Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy 
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
** copies of the Software, and to permit persons to whom the Software is 
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in 
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
** OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
** DEALINGS IN THE SOFTWARE.
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <wchar.h>
#include <string.h>
#include <thread> 

#include "sccorlib.h"

// CSA_SIZE defines the size (in 8-byte longs) of the coroutine storage area (CSA).
// The CSA stores stack contents for coroutines not currently running.
// The 90000 8-byte entries = 720,000 bytes reserved for the CSA.
const int CSA_SIZE = 90000 ;

//#define EXTRA_STACK    40    // longs for calling memcpy and memcpy's locals
#define EXTRA_STACK    60    // longs for calling memcpy and memcpy's locals
#define FILLER_VALUE 0xffffffffffffffff  // in csa 
                             
// Some environment-dependent magic constants.
#if defined(__APPLE__) && defined(__MACH__)
   #define IN_REGISTERS_COUNT 6 // macOS uses the x86_64 ABI
   #define MAGIC_OFFSET 0x68 // Clang moves stack down / up by this amount
                             //   in popCoroutine's prologue / epilogue
   #define CLEANUP_OFFSET 9  // bytes into cleanup, to keep same ebp, esp
   #define CLEANUP_RSP_ADJUST 0x18 // Clang moves stack back by this amount
                             //   in cleanup's epilogue
#elif defined(CYGWIN)
   #define SHADOW_SPACE_VALUE 0x5555555555555555  // in csa 
   #define IN_REGISTERS_COUNT 4 // Cygwin uses the X64 ABI
   #define MAGIC_OFFSET 0x98 // Clang moves stack down / up by this amount
                             //   in popCoroutine's prologue / epilogue
   #define CLEANUP_OFFSET 13 // bytes into cleanup, to keep same ebp, esp
   #define CLEANUP_STACK_ADJUST 0x38 // Clang moves stack back by this amount
                                     //   in cleanup's epilogue
   #define COBEGIN_STACK_ADJUST 0x58 // cobegin adjusts stack by this amount
#else
   #pragma GCC error "Only 64-bit macOS or Windows Cygwin with Clang supported."
#endif

// Internal prototypes.
HIDE void cleanup( void ) ;
HIDE void popCoroutine( void ) ;
HIDE void setBase( void ) ;

// Utility prototype.
//#define DEBUG_OUTPUT
//#ifdef DEBUG_OUTPUT
//void dump( const void * const pMemory, int size, unsigned char *pShow = 0 ) ;
//#endif // def DEBUG_OUTPUT

/*******************************************************************************
*                             Module Variables                                 *
*******************************************************************************/
/*HIDE*/ long csa[CSA_SIZE],  // coroutine storage area
          *csavail = csa, // points to free space in csa
#if defined(CYGWIN)
          *COBEGIN_EPILOG,
#else 
          coRBP,          // cobegin's rbp
#endif
          _size,
          *_RSP,
          *baseMinusSize, // addr for inserting stack for popped coroutine
          *base ;         // base of multi-tasker stack

HIDE int  coroutineCount = 0 ;


/*******************************************************************************
*                         Private Function Definitions                         *
*******************************************************************************/

/*******************************************************************************
* setBase                                                                      *
*                                                                              *
* Purpose: Determines the base of the coresume stack frame (i.e., rbp of each  *
*          coroutine during its execution. The 'base' value is determined from *
*          within this setBase function after it has been called by cobegin.   *
*          The top of a coroutine's stack frame is popCoroutine's saved        *
*          register area.                                                      *
*******************************************************************************/
HIDE void setBase( void )
{
   asm ( "movq %%rbp, %0" : "=rm" (base) : /* no inputs */ ) ;

#if defined(__APPLE__) && defined(__MACH__)
   coRBP = *base ;
#endif
}     

/*******************************************************************************
* popCoroutine                                                                 *
*                                                                              *
* Purpose: (Re)starts a coroutine from the coroutine storage area.             *
*                                                                              *
* Prerequisite: The rsp has already been moved so that popCoroutine's stack    *
*               frame will be out of the way of next coroutine's stack frame.  *
*                                                                              *
*******************************************************************************/
HIDE void popCoroutine( void )                                               
{
   // Tell Clang to save rbx (and rdi and rsi in Cygwin), as required by the
   // x86_64 ABI, so we'll have a  repeatable (if redundant) behavior in this 
   // routine's prologue (since it doesn't seem possible to get Clang to treat 
   // a C routine as a _declspec( naked ) routine). Clang will pop the rbx 
   // (and rdi and rsi in Cygwin) register(s) here and again in the caller.
#if defined(CYGWIN)
   asm volatile ( "nop\n\t" : /* no input */ : /* no output */ : "%rsi", 
                                                                 "%rdi", 
                                                                 "%rbx" ) ;
#else
   asm volatile ( "nop\n\t" : /* no input */ : /* no output */ : "%rbx" ) ;
#endif
   _size = *--csavail ;       // number of 64-bit entries in coroutine instance

   // The high-order byte of _size is marked for coroutine instances which 
   // have not yet been loaded. This allows us to load the appropriate 
   // x86_64 (macOS) or X64 (Cygwin) ABI registers for a simulated call to 
   // the instance.  Subsequently, the high-order byte of _size will be 
   // 0x00 and no registers need be loaded.
   int8_t mark = 0 ;
   long *p = csavail - 1 ;          // temp pointer to first parameter
   if ( _size < 0 ) {
      // This is a virgin coroutine instance, so we'll simulate a call 
      // by loading the appropriate registers. 
      mark = ( (int8_t)(_size >> 56) ) & 0x7F ;
      _size &= 0x00ffffffffffffff ; // omit mark for below
   }
   baseMinusSize = base - _size - 2 ; // note pointer arithmetic
   csavail -= _size ;               // note pointer arithmetic

   // Fetch coroutine from csavail to (base - _size - 2) for _size longs.
   _RSP = baseMinusSize - MAGIC_OFFSET / sizeof( long ) ;
   memcpy( baseMinusSize, csavail, _size * sizeof( long ) ) ;
   if ( mark > 0) {
      if ( mark % 2 ) {
         --p ; // skip filler
      }      
      long *first_p = p ;           // pointer to first parameter
      int8_t excessCount = mark - IN_REGISTERS_COUNT ;
      if ( mark > IN_REGISTERS_COUNT ) {
         // We'll leave excess parameters (more than the IN_REGISTERS_COUNT, 
         // which go in registers) on the stack.
         mark = IN_REGISTERS_COUNT ;
         p -= excessCount ;         // point to IN_REGISTERS_COUNTth parameter
      } 
      if ( excessCount > 0 ) {
         // Move non-register parameters to their place on the stack.
#if defined(__APPLE__) && defined(__MACH__)
         memmove( baseMinusSize + 4, first_p - excessCount + 1, 
                  excessCount * sizeof( long ) ) ;
#elif defined(CYGWIN)
         // The excess parameters go on top of the shadow space (32-byte
         // area reserved for the callee's use).
         memmove( baseMinusSize + 6 + 4, first_p - excessCount + 1, 
                  excessCount * sizeof( long ) ) ;
#else
   #pragma GCC error "Only 64-bit macOS or Windows Cygwin with Clang supported."
#endif
      }
      switch ( mark ) {
#if defined(__APPLE__) && defined(__MACH__)
         case 6:
            asm ( "movq %0, %%r9" : /* no outputs */ : "rm" (*p) ) ;
            --p ;                   // contine to next case

         case 5:
            asm ( "movq %0, %%r8" : /* no outputs */ : "rm" (*p) ) ;
            --p ;                   // contine to next case

         case 4:
            asm ( "movq %0, %%rcx" : /* no outputs */ : "rm" (*p) ) ;
            --p ;                   // contine to next case

         case 3:
            asm ( "movq %0, %%rdx" : /* no outputs */ : "rm" (*p) ) ;
            --p ;                   // contine to next case

         case 2:
            asm ( "movq %0, %%rsi" : /* no outputs */ : "rm" (*p) ) ;
            --p ;                   // contine to next case

         case 1:       
            asm ( "movq %0, %%rdi" : /* no outputs */ : "rm" (*p) ) ;
#elif defined(CYGWIN)
         case 4:
            asm ( "movq %0, %%r9" : /* no outputs */ : "rm" (*p) ) ;
            --p ;                   // contine to next case

         case 3:
            asm ( "movq %0, %%r8" : /* no outputs */ : "rm" (*p) ) ;
            --p ;                   // contine to next case

         case 2:
            asm ( "movq %0, %%rdx" : /* no outputs */ : "rm" (*p) ) ;
            --p ;                   // contine to next case

         case 1:       
            asm ( "movq %0, %%rcx" : /* no outputs */ : "rm" (*p) ) ;
#else
   #pragma GCC error "Only 64-bit macOS or Windows Cygwin with Clang supported."
#endif
            break ;      
      }
   }

   asm volatile ( "movq %0, %%rsp" : /* no outputs */ : "rm" (_RSP) : "%rsp" ) ;
}

/*******************************************************************************
* cleanup                                                                      *
*                                                                              *
* Purpose: Retrieves another coroutine from the csa when a coroutine returns   *
*          to end its execution.                                               *
*                                                                              *
*          Handles task termination by returning to cobegin when there are no  *
*          more active coroutines.                                             *
*******************************************************************************/
HIDE void cleanup( void )
{
   // cleanup must restore cobegin's rbx (and rdi and rsi on Cygwin) on return.
#if defined(CYGWIN)
   asm volatile ( "nop\n\t" : /* no input */ : /* no output */ : "%rsi", 
                                                                 "%rdi", 
                                                                 "%rbx" ) ;
#else
   asm volatile ( "nop\n\t" : /* no input */ : /* no output */ : "%rbx" ) ;
#endif
   if ( !--coroutineCount )
   {
      // Adjust rsp to return to point to rbx as in cobegin's epilog.
#if defined(CYGWIN)
      asm volatile ( "movq %0, %%rsp" : /* no outputs */ 
                                      : "rm" (COBEGIN_EPILOG) : "%rsp" ) ;
#else
      _RSP = (long *)coRBP - CLEANUP_RSP_ADJUST / sizeof( long ) - 1 ;  // cobegin's 
                                                                        //   RBX
      asm volatile ( "movq %0, %%rsp" : /* no outputs */ : "rm" (_RSP) : "%rsp" ) ;
#endif

      return ;
   }

   // Move popCoroutine's stack frame out of the way of the coroutine instance
   // we're copying in.
   _size = *(csavail - 1) ; // number of longs in coroutine entry
   _size &= 0x00ffffffffffffff ; // omit mark

   asm ( "movq %%rsp, %0" : "=rm" (_RSP) : /* no inputs */ ) ;

   // The stack pointer should be on a 16-byte boundary before any call.
   _RSP -= _size + EXTRA_STACK /*+ 1*/ ;
   asm volatile ( "movq %0, %%rsp" : /* no outputs */ : "rm" (_RSP) : "%rsp" ) ;

   popCoroutine() ;
}


/*******************************************************************************
*                            Public Functions (API)                            *
*******************************************************************************/

/*******************************************************************************
* cobegin                                                                      *
*                                                                              *
* Purpose: Initialize the multitasker and start n coroutine instances          *
*          on the ring.                                                        *
*                                                                              *
* Remarks on usage : Associated with each coroutine is an argument             *
*                    list (which may be empty).  For example,                  *
*                                                                              *
*                       cobegin( 2,                                            *
*                                coroutine1, 0,                                *
*                                coroutine2, 3, longvar1, longvar2, longvar3 );*
*                                                                              *
*                    would start two coroutine instances, coroutine1 and       *
*                    coroutine2.  The first takes no arguments and             *
*                    the second takes three longs (64-bits each) as arguments: *
*                                                                              *
*                       3 = (sizeof longvar1 + sizeof longvar2                 *
*                           + sizeof longvar3) / (8 bytes/long)                *
*                         = 24 / 8                                             *
*******************************************************************************/
void cobegin( int n, ... )
{
   int8_t  argCount, fillerCount = 0 ;
   va_list arg ;

   setBase() ;

   asm volatile ( "movq %%rsp, %0" : "=rm" (COBEGIN_EPILOG): /* no inputs */ ) ;
   // Address of saved rbx at top of cobegin's epilog:
   COBEGIN_EPILOG += ( COBEGIN_STACK_ADJUST - CLEANUP_STACK_ADJUST ) 
                                                               / sizeof(long) ; 

   #ifdef DEBUG_OUTPUT
   printf( " base: 0x%08lx\n", (unsigned long)base ) ;
   #endif // def DEBUG_OUTPUT

   va_start( arg, n ) ;
   while ( n-- ) {
      // This code is essentially an:
      //    invoke( coroutine, argCount, arg1, arg2, arg3, ... ) ;

                        // popCoroutine's rbp
      *csavail++ = 0 ;  // rbx placeholder
#if defined(CYGWIN)
      *csavail++ = 0 ;  // rdi placeholder
      *csavail++ = 0 ;  // rsi placeholder
#endif
      *csavail++ = (long)base ;
      *csavail++ = (long)( va_arg( arg, COROUTINE ) ) ;
      *csavail++ = (long)( (long)cleanup + CLEANUP_OFFSET ) ; // keeping same
                                                              //  rbp and rsp
      fillerCount = 0 ;
#if defined(CYGWIN)
      *csavail++ = SHADOW_SPACE_VALUE ;  // 32-byte shadow space
      ++fillerCount ;             
      *csavail++ = SHADOW_SPACE_VALUE ;  
      ++fillerCount ;             
      *csavail++ = SHADOW_SPACE_VALUE ;  
      ++fillerCount ;             
      *csavail++ = SHADOW_SPACE_VALUE ;  
      ++fillerCount ;             
#endif
      argCount = va_arg( arg, int ) ;
      if ( argCount > 0) { 
         for ( int i = 0; i < argCount; i++ ) {
            *csavail++ = (long)( va_arg( arg, long ) );
         }

         switch ( argCount % 2 ) // filler to ensure 16-byte boundary;
         {
            case 0:
               break ;          // do nothing

            case 1:
               *csavail++ = FILLER_VALUE ;  
               ++fillerCount ;             
               break;
         }
      } else if ( argCount == 0 ) {
         *csavail++ = FILLER_VALUE ;  
         ++fillerCount ;             
         *csavail++ = FILLER_VALUE ;  
         ++fillerCount ;             
      } 
#if defined(CYGWIN)
#define LONG_COUNT 4
#else
#define LONG_COUNT 2
#endif
      long coroutineSize = argCount + fillerCount
         + ( sizeof( long ) * LONG_COUNT + (sizeof( COROUTINE ) * 2) ) 
                                                             / sizeof( long ) ;
      // Since this coroutine hasn't been executed, we mark the size in the 
      // high-order byte with (0x80 | argCount) so popCoroutine
      // can load the ABI registers with the parameters only on startup
      // of the coroutine. So, if we mark the high-order byte as 0x83, 
      // there are 3 args for the coroutine, which will be loaded into 
      // registers rdi, rsi, and rdx by popCoroutine on the first time  
      // the coroutine is loaded (to simulate calling the coroutine).  
      // Subsequently, the high-order byte will be zero, so no registers 
      // will get loaded by popCoroutine.
      uint8_t mark = 0x80 | argCount ;
      coroutineSize |= ( (long)mark << 56 ) ;
      *csavail++ = coroutineSize ;
      ++coroutineCount ;
   }
   va_end( arg );

   #ifdef DEBUG_OUTPUT
   #endif // def DEBUG_OUTPUT

   // Tell Clang to save rbx (and rdi and rsi on Cygwin)** so popCoroutine 
   // can write the new routine's value to the stack location from which 
   // it will be restored:
   //     :  :  :
   //    pop %rbx    <----- baseMinusSize points to rbx for stack frame top
   //  **pop %rdi    
   //  **pop %rsi
   //    pop %rbp
   //    ret
   //
   // in cobegin's epilogue.
#if defined(CYGWIN)
   asm volatile ( "nop\n\t" : /* no input */ : /* no output */ : "%rsi", 
                                                                 "%rdi", 
                                                                 "%rbx" ) ;
#else
   asm volatile ( "nop\n\t" : /* no input */ : /* no output */ : "%rbx" ) ;
#endif
   if ( coroutineCount > 0 ) { 
      // Move popCoroutine's stack frame out of the way of the coroutine
      // we're copying in.
      _size = *(csavail - 1) ;      // number of longs in coroutine entry
      _size &= 0x00ffffffffffffff ; // omit mark

      asm ( "movq %%rsp, %0" : "=rm" (_RSP) : /* no inputs */) ;

      // The stack pointer should be on a 16-byte boundary before any call.
      _RSP -= _size + EXTRA_STACK /*+ 1*/ ;
      asm volatile ("movq %0, %%rsp" : /* no outputs */ : "rm" (_RSP) : "%rsp") ;

      popCoroutine() ;
   }
}                                                                

/*******************************************************************************
* coresume                                                                     *
*                                                                              *
* Purpose: performs an unconditional task switch.                              *
*******************************************************************************/
void coresume( void )
{
   long *_BPX ;

   // No-ops if no task in csa.
   if (coroutineCount > 1) {
      // Compute the size of the current coroutine's stack frame plus its
      // return address and stored values for its rbx (and rdi and rsi for
      // Cygwin).
      asm ("movq %%rbp, %0" : "=rm" (_BPX) : /* no inputs */) ;
#if defined(CYGWIN)
      _BPX -= /*3*/ -1 ;                   // include our rbx, rsi, and rdi saved 
                                    //   by coresume
                                    //   (note pointer arithmetic)
#else
      _BPX -= 1 ;                   // include our rbx, rsi, and rdi saved 
                                    //   by coresume
                                    //   (note pointer arithmetic)
#endif
      _size = (base - 2 - _BPX) ;   // coroutine extends from 1 or 3 longs 
                                    // "above" coresume's rbp to 2 longs 
                                    // "above" base.

      // Open a slot for old task.  The extra long is for the coroutine size.
      memmove( csa + _size + 1, csa, (csavail - csa) * sizeof( long ) ) ;
      csavail += _size + 1 ;

      // Store the coroutine in the csa along with the coroutine size.
      // The size of the coroutine is not itself included in the coroutine
      // "size" that is stored in the csa.  Note that the csa is addressed
      // via pointer arithmetic.
      memmove( csa, (void *)_BPX, _size * sizeof( long ) ) ;
      csa[_size] = _size ;

      // Move popCoroutine's stack frame out of the way of the coroutine
      // we're copying in. We move the stack here before calling popCoroutine
      // so that rbp (and associated temps) is out of harm's way.
      _size = *( csavail - 1 ) ;    // number of longs in coroutine entry
      _size &= 0x00ffffffffffffff ; // omit mark

      asm ( "movq %%rsp, %0" : "=rm" (_RSP) : /* no inputs */ ) ;

      // The stack pointer should be on a 16-byte boundary before any call.
      _RSP -= _size + EXTRA_STACK /*+ 1*/ ;
      asm volatile ( "movq %0, %%rsp" : /* no outputs */ : "rm" (_RSP) : "%rsp" ) ;

      popCoroutine() ;
   }

   // Tell Clang to save rbx (and rdi and rsi on Cygwin)** so popCoroutine 
   // can write the new routine's value to the stack location from which 
   // Clang will restore its saved register:
   //     :  :  :
   //  **pop %rsi
   //  **pop %rdi
   //    pop %rbx
   //    pop %rbp
   //    ret
   //
   // in coresume's epilogue.
#if defined(CYGWIN)
   asm volatile ( "nop\n\t" : /* no input */ : /* no output */ : "%rsi", 
                                                                 "%rdi", 
                                                                 "%rbx" ) ;
#else
   asm volatile ( "nop\n\t" : /* no input */ : /* no output */ : "%rbx" ) ;
#endif
}


/*******************************************************************************
* getCoroutineCount                                                            *
*                                                                              *
* Purpose: returns the number of active coroutines.                            *
*******************************************************************************/
int getCoroutineCount( void )
{
   return coroutineCount ;
}

/*******************************************************************************
* invoke                                                                       *
*                                                                              *
* Purpose: place a new coroutine instance on the multi-tasker ring.            *
*                                                                              *
*          Creates an initial stack for the coroutine consisting of:           *
*             - initial value for rbx (and rdi and rsi in Cygwin) and          *
*             - initial rbp value (base)                                       *
*             - address of the coroutine                                       *
*             - the passed in arguments (any number of longs)                  *
*                                                                              *
* Note: the new coroutine will be on the ring, but no task switch is performed.*
*       The new coroutine will not be executed until the next coresume.        *
*******************************************************************************/
void invoke( COROUTINE coroutine, int argCount, ... )
{
   int fillerCount = 0 ;
   va_list arg ;
   va_start( arg, argCount ) ;

   *csavail++ = 0 ;     // rbx placeholder
#if defined(CYGWIN)
   *csavail++ = 0 ;  // rdi placeholder
   *csavail++ = 0 ;  // rsi placeholder
#endif
   *csavail++ = (long)base ;
   *csavail++ = (long)coroutine ;
   *csavail++ = (long)( (long)cleanup + CLEANUP_OFFSET ) ; // keeping same
                                                           //   rbp and rsp
   fillerCount = 0 ;
#if defined(CYGWIN)
   *csavail++ = SHADOW_SPACE_VALUE ;  // 32-byte shadow space
   ++fillerCount ;             
   *csavail++ = SHADOW_SPACE_VALUE ;  
   ++fillerCount ;             
   *csavail++ = SHADOW_SPACE_VALUE ;  
   ++fillerCount ;             
   *csavail++ = SHADOW_SPACE_VALUE ;  
   ++fillerCount ;             
#endif
   if ( argCount > 0 ) { 
      for ( int i = 0; i < argCount; i++ ) {
         *csavail++ = (long)( va_arg( arg, long ) ) ;
      }

      switch (argCount % 2)        // filler to ensure 16-byte boundary;
      {
         case 0:
            break ;                 // do nothing

         case 1:
            *csavail++ = FILLER_VALUE ;  
            ++fillerCount ;             
            break;
      }
   } else if ( argCount == 0 ) {
      *csavail++ = FILLER_VALUE ;  
      ++fillerCount ;             
      *csavail++ = FILLER_VALUE ;  
      ++fillerCount ;             
   }

#if defined(CYGWIN)
#define LONG_COUNT 4
#else
#define LONG_COUNT 2
#endif
   long coroutineSize = argCount + fillerCount
      + ( sizeof( long ) * LONG_COUNT + (sizeof( COROUTINE ) * 2) ) / sizeof( long ) ;
   // Since this coroutine hasn't been executed, we mark the size in the 
   // high-order byte with (0x80 | argCount) so popCoroutine
   // can load the ABI registers with the parameters only on startup
   // of the coroutine. So, if we mark the high-order byte as 0x83, 
   // there are 3 args for the coroutine, which will be loaded into 
   // registers rdi, rsi, and rdx by popCoroutine on the first time  
   // the coroutine is loaded (to simulate calling the coroutine).  
   // Subsequently, the high-order byte will be zero, so no registers 
   // will get loaded by popCoroutine.
   uint8_t mark = 0x80 | argCount ;
   coroutineSize |= ((long)mark << 56) ;
   *csavail++ = coroutineSize ;
   ++coroutineCount ;

   va_end( arg ) ;

   #ifdef DEBUG_OUTPUT
   #endif // def DEBUG_OUTPUT
}

/*******************************************************************************
* sleepMs                                                                      *
*                                                                              *
* Purpose: sleeps (or executes other threads) for a specified number of ms.    *
*          Behaves just like Sleep() in Windows.                               *
*******************************************************************************/
void sleepMs( unsigned long sleepms )
{
   std::this_thread::sleep_for( std::chrono::milliseconds( sleepms ) ) ;
}


/*******************************************************************************
* wait                                                                         *
*                                                                              *
* Purpose: waits for at least a specified amount of milliseconds while         *
* continuing other coroutines.                                                 *
*******************************************************************************/
void wait( unsigned long waitMs )
{
   // Delay for at least waitMs milliseconds.
   auto go = std::chrono::system_clock::now() + std::chrono::milliseconds( waitMs ) ;
   when( std::chrono::system_clock::now() >= go ) ; 
}

/*******************************************************************************
* waitEx                                                                       *
*                                                                              *
* Purpose: waits for an extended period while continuing other coroutines.     *
*          The waiting period is interrupted if the boolean becomes false.     *
*******************************************************************************/
void waitEx( unsigned long waitMs, bool *continuing, bool *canceling )
{
   auto go = std::chrono::high_resolution_clock::now() 
             + std::chrono::milliseconds( waitMs ) ;

   when( ( std::chrono::high_resolution_clock::now() >= go ) 
         || *continuing == false  
         || ( canceling != NULL && *canceling == true ) ) ; 
}

