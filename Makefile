SUBNAME = querydata
SPEC = smartmet-engine-$(SUBNAME)
INCDIR = smartmet/engines/$(SUBNAME)

REQUIRES = gdal jsoncpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

enginedir = $(datadir)/smartmet/engines

# Compiler options

DEFINES = -DUNIX -D_REENTRANT

LIBS += -L$(libdir) \
	$(REQUIRED_LIBS) \
	-lsmartmet-spine \
	-lsmartmet-gis \
	-lsmartmet-macgyver \
	-lsmartmet-newbase \
	-lboost_date_time \
	-lboost_regex \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_iostreams \
	-lboost_serialization \
	-lboost_system \
	-lprotobuf \
	-lbz2 -lz

# What to install

LIBFILE = $(SUBNAME).so

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
	$(CXX) $(LDFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)
	@echo Checking $(LIBFILE) for unresolved references
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol ; \
		then rm -v $(LIBFILE); \
		exit 1; \
	fi

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
	rpmbuild -tb $(SPEC).tar.gz
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
