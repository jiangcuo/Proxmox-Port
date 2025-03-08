BIN_TO_HEX:= bin/bin-to-hex

$(BIN_TO_HEX): $(srcdir)/util/bin-to-hex.c
	@$(MKDIR) -p $(@D)
	$(LINK.o) $(CFLAGS) -o $@ $^

$(BIN_TO_HEX): CC=$(BUILD_CC)
$(BIN_TO_HEX): CFLAGS=$(BUILD_CFLAGS)
$(BIN_TO_HEX): LDFLAGS=

dist += util/Makefile util/bin-to-hex.c
clean += util/bin-to-hex.o $(BIN_TO_HEX)
