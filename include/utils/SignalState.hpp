#ifndef SIGNALSTATE_HPP
# define SIGNALSTATE_HPP

# include <signal.h>

namespace SignalState
{
	extern volatile sig_atomic_t shutdownRequested;

	void requestShutdown(int signalNumber);
}

#endif
