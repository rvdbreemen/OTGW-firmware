Requirements
============

The provided files are intended to be used together with the gputils tools.
At least version 1.5.0 is required.
The gputils tools can be downloaded from https://gputils.sourceforge.io/
On many linux distributions the gputils package can be installed via the
package manager, although some may be too old. In that case, it will need to
be installed from source.

The OTGW source tarball includes a Makefile to simplify building the OTGW
firmware. To be able to use it, the GNU make command must be available.

A Tcl interpreter is also needed to regenerate the "build.asm" file. This can
either be tclsh or a tclkit/tclkitsh.

To be able to run the test-suite, several additional packages are needed:

RPM based systems
-----------------
Required packages:
tcl subversion make libtool flex bison
popt-devel glib2-devel readline-devel

Optional packages:
gtk2-devel (gui), lyx (doc)

Debian based systems
--------------------
Required packages:
tcl subversion make libtool flex bison
libpopt-dev libglib2.0-dev libreadline-dev

Optional packages:
libgtk2.0-dev (gui), lyx (doc)

Pacman based systems
--------------------
Required packages:
tcl subversion gcc make autoconf automake libtool pkgconfig flex bison
popt glib2 readline

Optional packages:
gtk2 (gui), lyx (doc)


Compiling
=========

To create a hex file that can be loaded into the PIC16F1847 using OTmonitor
or a PIC programmer, follow these steps:
- Unpack the tarball in a convenient location:
    wget -qO- https://otgw.tclcode.com/download/otgw-6.6.tgz | tar xzv
- Go to the otgw-6.6 directory: cd otgw-6.6
- Run: make

The "build.asm" file is supposed to be auto-generated at the start of every
build. The included "build.tcl" script takes care of that. It is executed by
tclsh. If the tclsh executable is not in your PATH, pass the location to the
make command:
    make TCLSH=/path/to/tclsh

To build the diagnose firmware:
    make diagnose.hex

To build the interface firmware:
    make interface.hex


Testing
=======

A test suite is provided. Running it will verify that the covered features
are still working after a code change. The test suite relies on the open
source PIC simulator project gpsim [http://gpsim.sourceforge.net/].

Support for the PIC16F1847 used in the OTGW was added in version 0.32.1.
Building gpsim requires some resources to be available. On linux these can
be installed using the package manager.

This is how to build gpsim 0.32.1:
    wget -qO- https://sourceforge.net/projects/gpsim/files/gpsim/0.32.0/gpsim-0.32.1.tar.gz | tar xzv
    cd gpsim-0.32.1
    ./configure --disable-gui
    make -j4
Then as root:
    make install

The test suite also needs a simulated central heating system to test the
OTGW code against. This is provided in the form of a gpsim module, available
in the gpsim subdirectory of the source tarball. To build it, use a similar
procedure as above:
    cd chmodule
    ./autogen.sh
    ./configure
    make -j4
Then as root:
    make install

If the configure command fails to find (the correct) gpsim headers, you can
help it by providing the --with-gpsim=/usr/local/include/gpsim option.

Each test is defined in a file in the tests directory. The complete test
suite can be executed using:
    make test

Specific tests may be selected by passing the TESTFLAGS variable to the make
command. For example, to run only the thermostat and standalone tests:
    make test TESTFLAGS="-match 'thermostat-* standalone-*'"


Alternative setup
=================

If you are unable or unwilling to use root permissions to install gpsim, or
gputils if you need it, it is possible to install things in a different
location. The file-hierarchy man page suggests to use ~/.local for user-
installed applications.

To limit the places where you have to modify the location you have chosen,
the description below uses the PREFIX variable. Set that variable to the
desired installation location. For example:
    PREFIX=$HOME/.local

gputils:
    wget -qO- https://sourceforge.net/projects/gputils/files/gputils/1.5.0/gputils-1.5.2.tar.bz2 | tar xjv
    cd gputils-1.5.2
    ./configure --prefix=$PREFIX
    make -j4
    make install
    cd ..

gpsim:
    svn checkout svn://svn.code.sf.net/p/gpsim/code/branches/p16f1847 gpsim
    cd gpsim
    ./autogen.sh
    ./configure --prefix=$PREFIX --disable-gui
    make -j4
    make install
    cd ..

otgw-6.6:
    wget -qO- https://otgw.tclcode.com/download/otgw-6.6.tgz | tar xzv
    cd otgw-6.6
    make
    # Or, if $PREFIX/bin is not in your PATH:
    PATH=$PREFIX/bin:$PATH make

chmodule:
    cd chmodule
    ./autogen.sh
    ./configure --with-gpsim=$PREFIX/include/gpsim --prefix=$PREFIX
    make -j4
    make install
    cd ..


Customizing
===========

The provided Makefile allows for some customization via a Makefile-local.mk
file. It is advised to put any customizations in this file rather than
editing the Makefile, because then your changes will not be lost when the
sources are updated to a new version. In the Makefile-local.mk file you can
do things like specifying variables for the used binaries, if they are not
on your default PATH. By putting this in the Makefile-local.mk file, you
don't have to remember to pass variables to the make command each time.

For example, with the following lines in Makefile-local.mk, the make command
can be invoked normally and it will find the tools that were installed under
~/usr/local:

  USERBIN = $(HOME)/usr/local/bin
  GPASM = $(USERBIN)/gpasm
  GPLINK = $(USERBIN)/gplink
  GPSIM = $(USERBIN)/gpsim

  test: export LD_LIBRARY_PATH = $(HOME)/usr/local/lib
