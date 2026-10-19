CC ?= cc
AR ?= ar
INSTALL ?= install
PREFIX ?= /usr/local
DESTDIR ?=
PKG_CONFIG ?= pkg-config
CLANG_FORMAT ?= clang-format

BUILD_DIR := build
LIB_DIR := $(BUILD_DIR)/lib
TEST_DIR := $(BUILD_DIR)/tests
EXAMPLE_DIR := $(BUILD_DIR)/examples
INCLUDE_DIR := include
LIB_NAME := oopdbmsc
LIB_STATIC := $(LIB_DIR)/lib$(LIB_NAME).a
LIB_SONAME := lib$(LIB_NAME).so.0
LIB_SHARED := $(LIB_DIR)/$(LIB_SONAME)
LIB_LINK := $(LIB_DIR)/lib$(LIB_NAME).so

WARNINGS := -Wall -Wextra -Wpedantic
CFLAGS ?= -O2
CPPFLAGS := -D_POSIX_C_SOURCE=200809L -I$(INCLUDE_DIR)
CFLAGS += -std=c99 $(WARNINGS) -fPIC -fvisibility=hidden
LDFLAGS ?=
ENGINE_CFLAGS = $(shell $(PKG_CONFIG) --cflags oopdbms-engine 2>/dev/null)
ENGINE_LIBS = $(shell $(PKG_CONFIG) --libs oopdbms-engine 2>/dev/null)
ENGINE_LIBDIR = $(shell $(PKG_CONFIG) --variable=libdir oopdbms-engine 2>/dev/null)
ENGINE_RPATH = -Wl,-rpath,$(ENGINE_LIBDIR)

SOURCES := src/oopdbms.c
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
FORMAT_FILES := $(shell find src tests examples include -type f \
		\( -name '*.c' -o -name '*.h' \))
STATIC_TEST := $(TEST_DIR)/library-static-test
SHARED_TEST := $(TEST_DIR)/library-shared-test
QUICK_START := $(EXAMPLE_DIR)/quick_start
QUICK_START_DB ?= $(BUILD_DIR)/quick-start.oopdb

.PHONY: all check-engine clean format format-check test quick_start install \
	uninstall

all: check-engine $(LIB_STATIC) $(LIB_SHARED) $(LIB_LINK)

check-engine:
	@$(PKG_CONFIG) --atleast-version=0.2.0 oopdbms-engine || { \
		echo "oopdbms-engine >= 0.2.0 is required; install OOPDBMS with 'make install' first" >&2; \
		exit 1; \
	}

format:
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

$(LIB_STATIC): $(OBJECTS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(LIB_SHARED): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) -shared -Wl,-soname,$(LIB_SONAME) $(LDFLAGS) $(ENGINE_RPATH) \
		-o $@ $^ $(ENGINE_LIBS)

$(LIB_LINK): $(LIB_SHARED)
	ln -sf $(LIB_SONAME) $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ENGINE_CFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(STATIC_TEST): tests/library_test.c $(LIB_STATIC)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ENGINE_CFLAGS) $(CFLAGS) $< $(LIB_STATIC) \
		$(LDFLAGS) $(ENGINE_RPATH) $(ENGINE_LIBS) -o $@

$(SHARED_TEST): tests/library_test.c $(LIB_LINK)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ENGINE_CFLAGS) $(CFLAGS) $< -L$(LIB_DIR) -l$(LIB_NAME) \
		-Wl,-rpath,'$$ORIGIN/../lib' $(LDFLAGS) -o $@

test: check-engine $(STATIC_TEST) $(SHARED_TEST)
	$(STATIC_TEST)
	$(SHARED_TEST)

$(QUICK_START): examples/main.c $(LIB_STATIC)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ENGINE_CFLAGS) $(CFLAGS) $< $(LIB_STATIC) \
		$(LDFLAGS) $(ENGINE_RPATH) $(ENGINE_LIBS) -o $@

quick_start: check-engine $(QUICK_START)
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
