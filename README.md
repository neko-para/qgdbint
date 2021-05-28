# QGdbInt

This is a simple interface of gdb in Qt.

## Ref

class ```QGdbProcessManager``` provide a interface to excute command(in GDB/MI).

class ```QGdb``` provide a simple interface to do some high-level action.

## Current Supported

* Full low level IO and record parsing
* Run and continue
* Step, step in and step out
* Add and delete breakpoints, customize breakpoints (e.g. countdown, condition)
* Evaluate expressions