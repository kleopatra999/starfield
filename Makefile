CC = gcc

LDOPTS = -L/sw/lib -ljpeg -lpng -lmx -framework QuickTime -framework CoreFoundation -framework CoreServices -framework ApplicationServices
CCOPTS = -I/sw/include -DRWIMG_JPEG -DRWIMG_PNG -g -Wall -O2

OBJS = bitmap.o zoom.o rwpng.o rwjpeg.o readimage.o writeimage.o starfield.o

starfield : $(OBJS)
	$(CC) $(LDOPTS) -o starfield $(OBJS)

%.o : %.c
	$(CC) $(CCOPTS) -c $<

clean :
	rm -f $(OBJS) starfield *~