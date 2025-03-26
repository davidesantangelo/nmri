# Makefile for NMRI calculator and tests

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -O2
LDFLAGS = -lm
PREFIX = /usr/local

all: nmri nmri_tests

# Build the main calculator program
nmri: nmri.c
	$(CC) $(CFLAGS) -o nmri nmri.c $(LDFLAGS)

# Build the test program
nmri_tests: nmri_tests.c nmri_for_tests.o
	$(CC) $(CFLAGS) -o nmri_tests nmri_tests.c nmri_for_tests.o $(LDFLAGS)

# Create an object file specifically for testing
nmri_for_tests.o: nmri.c
	$(CC) $(CFLAGS) -c -o nmri_for_tests.o nmri.c -DFOR_TESTING

# Run the tests
test: nmri_tests
	./nmri_tests

# Install the calculator
install: nmri
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 nmri $(DESTDIR)$(PREFIX)/bin

# Uninstall the calculator
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/nmri

# Clean up
clean:
	rm -f nmri nmri_tests *.o

.PHONY: all test clean install uninstall
