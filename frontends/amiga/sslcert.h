/*
 * Copyright 2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AMIGA_SSLCERT_H
#define AMIGA_SSLCERT_H
struct nsurl;
struct ssl_cert_info;

/**
 * Prompt the user to verify a certificate with issues.
 *
 * \param url The URL being verified.
 * \param certs The certificate to be verified
 * \param num The number of certificates to be verified.
 * \param cb Callback upon user decision.
 * \param cbpw Context pointer passed to cb
 * \return NSERROR_OK or error code if prompt creation failed.
 */
nserror ami_cert_verify(struct nsurl *url, 
		const struct ssl_cert_info *certs, unsigned long num,
		nserror (*cb)(bool proceed, void *pw), void *cbpw);
#endif

