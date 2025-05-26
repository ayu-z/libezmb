.PHONY: all core others clean install

SUBDIRS := $(shell find . -type f -name Makefile ! -path "./Makefile" ! -path "./core/Makefile" -exec dirname {} \;)

all: core install others

core:
	@echo ">>> Building core ..."
	$(MAKE) -C core

install:
	@echo ">>> Installing core ..."
	$(MAKE) -C core install

others:
	@for dir in $(SUBDIRS); do \
		echo ">>> Building in $$dir ..."; \
		$(MAKE) -C $$dir || exit 1; \
	done

clean:
	@echo ">>> Cleaning core ..."
	$(MAKE) -C core clean || true
	@for dir in $(SUBDIRS); do \
		echo ">>> Cleaning in $$dir ..."; \
		$(MAKE) -C $$dir clean || true; \
	done
