PROG = $(patsubst %.cpp,%,$(wildcard *Test.cpp))

REQUIRES = configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

CFLAGS = -pipe -DUNIX -O0 -g $(FLAGS)

INCLUDES += \
	-I../querydata \

LIBS += \
	../querydata.so \
	-lsmartmet-spine \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lsmartmet-gis \
	$(CONFIGPP_LIBS) \
	-lboost_thread \
	-lboost_regex \
	-lboost_iostreams \
	-lboost_atomic \
	-lbz2 -lz -lpthread -ldl

all: $(PROG)
clean:
	rm -f $(PROG) *~

test: $(PROG)
	@echo Running tests:
	@for prog in $(PROG); do \
	./$$prog; \
	done

$(PROG) : % : %.cpp ../querydata.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
