OpenGL Example
==============

This example suggests how to port your OpenGL application to SIXEL terminal.

  ![opengl](https://raw.githubusercontent.com/saitoha/libsixel/data/data/example_opengl.gif)


How to Build
------------

Linux/BSDs:

  1. Install OSMesa package on your distribution

      $ sudo apt-get install libosmesa6-dev

  2. Build

      $ ./configure && make

OSX:

  1. Build

      $ ./configure && make


Run (only works on SIXEL terminals)
-----------------------------------

  $ ./demo


License
--------

GLX pbuffer initialization part is originally written by
Brian Paul for the "OpenGL and Window System Integration"
course presented at SIGGRAPH '97.  Updated on 5 October 2002.

Updated on 31 January 2004 to use native GLX by
Andrew P. Lentvorski, Jr. <bsder@allcaps.org>

Hayaki Saito <saitoha@me.com> added OSMesa and OSX pbuffer
initialization code.

original source:
https://cgit.freedesktop.org/mesa/demos/tree/src/xdemos/glxpbdemo.c

original license:

```
Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

Note that some configure scripts and m4 macros are distributed under the terms
of the special exception to the GNU General Public License.

OpenGL is a trademark of [Silicon Graphics Incorporated](http://www.sgi.com/).
