INCLUDE= -Isrc -Ithird_party/uthash/src/ -Ithird_party/googletest/googletest/include/
LDPATH = -L third_party/googletest/build/googlemock/gtest
CFLAGS=-std=gnu11 -fpic -pthread -O2 -g -fno-strict-aliasing -fwrapv -Wall -Wextra $(INCLUDE) $(OPTFLAGS) $(LDPATH)
CXXFLAGS=-std=c++11 -fpic -pthread -O2 -g -fno-strict-aliasing -fwrapv -Wall -Wextra $(INCLUDE) $(OPTFLAGS) $(LDPATH)
LIBS=-luuid -lev -lgtest $(OPTLIBS)
LDLIBS=$(LIBS)
PREFIX?=/usr/local

SOURCES=$(wildcard src/**/*.c src/*.c)
OBJECTS=$(patsubst %.c,%.o,$(SOURCES))

TEST_SRC=$(wildcard tests/*_test*.c tests/*_test*.cpp)
TEST_C=$(patsubst %.c,%,$(TEST_SRC))
TESTS=$(patsubst %.cpp,%,$(TEST_C))

TARGET_NAME=kcpev
TARGET=$(CURDIR)/build/lib$(TARGET_NAME).a
SO_TARGET=$(patsubst %.a,%.so,$(TARGET))

# The Target Build
all: $(TARGET) $(SO_TARGET) tests

dev: CFLAGS=-g -Wall -Isrc -Wall -Wextra $(OPTFLAGS)
dev: all

$(TARGET): CFLAGS += -fPIC
$(TARGET): build $(OBJECTS)
	ar rcs $@ $(OBJECTS)
	ranlib $@

$(SO_TARGET): $(TARGET) $(OBJECTS)
	$(CC) -shared -o $@ -luuid -lev $(OBJECTS)

build:
	@mkdir -p build
	@mkdir -p bin

# The Unit Tests
.PHONY: tests
tests: LDLIBS = $(SO_TARGET) $(LIBS)
# tests: CFLAGS += -pg
tests: $(TESTS)

valgrind:
	VALGRIND="valgrind --log-file=/tmp/valgrind-%p.log" $(MAKE)

# The Cleaner
clean:
	rm -rf build $(OBJECTS) $(TESTS)
	rm -rf bin
	rm -f tests/tests.log
	find . -name "*.gc*" -exec rm {} \;
	rm -rf `find . -name "*.dSYM" -print`

# The Install
install: all
	install -d $(DESTDIR)/$(PREFIX)/lib/
	install $(TARGET) $(DESTDIR)/$(PREFIX)/lib/

# The Checker
BADFUNCS='[^_.>a-zA-Z0-9](str(n?cpy|n?cat|xfrm|n?dup|str|pbrk|tok|_)|stpn?cpy|a?sn?printf|byte_)'
check:
	@echo Files with potentially dangerous functions.
	@egrep $(BADFUNCS) $(SOURCES) || true

