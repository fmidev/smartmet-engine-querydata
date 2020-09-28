SUBNAME = querydata
SPEC = smartmet-engine-$(SUBNAME)
INCDIR = smartmet/engines/$(SUBNAME)

enginedir = $(datadir)/smartmet/engines
include common.mk

# Compiler options

DEFINES = -DUNIX -DWGS84 -D_REENTRANT

-include $(HOME)/.smartmet.mk
GCC_DIAG_COLOR ?= always
CXX_STD ?= c++11

# Special external dependencies

ifneq "$(wildcard /usr/include/boost169)" ""
  INCLUDES += -isystem /usr/include/boost169
  LIBS += -L/usr/lib64/boost169
endif

ifneq "$(wildcard /usr/gdal30/include)" ""
  INCLUDES += -isystem /usr/gdal30/include
  LIBS += -L/usr/gdal30/lib
else
  INCLUDES += -isystem /usr/include/gdal
endif

ifeq ($(USE_CLANG), yes)

 FLAGS = \
	-std=$(CXX_STD) -fPIC -MD \
	-Wno-c++98-compat \
	-Wno-padded \
	-Wno-missing-prototypes \
	-Wno-float-equal \
	-Wno-sign-conversion \
	-Wno-missing-variable-declarations \
	-Wno-global-constructors \
	-Wno-shorten-64-to-32 \
	-Wno-unused-macros \
	-Wno-documentation-unknown-command

 INCLUDES += \
	-I$(includedir)/smartmet \
	`pkg-config --cflags jsoncpp`

else

 FLAGS = -std=$(CXX_STD) -fPIC -MD -Wall -W -Wno-unused-parameter -fno-omit-frame-pointer -fdiagnostics-color=$(GCC_DIAG_COLOR)

 FLAGS_DEBUG = \
	-Wcast-align \
	-Winline \
	-Wno-multichar \
	-Wno-pmf-conversions \
	-Wpointer-arith \
	-Wcast-qual \
	-Wwrite-strings \
	-Wsign-promo \
	-Wno-deprecated-declarations

 FLAGS_RELEASE = -Wuninitialized

 INCLUDES += \
	-I$(includedir)/smartmet \
	`pkg-config --cflags jsoncpp`

endif

ifeq ($(TSAN), yes)
  FLAGS += -fsanitize=thread
endif
ifeq ($(ASAN), yes)
  FLAGS += -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=undefined -fsanitize-address-use-after-scope
endif

# Compile options in detault, debug and profile modes

CFLAGS_RELEASE = $(DEFINES) $(FLAGS) $(FLAGS_RELEASE) -DNDEBUG -O2 -g
CFLAGS_DEBUG   = $(DEFINES) $(FLAGS) $(FLAGS_DEBUG)   -Werror  -O0 -g

ifneq (,$(findstring debug,$(MAKECMDGOALS)))
  override CFLAGS += $(CFLAGS_DEBUG)
else
  override CFLAGS += $(CFLAGS_RELEASE)
endif

LIBS += -L$(libdir) \
	-lsmartmet-spine \
	-lsmartmet-gis \
	-lsmartmet-macgyver \
	-lsmartmet-newbase \
	-lgdal \
	`pkg-config --libs jsoncpp` \
	-lboost_date_time \
	-lboost_regex \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_iostreams \
	-lboost_serialization \
	-lboost_system \
	-lfmt \
	-lprotobuf \
	-lbz2 -lz

# What to install

LIBFILE = $(SUBNAME).so

# How to install

INSTALL_PROG = install -p -m 775
INSTALL_DATA = install -p -m 664

# Compilation directories

vpath %.cpp $(SUBNAME)
vpath %.h $(SUBNAME)
vpath %.o $(objdir)

# The files to be compiled
PB_SRCS = $(wildcard *.proto)
COMPILED_PB_SRCS = $(patsubst %.proto, $(SUBNAME)/%.pb.cpp, $(PB_SRCS))
COMPILED_PB_HDRS = $(patsubst %.proto, $(SUBNAME)/%.pb.h, $(PB_SRCS))

SRCS = $(filter-out %.pb.cpp, $(wildcard $(SUBNAME)/*.cpp)) $(COMPILED_PB_SRCS)
HDRS = $(filter-out %.pb.h, $(wildcard $(SUBNAME)/*.h)) $(COMPILED_PB_HDRS)
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

.PHONY: rpm

# The rules

all: objdir $(LIBFILE)
debug: all
release: all
profile: all

$(LIBFILE): $(SRCS) $(OBJS)
	$(CC) $(LDFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)

clean:
	rm -f $(LIBFILE) $(OBJS) *~ $(SUBNAME)/*~
	rm -f $(SUBNAME)/QueryDataMessage.pb.cpp $(SUBNAME)/QueryDataMessage.pb.h
	rm -rf obj

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

rpm: clean protoc $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -ta $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

protoc: $(COMPILED_PB_SRCS)

querydata/%.pb.cpp: %.proto; mkdir -p tmp
	protoc --cpp_out=tmp QueryDataMessage.proto
	mv tmp/QueryDataMessage.pb.h $(SUBNAME)/
	mv tmp/QueryDataMessage.pb.cc $(SUBNAME)/QueryDataMessage.pb.cpp
	rm -rf tmp 

test:
	@echo Querydata engine has no automatically runnable tests as for now
	@echo There are some tests under test subdirectory but remain unautomated
	@test "$$CI" = "true" && true || false

-include $(wildcard obj/*.d)
