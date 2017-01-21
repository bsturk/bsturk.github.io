import os, string, socket

mess_args     = r'ti99_4a -video ddraw -joystick -rompath g:\rec\emu\multi\mess\bios -cfg_directory g:\rec\emu\multi\mess\cfg'
xbas_args     = r'-cartridge g:\rec\emu\ti\ti99_4a\media\cart\dev\extended_basic.rpk'
disk_args     = r'-cartridge g:\rec\emu\ti\ti99_4a\media\cart\util\disk_manager_1.rpk'

title         = 'TI 99/4A'
has_submenus  = True
app_name      = r'g:\rec\emu\multi\mess\mess.exe'
app_args      = mess_args + ' -cart1 "%F"'
rom_dirs      = ''
pic_dirs      = ''
rom_exts      = 'rpk;dsk'
pre_cmd       = r'g:\dev\proj\frontend\pygame\joykey.bat'
pre_cmd_args  = 'ti99.cfg'
post_cmd      = r'g:\dev\proj\frontend\pygame\joykey.bat'
post_cmd_args = 'menu.cfg'
pic           = r'pics/ti99.jpg'
info          = 'FCTN key -> Alt   Redo -> FCTN & 8   Back -> FCTN & 9;Exit -> F6 then Esc or hold Fire for 3+ seconds'

rom_root      = r'g:\rec\emu\ti\ti99_4a\media\cart\games'
dsk_root      = r'g:\rec\emu\ti\ti99_4a\media\dsk\games'
pic_root      = r'g:\rec\emu\ti\ti99_4a\pic\games'

if os.name == 'posix':
    home_dir = os.path.expanduser( '~' )
    rom_root = home_dir + r'/rec/emu/ti/ti99_4a/media/cart/games'
    dsk_root = home_dir + r'/rec/emu/ti/ti99_4a/media/dsk/games'
    pic_root = home_dir + r'/rec/emu/ti/ti99_4a/pic/games'

for letter in string.lowercase:
    rom_dirs += rom_root + os.sep + letter + ';'
    rom_dirs += dsk_root + os.sep + letter + ';'
    pic_dirs += pic_root + os.sep + letter + ';'

rom_dirs += rom_root + os.sep + '0_9' + ';'
rom_dirs += dsk_root + os.sep + '0_9'
pic_dirs += pic_root + os.sep + '0_9'

overrides     = { 
        #'.+\.dsk' : ( 'DEFAULT', 'DEFAULT', mess_args + r' -peb:slot7 hfdc -floppydisk1 "%F" ' + xbas_args,  'DEFAULT',  'DEFAULT',   'DEFAULT',  'DEFAULT',  'DEFAULT' ),
        '.+\.dsk' : ( 'DEFAULT', 'DEFAULT', mess_args + r' -peb:slot7 hfdc -floppydisk1 "%F" ' + disk_args,  'DEFAULT',  'DEFAULT',   'DEFAULT',  'DEFAULT',  'DEFAULT' ),
}

filters       = [
    '.*-alt.*.rpk',
    '.*-alt.*.dsk',
    '.*-beta.*.rpk',
]

ignores       = [
    'tunnels_of_doom_quest_or_pennies.dsk',
]
