# Overview of A Template Library

## Setting up repo

```bash
git clone https://github.com/contactandyc/a-template-library.git your-new-library
./bin/convert-repo "Your New Library" 3.2.1 2024-12-30 "Andy Curtis" "contactandyc@gmail.com" "linkedin.com/in/andycurtis"
```

Set remote origin and push
```bash
git remote add origin https://github.com/contactandyc/your-new-library.git
git push -u origin main
```

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

