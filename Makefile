PROGNAME = fseventwait

LIBS += -framework CoreServices

$(PROGNAME): $(PROGNAME).o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	$(RM) *.o $(PROGNAME)
