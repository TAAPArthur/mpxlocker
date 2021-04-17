BIN := mpxlocker
all: $(BIN)

install: $(BIN)
	install -Dt "$(DESTDIR)/usr/bin" $^

$(BIN): $(BIN).o
	${CC} -o $@ $< -lxcb -lxcb-xinput

clean:
	rm -f $(BIN) *.o
