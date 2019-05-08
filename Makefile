# Z-Box Makefile
INCLUDES=-I include -I zlib -DENABLE_ZLIB
INDENT_FLAGS=-br -ce -i4 -bl -bli0 -bls -c4 -cdw -ci4 -cs -nbfda -l100 -lp -prs -nlp -nut -nbfde -npsl -nss

OBJS = \
	release/main.o \
	release/unpack.o \
	release/zstream.o \
	release/pack.o \
	release/stream.o \
	release/scan.o \
	release/crc32b.o \
	release/util.o \
	release/inffast.o \
	release/deflate.o \
	release/inftrees.o \
	release/trees.o \
	release/inflate.o \
	release/adler32.o \
	release/infback.o \
	release/crc32.o \
	release/zutil.o

all: host

internal: prepare zlib_rule
	@echo "  CC    src/main.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/main.c -o release/main.o
	@echo "  CC    src/unpack.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/unpack.c -o release/unpack.o
	@echo "  CC    src/zstream.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/zstream.c -o release/zstream.o
	@echo "  CC    src/pack.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/pack.c -o release/pack.o
	@echo "  CC    src/stream.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/stream.c -o release/stream.o
	@echo "  CC    src/scan.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/scan.c -o release/scan.o
	@echo "  CC    src/crc32b.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/crc32b.c -o release/crc32b.o
	@echo "  CC    src/util.c"
	@$(CC) $(CFLAGS) $(INCLUDES) src/util.c -o release/util.o
	@echo "  LD    release/zbox"
	@$(LD) -o release/zbox $(OBJS) $(LDFLAGS)

zlib_rule:
	@echo "  CC    zlib/inffast.c"
	@$(CC) $(CFLAGS) $(INCLUDES) zlib/inffast.c -o release/inffast.o -Wno-implicit-fallthrough -DZ_SOLO
	@echo "  CC    zlib/deflate.c"
	@$(CC) $(CFLAGS) $(INCLUDES) zlib/deflate.c -o release/deflate.o -Wno-implicit-fallthrough -DZ_SOLO
	@echo "  CC    zlib/inftrees.c"
	@$(CC) $(CFLAGS) $(INCLUDES) zlib/inftrees.c -o release/inftrees.o -Wno-implicit-fallthrough -DZ_SOLO
	@echo "  CC    zlib/trees.c"
	@$(CC) $(CFLAGS) $(INCLUDES) zlib/trees.c -o release/trees.o -Wno-implicit-fallthrough -DZ_SOLO
	@echo "  CC    zlib/inflate.c"
	@$(CC) $(CFLAGS) $(INCLUDES) zlib/inflate.c -o release/inflate.o -Wno-implicit-fallthrough -DZ_SOLO
	@echo "  CC    zlib/adler32.c"
	@$(CC) $(CFLAGS) $(INCLUDES) zlib/adler32.c -o release/adler32.o -Wno-implicit-fallthrough -DZ_SOLO
	@echo "  CC    zlib/infback.c"
	@$(CC) $(CFLAGS) $(INCLUDES) zlib/infback.c -o release/infback.o -Wno-implicit-fallthrough -DZ_SOLO
	@echo "  CC    zlib/crc32.c"
	@$(CC) $(CFLAGS) $(INCLUDES) zlib/crc32.c -o release/crc32.o -Wno-implicit-fallthrough -DZ_SOLO
	@echo "  CC    zlib/zutil.c"
	@$(CC) $(CFLAGS) $(INCLUDES) zlib/zutil.c -o release/zutil.o -Wno-implicit-fallthrough -DZ_SOLO

prepare:
	@mkdir -p release

host:
	@make internal \
		CC=gcc \
		LD=gcc \
		CFLAGS='-c -Wall -Wextra -O3 -ffunction-sections -fdata-sections -Wstrict-prototypes' \
		LDFLAGS='-Wl,--gc-sections -Wl,--relax'

host_eo:
	@make internal \
		CC=gcc \
		LD=gcc \
		CFLAGS='-c -Wall -Wextra -O3 -ffunction-sections -fdata-sections -Wstrict-prototypes -DEXTRACT_ONLY' \
		LDFLAGS='-Wl,--gc-sections -Wl,--relax'

win32:
	@make internal \
		CC=i686-w64-mingw32-gcc \
		LD=i686-w64-mingw32-gcc \
		CFLAGS='-c -Wall -Wextra -O3 -ffunction-sections -fdata-sections -DWIN32_BUILD' \
		LDFLAGS='-s -Wl,--gc-sections -Wl,--relax -lws2_32'

win64:
	@make internal \
		CC=x86_64-w64-mingw32-gcc \
		LD=x86_64-w64-mingw32-gcc \
		CFLAGS='-c -Wall -Wextra -O3 -ffunction-sections -fdata-sections -DWIN32_BUILD' \
		LDFLAGS='-s -Wl,--gc-sections -Wl,--relax -lws2_32'

install:
	@cp -v release/zbox /usr/bin/zbox

uninstall:
	@rm -fv /usr/bin/zbox

indent:
	@indent $(INDENT_FLAGS) ./*/*.h
	@indent $(INDENT_FLAGS) ./*/*.c
	@rm -rf ./*/*~

clean:
	@echo "  CLEAN ."
	@rm -rf release

analysis:
	@scan-build make
	@cppcheck --force include/*.h
	@cppcheck --force src/*.c

gendoc:
	@doxygen aux/doxygen.conf
