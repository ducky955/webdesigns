# Renders the OLED layouts against a virtual panel and checks they fit.
CXXFLAGS = -std=gnu++17 -O1 -g -Wall -Wextra -DUSE_OLED=1 -I. -I../sim -I../../src
SRC = screen_test.cpp fake_panel.cpp ../../src/display.cpp

ARDUINOJSON = ../sim/ArduinoJson.h
ARDUINOJSON_URL = https://github.com/bblanchon/ArduinoJson/releases/download/v7.1.0/ArduinoJson-v7.1.0.h

screen_test: $(ARDUINOJSON) $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

$(ARDUINOJSON):
	@echo "fetching ArduinoJson (one time)..."
	@curl -sSL -o $@ $(ARDUINOJSON_URL)

.PHONY: run clean
run: screen_test
	./screen_test

clean:
	rm -f screen_test
