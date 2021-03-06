/* GPRS utility functions */

/* (C) 2010 by Harald Welte <laforge@gnumonks.org>
 * (C) 2010-2014 by On-Waves
 * (C) 2013 by Holger Hans Peter Freyther
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <openbsc/gprs_utils.h>

#include <osmocom/core/msgb.h>
#include <osmocom/gprs/gprs_ns.h>

#include <string.h>

/* FIXME: this needs to go to libosmocore/msgb.c */
struct msgb *gprs_msgb_copy(const struct msgb *msg, const char *name)
{
	struct libgb_msgb_cb *old_cb, *new_cb;
	struct msgb *new_msg;

	new_msg = msgb_alloc(msg->data_len, name);
	if (!new_msg)
		return NULL;

	/* copy data */
	memcpy(new_msg->_data, msg->_data, new_msg->data_len);

	/* copy header */
	new_msg->len = msg->len;
	new_msg->data += msg->data - msg->_data;
	new_msg->head += msg->head - msg->_data;
	new_msg->tail += msg->tail - msg->_data;

	new_msg->l1h = new_msg->_data + (msg->l1h - msg->_data);
	new_msg->l2h = new_msg->_data + (msg->l2h - msg->_data);
	new_msg->l3h = new_msg->_data + (msg->l3h - msg->_data);
	new_msg->l4h = new_msg->_data + (msg->l4h - msg->_data);

	/* copy GB specific data */
	old_cb = LIBGB_MSGB_CB(msg);
	new_cb = LIBGB_MSGB_CB(new_msg);

	new_cb->bssgph = new_msg->_data + (old_cb->bssgph - msg->_data);
	new_cb->llch = new_msg->_data + (old_cb->llch - msg->_data);

	/* bssgp_cell_id is a pointer into the old msgb, so we need to make
	 * it a pointer into the new msgb */
	new_cb->bssgp_cell_id = new_msg->_data + (old_cb->bssgp_cell_id - msg->_data);
	new_cb->nsei = old_cb->nsei;
	new_cb->bvci = old_cb->bvci;
	new_cb->tlli = old_cb->tlli;

	return new_msg;
}

/* TODO: Move this to libosmocore/msgb.c */
int gprs_msgb_resize_area(struct msgb *msg, uint8_t *area,
			    size_t old_size, size_t new_size)
{
	int rc;
	uint8_t *rest = area + old_size;
	int rest_len = msg->len - old_size - (area - msg->data);
	int delta_size = (int)new_size - (int)old_size;

	if (delta_size == 0)
		return 0;

	if (delta_size > 0) {
		rc = msgb_trim(msg, msg->len + delta_size);
		if (rc < 0)
			return rc;
	}

	memmove(area + new_size, area + old_size, rest_len);

	if (msg->l1h >= rest)
		msg->l1h += delta_size;
	if (msg->l2h >= rest)
		msg->l2h += delta_size;
	if (msg->l3h >= rest)
		msg->l3h += delta_size;
	if (msg->l4h >= rest)
		msg->l4h += delta_size;

	if (delta_size < 0)
		msgb_trim(msg, msg->len + delta_size);

	return 0;
}

/* TODO: Move these conversion functions to a utils file. */
/**
 * out_str needs to have rest_chars amount of bytes or 1 whatever is bigger.
 */
char * gprs_apn_to_str(char *out_str, const uint8_t *apn_enc, size_t rest_chars)
{
	char *str = out_str;

	while (rest_chars > 0 && apn_enc[0]) {
		size_t label_size = apn_enc[0];
		if (label_size + 1 > rest_chars)
			return NULL;

		memmove(str, apn_enc + 1, label_size);
		str += label_size;
		rest_chars -= label_size + 1;
		apn_enc += label_size + 1;

		if (rest_chars)
			*(str++) = '.';
	}
	str[0] = '\0';

	return out_str;
}

int gprs_str_to_apn(uint8_t *apn_enc, size_t max_len, const char *str)
{
	uint8_t *last_len_field;
	int len;

	/* Can we even write the length field to the output? */
	if (max_len == 0)
		return -1;

	/* Remember where we need to put the length once we know it */
	last_len_field = apn_enc;
	len = 1;
	apn_enc += 1;

	while (str[0]) {
		if (len >= max_len)
			return -1;

		if (str[0] == '.') {
			*last_len_field = (apn_enc - last_len_field) - 1;
			last_len_field = apn_enc;
		} else {
			*apn_enc = str[0];
		}
		apn_enc += 1;
		str += 1;
		len += 1;
	}

	*last_len_field = (apn_enc - last_len_field) - 1;

	return len;
}

