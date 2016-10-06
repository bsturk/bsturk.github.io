mess_args    = 'ti99_4a -now -nonu'            ##  not a mandatory entry, just useful since it could be re-used a lot

title        = 'Ti-99/4A'
has_submenus = True
app_name     = r'g:\rec\emu\misc\mess\mess.exe'
app_args     = mess_args + ' -cart "%F"'
rom_dirs     = r'g:\rec\emu\ti\ti99_4a\dsk\games\;g:\rec\emu\ti\ti99_4a\dsk\games\0_9;g:\rec\emu\ti\ti99_4a\dsk\games\a;g:\rec\emu\ti\ti99_4a\dsk\games\b;g:\rec\emu\ti\ti99_4a\dsk\games\c;g:\rec\emu\ti\ti99_4a\dsk\games\d;g:\rec\emu\ti\ti99_4a\dsk\games\e;g:\rec\emu\ti\ti99_4a\dsk\games\f;g:\rec\emu\ti\ti99_4a\dsk\games\g;g:\rec\emu\ti\ti99_4a\dsk\games\h;g:\rec\emu\ti\ti99_4a\dsk\games\i;g:\rec\emu\ti\ti99_4a\dsk\games\j;g:\rec\emu\ti\ti99_4a\dsk\games\k;g:\rec\emu\ti\ti99_4a\dsk\games\l;g:\rec\emu\ti\ti99_4a\dsk\games\m;g:\rec\emu\ti\ti99_4a\dsk\games\n;g:\rec\emu\ti\ti99_4a\dsk\games\o;g:\rec\emu\ti\ti99_4a\dsk\games\p;g:\rec\emu\ti\ti99_4a\dsk\games\q;g:\rec\emu\ti\ti99_4a\dsk\games\r;g:\rec\emu\ti\ti99_4a\dsk\games\s;g:\rec\emu\ti\ti99_4a\dsk\games\t;g:\rec\emu\ti\ti99_4a\dsk\games\u;g:\rec\emu\ti\ti99_4a\dsk\games\v;g:\rec\emu\ti\ti99_4a\dsk\games\w;g:\rec\emu\ti\ti99_4a\dsk\games\x;g:\rec\emu\ti\ti99_4a\dsk\games\y;g:\rec\emu\ti\ti99_4a\dsk\games\z'
rom_exts     = 'bin;dsk'
pic_dir      = r'g:\rec\emu\ti\ti99_4a\pic\games'
pre_cmd      = r'g:\rec\emu\misc\utils\joytokey\joykey.bat'
pre_cmd_args = 'ti99.cfg'
post_cmd     = r'g:\rec\emu\misc\utils\joytokey\joykey.bat'
post_cmd_args= 'menu.cfg'
pic          = r'pics\ti99.jpg'
info         = 'Texas Instruments 99/4A'

overrides    = { 
    'parsec_c.bin' : ( 'parsec', 'DEFAULT', mess_args + r' -cart %D\parsec_g.bin -cart %D\parsec_c.bin',  'DEFAULT',  'DEFAULT',   'DEFAULT',  'DEFAULT',  'DEFAULT' ),
}

ignores      = [
    'parsec_g.bin'
]
