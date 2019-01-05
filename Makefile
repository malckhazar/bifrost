CC 	= gcc
AR	= ar
RANLIB  = ranlib
STRIP	= strip

TARGET  = bifrost

LIBRARIES = gio-2.0 dbus-1

CFLAGS	= -Wall -Wextra -fPIC -O0 -g -pthread `pkg-config --cflags $(LIBRARIES)`
LDFLAGS	= -pthread
LIBS = `pkg-config --libs $(LIBRARIES)`

SOURCES = $(shell ls ./*.c)
SOURCES += $(shell ls ipc/*.c)
OBJECTS = $(SOURCES:.c=.o)

#SOURCES_TEST = test/dn-ipc_test.c
#OBJECTS_TEST = $(SOURCES_TEST:.c=.o)

all: $(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS) $(SOURCES:.c=.d) core
	
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(SOURCES:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MMD -MP $< >$@

#$(TARGET).a: $(OBJECTS)
#	$(AR) -rcs $(TARGET).a $(OBJECTS)
#	$(RANLIB) $(TARGET).a

#$(TARGET).so: $(OBJECTS)
#	$(CC) -shared $(LDFLAGS) $(LIBS) -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

