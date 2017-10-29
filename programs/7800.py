import os, socket

title         = 'Atari 7800'
has_submenus  = True
app_name      = r'g:\rec\emu\multi\mess\mess.exe'
app_args      = r'a7800 -video ddraw -waitvsync -rompath g:\rec\emu\multi\mess\bios -cfg_directory g:\rec\emu\multi\mess\cfg -joystick -cart "%F"'
rom_dirs      = r'g:\rec\emu\atari\7800\media\cart\games'
rom_exts      = 'a78;rom'
pic_dirs      = r'g:\rec\emu\atari\7800\pic\games'
pre_cmd       = r'g:\rec\emu\utils\emu_box_scripts\joykey.bat'
pre_cmd_args  = r'7800.cfg'
post_cmd      = pre_cmd
post_cmd_args = r'menu.cfg'
pic           = r'pics/7800.jpg'
info          = 'Select -> S      Reset -> R;Pause -> P     Exit -> Esc or hold Fire for 3+ seconds'

if socket.gethostname() == 'gpdwin':
    info     = 'Select -> S      Reset -> R;Pause -> P     Exit -> Esc or hold Select for 3+ seconds'
    app_name = r'g:\rec\emu\multi\mess-gpd\mess.exe'
    app_args = r'a7800 -video ddraw -waitvsync -rompath g:\rec\emu\multi\mess-gpd\bios -cfg_directory g:\rec\emu\multi\mess-gpd\cfg -joystick -cart "%F"'

if os.name == 'posix':
    home_dir = os.path.expanduser( '~' )
    pic_dirs = home_dir + r'/rec/emu/atari/7800/pic/games'
    rom_dirs = home_dir + r'/rec/emu/atari/7800/media/cart/games'

## includes light gun and paddle games
ignores       = [
    'asteroids_unencrypted.a78',
    'alien_brigade.a78',
    'barnyard_blaster.a78',
    'crossbow.a78',
    'meltdown.a78',
    'sentinel.a78',
    'crazy_bricks.a78',
    'ms_pac_man-320.a78',
    'ms_pac_man-320-pokey.a78',
    'rampart.a78',			##  not complete prototype
]
