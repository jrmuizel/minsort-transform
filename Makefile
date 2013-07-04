SUBDIRS = minsort_transform minsort_rev_transform

.PHONY: all $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

install:
	for i in $(SUBDIRS) ; do \
		make -C $$i install; \
	done

clean:
	for i in $(SUBDIRS) ; do \
		make -C $$i clean; \
	done
