#pragma once

#include <map>
#include <queue>

#include <TTCN3.hh>

extern "C" {
#include <osmocom/e1d/proto_clnt.h>
}

#include "E1TS_PortTypes.hh"

namespace E1TS__PortType {

using namespace E1TS__PortTypes;

class E1TS__PT_PROVIDER;

class DerivedId {
public:
	DerivedId(const E1TS__identity &id);
	unsigned int interface_nr;
	unsigned int line_nr;
	unsigned int ts_nr;
};

class QueueEntry {
public:
	QueueEntry(const uint8_t *pdata, unsigned int plen);
	~QueueEntry();

	uint8_t *data;
	unsigned int len;
};

class E1_Timeslot {
public:
	E1_Timeslot(E1TS__PT_PROVIDER &pr, E1TS__identity id, E1TS__mode mode, int fd);
	~E1_Timeslot();

	int enqueue_tx(const uint8_t *data, unsigned int len);
	int dequeue_tx(void);

	E1TS__identity m_id;
	int m_fd;

private:
	E1TS__PT_PROVIDER &m_pt;
	E1TS__mode m_mode;
	std::queue<QueueEntry *> m_tx_queue;
};


class E1TS__PT_PROVIDER : public PORT {
public:
	E1TS__PT_PROVIDER(const char *par_port_name);
	~E1TS__PT_PROVIDER();

	void set_parameter(const char *parameter_name, const char *parameter_value);

private:
	void Handle_Fd_Event(int fd, boolean is_readable, boolean is_writable, boolean is_error);

protected:
	void user_map(const char *system_port);
	void user_unmap(const char *system_port);

	void user_start();
	void user_stop();

	void outgoing_send(const E1TS__open& send_par);
	void outgoing_send(const E1TS__close& send_par);
	void outgoing_send(const E1TS__unitdata& send_par);

	virtual void incoming_message(const E1TS__result& incoming_par) = 0;
	virtual void incoming_message(const E1TS__unitdata &incoming_par) = 0;

public:
	void log(const char *fmt, ...);

private:
	/* parameter */
	const char *m_e1d_socket_path = E1DP_DEFAULT_SOCKET;

	E1_Timeslot *ts_by_fd(int fd);
	E1_Timeslot *ts_by_id(const E1TS__identity &id);

	/* client to the E1 Daemon */
	struct osmo_e1dp_client *m_e1d_clnt;

	/* per-timeslot file descriptors */
	std::map<DerivedId, E1_Timeslot *> m_ts_by_id;
	std::map<int, E1_Timeslot *> m_ts_by_fd;

};



}
