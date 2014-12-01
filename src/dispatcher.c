#include "dispatcher.h"
#define MAX_INFLIGHT 256

uint16_t _fingerprint(dpacket_t *packet);

dispatcher_t *dispatcher_setup(pgen_t *pgen, int num_readers, int num_writers) {
	dispatcher_t *d = (dispatcher_t *)malloc(sizeof(dispatcher_t));
	d->num_readers = num_readers;
	d->num_writers = num_writers;

	d->in_flight = ATOMIC_VAR_INIT(0);

	d->pgen = pgen;
	d->png = png_setup();
	d->r = r_setup(MAX_ADDR);
	d->hist = hist_setup();
	d->treaders = malloc(sizeof(pthread_t) * (unsigned int)num_readers);
	d->twriters = malloc(sizeof(pthread_t) * (unsigned int)num_writers);
	d->ireaders = malloc(sizeof(reader_info_t *) * (unsigned int)num_readers);
	d->iwriters = malloc(sizeof(writer_info_t *) * (unsigned int)num_writers);

	for (int i = 0; i < num_readers; i++) {
		reader_info_t *reader = reader_setup(&d->in_flight, MAX_INFLIGHT, d->png, d->r, d->hist);
		d->ireaders[i] = reader;
		pthread_create(&(d->treaders[i]), NULL, reader_start, (void *)reader);
	}
	for (int i = 0; i < num_writers; i++) {
		writer_info_t *w = writer_setup(&d->in_flight, MAX_INFLIGHT, d->png, d->r);
		d->iwriters[i] = w;
		pthread_create(&(d->twriters[i]), NULL, writer_start, (void *)w);
	}

	return d;
}

void dispatcher_tear_down(dispatcher_t *d) {
	for (int i = 0; i < d->num_readers; i++) {
		// send the reader a null packet to stop it
		reader_send_packet(d->ireaders[i], NULL);
		pthread_join(d->treaders[i], NULL);

		reader_tear_down(d->ireaders[i]);
	}
	free(d->ireaders);
	free(d->treaders);

	for (int i = 0; i < d->num_writers; i++) {
		// send the writer a null packet to stop it
		writer_send_packet(d->iwriters[i], NULL);
		pthread_join(d->twriters[i], NULL);

		writer_tear_down(d->iwriters[i]);
	}
	free(d->iwriters);
	free(d->twriters);

	png_tear_down(d->png);
	r_tear_down(d->r);
	hist_tear_down(d->hist);

	free(d);
}

void dispatch(dispatcher_t *d) {
	cpacket_t *cp = NULL;
	dpacket_t *dp = NULL;

	while (atomic_load(&d->in_flight) == MAX_INFLIGHT) {}
	get_packet(d->pgen, &dp, &cp);

	atomic_fetch_add(&d->in_flight, 1);

	assert(cp != NULL || dp != NULL);
	if (dp != NULL) {
		reader_info_t *r = d->ireaders[dp->seed % d->num_readers];
		reader_send_packet(r, dp);
	} else {
		writer_info_t *w = d->iwriters[cp->address % d->num_writers];
		writer_send_packet(w, cp);
	}
}

serial_t *serial_setup(pgen_t *pgen) {
	serial_t *s = (serial_t *)malloc(sizeof(serial_t));

	s->pgen = pgen;
	s->png = png_setup();
	s->r = r_setup(MAX_ADDR);
	s->hist = hist_setup();

	return s;
}

void serial_tear_down(serial_t *s) {
	png_tear_down(s->png);
	r_tear_down(s->r);
	hist_tear_down(s->hist);
	free(s);
}

void serial_dispatch(serial_t *s) {
	cpacket_t *cp = NULL;
	dpacket_t *dp = NULL;

	get_packet(s->pgen, &dp, &cp);

	assert(cp != NULL || dp != NULL);
	if (dp != NULL) {
        //as if a reader processes it
        if (png_allow(s->png, dp->src) && 
            r_accept(s->r, dp->dest, dp->src)){
            hist_update(s->hist, _fingerprint(dp));
        }
	} else {
        //as if a writer processes it
		png_update(s->png, cp->address, cp->persona_non_grata);
        r_update(s->r, cp->accepting_range, cp->address, cp->address_begin,cp->address_end);
	}
}
