#!/usr/bin/env python

################################################################################
#
# file:     frontend.py
#
# author:   brian m sturk   bsturk@comcast.net
#                           http://users.adelphia.net/~bsturk
# purpose:
#           Menu based launcher useful for emulators
#
# dependencies:
#
#           Uses the pygame library.
#
# created:  12/19/05
# last_mod: 04/22/07
# version:  0.7
#
# usage, etc:
#
#           python frontend.py, or if file is associated w/ python just
#           double click.  
#
#           Best results if this file is added to startup folder.
#           See sections Customization and Key mappings for tweakable
#           parameters.  I use this frontend in a custom built PC which
#           I keep in living room.  The idea was to have a frontend that
#           would allow me to start up games from a wireless game 
#           controller while sitting on my couch.  I use JoyToKey to 
#           convert joystick input to keyboard input.  It works very well.
#
# arguments:
#
#           -window  Force windowed mode rather than the default, fullscreen
#           -v       Enable debug printing
#
# TODO:
#
#         * Allow specifying spawn flags for pre/app/post, i.e. NOWAIT
#         * More informative text when pic not found/not loaded
#         * In addition to overrides, there should be support for additions that
#           aren't in any of the specified paths, i.e. for util programs
#         * Add support for viewing and scrolling through game docs
#         * Finish center scrolling, fix render_entries to accomodate scrolling
#
# history:  
#
#  0.1      12/21/05  - Initial release
#  0.2      01/22/06  - Bug fixes, added key repeat and params
#  0.3      01/30/06  - Added a few more features like showing rom count
#                       and showing currently selected rom over pic
#  0.4      03/25/06  - Added support for multiple image directories (pic_dirs), 
#                       and searching for subdir in each based on first letter
#                       or rom/dsk image
#           04/24/06  - Fixed bug where non-override name was being shown in pic
#                       window.
#  0.5      06/06/06  - Overriden name is now also checked for picture name
#                     - Fixed bug where short_name was matching other names 
#                       when including the name in it.  Fixed by using re.match 
#                       instead of re.search.
#                       i.e. parsec matching in beyond_parsec
#                     - Fixed sorting of entries.  sort needed to be keyed off basename for
#                       filenames
#                     - Added support for optional display regex which will be applied
#                       to display names.  Is useful for filtering out "Good" codes.
#                     - Fixed potential issue w/ short_name vars shadowing routine name called short_name
#                     - Fixed warnings found by pychecker
#                     - Warp the mouse to 0,0 when starting program 
#                       (applewin, although it still overrides it)
#                     - Added check for existance of menu sub-directory
#                     - Added support for skipping through entries by letter.  This is done by holding down
#                       a key marked as _alpha_menu_shift_keys and then pressing a key in _fast_menu_up_keys or a
#                       key in _fast_menu_down_keys.  This way one press will go from any entry in 'a' to the first 
#                       'b' entry.
#                     - Added support for KEYUP events and differentiated between keys that should respond
#                       to KEYDOWN (scrolling type) and KEYUP (button presses/held).
#                     - Fixed a bug where the wrong override index was being used.
#  0.6     11/20/06   - Fixed handling of uppercase letters.  New behavior is to treat all caps as one letter
#                       so they are all skipped if using the accelerated buttons.  This is mainly for my
#                       tastes since I tend to not have files with uppercase and scrolling through 
#                       potentially 26 single entries to get to the lowercase ones would be painful.
#                     - Sped up image lookups
#  0.7     04/28/07   - Fixed bug with not displaying updated info when cycling by letter
#                     - Added support for command-line flag -window
#                     - Revamped key jumping.  Now works correctly, including capital names
#                     - Made alpha values customizable
#                     - Added parent menu title name in rom count area.  Frees up line for info
#                     - Added -v flag which enables debug printing
#                     - Separated out filters and ignores, filters take a long time if there are many of them
#                     - Made overrides, filters, and ignores optional
#                     - Fixed potential crash in dump_menu when optional elements aren't present
#                     - Compile filter regexes to speed things up a bit
#
###############################################################################

DEBUG   = False
VERSION = '0.7'

import os, sys, string, re

import pygame, pygame.font, pygame.cursors, pygame.draw, pygame.time
from pygame.locals import *

##################
##  Customization
##################

_horiz_res                = 800
_vert_res                 = 600
_run_fullscreen           = True

##  all these are in pixels

_menu_font_size           = 32
_info_font_size           = 26
_rom_pic_font_size        = 30
_rom_count_font_size      = 24

_item_spacing             = 0
_item_padding             = 3

_bg_color                 = 0, 0, 0
_fg_color                 = 250, 240, 230
_cur_sel_color            = 0, 255, 0
_border_color             = 250, 240, 230
_dlg_bg_color             = 125, 125, 125
_dlg_fg_color             = 0, 0, 0
_rom_pic_bg_color         = 255, 255, 255
_rom_pic_fg_color         = 0, 0, 0
_rom_name_alpha           = 225             # 0 to 255, 0 is completely transparent
_dlg_alpha                = 250

_key_repeat_delay         = 200
_key_repeat_interval      =  75

_main_title               = 'EMU Launcher'
_jump_amount              = 10     ##  number of entries to skip when using fast menu traversal
_show_name_in_pic         = True

#################
##  Key mappings
#################

_menu_up_keys             = [ K_UP ]
_menu_down_keys           = [ K_DOWN ]
_select_keys              = [ K_F9 ]
_back_keys                = [ K_F10 ] 
_fast_menu_up_keys        = [ K_LEFT ]
_fast_menu_down_keys      = [ K_RIGHT ]
_alpha_menu_shift_keys    = [ K_F11 ]
_quit_keys                = [ K_F12 ]

##################
##  End Customization
##################

##  NOTE:  These #s assume that a returned surface will never be any of these values

kLOAD_NEW_IMAGE           = -1
kNO_IMAGE                 = 0
                          
kNO_CHANGE                = 0
kMENU_CHANGED             = 1 
kSEL_CHANGED              = 2 
                          
kMENU_SUBDIR              = 'menus'
kEXIT_SENTINAL            = 'EXIT'
kDEFAULT_SENTINAL         = 'DEFAULT'

## index into override tuple

kOVERRIDE_NAME_INDEX      = 0
kOVERRIDE_APP_INDEX       = 1
kOVERRIDE_ARGS_INDEX      = 2
kOVERRIDE_PRE_INDEX       = 3
kOVERRIDE_PRE_ARGS_INDEX  = 4
kOVERRIDE_POST_INDEX      = 5
kOVERRIDE_POST_ARGS_INDEX = 6
kOVERRIDE_INFO_INDEX      = 7

_screen                   = None
_img                      = kLOAD_NEW_IMAGE
_menu_font                = None
_info_font                = None
_rom_pic_font             = None
_rom_count_font           = None
_cur_index                = 1      ##  always starts at the first, 1 based
_pop_index                = 1      ##  always starts at the first, 1 based
_max_display              = 0      ##  max entries that can be shown in menu with given font size
_select_center_index      = 0      ##  index of displayed entries that will be centered given font size
_mouse_info               = None   ##  used for detecting double clicks

_menus                    = {}     ##  holds dict of top level menus
_choices                  = {}     ##  holds dict of current menus
_menu_full_names          = []     ##  holds current menu names in full
_in_submenu               = False

_entry_rect               = pygame.Rect( 0, 0, 0, 0 )
_info_rect                = pygame.Rect( 0, 0, 0, 0 )
_pic_rect                 = pygame.Rect( 0, 0, 0, 0 )

_entry_surface            = None
_info_surface             = None
_pic_surface              = None
_launch_dlg_surface       = None

#############

key_mapping = {

    K_0 : '0',
    K_1 : '1',
    K_2 : '2',
    K_3 : '3',
    K_4 : '4',
    K_5 : '5',
    K_6 : '6',
    K_7 : '7',
    K_8 : '8',
    K_9 : '9',
    K_a : 'a',
    K_b : 'b',
    K_c : 'c',
    K_d : 'd',
    K_e : 'e',
    K_f : 'f',
    K_g : 'g',
    K_h : 'h',
    K_i : 'i',
    K_j : 'j',
    K_k : 'k',
    K_l : 'l',
    K_m : 'm',
    K_n : 'n',
    K_o : 'o',
    K_p : 'p',
    K_q : 'q',
    K_r : 'r',
    K_s : 's',
    K_t : 't',
    K_u : 'u',
    K_v : 'v',
    K_w : 'w',
    K_x : 'x',
    K_y : 'y',
    K_z : 'z',
}

#############

def main():

    global _choices
    global _screen
    global _mouse_info
    global _menu_font
    global _info_font
    global _rom_pic_font
    global _rom_count_font
    global _img
    global _menu_full_names
    global _key_repeat_delay
    global _key_repeat_interval

    if ( len( sys.argv ) > 1 ):
        handle_cmd_line_args( sys.argv )

    _choices = load_menus()

    pygame.init()

    try:

        if _run_fullscreen:
            _screen = pygame.display.set_mode( ( _horiz_res, _vert_res ), FULLSCREEN )
        else:
            _screen = pygame.display.set_mode( ( _horiz_res, _vert_res ) )

    except Exception, e:
        print 'pygame.display.set_mode(): failed, is mouse plugged in? ' + str( e )
        sys.exit()

    pygame.key.set_repeat( _key_repeat_delay, _key_repeat_interval )

    update_title()

    _mouse_info = mouse_info( 0, 0, 0 )

    pygame.mouse.set_cursor( *pygame.cursors.arrow )
    pygame.mouse.set_pos( 0, 0 )

    background = pygame.Surface( _screen.get_size() )
    background = background.convert()

    background.fill( _bg_color )

    _menu_font        = pygame.font.Font( None, _menu_font_size )
    _info_font        = pygame.font.Font( None, _info_font_size )
    _rom_pic_font     = pygame.font.Font( None, _rom_pic_font_size )
    _rom_count_font   = pygame.font.Font( None, _rom_count_font_size )

    calc_text_region()
    calc_pic_region()
    calc_info_region()

    ##  _menu_full_names holds the full name list, shortened has just the equiv of basename

    _menu_full_names, shortened = short_entries( _choices.keys() )

    ##  START main program loop

    done = False

    while not done:

        pygame.display.update()

        e = pygame.event.wait()             ##  wait for a GUI event rather than chewing the CPU

        ret = kNO_CHANGE

        if e.type == QUIT:

            done = True
            break
        
        elif e.type == KEYDOWN:
            ret = handle_key( e, e.type, shortened )                                         

        elif e.type == KEYUP:
            ret = handle_key( e, e.type, shortened )                                         

        elif e.type is MOUSEBUTTONUP:
            ret = handle_mouse( e )

        elif e.type is MOUSEMOTION:
            continue

        ##  clear the screen and other surfaces

        _screen.blit       ( background, ( 0, 0 ) )
        _entry_surface.blit( background, ( 0, 0 ) )
        _pic_surface.blit  ( background, ( 0, 0 ) )
        _info_surface.blit ( background, ( 0, 0 ) )

        if ret == kMENU_CHANGED:

            _menu_full_names, shortened = short_entries( _choices.keys() )
            _img = kLOAD_NEW_IMAGE     ##  menu change means sel change too, stale cached image

        if ret == kSEL_CHANGED:
            _img = kLOAD_NEW_IMAGE     ##  stale cached image

        render_rects()
        render_entries( shortened )
        render_pic()
        render_info()
        render_other()

        if ret == kMENU_CHANGED:
            update_title()

        pygame.display.flip()

    ##  END main program loop

#############

def handle_cmd_line_args( args ):

    global _run_fullscreen
    global DEBUG

    for arg in args:

        if arg == '-window':
            print 'handle_cmd_line_args(): Running in a window'
            _run_fullscreen = False

        if arg == '-v':
            print 'handle_cmd_line_args(): Enabling debug mode'
            DEBUG = True

#############

def load_menus():

    global _menus

    each = None

    if not os.path.exists( kMENU_SUBDIR ):

        print 'load_menus(): menu subdirectory ' + kMENU_SUBDIR + ' doesn\'t exist.  No menus to load'
        sys.exit()

    try:
        menus = os.listdir( kMENU_SUBDIR )

        for each in menus:

            if each.endswith( '.py' ):

                module_name = each[ :-3 ]

                if module_name == '__init__':   ## package info, skip it
                    pass

                else:
                
                    m = menu_item( kMENU_SUBDIR, module_name )

                    _menus[ m.title ] = m

                    dump_menu( m )

    except Exception, e:

        print 'load_menus(): Couldn\'t load menus, last one attempted was ' + str( each ) + ' exiting... ' + str( e )
        sys.exit()

    return _menus

#############

def load_submenu( parent_menu ):

    exts           = []
    file_dict      = {}

    if parent_menu.rom_exts == '':
        print 'load_submenu(): has_submenus is True but no rom paths to search'

    else:

        compiled_regexes = []

        for pat in parent_menu.filters:
            compiled_regexes.append( re.compile( pat ) )

        exts = string.split( parent_menu.rom_exts, ';' )
        
        if parent_menu.rom_dirs != '':

            dirs = string.split( parent_menu.rom_dirs, ';' )

            for d in dirs:

                file_list = []

                try:
                    file_list = os.listdir( d )

                except Exception, e:
                    print 'load_submenu(): Warning: Couldn\'t load files in ' + d + ' : ' + str( e )
                    continue

                for f in file_list:

                    ext_ind   = f.rfind( '.' )
                    path_ind  = f.rfind( os.path.sep )

                    ext       = f[ ext_ind + 1: ]
                    file_name = f[ path_ind + 1 : ]

                    full_name = os.path.join( d, f )

                    if ext in exts:

                        add_it = True

                        if file_name in parent_menu.ignores:
                            add_it = False

                        if should_filter( parent_menu, file_name, compiled_regexes ):
                            add_it = False

                        if add_it:
                            file_dict[ full_name ] = parent_menu

    parent_menu.subdir_entries = file_dict;

    return file_dict

#############

def should_filter( menu, file_name, compiled_regexes ):

    for pattern in compiled_regexes:

        m = pattern.match( file_name )

        if m:
            return True

    return False

#############

def find_pic( parent_menu, file_name ):

    found      = False
    image_name = ''

    dirs = string.split( parent_menu.pic_dirs, ';' )

    for d in dirs:
        
        for each in [ 'png', 'jpg', 'gif', 'bmp', 'pcx' ]:

            ##  Try directory w/ letter or num subdir, optimization rather 
            ##  than going through up to 27 dirs for each image

            letter = file_name[ 0 ]

            if not letter.isalpha():
                letter = '0_9'

            dir_name   = os.path.join( d, letter )
            image_name = ''

            if os.path.exists( dir_name ):
                image_name = os.path.join( dir_name, file_name + '.' + each )
            else:
                image_name = os.path.join( d, file_name + '.' + each )

            if os.path.exists( image_name ):
                found = True
                break;

        if found:
            break

    return found, image_name
             
#############

def get_cur_sel():

    menu    = None
    sel_key = ''

    if len( _menu_full_names ) > 0:

        sel_key = _menu_full_names[ _cur_index - 1 ]       ##  _cur_index is 1 based
        menu    = _choices[ sel_key ]

    return menu, sel_key

#############

def go_forward():

    global _choices
    global _cur_index
    global _in_submenu
    global _pop_index

    ret = kNO_CHANGE

    menu, key = get_cur_sel()

    if _in_submenu:
        exec_app( menu, key )

    else:

        if menu.has_submenus:

            sm = load_submenu( menu )

            if len( sm ) > 0:

                _in_submenu = True
                _pop_index  = _cur_index 
                _cur_index  = 1             ##  start at the top
                _choices    = sm
                ret         = kMENU_CHANGED

        else:

            exec_app( menu, key )

    return ret

#############

def go_back():

    global _cur_index
    global _in_submenu
    global _choices

    if _in_submenu:

        _cur_index  = _pop_index
        _choices    = _menus
        _in_submenu = False

        return kMENU_CHANGED

    return kNO_CHANGE

#############

def jump_to_first( key_pressed, short_name_entries, going_down = True ):

    ##  This routine handles when an alpha character is pressed.  
    ##  The selection jumps to the first entry with this letter.

    global _cur_index

    menu    = None
    found   = False
    count   = 0
    ret     = kNO_CHANGE

    menu, key = get_cur_sel()
           
    for e in short_name_entries:

        if _in_submenu:     ##  check for overrides if viewing roms/prgs etc

            if has_override( menu, e ):
 
                ##  specified to override certain fields

                new_name = get_override( menu, e, kOVERRIDE_NAME_INDEX )

                ##  check for display name field to be overriden

                if new_name != kDEFAULT_SENTINAL:
                    e = new_name

        if e[ 0 ] == key_pressed:
            found = True
            break

        count += 1

    if found:

        ret = kSEL_CHANGED
        _cur_index = count + 1

    else:

        ##  recursive call

        if going_down:

            if key_pressed == 'z':      ##  no where to go
                return

            ret = jump_to_first( get_next_key( key_pressed ), short_name_entries )    ##  find next letter with an entry

        else:
            if key_pressed == '0':      ##  no where to go
                return

            ret = jump_to_first( get_prev_key( key_pressed ), short_name_entries, False )    ##  find previous letter with an entry
    
    return ret

#############

def handle_app( cur_menu, menu_entry ):

    sn         = short_name( menu_entry )
    app        = cur_menu.app_name
    args       = cur_menu.app_args
    args_tuple = ()
    
    if has_override( cur_menu, sn ):

        ##  specified to override certain fields

        new_app = get_override( cur_menu, sn, kOVERRIDE_APP_INDEX )

        if new_app != kDEFAULT_SENTINAL:
            app = new_app

        new_args = get_override( cur_menu, sn, kOVERRIDE_ARGS_INDEX )

        if new_args != kDEFAULT_SENTINAL:
            args = new_args

    tmp   = []
    tmp.append( app )

    args = string.replace( args, '%F', menu_entry )
    args = string.replace( args, '%f', short_name( menu_entry ) )
    args = string.replace( args, '%D', full_dir_name( menu_entry ) )
    args = string.split( args, ';' )

    tmp += args

    ##  convert list to tuple

    args_tuple = tuple( tmp )

    return app, args_tuple

#############

def handle_pre_cmd( cur_menu, menu_entry ):

    sn      = short_name( menu_entry )
    pre_cmd = cur_menu.pre_cmd
    args    = cur_menu.pre_cmd_args
    
    if has_override( cur_menu, sn ):

        ##  specified to override certain fields

        new_pre  = get_override( cur_menu, sn, kOVERRIDE_PRE_INDEX )

        if new_pre != kDEFAULT_SENTINAL:
            pre_cmd = new_pre

        new_args = get_override( cur_menu, sn, kOVERRIDE_PRE_ARGS_INDEX )

        if new_args != kDEFAULT_SENTINAL:
            args = new_args

    args = ( pre_cmd, args, )

    return pre_cmd, args

#############

def handle_post_cmd( cur_menu, menu_entry ):

    sn       = short_name( menu_entry )
    post_cmd = cur_menu.post_cmd
    args     = cur_menu.post_cmd_args
    
    if has_override( cur_menu, sn ):

        ##  specified to override certain fields

        new_post = get_override( cur_menu, sn, kOVERRIDE_POST_INDEX )

        if new_post != kDEFAULT_SENTINAL:
            post_cmd = new_post

        new_args = get_override( cur_menu, sn, kOVERRIDE_POST_ARGS_INDEX )

        if new_args != kDEFAULT_SENTINAL:
            args = new_args

    args = ( post_cmd, args, )

    return post_cmd, args

#############

def exec_app( cur_menu, key ):

    global _screen

    dbg_print( 'exec_app() ' + cur_menu.title + ' ' + key )

    show_launch_dlg( display_name( cur_menu, key ) )

    pygame.time.wait( 1000 )        ##  let dialog show for visual feedback that we launched
        
    app, args = handle_app( cur_menu, key )

    if app == kEXIT_SENTINAL:
        sys.exit( 0 )

    pre_cmd, pre_args = handle_pre_cmd( cur_menu, key )

    if pre_cmd != None and pre_cmd != '':
        dbg_print( 'exec_app(): Running pre_cmd ' + pre_cmd + ' with args ' + str( pre_args ) )

        cur_dir = os.getcwd() 
        new_dir = get_dir( pre_cmd )
        os.chdir( new_dir )

        try:
            os.spawnv( os.P_WAIT, pre_cmd, pre_args )
            pass

        except Exception, e:
            print 'exec_app(): Couldn\'t run pre_cmd ' + pre_cmd + ' with args ' + str( pre_args ) + ' : ' + str( e )

        os.chdir( cur_dir )

    dbg_print( 'exec_app(): Running ' + app )
    dbg_print( 'exec_app(): With args: ' )

    for each in args:
        dbg_print( each )

    if _run_fullscreen:

        ##  let a different app run full screen by going windowed temporarily

        _screen = pygame.display.set_mode( ( _horiz_res, _vert_res ) )

    try:

        cur_dir = os.getcwd() 
        new_dir = get_dir( app )
        os.chdir( new_dir )

        dbg_print( 'exec_app(): Running app ' + app + ' with args ' + str( args ) )

        os.spawnv( os.P_WAIT, app, args )

    except Exception, e:
        print 'exec_app(): Couldn\'t run app ' + app + ' : ' + str( e )

    pygame.event.clear()        ##  pygame seems to pick up keystrokes of spawned app

    os.chdir( cur_dir )

    hide_launch_dlg()

    post_cmd, post_args = handle_post_cmd( cur_menu, key )

    if post_cmd != None and post_cmd != '':

        cur_dir = os.getcwd() 
        new_dir = get_dir( post_cmd )
        os.chdir( new_dir )

        dbg_print( 'exec_app(): Running post_cmd ' + post_cmd + ' with args ' + str( post_args ) )

        try:
            os.spawnv( os.P_WAIT, post_cmd, post_args )
            pass

        except Exception, e:
            dbg_print( 'exec_app(): Couldn\'t run post_cmd ' + post_cmd + ' with args ' + str( post_args ) + ' : ' + str( e ) )

        os.chdir( cur_dir )

    if _run_fullscreen:

        ##  go back to full-screen if we were fs now that everything is done

        _screen = pygame.display.set_mode( ( _horiz_res, _vert_res ), FULLSCREEN )

    pygame.event.clear()        ##  pygame seems to pick up keystrokes of spawned app

#############

def handle_key( event, type, shortend_entries ):

    global _cur_index

    ret = kNO_CHANGE

    if type == KEYDOWN:

        if event.key in _menu_down_keys:

            dbg_print( 'handle_key(): menu down key selected' )

            ##  move the selection down

            if _cur_index < len( _choices ):
                _cur_index += 1

            ret = kSEL_CHANGED

        if event.key in _menu_up_keys:

            dbg_print( 'handle_key(): menu up key selected' )
            
            ##  move the selection up

            if _cur_index > 1:
                _cur_index -= 1

            ret = kSEL_CHANGED

        if event.key in _fast_menu_down_keys:

            dbg_print( 'handle_key(): fast menu down key selected' )

            if alpha_menu_shift_pressed():
                ret = jump_to_next_letter( shortend_entries )       ##  routine modifies _cur_index

            else:

                if ( _cur_index + _jump_amount ) < len( _choices ):
                    _cur_index += _jump_amount

                else:
                    _cur_index = len( _choices )

                ret = kSEL_CHANGED

        if event.key in _fast_menu_up_keys:

            dbg_print( 'handle_key(): fast menu up key selected' )

            if alpha_menu_shift_pressed():
                ret = jump_to_prev_letter( shortend_entries )        ##  routine modifies _cur_index
                
            else:

                if ( _cur_index - _jump_amount ) > 1:
                    _cur_index -= _jump_amount

                else:
                    _cur_index = 1

                ret = kSEL_CHANGED

    if type == KEYUP:

        if event.key in _select_keys:

            dbg_print( 'handle_key(): select key selected' )
            ret = go_forward()

        if event.key in _back_keys:

            dbg_print( 'handle_key(): back key selected' )
            ret = go_back()

        if event.key in _quit_keys:
            dbg_print( 'handle_key(): quit key selected' )
            sys.exit( 0 )

        if is_alpha( event.key ):
            dbg_print( 'handle_key(): alpha key pressed ' + str( event.key ) )

            ascii_char = pygame_key_to_ascii( event.key )

            if is_alpha( ascii_char ):
                if event.mod & KMOD_SHIFT:
                    ascii_char = string.upper( ascii_char )
            
            ret = jump_to_first( ascii_char, shortend_entries )

    return ret

#############

def handle_mouse( event ):

    global _cur_index

    ret = kNO_CHANGE

    d_click = False

    if _mouse_info.double_clicked( event ):
        d_click = True

    x_click, y_click  = event.pos

    ##  Don't even bother if the user didn't click on the menu area

    if not _entry_rect.collidepoint( x_click, y_click ):
        return

    x = _entry_rect.left
    y = _entry_rect.top + ( _item_padding * 3 )     ##  border, selection rect, and upper spacing - so * 3

    start = 0

    if _cur_index >= _max_display:
        start = _cur_index - _max_display

    for num in range( _max_display ):

        height = ( _menu_font_size + _item_spacing )
        offset = height * num

        rect = pygame.Rect( x, y + offset, _entry_rect.width, height )

        if rect.collidepoint( x_click, y_click ):

            ret = kMENU_CHANGED
            _cur_index = start + num + 1  ##  _cur_index is 1 based

            if d_click:
                ret = go_forward()

            break

    return ret         

#############

def render_entries( shortened ):

    ##  Draw all of the text string choices

    count = 0
    total = len( shortened )
    x     = 0
    y     = _entry_rect.top + ( _item_padding * 3 )     ##  border, selection rect, and upper spacing - so * 3

    start = 0

    if _cur_index >= _max_display:
        start = _cur_index - _max_display

    end = start + _max_display

    shortened = shortened[ start : end ]

    menu = None

    if _in_submenu:
        menu, key = get_cur_sel()

    for e in shortened:

        if _in_submenu:
            e = display_name( menu, e )

        text    = _menu_font.render( e, 1, _fg_color )
        textpos = text.get_rect()

        x = _item_padding + ( ( _entry_rect.width - textpos.width ) / 2 )

        ##  align to the left if text is wider than area

        if textpos.width > ( _entry_rect.width - ( 2 * _item_padding ) ):
            x = _entry_rect.left + ( 2 * _item_padding )        

        offset = ( _menu_font_size + _item_spacing ) * count
        textpos = textpos.move( x, y + offset )

        _entry_surface.blit( text, textpos )

        count += 1

    ##  Draw rom count/index below entries to left

    rom_count_text = str( _cur_index ) + ' of ' + str( total )

    if _in_submenu:

        ##  Draw emulator name if in submenu
        rom_count_text = menu.title + ' : ' + rom_count_text

    text    = _rom_count_font.render( rom_count_text, 1, _fg_color )
    textpos = text.get_rect()

    x = _item_padding + ( ( _entry_rect.width - textpos.width ) / 2 )
    y = _entry_rect.height + _item_padding

    textpos = textpos.move( x, y )
    _entry_surface.blit( text, textpos )

    ##  draw the whole thing onto the main surface
    _screen.blit( _entry_surface, ( 0, 0 ) )

#############

def render_rects():

    ##  Draw border rect for entries

    r = _entry_rect.inflate( -_item_padding, -_item_padding )
    pygame.draw.rect( _entry_surface, _border_color, r, 2 )

    ##  Draw line separating pic and info areas

    pygame.draw.line( _screen, _border_color, ( _entry_rect.left, _pic_rect.bottom ), ( _horiz_res, _pic_rect.bottom ), 2 )

    ##  Draw a rectangle around the current selection

    height = _menu_font_size + _item_spacing
    width  = _entry_rect.width - ( _item_padding * 2 )

    index = _cur_index 

    if _cur_index > _max_display:
        index = _max_display

    offset = ( index - 1 ) * height

    x = _entry_rect.left + _item_padding
    y = _entry_rect.top  + ( _item_padding * 2 ) + offset

    outline = pygame.Rect( x, y, width, height )
    outline.inflate_ip( -_item_padding, -_item_padding )

    pygame.draw.rect( _entry_surface, _cur_sel_color, outline, 2 )

#############

def render_pic():

    global _img

    file_name = ''
    menu, key = get_cur_sel()

    ext_ind   = key.rfind( '.' )
    path_ind  = key.rfind( os.path.sep )
    file_name = key[ path_ind + 1 : ext_ind ]

    sn = key

    if _in_submenu:
        sn = display_name( menu, key )

    pic_rec = _pic_rect.inflate( -2 * _item_padding, -2 * _item_padding )

    if _img == kNO_IMAGE:
        show_no_image()

        if _show_name_in_pic:
            show_rom_name( sn, pic_rec )

        return

    if _img == kLOAD_NEW_IMAGE:    ##  image is not cached

        found = False

        if _in_submenu:
            found, image = find_pic( menu, file_name )

            ##  see if the pic shows up w/ the display name of an override

            if sn != file_name and not found:
                found, image = find_pic( menu, sn )

        else:
            image = menu.pic

            if os.path.exists( image ):
                found = True
            
        if found:

            try:
                img = pygame.image.load( image )
                _img = pygame.transform.scale( img, ( pic_rec.width, pic_rec.height )  )

            except:
                found = False

        if not found:

            _img = kNO_IMAGE
            show_no_image()

            if _show_name_in_pic:
                show_rom_name( sn, pic_rec )

            return

    ##  Will hit here when _img is kLOAD_NEW_IMAGE and when _img is a real surface

    _screen.blit( _img, pic_rec )

    ##  Put rom name over pic if enabled

    if _show_name_in_pic:
        show_rom_name( sn, pic_rec )

#############

def render_info():

    menu, key = get_cur_sel()
    sn        = short_name( key )
    info      = menu.info

    if has_override( menu, sn ):

        ##  specified to override certain fields

        new_info = get_override( menu, sn, kOVERRIDE_INFO_INDEX )

        ##  check for display info field to be overriden

        if new_info != kDEFAULT_SENTINAL:
            info = new_info

    if info != '':

        lines = string.split( info, ';' )

        count = 0
        y     = _info_rect.top + _item_padding

        for l in lines:

            text    = _info_font.render( l, 1, _fg_color )
            textpos = text.get_rect()

            if textpos.width > _info_rect.width:
                textpos.width = _info_rect.width

            x = _info_rect.left + ( 2 * _item_padding ) + ( ( _info_rect.width - textpos.width ) / 2 )

            offset = ( _info_font_size + _item_spacing ) * count
            textpos = textpos.move( x, y + offset )

            _screen.blit( text, textpos )

            count += 1

#############

def render_other():

    if _launch_dlg_surface != None:

        x = _horiz_res / 4
        y = _vert_res  / 4

        textpos = _launch_dlg_surface.get_rect()
        textpos = textpos.move( x, y )

        _screen.blit( _launch_dlg_surface, textpos )

#############

def show_rom_name( filename, pic_rec ):

    if not _in_submenu:
        return

    text    = _rom_pic_font.render( filename, 1, _rom_pic_fg_color )
    textpos = text.get_rect()

    if textpos.width > pic_rec.width:
        textpos.width = pic_rec.width

    x = pic_rec.left + ( 2 * _item_padding ) + ( ( pic_rec.width - textpos.width ) / 2 )
    y = ( pic_rec.y + pic_rec.height ) - ( _rom_pic_font_size + _item_spacing )

    textpos = textpos.move( x, y )
    rectpos = textpos.inflate( 15, 5 )

    rom_pic_surface = pygame.Surface( ( rectpos.w, rectpos.h ) )
    rom_pic_surface.set_alpha( _rom_name_alpha )
    rom_pic_surface.fill ( _rom_pic_bg_color )       ##  clear it

    _screen.blit( rom_pic_surface, rectpos )
    _screen.blit( text, textpos )

#############

def calc_text_region():

    global _entry_surface
    global _max_display
    global _select_center_index

    ##  menu entries take up left most third

    w =  _horiz_res / 3
    h =  _vert_res

    _entry_rect.left   = 0
    _entry_rect.top    = 0
    _entry_rect.width  = w
    _entry_rect.height = h - ( _rom_count_font_size + ( _item_spacing * 2 ) )    ##  leave room for rom count 

    _entry_surface = pygame.Surface( ( w, h ) )

    pos     = _entry_rect.height
    count   = 0

    while pos > _menu_font_size:

        count += 1
        pos -= _menu_font_size + _item_spacing

    _max_display = count
    _select_center_index = _max_display / 2

    dbg_print( 'calc_text_region(): a max of ' + str( _max_display ) +  ' entries can be displayed at one time and ' + str( _select_center_index ) + ' will be the center index' )

#############

def calc_pic_region():

    global _pic_surface

    w = 2 * ( _horiz_res / 3 )  ##  take up right 2/3 of screen
    h = 2 * ( _vert_res / 3 )   ##  take up top 2/3 of screen

    x = _entry_rect.right + 1

    _pic_rect.left   = x
    _pic_rect.top    = 0
    _pic_rect.width  = w
    _pic_rect.height = h

    _pic_surface = pygame.Surface( ( w, h ) )

#############

def calc_info_region():

    global _info_surface

    w = 2 * ( _horiz_res / 3 )  ##  take up right 2/3
    h = _vert_res / 3           ##  take up bottom 1/3

    x = _entry_rect.right + 1
    y = _pic_rect.bottom  + 1

    _info_rect.left   = x
    _info_rect.top    = y
    _info_rect.width  = w
    _info_rect.height = h

    _info_surface = pygame.Surface( ( w, h ) )

#############

def show_launch_dlg( entry ):

    global _launch_dlg_surface

    w = _horiz_res / 2
    h = _vert_res  / 2

    lines = [ 'Launching', entry ]

    _launch_dlg_surface = pygame.Surface( ( w, h ) )

    _launch_dlg_surface.set_alpha( _dlg_alpha )
    _launch_dlg_surface.fill ( _dlg_bg_color )       ##  clear it

    dlgpos = _launch_dlg_surface.get_rect()

    pygame.draw.rect( _launch_dlg_surface, _border_color, dlgpos, 2 )

    count = 0
    y     = dlgpos.top + ( ( dlgpos.bottom - dlgpos.top ) / 2 ) + _item_padding

    for l in lines:

        text    = _info_font.render( l, 1, _dlg_fg_color )
        textpos = text.get_rect()

        new_y   = y - textpos.height 

        if textpos.width > dlgpos.width:
            textpos.width = dlgpos.width

        x = dlgpos.left + ( 2 * _item_padding ) + ( ( dlgpos.width - textpos.width ) / 2 )

        offset = ( _info_font_size + _item_spacing ) * count
        textpos = textpos.move( x, new_y + offset )

        _launch_dlg_surface.blit( text, textpos )

        count += 1

    render_other()
    pygame.display.flip()

#############

def hide_launch_dlg():

    ##  TODO: Is there a way to destroy surfaces, so no mem leak?

    global _launch_dlg_surface
    _launch_dlg_surface = None

#############

def show_no_image():

    lines  = [ 'No image found', '-or-', 'Couldn\'t load image' ]
    size   = _info_font_size * 2
    y      = ( _pic_rect.bottom / 2 ) - _item_spacing - ( size * 1.5 )
    count  = 0
    font   = pygame.font.Font( None, size )

    for l in lines:

        text    = font.render( l, 1, _fg_color )
        textpos = text.get_rect()

        if textpos.width > _pic_rect.width:
            textpos.width = _pic_rect.width

        x = _pic_rect.left + ( 2 * _item_padding ) + ( ( _pic_rect.width - textpos.width ) / 2 )

        offset = ( size + _item_spacing ) * count
        textpos = textpos.move( x, y + offset )

        _screen.blit( text, textpos )

        count += 1

#############

def short_entries( choices ):

    ##  returns the list of choices sorted, and the list of short names

    e = []

    if _in_submenu:

        ##  choices (files) are full paths and we want to sort by basename

        choices.sort( key = os.path.basename )

    else:
        choices.sort()

    for k in choices:
            
        if _in_submenu:

            file_name = short_name( k )

            e.append( file_name )

        else:

            e.append( k )

    return choices, e

#############

def short_name( long_name ):

    ##  showing full path of roms/dsks/zips, remove 
    ##  full path for displaying returning filename
    ##  and extension

    path_ind = long_name.rfind( os.path.sep )
    sn       = long_name[ path_ind + 1 : ]

    return sn

#############

def display_name( menu, long_name ):

    ##  showing full path of roms/dsks/zips, remove 
    ##  extension and full path for displaying

    ext_ind  = long_name.rfind( '.' )
    path_ind = long_name.rfind( os.path.sep )
    dn       = long_name[ path_ind + 1 : ext_ind ]

    sn = short_name( long_name )

    if has_override( menu, sn ):

        ##  specified to override certain fields

        new_name = get_override( menu, sn, kOVERRIDE_NAME_INDEX )

        ##  check for display name field to be overriden

        if new_name != kDEFAULT_SENTINAL:
            dn = new_name
         
    else:

        ##  no override explicitly, see if there is a display subst regex

        if menu.display_regex != None:
            dn = apply_display_regex( dn, menu.display_regex )

    return dn

#############

def full_dir_name( long_name ):

    path_ind = long_name.rfind( os.path.sep )
    d        = long_name[ :path_ind + 1 ]

    return d

#############

def alpha_menu_shift_pressed():

    ret = False

    pygame.event.pump()
    bool_array = pygame.key.get_pressed()

    for each in _alpha_menu_shift_keys:

        if bool_array[ each ]:
            ret = True
            break

    return ret

#############

def get_next_key( cur_key ):

    next_key = chr( ord( cur_key ) + 1 )

    if '0' <= cur_key <= '9':
        next_key = 'A'

    if cur_key == 'Z':
        next_key = 'a'

    if cur_key == 'z':
        next_key = 'z'               # stay put in the z's

    return next_key

#############

def get_prev_key( cur_key ):

    prev_key = chr( ord( cur_key ) - 1 )

    if cur_key == 'A':
        prev_key = '9'

    if cur_key == 'a':
        prev_key = 'Z'

    if cur_key == '0':
        prev_key = '0'               # stay put in the 0's

    return prev_key

#############

def jump_to_next_letter( shortend_entries ):

    ##  sort order is 0-9 -> A-Z -> a-z

    menu, key   = get_cur_sel()
    sn          = display_name( menu, key )

    cur_letter  = sn[ 0 ]
    next_letter = get_next_key( cur_letter )

    ret = jump_to_first( next_letter, shortend_entries )

    return ret

#############

def jump_to_prev_letter( shortend_entries ):

    ##  sort order is 0-9 -> A-Z -> a-z

    menu, key = get_cur_sel()
    sn        = display_name( menu, key )

    cur_letter  = sn[ 0 ]
    prev_letter = get_prev_key( cur_letter )

    ret = jump_to_first( prev_letter, shortend_entries, False )

    return ret

#############

def has_override( menu, sn ):

    for key in menu.overrides.keys():

        m = re.match( key, sn )

        if m:
            return True

    return False

#############

def get_override( menu, sn, override_index ):

    ovr = 'DEFAULT'

    for key in menu.overrides.keys():

        m = re.match( key, sn )

        if m:
            ovr = menu.overrides[ key ][ override_index ]

    return ovr

#############

def apply_display_regex( name, regexes ):

    new_name = name

    if regexes != '':

        rs = string.split( regexes, ';' )

        ##  form is match_pattern:substitution

        for each in rs:

            if each != '':

                fields = string.split( each, ':' )

                m = fields[ 0 ]
                s = fields[ 1 ]

                new_name = re.sub( m, s, new_name ) 

    return new_name

#############

def update_title():

    if _in_submenu:

        menu, key = get_cur_sel()

        pygame.display.set_caption( _main_title + ' < ' + menu.title + ' >' )

    else:
        pygame.display.set_caption( _main_title )

#############

def get_dir( app ):

    path_ind = app.rfind( os.path.sep )

    if path_ind == -1:
        return app

    return app[ : path_ind ]

#############

def is_alpha( key ):

    if K_a <= key <= K_z:
        return True

    return False

#############

def pygame_key_to_ascii( pygame_key ):

    return key_mapping[ pygame_key ]

#############

def dump_menu( menu ):

    if not DEBUG:
        return

    dbg_print( 'Dumping menu...' )
    dbg_print( '' )
    dbg_print( 'title -> '         + menu.title )
    dbg_print( 'has_submenus -> '  + str( menu.has_submenus ) )
    dbg_print( 'app_name -> '      + menu.app_name )
    dbg_print( 'app_args -> '      + menu.app_args )
    dbg_print( 'rom_dirs -> '      + menu.rom_dirs )
    dbg_print( 'rom_exts -> '      + menu.rom_exts )
    dbg_print( 'pic_dirs -> '      + menu.pic_dirs )
    dbg_print( 'pre_cmd -> '       + menu.pre_cmd )
    dbg_print( 'pre_cmd_args -> '  + menu.pre_cmd_args )
    dbg_print( 'post_cmd -> '      + menu.post_cmd )
    dbg_print( 'post_cmd_args -> ' + menu.post_cmd_args )
    dbg_print( 'pic -> '           + menu.pic )
    dbg_print( 'info -> '          + menu.info )
 
    dbg_print( 'overrides list' )

    for each in menu.overrides.keys():
        dbg_print( '\t ' + each  )

        tup = menu.overrides[ each ]

        for t in tup:
            dbg_print( '\t\t ' + t )

    dbg_print( 'filter list' )

    for each in menu.filters:
        dbg_print( '\t ' + each )

    dbg_print( 'ignores list' )

    for each in menu.ignores:
        dbg_print( '\t ' + each )

    dbg_print( '' )

#############

def dbg_print( str ):

    if DEBUG:
        print 'DEBUG: ' + str

###############################################################################

class mouse_info:

    ##  Max number of ticks between clicks to register double mouse click

    kDOUBLECLICK = 400

    def __init__( self, x, y, ticks ):

        self.x     = x
        self.y     = y
        self.ticks = ticks 

    #############

    def double_clicked( self, event ):

        x, y = event.pos

        num_ticks     = pygame.time.get_ticks()
        elapsed_ticks = num_ticks - self.ticks

        if self.x == x and self.y == y:

            dbg_print( 'double_clicked(): Elapsed ticks since last click ' + str( elapsed_ticks ) )

            if elapsed_ticks <= self.kDOUBLECLICK:
                return 1

        self.x     = x
        self.y     = y
        self.ticks = num_ticks

        return 0

###############################################################################

class menu_item:

    def __init__( self, subdir, name ):

        m = __import__( subdir + '.' + name, globals(), locals(), [name] ) 

        self.title          = m.title
        self.has_submenus   = m.has_submenus
        self.app_name       = m.app_name
        self.app_args       = m.app_args
        self.rom_dirs       = m.rom_dirs 
        self.rom_exts       = m.rom_exts 
        self.pic_dirs       = m.pic_dirs
        self.pre_cmd        = m.pre_cmd  
        self.pre_cmd_args   = m.pre_cmd_args
        self.post_cmd       = m.post_cmd 
        self.post_cmd_args  = m.post_cmd_args
        self.pic            = m.pic
        self.info           = m.info

        self.subdir_entries = {}

        ##  optional arguments, if they don't exist, they'll be init'ed to None

        try:
            self.display_regex = m.display_regex

        except:
            self.display_regex = None

        try:
            self.overrides = m.overrides

        except:
            self.overrides = {}

        try:
            self.ignores   = m.ignores

        except:
            self.ignores   = []

        try:
            self.filters   = m.filters

        except:
            self.filters   = []

###############################################################################

if __name__ == '__main__':
    main()
