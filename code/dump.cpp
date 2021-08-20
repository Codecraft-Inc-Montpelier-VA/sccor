// dump.cpp -- displays contents of memory.  <by CWR Campbell>
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
#include <string.h>

typedef unsigned char byte;

void dump( const void * const pMemory, int size, byte *pShow = 0 ) ;

////////////////////////////////////////////////////////////////////////////////
//
// dump
//
// This internal debugging tool dumps formatted memory to the screen in the
// traditional fashion:
//
//                       0 1 2 3  4 5 6 7  8 9 a b  c d e f  0123 4567 89ab cdef
//
//    0000000080129ce0        67 76543210 01234567 76543210     g vT2. .#Eg vT2.
//    0000000080129cf0  01234567 76543210 01234567 76543210  .#Eg vT2. .#Eg vT2.
//    0000000080129d00  01234567 76                          .#Eg v
//
// This example displays 34 bytes starting at location 0x80129ce3:
//
//    dump( 0x80129ce3, 34 );
//
// the pShow optional parameter, if non-zero, causes a different address
// (e.g., a device address) to be displayed.
//

void dump( const void * const pMemory, int size, byte *pShow )
{
   byte *pMem = (byte *)( (unsigned long)pMemory & 0xfffffffffffffff0 ) ;
   byte *p ;
   const int dataBytesDisplayedPerLine = 16 ;
   const int LOG_LINE_SIZE = 133 ;
   char logline[ LOG_LINE_SIZE ] ;

   // See if the display start address is different from the actual memory
   // address.
   if ( pShow == 0 )
   {
      // We'll display the actual memory address.
      pShow = pMem ;
   }
   // Show the header and a blank line.  Then clear out the cout buffer so
   // printf output will come out at the right time.
   printf( "\n               0 1 2 3  4 5 6 7  8 9 a b  c d e f  " ) ;
   printf( "0123 4567 89ab cdef\n\n" );
   for ( ; pMem < (byte *)pMemory + size; pMem += dataBytesDisplayedPerLine,
                                         pShow += dataBytesDisplayedPerLine )
   {
      // Show the memory address and the hex values.
      printf( "%12lx  ", (unsigned long)pShow ) ;
      for ( p = pMem; p < pMem + dataBytesDisplayedPerLine; p++ )
      {
         if ( p - pMem == 4 || p - pMem == 8 || p - pMem == 12 )
         {
            printf( " " ) ;
         }

         if ( ( p >= pMemory ) && ( p < (byte *)pMemory + size ) )
         {
            printf( "%02x", *p ) ;
         }
         else
         {
            printf( "  " ) ;
         }
      }
      printf( "  " ) ;

      // Show the character values.
      for ( p = pMem; p < pMem + dataBytesDisplayedPerLine; p++ )
      {
         if ( p - pMem == 4 || p - pMem == 8 || p - pMem == 12 )
         {
            printf( " " ) ;
         }

         if ( ( p >= pMemory ) && ( p < (byte *)pMemory + size ) )
         {
            if ( *p < 0x20 )
            {
               printf( "." ) ;
            }
            else
            {
               printf( "%c", *p ) ;
            }
         }
         else
         {
            printf( " " ) ;
         }
      }

      // Go to the next line of the dump.
      printf( "\n" ) ;
   }

   // Finish with a blank line.
   printf( "\n" ) ;
}
