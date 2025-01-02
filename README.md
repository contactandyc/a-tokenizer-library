# Overview of A Template Library


## Building

### Build and Install
```bash
mkdir -p build
cd build
cmake ..
make
sudo make install
```

### Uninstall
```bash

```

### Build Tests and Test Coverage
```bash
mkdir -p build
cd build
cmake BUILD_TESTING=ON ENABLE_CODE_COVERAGE=ON ..
make
ctest
make coverage
```

### Sample usage

See tests/src/parse.c and tests/src/parse_expression.c for sample usage.

Build tests
```bash
cd build
cmake BUILD_TESTING=ON ..
make
cd tests
```

Test an expression
```bash
$ ./parse_expression '(a OR b c d OR d) not "e g"'
not
	"
		e
		g
	(
		or
			a
			(
				b
				c
				d
			d
```

Test parsing this README file
```bash
$ ./parse ../../README.md
Overview
of
A
Template
Library
Building
Build
and
Install
```
