# Host build of the pure-C++ parts, so the mining math can be verified
# without flashing anything.
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra

test_host: test_host.cpp ../src/sha256d.cpp ../src/btc_util.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

.PHONY: run clean
run: test_host
	./test_host

clean:
	rm -f test_host
