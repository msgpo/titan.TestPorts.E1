/* Copyright (c) 2020 Harald Welte <laforge@osmocom.org> */

#include <iterator>

#include "E1TS_PT.hh"
#include "E1TS_PortType.hh"

extern "C" {
#include <osmocom/core/application.h>
}

#include <poll.h>
#include <unistd.h>

using namespace E1TS__PortTypes;

namespace E1TS__PortType {

/* somehow std::map() won't work wit E1TS_identity as key */
DerivedId::DerivedId(const E1TS__identity &id)
{
	interface_nr = id.interface__nr();
	line_nr = id.line__nr();
	ts_nr = id.ts__nr();
}

bool operator<(const DerivedId &fk, const DerivedId &lk) {
	if (fk.interface_nr < lk.interface_nr)
		return true;
	else if (fk.interface_nr == lk.interface_nr) {
		if (fk.line_nr < lk.line_nr)
			return true;
		else if (fk.line_nr == lk.line_nr) {
			if (fk.ts_nr < lk.line_nr)
				return true;
		}
	}
	return false;
}


QueueEntry::QueueEntry(const uint8_t *pdata, unsigned int plen):
	len(plen)
{
	data = (uint8_t *) malloc(len);
	memcpy(data, pdata, len);
}

QueueEntry::~QueueEntry()
{
	free(data);
}

E1_Timeslot::E1_Timeslot(E1TS__PT_PROVIDER &pt, E1TS__identity id, E1TS__mode mode, int fd)
	: m_pt(pt), m_id(id), m_mode(mode), m_fd(fd)
{
	m_pt.log("creating %d:%d:%d fd=%d",
		 (int)m_id.interface__nr(), (int)m_id.line__nr(), (int)m_id.ts__nr(), m_fd);
}

E1_Timeslot::~E1_Timeslot()
{
	m_pt.log("destroying %d:%d:%d fd=%d",
		 (int)m_id.interface__nr(), (int)m_id.line__nr(), (int)m_id.ts__nr(), m_fd);

	close(m_fd);

	/* iterate over tx-queue and free all elements */
	while (!m_tx_queue.empty()) {
		struct QueueEntry *qe = m_tx_queue.front();
		printf("qe=%p\n", qe);
		m_tx_queue.pop();
		delete qe;
	}
}

/* enqueue to-be-transmitted data */
int E1_Timeslot::enqueue_tx(const uint8_t *data, unsigned int len)
{
	struct QueueEntry *qe = new QueueEntry(data, len);
	if (!qe)
		return 0;
	m_tx_queue.push(qe);

	return 1;
}

/* dequeue + write next-to-be-transmitted data from queue */
int E1_Timeslot::dequeue_tx(void)
{
	struct QueueEntry *qe;
	int rc;

	if (m_tx_queue.empty()) {
		/* queue is empty; unsubscribe write-events */
		return 0;
	}

	qe = m_tx_queue.front();
	m_tx_queue.pop();
	rc = write(m_fd, qe->data, qe->len);
	if (rc < 0) {
		TTCN_error("error during write: %s\n", strerror(errno));
		/* FIXME: close/delete fd */
	}
	else if (rc < qe->len)
		TTCN_error("could only write %u of %u bytes\n", rc, qe->len);

	delete qe;

	return 1;
}



E1TS__PT_PROVIDER::E1TS__PT_PROVIDER(const char *par_port_name)
	: PORT(par_port_name)
{
	osmo_init_logging2(NULL, NULL);
}

E1TS__PT_PROVIDER::~E1TS__PT_PROVIDER()
{
}

void E1TS__PT_PROVIDER::log(const char *fmt, ...)
{
	TTCN_Logger::begin_event(TTCN_WARNING);
	TTCN_Logger::log_event("E1TS Test port (%s): ", get_name());
	va_list args;
	va_start(args, fmt);
	TTCN_Logger::log_event_va_list(fmt, args);
	va_end(args);
	TTCN_Logger::end_event();
}

void E1TS__PT_PROVIDER::set_parameter(const char *parameter_name, const char *parameter_value)
{
	if (!strcmp(parameter_name, "e1d_socket_path"))
		m_e1d_socket_path = parameter_value;
	else
		TTCN_error("Unsupported E1TS test port parameter `%s'.", parameter_name);
}

void E1TS__PT_PROVIDER::Handle_Fd_Event(int fd, boolean is_readable, boolean is_writable,
					boolean is_error)
{
	uint8_t buf[65535];
	E1_Timeslot *ts;
	int rc;

	/* find E1TS_identity by fd */
	ts = ts_by_fd(fd);

	if (!ts)
		TTCN_error("Unknown file descriptor %d\n", fd);

	if (is_readable) {
		rc = read(fd, buf, sizeof(buf));
		if (rc > 0)
			incoming_message(E1TS__unitdata(ts->m_id, OCTETSTRING(rc, buf)));
		else if (rc == 0) {
			TTCN_error("EOF on E1TS fd, closing");
			m_ts_by_id.erase(m_ts_by_id.find(ts->m_id));
			m_ts_by_fd.erase(m_ts_by_fd.find(ts->m_fd));
			Handler_Remove_Fd(ts->m_fd);
			delete ts;
		}
	}

	if (is_writable) {
		/* dequeue next message; unregister for 'write' if nothing to write */
		if (ts->dequeue_tx() == 0)
			Handler_Remove_Fd_Write(ts->m_fd);
	}
}

void E1TS__PT_PROVIDER::user_map(const char * /*system_port*/)
{
	m_e1d_clnt = osmo_e1dp_client_create(NULL, m_e1d_socket_path);
}

void E1TS__PT_PROVIDER::user_unmap(const char * /*system_port*/)
{
	/* close/destroy all timeslots */
	for (auto it = m_ts_by_id.begin(); it != m_ts_by_id.end(); it++) {
		E1_Timeslot *ts = it->second;
		Handler_Remove_Fd(ts->m_fd);
		delete ts;
	}
	m_ts_by_id.clear();
	m_ts_by_fd.clear();

	/* close client connection to daemon */
	osmo_e1dp_client_destroy(m_e1d_clnt);
}

void E1TS__PT_PROVIDER::user_start()
{
}

void E1TS__PT_PROVIDER::user_stop()
{
}

static enum osmo_e1dp_ts_mode e1dp_mode(E1TS__mode in)
{
	switch (in) {
	case E1TS__PortTypes::E1TS__mode::E1TS__MODE__RAW:
		return E1DP_TSMODE_RAW;
	case E1TS__PortTypes::E1TS__mode::E1TS__MODE__HDLCFCS:
		return E1DP_TSMODE_HDLCFCS;
	default:
		TTCN_error("Unknown E1TS_mode %d\n", in);
	}
}

E1_Timeslot *E1TS__PT_PROVIDER::ts_by_fd(int fd)
{
	auto it = m_ts_by_fd.find(fd);
	if (it == m_ts_by_fd.end()) {
		TTCN_error("couldn't find FD for identity");
		return NULL;
	} else
		return it->second;
}


E1_Timeslot *E1TS__PT_PROVIDER::ts_by_id(const E1TS__identity& id)
{
	auto it = m_ts_by_id.find(id);
	if (it == m_ts_by_id.end())
		return NULL;
	else
		return it->second;
}


void E1TS__PT_PROVIDER::outgoing_send(const E1TS__open& send_par)
{
	int fd;
	enum osmo_e1dp_ts_mode mode = e1dp_mode(send_par.mode());

	fd = osmo_e1dp_client_ts_open(m_e1d_clnt, send_par.id().interface__nr(),
				      send_par.id().line__nr(), send_par.id().ts__nr(), mode);

	if (fd >= 0) {
		E1_Timeslot *ts = new E1_Timeslot(*this, send_par.id(), send_par.mode(), fd);
		m_ts_by_id.insert(std::make_pair(send_par.id(), ts));
		m_ts_by_fd.insert(std::make_pair(fd, ts));
		Handler_Add_Fd_Read(fd);
		incoming_message(E1TS__result(send_par.req__hdl(), 0));
	} else {
		incoming_message(E1TS__result(send_par.req__hdl(), fd));
	}
}

void E1TS__PT_PROVIDER::outgoing_send(const E1TS__close& send_par)
{
	/* find fd by map */
	E1_Timeslot *ts = ts_by_id(send_par.id());

	if (!ts)
		return;

	m_ts_by_id.erase(m_ts_by_id.find(send_par.id()));
	m_ts_by_fd.erase(m_ts_by_fd.find(ts->m_fd));
	Handler_Remove_Fd(ts->m_fd);
	delete ts;
}

void E1TS__PT_PROVIDER::outgoing_send(const E1TS__unitdata& send_par)
{
	/* find fd by map */
	E1_Timeslot *ts = ts_by_id(send_par.id());

	if (!ts)
		return;

	ts->enqueue_tx(send_par.data(), send_par.data().lengthof());
	Handler_Add_Fd_Write(ts->m_fd);
}


} /* namespace */
