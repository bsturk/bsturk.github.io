#!/usr/bin/env python

################################################################################
#
# file:     cab_menu.py
#
# author:   brian m sturk   bsturk@adelphia.net
#                           http://users.adelphia.net/~bsturk
# purpose:
#           Menu based launcher useful for arcade cabinets.
#           Keys can be assigned and programs to launch changed
#           just by modifying this file.  Script can also
#           exit to windows and reboot/shutdown.  Basically allows
#           you to not need a keyboard to use the cabinet.
#
#           Windows centric, w/ very small changes it could
#           run on Linux.  My cabinet runs Win98 and Win2k.
#           To reboot on Win2k you can use shutdown.exe from the
#           NT resource kit.  If an alternate one is used, the
#           routine handle_shutdown() will need to be modified.
#           XP and Win9X are handled using built in commands.
#           TODO: Provide link.
#
# dependencies:
#           Uses the pygame library.
#
# created:  04/16/02
# last_mod: 10/02/11
# version:  0.4
#
# usage, etc:
#           python cab_menu.py, or if file is associated w python just
#           double click.  Best results if added to startup folder.
#           See sections Customization and Key mappings for tweakable
#           parameters.
#
# history:  
#           04/19/02    Initial release, version 0.1
#           04/21/02    Added support for command arguments to apps
#           01/23/03    Fixed launching of apps on Win98, restart for Win2k/XP,
#                       Detect diff versions of Windows, and mouse double click
#                       support.
#           02/18/05    Version 0.3.  Added support for rotating menu.
#                       Also added support for background images.
#           10/02/11    Version 0.4.  Updated to get full-screen working w/ new pygame way.
#                       Also switched to non DirectX SDL driver.
#
###############################################################################

DEBUG = 0

import os

## direct x SDL is broken > XP SP1
os.environ['SDL_VIDEODRIVER'] = 'windib'

import pygame, pygame.font, pygame.cursors, pygame.draw, pygame.time

from pygame.locals import *

##################
##  Customization
##################

_horiz_res       = 800
_vert_res        = 600
_run_fullscreen  = 1
_orient_horiz    = 1

_title           = 'S T U R C A D E  Launcher'

_font_size       = 42       ##  in pixels
_item_spacing    = 10       ##  in pixels

_bg_color        = 0, 0, 0
_fg_color        = 250, 240, 230
_cur_sel_color   = 0, 255, 0

_bg_image        = 'NONE'

#################
##  Key mappings
#################

_exe_keys              = [ K_RETURN, K_1 ]
_up_keys               = [ K_UP, K_RIGHT ]
_down_keys             = [ K_DOWN, K_LEFT ]
_quit_keys             = [ K_ESCAPE ]
_orient_keys           = [ K_LALT, K_LCTRL ]

#############################################################
##  Change these depending on what is running on your cabinet
#############################################################

##  Format is:
##  
##      Name of app to display : ( app to run, ( arg1, arg2, ), background image )
##
##  NOTE:  You must use a full path here
##
##      Valid 'symbolic' apps are:
##         
##          EXIT        ( just quits this program )
##          RESTART
##          SHUTDOWN
##    
##      Use NONE to not display a background image
##  

_choices = { 

    'MAME'            : ( r'advmenu.exe', ( r'advmenu.exe', ), r'arcade.jpg' ),
    'Misfit Mame'     : ( r'c:\mame_cab\emu\misfit_mame\misfit.bat', (), r'arcade.jpg' ),
    'Dragon\'s Lair'  : ( r'c:\mame_cab\emu\daphne\dlair.bat', (), 'dlair.jpg' ),
    'Space Ace'       : ( r'c:\mame_cab\emu\daphne\ace.bat', (), 'space.jpg' ),
    'Restart'         : ( 'RESTART', (), 'NONE' ),
    'Exit to Windows' : ( 'EXIT', (), 'NONE' ),
}

##################
##  END Customization
##################

_screen                = None
_font                  = None
_x_start               = 0
_y_start               = 0
_cur_index             = 2           ##  start at 2, which for me is 'MAME',  python dicts are not in order so this may vary
_mouse_info            = None        ##  used for detecting double clicks

kOK                    = 0 
kQUIT                  = -1

kSHUTDOWN              = 1
kRESTART               = 2

kWINNT                 = 1
kWIN2K                 = 2
kWINXP                 = 3
kWIN9X                 = 4

##  Max amount based on text strings

_horizontal_space_used = 0
_vertical_space_used   = 0

##  Max number of ticks between clicks to register double mouse click

kDOUBLECLICK           = 300

#############

def main():

    pygame.init()

    global _screen

    if _run_fullscreen: 
        dbg_print( 'Running fullscreen' )
        _screen = pygame.display.set_mode( ( _horiz_res, _vert_res ), pygame.FULLSCREEN )

    else:
        _screen = pygame.display.set_mode( ( _horiz_res, _vert_res ) )

    global _mouse_info
    _mouse_info = mouse_info( 0, 0, 0 )

    pygame.mouse.set_cursor( *pygame.cursors.tri_left )
    pygame.display.set_caption( _title )

    background = pygame.Surface( _screen.get_size() )
    background = background.convert()
    background.fill( _bg_color )

    global _font
    _font = pygame.font.Font( None, _font_size )

    ##  NOTE: assumption is that user can't resize the window

    calc_text_region()

    ##  START main program loop

    done    = 0
    invoked = 0     ##  used to workaround 'catching' emulator 'esc' press
    execute = 0

    select_img()

    while not done:

        pygame.display.update()

        for e in pygame.event.get():

            if e.type == QUIT or ( e.type == KEYUP and e.key in _quit_keys ):

                ##  TODO:  The below was added to eat up 'esc' from
                ##         emulator.  However, this also means that
                ##         after launching something that doesn't exhibit
                ##         the issue, you need to 'quit' twice.  i.e. press
                ##         the X (close) button.

                dbg_print( 'quit requested' )

                if invoked == 1:

                    dbg_print( 'invoked is 1' )
                    ##  May have soaked up 'esc' key from emulators

                    invoked = 0
                    continue

                else:

                    dbg_print( 'invoked is not 1' )
                    done = 1
                    break
            
            if e.type == KEYUP:

                if e.key in _up_keys or e.key in _down_keys:
                    handle_key( e )                                         

                if e.key in _exe_keys:
                    execute = 1

                if e.key in _orient_keys:

                    global _orient_horiz
                    _orient_horiz = not _orient_horiz
                    calc_text_region()

            if e.type is MOUSEBUTTONUP:

                handle_mouse( e )

                if double_clicked( e ):
                    execute = 1

            if execute:

                execute = 0
                invoked = 1

                ret = start_selected()

                if ret == kQUIT:
                    done = 1
                    break

        ##  clear the screen w/ solid background color

        _screen.blit( background, ( 0, 0 ) )

        draw_image()
        render_text()
        render_select_rect()

        pygame.display.flip()

    ##  END main program loop

#############

def calc_text_region():

    ##  calculate upper left corner to start displaying entries

    global _horizontal_space_used, _vertical_space_used

    for key in _choices.keys():
        
        text_width, text_height = _font.size( key )

        if text_width > _horizontal_space_used:
            _horizontal_space_used = text_width

    _vertical_space_used = 0
    _vertical_space_used += len( _choices ) * _font_size;
    _vertical_space_used += ( len( _choices ) - 1 ) * _item_spacing;

    global _x_start
    global _y_start

    if _orient_horiz:
        _x_start = ( _horiz_res - _horizontal_space_used ) / 2
        _y_start = ( _vert_res - _vertical_space_used ) / 2

    else:
        _x_start = ( _horiz_res - _vertical_space_used ) / 2
        _y_start = ( _vert_res - _horizontal_space_used ) / 2

    dbg_print( 'x start is ' + str( _x_start ) )
    dbg_print( 'y start is ' + str( _y_start ) )

#############

def draw_image():

    global _bg_image

    ##  draw background image if one was specified for this selection

    if _bg_image != 'NONE':

        img = pygame.image.load( _bg_image )

        if not _orient_horiz: 
            img  = pygame.transform.rotate( img, 90 )   ##  TODO: Not perfect

        _screen.blit( img, ( 0, 0 ) )

#############

def render_text():

    ##  Draw all of the text string choices

    count = 0

    for key in _choices.keys():

        text    = _font.render( key, 1, _fg_color )
        textpos = text.get_rect()

        offset = ( _font_size + _item_spacing ) * count

        if _orient_horiz:
            textpos = textpos.move( _x_start, _y_start + offset )

        else:
            text    = pygame.transform.rotate( text, 90 )
            textpos = textpos.move( _x_start + offset, _y_start )

        _screen.blit( text, textpos )

        count += 1

#############

def render_select_rect():

    ##  Draw a rectangle around the current selection

    if _orient_horiz:

        height = _font_size + _item_spacing
        width  = _horizontal_space_used + _item_spacing

        offset = ( _cur_index - 1 ) * height

        x = _x_start - ( _item_spacing / 2 )
        y = _y_start - ( _item_spacing / 2 ) + offset

    else:

        width = _font_size + _item_spacing
        height  = _horizontal_space_used + _item_spacing

        offset = ( _cur_index - 1 ) * width

        x = _x_start - ( _item_spacing / 2 ) + offset
        y = _y_start - ( _item_spacing / 2 )

    outline = ( x, y, width, height )

    pygame.draw.rect( _screen, _cur_sel_color, outline, 1 )

#############

def handle_key( event ):

    global _cur_index

    if event.key == K_DOWN or event.key == K_RIGHT:

        ##  move the selection down

        if _cur_index < len( _choices ):
            _cur_index += 1

    if event.key == K_UP or event.key == K_LEFT:

        ##  move the selection up

        if _cur_index > 1:
            _cur_index -= 1

    select_img()

#############

def select_img():

    ##  update the image if there is one

    count = 0
    img   = 'NONE'

    for key in _choices.keys():
        choice, args, image = _choices[ key ] 

        if _cur_index == ( count + 1 ):
            img = image
            break

        count += 1

    global _bg_image
    _bg_image = img

#############

def handle_mouse( event ):

    count = 0
    x, y  = event.pos

    for key in _choices.keys():

        text    = _font.render( key, 1, _fg_color )
        textpos = text.get_rect()

        offset = ( _font_size + _item_spacing ) * count
        textpos = textpos.move( _x_start, _y_start + offset )

        if textpos.collidepoint( x, y ):

            global _cur_index
            _cur_index = count + 1

            break

        count += 1

#############

def double_clicked( event ):

    x, y = event.pos

    num_ticks     = pygame.time.get_ticks()
    elapsed_ticks = num_ticks - _mouse_info.ticks

    if _mouse_info.x == x and _mouse_info.y == y:

        dbg_print( 'Elapsed ticks since last click ' + str( elapsed_ticks ) )

        if elapsed_ticks <= kDOUBLECLICK:
            return 1

    _mouse_info.x     = x
    _mouse_info.y     = y
    _mouse_info.ticks = num_ticks

    return 0

#############

def start_selected():

    ##  dictionaries are not ordered so we need to walk

    count = 0

    for key in _choices.keys():
        choice, args, image = _choices[ key ] 

        if _cur_index == ( count + 1 ):
            break

        count += 1

    ##  First check for symbolic app names

    if choice == 'EXIT':
        return kQUIT

    elif choice == 'RESTART':

        ret = handle_shutdown( kRESTART )

        if ret:
            return kQUIT

        return kOK

    elif choice == 'SHUTDOWN':

        ret = handle_shutdown( kSHUTDOWN )

        if ret:
            return kQUIT

        return kOK

    ##  Explicit launch of application

    else:

        global _screen

        if _run_fullscreen:

            ##  Let an app run fullscreen
            _screen = pygame.display.set_mode( ( _horiz_res, _vert_res ) )

            #pygame.display.toggle_fullscreen()

        pygame.mouse.set_cursor( *pygame.cursors.diamond )
        exec_app( choice, args, os.P_WAIT )
        pygame.mouse.set_cursor( *pygame.cursors.tri_left )

        if _run_fullscreen:

            ##  go back to fullscreen
            _screen = pygame.display.set_mode( ( _horiz_res, _vert_res ), pygame.FULLSCREEN )

            #pygame.display.toggle_fullscreen()

        return kOK

#############

def handle_shutdown( type ):

    choice = ''
    args   = None

    ver = win_ver()

    if ver == kWIN9X:

        ##  Codes are as follows:
        ##  0 - LOGOFF 1 - SHUTDOWN  2 - REBOOT  4 - FORCE  8 - POWEROFF

        choice = 'rundll32.exe'
        args   = ( 'rundll32.exe', 'shell32.dll,SHExitWindowsEx', type )

    elif ver == kWIN2K:

        choice = 'shutdown.exe'

        if type == kSHUTDOWN:
            args   = ( 'shutdown.exe', '/L' )

        if type == kRESTART:
            args   = ( 'shutdown.exe', '/L /R' )

    elif ver == kWINXP:

        choice = 'shutdown.exe'

        if type == kSHUTDOWN:
            args   = ( 'shutdown.exe', '-s' )

        if type == kRESTART:
            args   = ( 'shutdown.exe', '-r' )

    else:

        dbg_print( 'handle_shutdown(): Unknown platform' )
        return 0

    exec_app( choice, args, os.P_WAIT )

    return 1
    
#############

def exec_app( app, args, flag ):

    dbg_print( 'Running ' + app )
    dbg_print( 'With args: ' )

    for each in args:
        dbg_print( each )

    os.spawnv( flag, app, args )

#############

def win_ver():

    import os

    comspec = os.environ[ 'COMSPEC' ]

    if comspec.find( 'cmd.exe' ):

        version = os.popen( 'cmd.exe /c ver' ).read()

        if version.find( 'Windows 2000' ) >= 0:
            return kWIN2K

        elif version.find( 'Windows XP' ) >= 0:
            return kWINXP

        else:
            return kWINNT

    else:

        ##  Win9X uses command.com for comspec

        return kWIN9X
    
#############

class mouse_info:

    def __init__( self, x, y, ticks ):

        self.x     = x
        self.y     = y
        self.ticks = ticks 

#############

def dbg_print( str ):

    if DEBUG:
        print 'DEBUG: ' + str

#############

if __name__ == '__main__':
    main()    
