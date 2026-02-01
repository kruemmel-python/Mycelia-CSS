# executable flags for the test harness
CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -shared -fPIC
CXX_EXEFLAGS = -std=c++17 -O3 -Wall

# OS-spezifische Einstellungen
ifeq ($(OS),Windows_NT)
    TARGET = i18n_engine.dll
    CLEAN = del $(TARGET) 2>nul
    PYTHON = python
else
    TARGET = libi18n_engine.so
    CLEAN = rm -f $(TARGET)
    PYTHON = python3
endif

SRC = i18n_engine.cpp i18n_api.cpp

all: $(TARGET) qa

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) -static-libgcc -static-libstdc++

# Automatischer QA-Check nach dem Build
qa:
	@echo "Running Quality Assurance..."
	$(PYTHON) i18n_qa.py

test_app: main.cpp $(TARGET)
	$(CXX) $(CXX_EXEFLAGS) main.cpp -L. -li18n_engine -I. -o mycelia_test.exe

run: test_app
	@echo "Starte Mycelia CSS Test..."
	./mycelia_test.exe

clean:
	$(CLEAN)
