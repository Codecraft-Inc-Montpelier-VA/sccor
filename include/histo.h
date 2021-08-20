// histo.h -- Histogram file header           <by CWR Campbell>
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

#if ! defined __HISTO_H
#define __HISTO_H

//#include <iostream>

// How many indices where value is Over.
#define OVERN_TRACE_COUNT 10

class Histogram {
  public:
   // Standard constructor, defines the range and resolution of the distribution.
   Histogram( unsigned int min, unsigned int countsPer, int bins = 10 ) ;

   // (Virtual) destructor releases dynamic storage for all bins.
   virtual ~Histogram() ;

   // Function to reset a distribution to its empty state.
   void Reset() ;

   // Function to add another data point to the existing distribution.
   void Add( unsigned int data ) ;

   // Access function returning the number of values that have been added.
   unsigned int NValues( ) const { return n ; }

   // Access function returning the minimum value that has been added.
   unsigned int MinValue( ) const { return minData ; }

   // Access function returning the maximum value that has been added.
   unsigned int MaxValue( ) const { return maxData ; }

   // Access function returning the minimum bin.
   unsigned int MinBin( ) const { return minBin ; }

   // Access function returning the counts per bin.
   unsigned int CountsPerBin( ) const { return countsPerBin ; }

   // Access function returning the number of bins.
   int NBins( ) const { return nBins ; }

   // Function returning the mean (average) of all values added.  This value may
   // be subject to numerical imprecision - the accumulation sum is held in a
   // double size floating point variable.
   double MeanValue( ) const ;

   // Access function returning the number of data points that have accumulated
   // in a specific bin.  Bin 0 is the under range bin.  Bin N + 1 is the over
   // range bin, where N is the number of bins specified during creation.
   unsigned int BinCount( int binNumber ) const ;

   // Access function returning the largest number of data points that have been
   // accumulated into any single bin.
   unsigned int MaxBinCount( ) const { return maxFreq ; }

   // Function to display the distribution, one bin per line, in the form
   // "<bin #>, <counts> \n".
   // void Display( ostream & ) const;

  protected:
   // This is a (static) array of indices whose values go into nBins + 1 ("Over").
   // Only OVERN_TRACE_COUNT entries are tracked.
   unsigned int overN[OVERN_TRACE_COUNT] ;

   // This is a (static) array of values corresponding to the above indices.
   // Only OVERN_TRACE_COUNT entries are tracked.
   unsigned int overValueN[OVERN_TRACE_COUNT] ;

   // This is a (static) array of timestamps corresponding to the above indices.
   // Only OVERN_TRACE_COUNT entries are tracked.
   unsigned int overTSN[OVERN_TRACE_COUNT] ;

  private:
   // Number of counts per bin, defines resolution of the binning process.
   unsigned int countsPerBin ;

   // Number of bins in the distribution, defines the binned range.  Two extra
   // slots are allocated to count under and over range data points
   unsigned int nBins ;

   // This variable holds the minimum bound of the first bin.  All data points
   // below are binned as under range.
   unsigned int minBin ;

   // These variables hold the min and max data points that have been added.
   unsigned int minData, maxData ;

   // This variable holds the largest bin count of the histogram.
   unsigned int maxFreq ;

   // This variable holds the number of data points that have been added.
   unsigned int n ;

   // This variable accumulates the total sum of all data points, to calculate
   // the mean value.
   double summation ;

   // This is a pointer to the dynamically allocated bin array.
   unsigned int *binVector ;

   // Pointer to a surrounding guard area for binVector.
   unsigned int *binVectorArea ;
};

#endif
