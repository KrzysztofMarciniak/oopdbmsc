CC ?= cc
AR ?= ar
INSTALL ?= install
PREFIX ?= /usr/local
DESTDIR ?=

BUILD_DIR := build
LIB_DIR := $(BUILD_DIR)/lib
TEST_DIR := $(BUILD_DIR)/tests
EXAMPLE_DIR := $(BUILD_DIR)/examples
INCLUDE_DIR := include
VENDOR_DIR := vendor/oopdbms/src
LIB_NAME := oopdbmsc
LIB_STATIC := $(LIB_DIR)/lib$(LIB_NAME).a
LIB_SONAME := lib$(LIB_NAME).so.0
LIB_SHARED := $(LIB_DIR)/$(LIB_SONAME)
LIB_LINK := $(LIB_DIR)/lib$(LIB_NAME).so

WARNINGS := -Wall -Wextra -Wpedantic
CFLAGS ?= -O2
CPPFLAGS := -D_POSIX_C_SOURCE=200809L -I$(INCLUDE_DIR) -I$(VENDOR_DIR)
CFLAGS += -std=c99 $(WARNINGS) -fPIC -fvisibility=hidden
LDFLAGS ?=

VENDOR_SOURCES := $(shell find $(VENDOR_DIR) -type f -name '*.c' \
	-not -path '$(VENDOR_DIR)/tests/*' -not -path '$(VENDOR_DIR)/main.c')
SOURCES := src/oopdbms.c $(VENDOR_SOURCES)
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
STATIC_TEST := $(TEST_DIR)/library-static-test
SHARED_TEST := $(TEST_DIR)/library-shared-test
QUICK_START := $(EXAMPLE_DIR)/quick_start
QUICK_START_DB ?= $(BUILD_DIR)/quick-start.oopdb

.PHONY: all clean test quick_start install uninstall

all: $(LIB_STATIC) $(LIB_SHARED) $(LIB_LINK)

$(LIB_STATIC): $(OBJECTS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(LIB_SHARED): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) -shared -Wl,-soname,$(LIB_SONAME) $(LDFLAGS) -o $@ $^

$(LIB_LINK): $(LIB_SHARED)
	ln -sf $(LIB_SONAME) $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(STATIC_TEST): tests/library_test.c $(LIB_STATIC)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB_STATIC) $(LDFLAGS) -o $@

$(SHARED_TEST): tests/library_test.c $(LIB_LINK)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -L$(LIB_DIR) -l$(LIB_NAME) \
		-Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) -o $@

test: $(STATIC_TEST) $(SHARED_TEST)
	$(STATIC_TEST)
	$(SHARED_TEST)

$(QUICK_START): examples/main.c $(LIB_STATIC)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB_STATIC) $(LDFLAGS) -o $@

quick_start: $(QUICK_START)
	@mkdir -p "$(dir $(QUICK_START_DB))"
	rm -f "$(QUICK_START_DB)"
	$(QUICK_START) "$(QUICK_START_DB)"

install: all
	$(INSTALL) -d "$(DESTDIR)$(PREFIX)/include/oopdbms" \
		"$(DESTDIR)$(PREFIX)/lib"
	$(INSTALL) -m 644 $(INCLUDE_DIR)/oopdbms/oopdbms.h \
		"$(DESTDIR)$(PREFIX)/include/oopdbms/oopdbms.h"
	$(INSTALL) -m 644 $(LIB_STATIC) "$(DESTDIR)$(PREFIX)/lib/$(notdir $(LIB_STATIC))"
	$(INSTALL) -m 755 $(LIB_SHARED) "$(DESTDIR)$(PREFIX)/lib/$(LIB_SONAME)"
	ln -sf $(LIB_SONAME) "$(DESTDIR)$(PREFIX)/lib/$(notdir $(LIB_LINK))"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/include/oopdbms/oopdbms.h" \
		"$(DESTDIR)$(PREFIX)/lib/$(notdir $(LIB_STATIC))" \
		"$(DESTDIR)$(PREFIX)/lib/$(LIB_SONAME)" \
		"$(DESTDIR)$(PREFIX)/lib/$(notdir $(LIB_LINK))"

clean:
	rm -rf $(BUILD_DIR)

-include $(OBJECTS:.o=.d)
