// histo.cpp -- a basic histogram class    <by CWR Campbell>
//
// Copyright 2003 - 2021 Codecraft, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
// copies of the Software, and to permit persons to whom the Software is 
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
// DEALINGS IN THE SOFTWARE.

#include <iostream>
#include <stdlib.h>              // for "exit" in Cygwin
#include "histo.h"

#ifdef THE_TEST_BUILD
unsigned int RRTGetTime( void ) ;
#endif // def THE_TEST_BUILD

typedef unsigned char byte;

//void dump( const void * const pMemory, int size, byte *pShow = 0 ) ;

using namespace std ;

// Standard constructor defining the range and resolution of the distribution.
Histogram::Histogram( unsigned int min, unsigned int countsPer, int bins )
{
   const int extraSpace = 16 ; // ints == 64 bytes surrounding guard region
   minBin = min ;
   countsPerBin = countsPer ;
   nBins = bins ;
   minData = min + countsPer * bins ;
   maxData = min ;
   maxFreq = 0 ;
   n = 0 ;
   summation = 0.0 ;

   binVectorArea = new unsigned int[bins + 2 + extraSpace] ;
   if ( binVectorArea == NULL )
   {
      std::cout << "Histogram memory allocation failure" << std::endl ;
      exit( 1 ) ;
   }
   else
   {
      binVector = binVectorArea + extraSpace / 2 ;
      for ( int i = 0; i < bins + 2; ++i )
      {
         binVector[i] = 0 ;
      }
   }
   for ( int i = 0; i < OVERN_TRACE_COUNT; i++)
   {
      overN[i] = 0 ;
   }
}

// Routine to reset a distribution to its empty state.
void Histogram::Reset()
{
   minData = minBin + countsPerBin * nBins ;
   maxData = minBin ;
   maxFreq = 0 ;
   n = 0 ;
   summation = 0.0 ;
   for ( unsigned int i = 0; i < nBins + 2; ++i )
   {
      binVector[i] = 0 ;
   }
   for ( int i = 0; i < OVERN_TRACE_COUNT; i++)
   {
      overN[i] = 0 ;
   }
}

// (Virtual) destructor to release storage allocated by the instance.
Histogram::~Histogram()
{
   delete[] binVectorArea ;
}

// Routine to add data to the distribution.
void Histogram::Add( unsigned int data )
{
   unsigned int targetBin = 0 ;

   if ( data >= minBin )
   {
      targetBin = ( data - minBin ) / (unsigned int) countsPerBin + 1 ;
   }
   if ( targetBin > nBins )
   {
      targetBin = nBins + 1 ;

      // Capture up to 10 indices where values are Over.
      for ( int i = 0; i < OVERN_TRACE_COUNT; i++ )
      {
         if ( !overN[i] )
         {
            overN[i]      = n + 1 ; /* n hasn't been updated yet */
            overValueN[i] = data ;
            #ifdef THE_TEST_BUILD
            overTSN[i]    = RRTGetTime() ;
            #else
            overTSN[i]    = 0 ;
            #endif // def THE_TEST_BUILD
            break ;
         }
      }
   }
   if ( ++binVector[targetBin] > maxFreq )
   {
      maxFreq = binVector[targetBin] ;
   }
   summation += double( data ) ;
   ++n ;

   if ( data < minData )
   {
      minData = data ;
   }
   else if ( data > maxData )
   {
      maxData = data ;
   }
}

// Accessor function to return the current mean (average) value.
double Histogram::MeanValue( ) const
{
   if ( n > 0 )
   {
      return summation / int( n ) ;
   }
   else
   {
      return 0.0 ;
   }
}

// Accessor function to return the number of data points that have accumulated
// in the specified bin.
unsigned int Histogram::BinCount( int binNumber ) const
{
   if ( (binNumber >= 0) && ((unsigned int) binNumber <= nBins + 1) )
   {
      return binVector[binNumber] ;
   }
   else
   {
      return 0 ;
   }
}

// Routine to display the current status of each bin in the distribution to
// an output stream.
//void Histogram::Display( ostream &os ) const {
// unsigned int thisBin = minBin ;
//
// os << "under," << binVector[0] << end ;
// for ( unsigned int i = 1; i <= nBins; ++i ) {
//    os << thisBin << "," << binVector[i] << endl ;
//    thisBin += countsPerBin ;
// }
// os << "over," << binVector[ nBins + 1 ] << endl ;
//}