% UNWGT(1) Version 1.0 | Unwgt Documentation

NAME
====
unwgt - extracts and optionally verifies widget and RALF packages.

SYNOPSIS
========
**unwgt** **-x** **-f** _WIDGET_ [_OPTIONS_] [_MEMBER..._]

**unwgt** **-m** [_MOUNTPOINT_] **-f** _WIDGET_ [_OPTIONS_]

DESCRIPTION
===========
Command line tool to extract and verify a widget / package.
It was written as a test bench and example app for using libralf, however it may be of general use for debugging package issues.

The command line arguments are loosely based on [tar][1].

Operation mode
--------------
The options listed below tell unwgt what operation to perform.  Exactly one of
them must be given.

**-t, --list**

:   List the contents of an widget.  Arguments are optional.

**-x, --extract**

:   Extract files from the widget. Arguments are optional.  When given, they
specify names of the archive members to be extracted.

**-m, --mount**=_MOUNTPOINT_

:   Mounts the widget at the given mount point.  A mount point must be specified
and it must be an existing directory.  This is only valid for OCI packages.

**-M, --metadata**

:   Dumps the metadata of the package in JSON form.

**-S, --siginfo**

:   Dumps the signing details of the package.  Currently, this is just a dump of the signing certificate chain.


OPTIONS
=======

**-f, --file=**_WIDGET_

:   Use widget file _WIDGET_.  Currently, this must be specified, unlike [tar][1]
this tool does not support reading widgets from _stdin_.

**-C, --directory=**_DIRECTORY_

:   Extract widget contents to _DIRECTORY_, _DIRECTORY_ must exist.  If not
specified then widget is extracted into the current working directory.

**-n, --no-verify**

:   Skips widget verification.  If mounting then the EROFS image is mounted
without a dm-verity mapping.

**--cert-dir=**_DIRECTORY_

:   Read verification certificates from _DIRECTORY_.  By default, certificates
are read from the _/etc/sky/certs_ directory.

**-c, --cert=**_CERTIFICATE_

:   Path to a verifier certificate, can be specified multiple times.
If set then **--cert-dir** is ignored.

**-e, --enable-cert-expiry-check**

:   Enable checking of the certificate expiry time when verifying.
By default, expiry time is not checked.

**-h, --help**

:   Display a short option summary and exit.

**-v, --verbose**

:   Increases the log level, can be specified multiple times.

**-V, --version**

:   Print program version and copyright information and exit.


RETURN CODES
============
Unwgt returns 0 on success and a non-zero value on error.


EXAMPLES
========
**unwgt -x -f package.wgt -D /tmp/unpacked**

Extracts and verifies the entire contents of _package.wgt_ file into the
_/tmp/unpacked_ directory.

**unwgt -x -f package.wgt config.xml**

Extracts just the _config.xml_ file from the _package.wgt_ and writes it to the
current working directory.

**unwgt -t -f package.wgt --no-verify**

Lists the contents of the widget without performing any verification.

**unwgt -m /mnt/package -f package.ralf --cert=/etc/certs/app-rootca.crt**

Mounts _package.ralf_ to the _/mnt/package_ directory. The package will be
verified against a single certificate _/etc/certs/app-rootca.crt_.



[1]: https://man7.org/linux/man-pages/man1/tar.1.html
