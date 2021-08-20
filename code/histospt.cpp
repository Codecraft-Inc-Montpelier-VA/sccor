// histospt.cpp -- histogram support module
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

#include <stdio.h>
#include <string.h>     // strlen() and strupr() functions
#include <math.h>       // modf() and pow10() functions
#include <ctype.h>      // toupper macro
#include <stdlib.h>     // system() function
#include <errno.h>

#include "histospt.h"   // includes histo.h

#ifdef THE_TEST_BUILD
void   RRTLog( const char *message ) ;
#endif // def THE_TEST_BUILD

// Local prototypes.
void   ShowStatus( const char *msg ) ;
void   DisplayHistBins( TimeIntervalHistogram &dataSet, int radix ) ;
void   DisplayHistGraph( TimeIntervalHistogram &dataSet ) ;
double Roundf( double val, int nDigits ) ;

using namespace std ;

void TimeIntervalHistogram::tally( void )
{
   if ( firstTime )
   {
      // Prime the pump.
      if ( gettimeofday( &tvPrevTimeValue, NULL ) )
      {
         char str[ 80 ] ; // ample
         sprintf( str, "gettimeofday failed: errno is %i", errno ) ;
         printf( "%s\n", str ) ;
         #ifdef THE_TEST_BUILD
         RRTLog( str ) ;
         #endif // def THE_TEST_BUILD
      }

      // Do this only once.
      firstTime = false ;
   }
   else
   {
      // Get the current counter value.
      if ( gettimeofday( &tvNextTimeValue, NULL ) )
      {
         char str[ 80 ] ; // ample
         sprintf( str, "gettimeofday failed: errno is %i", errno ) ;
         printf( "%s\n", str ) ;
         #ifdef THE_TEST_BUILD
         RRTLog( str ) ;
         #endif // def THE_TEST_BUILD
      }

      // Add the interval to the histogram (in microseconds).
      long uDelta = ( tvNextTimeValue.tv_sec - tvPrevTimeValue.tv_sec ) * 1000000
                     + tvNextTimeValue.tv_usec - tvPrevTimeValue.tv_usec ;

      Add( (unsigned int)( uDelta ) ) ;

      // Save the counter value for next time
      tvPrevTimeValue = tvNextTimeValue ;
   }
}

void TimeIntervalHistogram::restartTimer( void )
{
   // Update the timer, without adding to the histogram.
   gettimeofday( &tvPrevTimeValue, NULL ) ;
}

void TimeIntervalHistogram::add( unsigned int data )
{
   // Call the base class to add an arbitrary point
   // (i.e., not a time interval) to the histogram.
   Add( data ) ;
}

void TimeIntervalHistogram::reset( void )
{
   // Call the base class to reset the histogram.
   Histogram::Reset() ;
}

void TimeIntervalHistogram::show( bool logToo )
{
   char str[128] ; // ample

   // Clear the canvass.
   for ( int i = 0; i < height; i++ )
   {
      for ( int j = 0; j < width; j++ )
      {
         window[ i ][ j ] = ' ' ;    // blank canvass
      }
      window[ i ][ width ] = '\0' ;  // null-terminated lines
   }

   // Label the histogram.
   DisplayHistBins( *this, base10 ) ;

   // Center the banner on the top line.
   sprintf( str, "%s", m_banner ) ;
   strncpy( &window[ 0 ][ ( 80 - strlen( m_banner )) / 2 ], str,
            strlen( str ) ) ;

   // Show the histogram values.
   DisplayHistGraph( *this ) ;

   // Show the sample count and mean.
   mean = Roundf( MeanValue(), 1 ) ;
//   sprintf( str, "N = %-5ld;  mean = %2.1lf", NValues(), mean ) ;
   sprintf( str, "N = %-5d;  mean = %2.1lf", NValues(), mean ) ;
   strncpy( &window[ height - 12 ][ col_b ], str, strlen( str ) ) ;

   // Show data about any "Over" values.
   overCount = BinCount( NBins() + 1 ) ;
   int line = height - 10 ;
   if ( overCount )
   {
      int offset = col_b ;
      if ( displayMode == graph )
      {
         sprintf( str, "Number of 'Over' values = %u. ", overCount ) ;
         strncpy( &window[ line ][ col_b ], str, strlen( str ) ) ;
         offset += strlen( str ) ;
      }
      sprintf( str, "Greatest delay = %u.", MaxValue() ) ;
      strncpy( &window[ line ][ offset ], str, strlen( str ) ) ;
      offset += strlen( str ) ;
      sprintf( str, !overN[1] ? "  Over index =" : "  Over indices =" ) ;
      strncpy( &window[ line ][ offset ], str, strlen( str ) ) ;
      offset += strlen( str ) ;
      int indicesOffset = offset ;
      for ( int i = 0; i < OVERN_TRACE_COUNT; i++ )
      {
         if ( !overN[i] )
         {
            break ;
         }
         sprintf( str, " %6u (%6u) @ %6u",
                  overN[i], overValueN[i], overTSN[i] ) ;
         strncpy( &window[ line ][ offset ], str, strlen( str ) ) ;
         offset += strlen( str ) ;
         if ( line == height )
         {
            break ;
         }
         else
         {
            offset = indicesOffset ;
            line++ ;
         }
      }
   }
   else
   {
      sprintf( str, "There are no 'Over' values." ) ;
      strncpy( &window[ line ][ col_b ], str, strlen( str ) ) ;
   }

   // Put the window array on the console (and, optionally, in the log).
   printf( "\r\n" ) ;
   printf( "-------------------------------------------------------------------"
           "------------\r\n" ) ;
   if ( logToo )
   {
      #ifdef THE_TEST_BUILD
      RRTLog( " " );
      RRTLog( "---------------------------------------------------------------"
               "----------------" ) ;
      #endif // def THE_TEST_BUILD
   }
   for ( int i = 0; i < line; i++ )
   {
      printf( "%s\r\n", &window[ i ][ 0 ] ) ;
      if ( logToo )
      {
         #ifdef THE_TEST_BUILD
         RRTLog( &window[ i ][ 0 ] ) ;
         #endif // def THE_TEST_BUILD
      }
   }
   printf( "-------------------------------------------------------------------"
           "------------\r\n" ) ;
   printf( "\r\n" ) ;
   if ( logToo )
   {
      #ifdef THE_TEST_BUILD
      RRTLog( "---------------------------------------------------------------"
               "----------------" ) ;
      RRTLog( " " ) ;
      #endif // def THE_TEST_BUILD
   }

   // Wait so the user can view the histogram.
//   pause();
}

//======================= SUPPORT CODE STARTS HERE ============================

// Function to pause and wait for a keystroke.
#if 0
void pause( void )
{
//   when( _kbhit() )
//   {
      if ( !getch() ) getch();   /* Wait for a(n extended) char */
//   }
}
#endif // 0

// Function to show status at the bottom of the display.
void ShowStatus( const char *msg )
{
// **??** TBD Borland thingy **??**   window( 56, 50, 80, 50 );
// **??** TBD Borland thingy **??**   clrscr();
   printf( "%s", msg );
}

// Function to label a frequency distribution in our window array.
void DisplayHistBins( TimeIntervalHistogram &dataSet, int radix )
{
   unsigned int thisBin = dataSet.MinBin() ;
   unsigned int countsPerBin = (unsigned int) dataSet.CountsPerBin() ;
   int nBins = min( dataSet.height - 10, dataSet.NBins() ) ;
   char str[16] ; // ample
   for ( int i = 0; i <= nBins + 1; ++i )
   {
      if ( i == 0 )
      {
         strncpy( &dataSet.window[ i + dataSet.top ][ dataSet.col ],
                  " Under", 6 ) ;
      }
      else if ( i < nBins + 1 )
      {
         if ( radix == base10 )
         {
//            sprintf( str, "%6lu ", thisBin );
            sprintf( str, "%6u ", thisBin ) ;
            strncpy( &dataSet.window[ i + dataSet.top ][ dataSet.col ], str,
                     strlen( str ) ) ;
         }
         else
         {
//            sprintf( str, "%6lX ", thisBin ) ;
            sprintf( str, "%6X ", thisBin ) ;
            strncpy( &dataSet.window[ i + dataSet.top ][ dataSet.col ], str,
                     strlen( str ) ) ;
         }
         thisBin += countsPerBin ;
      }
      else
      {
         strncpy( &dataSet.window[ i + dataSet.top ][ dataSet.col ],
                  "  Over", 6 ) ;
      }
   }
}

// Function to graph a frequency distribution in text mode.
void DisplayHistGraph( TimeIntervalHistogram &dataSet )
{
   int nBins = min( dataSet.height - 10, dataSet.NBins() ) ;
   unsigned int count ;
   double maxFreq = (double) dataSet.MaxBinCount() ;
   double barIP, barFP ;
   char countStr[ dataSet.width + 1 ] ;
   char str[128] ; // ample
   int strSize, barSize ;
   int i, j ;

   for ( i = 0; i <= nBins + 1; ++i )
   {
      count = dataSet.BinCount(i);
//      sprintf( countStr, "%lu", count );
      sprintf( countStr, "%u", count );
      if ( dataSet.displayMode == graph )
      {
         if ( count > 0L )
         {
            barFP = modf( double(count) / maxFreq * dataSet.maxBarSize,
                          &barIP ) ;
            barSize = int( barIP ) ;
            strSize = strlen( countStr ) ;
            if ( barSize >= strSize + 1 )
            {
               sprintf( str, "%s", countStr ) ;
               strncpy( &dataSet.window[ i + dataSet.top ][ dataSet.col_b ],
                        str, strlen( str ) ) ;
               barSize -= strSize;
            }
            for ( j = 0; j < barSize; ++j )
            {
               dataSet.window[ i + dataSet.top ][ dataSet.col_b + j ]
                                                              = dataSet.barChar ;
            }
            if ( barFP > 0.5 )
            {
               dataSet.window[ i + dataSet.top ][ dataSet.col_b + barSize ]
                                                              = dataSet.barChar ;
            }
            else if ( barFP > 0.05 )
            {
               dataSet.window[ i + dataSet.top ][ dataSet.col_b + barSize ]
                                                          = dataSet.halfBarChar ;
            }
         }
      }
      else
      {
         if ( count > 0L )
         {
            sprintf( str, "%s", countStr ) ;
            strncpy( &dataSet.window[ i + dataSet.top ][ dataSet.col_b ],
                     str, strlen( str ) ) ;
         }
      }
   }
}

// Function to round floating point numbers to the specified number of digits.
// Number of digits can be < 0 for rounding to tens, hundreds, etc.
double Roundf( double val, int nDigits )
{
   double factor ;                         // power of 10 to normalize to
   double intPart ;                        // ip of normalized float

   factor = pow( 10.0, nDigits ) ;
   if ( modf( val * factor, &intPart) > 0.5 )
   {
      // Round up.
      intPart += 1.0 ;
   }
   return intPart / factor ;
}

