# End-to-end simulation: the real firmware sources, a fake pool, and an
# independent Python check of every share it submits.
#
#   make run
#
# ArduinoJson is fetched once (the firmware needs it to parse stratum).
CXXFLAGS = -std=gnu++17 -O2 -Wall -Wextra -I. -I../../src
LDFLAGS  = -pthread
SRC = sim_main.cpp sim_support.cpp ../../src/stratum.cpp ../../src/miner.cpp \
      ../../src/sha256d.cpp ../../src/btc_util.cpp

ARDUINOJSON_URL = https://github.com/bblanchon/ArduinoJson/releases/download/v7.1.0/ArduinoJson-v7.1.0.h

sim: ArduinoJson.h $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDFLAGS)

ArduinoJson.h:
	@echo "fetching ArduinoJson (one time)..."
	@curl -sSL -o $@ $(ARDUINOJSON_URL)

.PHONY: run clean
run: sim
	./sim
	@echo
	python3 verify_sim.py

clean:
	rm -f sim sim_result.json
