#
#*++
# PROJECT:
#	sccor library on macOS and Windows Cygwin
# MODULE:
#	Makefile for the sccor library for mt of the sccor library..
#
# ABSTRACT:
#	This Makefile creates the macOS or Cygwin version of the sccor library.
#
#	Debug or Release versions of the sccor library may be selected.
#	By default a Debug executable is built. Use "make CFG=Release" for a
#	Release version.
#
#       "make"         creates the macOS or Cygwin version of sccor library, either 
#                      Release/sccorlib.a or Debug/sccorlib.a, depending on CFG.
#*--
#

RM=rm
MKDIR=mkdir
GCC=clang++

all : outfile

CODEDIR	= ./code
INCLUDEDIR = ./include

REQUIRED_DIRS = \
	$(CODEDIR)\
	$(NULL)

_MKDIRS := $(shell for d in $(REQUIRED_DIRS) ;	\
	     do					\
	       [ -d $$d ] || mkdir -p $$d ;	\
	     done)

VERSION=1.0

vpath %.cpp  $(CODEDIR)

PROGNAME = sccorlib.a

# If no configuration is specified, "Debug" will be used.
ifndef CFG
CFG=Debug
endif

# Set architecture and PIC options.
ARCH_OPT=
PIC_OPT=
DEFS=
OS := $(shell uname)
ifeq ($(OS),Darwin)
# It's macOS.
ARCH_OPT += -arch x86_64
PIC_OPT += -Wl,-no_pie
else ifeq ($(OS),$(filter CYGWIN_NT%, $(OS)))
# It's Windows.
DEFS=-D"CYGWIN"
else
$(error The sccor library requires either macOS Big Sur (11.0.1) or later or Cygwin.)
endif

OUTDIR=./$(CFG)
OUTFILE=$(OUTDIR)/$(PROGNAME)
OBJ=$(OUTDIR)/dump.o $(OUTDIR)/histo.o $(OUTDIR)/histospt.o $(OUTDIR)/kbhit.o $(OUTDIR)/mt.o 

#
# Configuration: Debug
#
ifeq "$(CFG)" "Debug"
COMPILE=$(GCC) -c $(ARCH_OPT) $(DEFS) -fno-stack-protector -std=c++17 -O0 -g -o "$(OUTDIR)/$(*F).o" -I$(INCLUDEDIR) "$<"
#LINK=$(GCC) $(ARCH_OPT) -g -o "$(OUTFILE)" $(PIC_OPT) $(OBJ) $(SCCORDIR)/sccorlib.a
endif

#
# Configuration: Release
#
ifeq "$(CFG)" "Release"
COMPILE=$(GCC) -c $(ARCH_OPT) $(DEFS) -fno-stack-protector -std=c++17 -O0 -o "$(OUTDIR)/$(*F).o" -I$(INCLUDEDIR) "$<"
#LINK=$(GCC) $(ARCH_OPT) -o "$(OUTFILE)" $(PIC_OPT) $(OBJ) $(SCCORDIR)/sccorlib.a
endif

# Pattern rules
$(OUTDIR)/%.o : $(CODEDIR)/%.cpp
	$(COMPILE)

$(OUTFILE): $(OUTDIR) $(OBJ)
	ar rcs $(OUTDIR)/sccorlib.a $(OBJ)

$(OUTDIR):
	$(MKDIR) -p "$(OUTDIR)"

CODEFILES =\
	.cpp\
	$(NULL)

CODE =\
	$(patsubst %,$(CODEDIR)/%,$(CODEFILES))\
	$(NULL)

CLEANFILES =\
	$(OBJ)\
	$(OUTFILE)\
	$(NULL)

.PHONY : all outfile clean

outfile : $(OUTFILE)

clean :
	$(RM) -f $(CLEANFILES)

