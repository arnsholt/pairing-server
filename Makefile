CXXFLAGS=-g -Wall `pkg-config --cflags protobuf grpc` -I bbpPairings
LDFLAGS=-lpq -lcrypto `pkg-config --libs protobuf grpc++` -LbbpPairings -l:bbpPairings.dll
CC=$(CXX)

SRCS=pairing-server.cpp database.cpp
OBJECTS=pairing-server.o database.o service.pb.o service.grpc.pb.o types.pb.o

.PHONY: build bbpPairings/bbpPairings.dll

build: bbpPairings/bbpPairings.dll pairing-server

pairing-server: $(OBJECTS)
pairing-server.cpp: service.grpc.pb.cc
service.pb.cc: service.proto types.pb.cc

bbpPairings/bbpPairings.dll:
	make -C bbpPairings bbpPairings.dll

%.pb.cc: %.proto
	protoc --cpp_out=. $<

%.grpc.pb.cc: %.proto %.pb.cc
	protoc --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` $<

clean:
	rm -f pairing-server *.o *.pb.*

# Magical code for automatically tracking dependencies of source files. Copied
# in its entirety from
# http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEPDIR:=.d
$(shell mkdir -p $(DEPDIR) > /dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.cc = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

%.o : %.c
%.o : %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

%.o : %.cpp
%.o : %.cpp $(DEPDIR)/%.d
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))
