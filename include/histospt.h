// histospt.h -- histogram support header file
//
// Copyright 2007 - 2021 Codecraft, Inc.
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

#if ! defined __HISTOSPT_H
#define __HISTOSPT_H

#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>                     // strncpy on Cygwin
#include "histo.h"                      // base class

// Global function to pause and wait for a keystroke.
//void pause( void );

// Display properties.
enum { base10, base16, graph, data };

// A simple class for tallying and displaying interval times.
class TimeIntervalHistogram : Histogram
{
   // Display canvass limits.
   const static int width  = 79;
   const static int height = 58;

   double maxBarSize;                  // 34.0* (half-screen) | 71.0 (full-scrn)
   double mean;
   double convFactor;                  // converts performance counts to µs
   int barChar;                        // 219
   int halfBarChar;                    // 221
   int col;                            // leftmost column of the graph:  1 | 41*
   int col_b;                          // leftmost column of the bars:  col + 7
   int maxCol;                         // col_b + maxBarSize
   int top;                            // top row of the graph:  1
   int bottom;                         // bottom row of the graph:  46
   int displayMode;                    // graph* | data
   bool firstTime;                     // first time switch to trigger some
                                       //   initialization
   struct timeval tvPrevTimeValue;
   struct timeval tvNextTimeValue;
   unsigned int overCount;
   char m_banner[width + 1];           // Histogram top title

   char window[height][width + 1];     // canvass for displaying the histogram

                     // Note: the asterisked values above are not yet supported.

   friend void DisplayHistGraph( TimeIntervalHistogram &dataSet );
   friend void DisplayHistBins( TimeIntervalHistogram &dataSet, int radix );

 public:

   // Standard constructor; defines the ???, range, and resolution of
   // the distribution.
   TimeIntervalHistogram( const char* banner = 0,
                          unsigned int min = 0,
                          unsigned int countsPer = 500,
                          int bins = 40 )
                        : Histogram( min, countsPer, bins ), maxBarSize( 71.0 ),
                          barChar( 219 ), halfBarChar( 221 ),
                          displayMode( data ), firstTime( true ),
                          col( 0 ), col_b( col + 7 ),
                          maxCol( col_b + (int)maxBarSize ),
                          top( 3 ), bottom( height - 13 )
   {
      if ( banner )
      {
         strncpy( m_banner, banner, width ); // protect against an overwrite
         m_banner[width] = '\0';             // insure null termination
      }
   }

   // Destructor releases ??? ???.
   ~TimeIntervalHistogram() { }

   // Add an entry to the time interval histogram, with a value equal to the
   // elapsed time since the previous tally request.  The first tally request
   // does not add an entry; it merely stores an initial time value in
   // preparation for subsequent tally requests.
   void tally( void );

   // Clear the histogram.
   void reset( void );

   // Updates the time interval without adding an entry to the histogram.
   // This allows capturing non-contiguous time intervals for the histogram.
   void restartTimer( void );

   // Show (and optionally log, if the boolean is true,) the histogram
   // and then wait for the user to hit a key.
   void show( bool logToo = false );

   // Add a scalar point (i.e., not a time interval) to the histogram.
   /* This is so I can create and display a normal histogram. CWRC */
   void add( unsigned int data );
};

#endif // ! defined __HISTOSPT_H
