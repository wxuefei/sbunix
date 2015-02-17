CC=gcc
AS=as
CFLAGS=-O1 -std=c99 -D__thread= -Wall -Werror -nostdinc -Iinclude -msoft-float -mno-sse -mno-red-zone -fno-builtin -fPIC -march=amdfam10 -g3 -fno-stack-protector
LD=ld
LDLAGS=-nostdlib
AR=ar

ROOTFS=rootfs
ROOTBIN=$(ROOTFS)/bin
ROOTLIB=$(ROOTFS)/lib

BIN_SRCS:=$(wildcard bin/*/*.c)
LIBC_SRCS:=$(shell find libc/ -type f -name *.[cs])
LIBC_OBJS_1:=$(LIBC_SRCS:%.c=obj/%.o)
BINS:=$(addprefix $(ROOTFS)/,$(wildcard bin/*))

.PHONY: all binary

all: $(BINS)

$(ROOTLIB)/libc.a: $(LIBC_OBJS_1:%.s=obj/%.o)
	$(AR) rcs $@ $^

$(ROOTLIB)/crt1.o: obj/crt/crt1.o
	cp $^ $@

$(ROOTLIB)/crtinit.o: obj/crt/crtinit.o
	cp $^ $@

$(BINS): $(ROOTLIB)/crt1.o $(ROOTLIB)/crtinit.o $(ROOTLIB)/libc.a $(shell find bin/ -type f -name *.c) $(wildcard include/*.h include/*/*.h)
	@$(MAKE) --no-print-directory BIN=$@ binary

binary: $(patsubst %.c,obj/%.o,$(wildcard $(BIN:rootfs/%=%)/*.c))
	$(LD) $(LDLAGS) -o $(BIN) $(ROOTLIB)/crt1.o $(ROOTLIB)/crtinit.o $^ $(ROOTLIB)/libc.a

obj/%.o: %.c $(wildcard include/*.h include/*/*.h)
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

obj/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) -c -o $@ $<

.PHONY: submit clean

SUBMITTO:=~mferdman/cse506-submit/

submit: clean
	tar -czvf $(USER).tgz --exclude=.empty --exclude=.*.sw? --exclude=*~ LICENSE Makefile bin crt libc include $(ROOTFS)
	@gpg --quiet --import cse506-pubkey.txt
	gpg --yes --encrypt --recipient 'CSE506' $(USER).tgz
	rm -fv $(SUBMITTO)$(USER)=*.tgz.gpg
	cp -v $(USER).tgz.gpg $(SUBMITTO)$(USER)=`date +%F=%T`.tgz.gpg

clean:
	find $(ROOTLIB) $(ROOTBIN) -type f ! -name .empty -print -delete
	rm -rfv obj kernel newfs.506 $(ROOTBOOT)/kernel/kernel
