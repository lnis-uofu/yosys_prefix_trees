#!/bin/bash

#MIT License
#
#Copyright (c) 2018 Wolfram Rösler
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in all
#copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#SOFTWARE.

# @brief Make version number
# @author Wolfram Rösler <wolfram@roesler-ac.de>
# @date 2017-02-12

# Check command line
if [ $# = 2 -a "$1" = "-o" ];then

    # Called with -o: Create a .o file with the version number
    bash $0 | ${CMAKE_CXX_COMPILER:-c++} -x c++ -c - -o "$2"
    exit

elif [ $# != 0 ];then

    # Illegal command line
    echo "Generate version number"
    echo "Usage: $0 [ -o version.o ]"
    exit 1
fi

# Get the git repository version number
REPO=$(git describe --dirty --always)

# Get the build time stamp
WHEN=$(date +"%Y-%m-%d %H:%M:%S")

# Get the user name
WHO="$USER"

# Get the machine name
WHERE=$(hostname)

# Get the OS version
WHAT=$(uname -sr)

# Put it all together
VERSION="$REPO (built $WHEN by $WHO on $WHERE with $WHAT)"

# Output the version number to the build log
echo "Building version $VERSION" >&2

# Create the version.cpp file
# NOTE: No includes to speed up compilation (remember that this file is
# re-created and compiled whenever a program is linked)
cat <<!
// This file was created by $0 on $(date)
// Do not edit - do not add to git
char const *Version() {
    return "@(#)$VERSION" + 4;
}
!
