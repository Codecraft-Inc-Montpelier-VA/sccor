// kbhit.cpp -- adapted from an article "Porting DOS Applications to Linux"
//              at http://www.linuxjournal.com/articles/lj/0017/1138/1138/1.html
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

#include <sys/select.h>

// In Cygwin, NULL is already defined. We'll define it for OS X.
#if defined(__APPLE__) && defined(__MACH__)
   #define NULL 0
#endif

int kbhit( void )
{
   struct timeval tv ;
   fd_set read_fd ;

   /* Do not wait at all, not even a microsecond */
   tv.tv_sec = 0 ;
   tv.tv_usec = 0 ;

   /* Must be done first to initialize read_fd */
   FD_ZERO( &read_fd ) ;

   /* Makes select() ask if input is ready:
    * 0 is the file descriptor for stdin    */
   FD_SET( 0, &read_fd ) ;

   /* The first parameter is the number of the
    * largest file descriptor to check + 1. */
   if( select( 1, &read_fd,
              NULL, /*No writes*/
              NULL, /*No exceptions*/
              &tv )
       == -1 )
      return 0 ;  /* An error occured */

   /*  read_fd now holds a bit map of files that are
    * readable. We test the entry for the standard
    * input (file 0). */
   if( FD_ISSET( 0, &read_fd ) )
      /* Character pending on stdin */
      return 1 ;

   /* no characters were pending */
   return 0 ;
}