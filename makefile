lint:
	@if [ -z "$(file)" ]; then \
		echo "Error: Please specify the file to lint using 'make lint file=<sourcefile.c>'"; \
		exit 1; \
	else \
		gcc -Wall -Wextra -pedantic -std=c99 -fsyntax-only "$(file)"; \
	fi