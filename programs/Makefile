SRCDIR = ..

# Example programs to compile, and a list of sources for each.
# To add a new test, put its name on the PROGS list
# and then add a name_SRC line that lists its source files.
PROGS = cat cmp cp echo halt hex-dump rm \
	lineup recursor stack-setup basic-syscall exec-loop exec-simple \
	printf noop doompintos

# The example files should start to work as intended in the following order: 
# Should work once the main-stack is correctly setup (Lab 1)
noop_SRC = noop.c

# Should work once the write() syscall is implemented (Lab 2)
stack-setup_SRC = stack-setup.c

# Should work once the basic system call handler is implemented (Lab 2)
basic-syscall_SRC = basic-syscall.c
printf_SRC = printf.c
cat_SRC = cat.c
cmp_SRC = cmp.c
cp_SRC = cp.c
echo_SRC = echo.c
halt_SRC = halt.c
hex-dump_SRC = hex-dump.c
lineup_SRC = lineup.c
rm_SRC = rm.c

# Should work once exec() is implemented (Lab 4)
exec-loop_SRC = exec-loop.c
exec-simple_SRC = exec-simple.c

# Should work once wait() is implemented (Lab 5)
recursor_SRC = recursor.c
#rrecursor_ng_SRC = recursor_ng.c

# doom
doompintos_SRC = doom/am_map.c doom/d_event.c doom/d_items.c doom/d_iwad.c \
           doom/d_loop.c doom/d_main.c doom/d_mode.c doom/d_net.c \
           doom/doomdef.c doom/doomstat.c doom/dstrings.c doom/f_finale.c \
           doom/f_wipe.c doom/g_game.c doom/hu_lib.c doom/hu_stuff.c \
           doom/info.c doom/m_argv.c doom/m_bbox.c doom/m_cheat.c \
           doom/m_config.c doom/m_controls.c doom/m_fixed.c doom/m_menu.c \
           doom/m_misc.c doom/m_random.c doom/p_ceilng.c doom/p_doors.c \
           doom/p_enemy.c doom/p_floor.c doom/p_inter.c doom/p_lights.c \
           doom/p_map.c doom/p_maputl.c doom/p_mobj.c doom/p_plats.c \
           doom/p_pspr.c doom/p_saveg.c doom/p_setup.c doom/p_sight.c \
           doom/p_spec.c doom/p_switch.c doom/p_telept.c doom/p_tick.c \
           doom/p_user.c doom/r_bsp.c doom/r_data.c doom/r_draw.c \
           doom/r_main.c doom/r_plane.c doom/r_segs.c doom/r_sky.c \
           doom/r_things.c doom/s_sound.c doom/sounds.c doom/st_lib.c \
           doom/st_stuff.c doom/tables.c doom/v_video.c doom/w_wad.c \
           doom/wi_stuff.c doom/z_zone.c \
           doom/i_system.c doom/i_video.c doom/w_file_stdc.c \
           doom/doomgeneric.c doom/doomgeneric_pintos.c

include $(SRCDIR)/Make.config
include $(SRCDIR)/Makefile.userprog

doompintos: LDLIBS += -lgcc
