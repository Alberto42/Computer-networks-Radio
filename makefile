all: sikradio-sender sikradio-receiver

sikradio-sender: sikradio-sender.cpp err.h err.cpp sikradio-sender.h common.cpp common.h
	g++ -std=c++17 -Wall -O2  sikradio-sender.cpp err.h err.cpp sikradio-sender.h common.cpp common.h -o sikradio-sender -lboost_program_options -pthread

sikradio-receiver: sikradio-receiver.cpp err.h err.cpp common.cpp common.h gui.h gui.cpp sikradio-receiver.h poll.h poll.cpp
	g++ -std=c++17 sikradio-receiver.cpp err.h err.cpp common.cpp common.h gui.h gui.cpp sikradio-receiver.h poll.h poll.cpp  -o sikradio-receiver -lboost_program_options -pthread

clean:
	rm sikradio-receiver sikradio-sender
