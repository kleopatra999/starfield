# StarField 0.1 - A Simple Star Trek Type Star Field Generator

StarField generates a field of stars through which the viewer moves,
reminiscent of the Star Trek TV series.  It can generate a series of
PNG images or a QuickTime movie (on MacOS).  Its stars are antialiased
white circles which move with subpixel accuracy.

## Compilation and Usage

For StarField to compile you'll need to have libpng installed, even if
you use QuickTime output.  libpng comes with pretty much every Linux
distribution.  If you use MacOS you can get it with [Homebrew](https://brew.sh).

First check out the comments in `Makefile` and make the required changes
if necessary.  Then type

    make

It should compile to an executable `starfield`.

If you start it with

    ./starfield

it should either produce a sequence of images called `out0000.png` to
some `outxxxx.png`, or a movie called `out.mov`, depending on whether
you enabled QuickTime in the `Makefile`.

If you want to change the parameters of the animation, like its
length, size, the number of stars, the speed, etc, take a look at the
comments in the top part of `starfield.c`, change the values accordingly
and compile and start again.

## Licence and Availability

StarField is free software distributed under the terms of the GNU
General Public License.  The file `COPYING` contains the text of the
license.

The source of StarField is available [on GitHub](https://github.com/schani/starfield).

--
Mark Probst <mark.probst@gmail.com>
