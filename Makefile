BUILD_SUBDIRS = threads userprog vm filesys

all::
	@echo "Run 'make' in subdirectories: $(BUILD_SUBDIRS)."
	@echo "This top-level make has only 'clean' targets."

CLEAN_SUBDIRS = $(BUILD_SUBDIRS) programs 

clean::
	for d in $(CLEAN_SUBDIRS); do $(MAKE) -C $$d $@; done
	rm -f TAGS tags
	rm -f cscope.files cscope.in.out cscope.out cscope.po.out
	rm -f compile_commands.json 

distclean:: clean
	find . -name '*~' -exec rm '{}' \;

TAGS_SUBDIRS = $(BUILD_SUBDIRS) devices lib
TAGS_SOURCES = find $(TAGS_SUBDIRS) -name \*.[chS] -print

TAGS::
	etags --members `$(TAGS_SOURCES)`

tags::
	ctags --fields=+l `$(TAGS_SOURCES)`

cscope.files::
	$(TAGS_SOURCES) > cscope.files

comp_comm::
	bzcat utils/compile_commands.json.bz2 | sed 's#FULL_PATH_HERE#$(shell pwd)#g' > compile_commands.json
	@echo "Please note, you might need to run 'make' in the $(BUILD_SUBDIRS) \
	directories before it starts working correctly"

cscope:: cscope.files
	cscope -b -q -k
