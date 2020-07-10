#!/usr/bin/env python

################################################################################
#
# file:     clipper.py
#
# purpose:  Synchronizes text in the CLIPBOARD with the X11 PRIMARY buffer.
#
#           Allows GTK/KDE apps X selection without needing <MiddleMouse>.
#           i.e.  Highlight text in xterm or yank text in vim, and then ctrl-v in galeon.
#
# author:   brian m sturk   bsturk@comcast.net
#                           http://www.briansturk.com
#
# created:  06/21/02
# last_mod: 02/24/06
# version:  0.2
#
# usage:    python clipper.py &
#
# history:      
#
################################################################################

import sys

try:
    import Tkinter

except:
    print 'This module uses Tkinter.  Please install the python Tkinter module...'
    sys.exit( 1 )

################################################################################

def check_clipboards():

    global root, withdrawn, freq, prev_clip_content, prev_pri_content

    debug        = 0
    clip_content = ''
    pri_content  = ''

    if not withdrawn:
        withdraw = 1
        root.withdraw()      ##  Hide the main window

    try:
        clip_content = root.selection_get( selection = 'CLIPBOARD' )

    except:
        pass

    try:
        pri_content = root.selection_get( selection = 'PRIMARY' )

    except:
        pass

    if pri_content != prev_pri_content:

        ##  The X11 selection has changed

        if pri_content != '' and pri_content != clip_content:

            ##  The primary selection wasn't set by using the clipboard, it was
            ##  set via some X11 app, so put the X11 contents into the clipboard.

            root.clipboard_clear()
            root.clipboard_append( pri_content )

    prev_pri_content  = pri_content
    prev_clip_content = clip_content

    root.after( freq, check_clipboards )
    
#############

root              = Tkinter.Tk()
prev_clip_content = ''
prev_pri_content  = ''

##  configurable params

withdrawn         = 0                   ##  show main window? 0 no, 1 yes
freq              = 1000                ##  how often to sync, in ms

#############

check_clipboards()

try:
    root.mainloop()

except:
    sys.exit( 1 )
