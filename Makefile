# AcadERP-Lite - top-level Makefile.  The real build lives in src/Makefile
# (it produces src/acaderp, which is copied to ./acaderp).
#
#   make          build
#   make demo     build, run the worked sample from the brief and check it
#   make test     build and run the whole test suite
#   make run      build and start the program (type commands, Ctrl-D to finish)
#   make clean    remove build products

all:
	$(MAKE) -C src
	cp src/acaderp acaderp

demo: all
	@echo "===== demo/sample.in (commands) ====="
	@cat -n demo/sample.in
	@echo
	@echo "===== program output ====="
	@./acaderp < demo/sample.in | tee demo/actual.out
	@echo
	@if diff -q demo/actual.out demo/sample.out > /dev/null; then \
	    echo "===== DEMO OK: output matches demo/sample.out ====="; \
	else \
	    echo "===== DEMO FAILED: differs from demo/sample.out ====="; diff demo/actual.out demo/sample.out; \
	fi
	@rm -f demo/actual.out

test: all
	./run_tests.sh

run: all
	./acaderp

clean:
	$(MAKE) -C src clean
	rm -f acaderp demo/actual.out

.PHONY: all demo test run clean
