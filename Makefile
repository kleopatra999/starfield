# if you want to export to a QuickTime movie, uncomment the following
# line
EXPORT_QUICKTIME = YES

# if you compile on MacOS X, uncomment the following line
MACOS_LDFLAGS = -lmx

CC = gcc

ifeq ($(EXPORT_QUICKTIME),YES)
EXPORT_CFLAGS = -DQUICKTIME
EXPORT_LDFLAGS = -framework QuickTime -framework CoreFoundation -framework CoreServices -framework ApplicationServices
else
EXPORT_CFLAGS =
EXPORT_LDFLAGS =
endif

LDOPTS = -L/sw/lib -lpng $(MACOS_LDFLAGS) $(EXPORT_LDFLAGS)
CCOPTS = -I/sw/include -DRWIMG_PNG -g -Wall -O2 $(EXPORT_CFLAGS)

OBJS = bitmap.o zoom.o rwpng.o readimage.o writeimage.o starfield.o

starfield : $(OBJS)
	$(CC) $(LDOPTS) -o starfield $(OBJS)

%.o : %.c
	$(CC) $(CCOPTS) -c $<

clean :
	rm -f $(OBJS) starfield *~
