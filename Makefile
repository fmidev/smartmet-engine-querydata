SUBNAME = querydata
SPEC = smartmet-engine-$(SUBNAME)
INCDIR = smartmet/engines/$(SUBNAME)

REQUIRES = gdal jsoncpp configpp fmt

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

enginedir = $(datadir)/smartmet/engines

# Compiler options

DEFINES = -DUNIX -D_REENTRANT

LIBS += -L$(libdir) \
	$(REQUIRED_LIBS) \
	-lsmartmet-timeseries \
	-lsmartmet-spine \
	-lsmartmet-gis \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lboost_regex \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_iostreams \
	-lboost_serialization \
	-lboost_system \
	-lbz2 -lz

# What to install

LIBFILE = $(SUBNAME).so

# Compilation directories

vpath %.cpp $(SUBNAME)
vpath %.h $(SUBNAME)
vpath %.o $(objdir)

# The files to be compiled
SRCS = $(wildcard $(SUBNAME)/*.cpp)
HDRS = $(wildcard $(SUBNAME)/*.h)
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

.PHONY: rpm

# The rules

all: objdir $(LIBFILE)
debug: all
release: all
profile: all

$(LIBFILE): $(SRCS) $(OBJS)
	$(CXX) $(LDFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)
	@echo Checking $(LIBFILE) for unresolved references
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol |\
			grep -Pv ':\ __(?:(?:a|t|ub)san_|sanitizer_)'; \
	then \
		rm -v $(LIBFILE); \
		exit 1; \
	fi

clean:
	rm -f $(LIBFILE) $(OBJS) *~ $(SUBNAME)/*~
	rm -rf obj
	# Not used any more, but make sure that these are removed
	rm -f querydata/QueryDataMessage.pb.cpp querydata/QueryDataMessage.pb.h

format:
	clang-format -i -style=file $(SUBNAME)/*.h $(SUBNAME)/*.cpp examples/*.cpp

install:
	@mkdir -p $(includedir)/$(INCDIR)
	@list='$(HDRS)'; \
	for hdr in $$list; do \
	  echo $(INSTALL_DATA) $$hdr $(includedir)/$(INCDIR)/$$(basename $$hdr); \
	  $(INSTALL_DATA) $$hdr $(includedir)/$(INCDIR)/$$(basename $$hdr); \
	done
	@mkdir -p $(enginedir)
	$(INSTALL_PROG) $(LIBFILE) $(enginedir)/$(LIBFILE)

objdir:
	@mkdir -p $(objdir)

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d, $@) -MT $@ -o $@ $<

obj/Engine.o: CFLAGS += -Wno-deprecated-declarations

test:
	@echo Querydata engine has no automatically runnable tests as for now
	@echo There are some tests under test subdirectory but remain unautomated
	@test "$$CI" = "true" && true || false

-include $(wildcard obj/*.d)
