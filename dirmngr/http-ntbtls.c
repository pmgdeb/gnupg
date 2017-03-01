/* http-ntbtls.c - Support for using NTBTLS with http.c
 * Copyright (C) 2017  Werner Koch
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dirmngr.h"
#include "certcache.h"
#include "validate.h"

#ifdef HTTP_USE_NTBTLS
# include <ntbtls.h>



/* The callback used to verify the peer's certificate.  */
gpg_error_t
gnupg_http_tls_verify_cb (void *opaque,
                          http_t http,
                          http_session_t session,
                          unsigned int http_flags,
                          void *tls_context)
{
  ctrl_t ctrl = opaque;
  ntbtls_t tls = tls_context;
  gpg_error_t err;
  int idx;
  ksba_cert_t cert;
  ksba_cert_t hostcert = NULL;
  unsigned int validate_flags;
  const char *hostname;

  (void)http;
  (void)session;

  log_assert (ctrl && ctrl->magic == SERVER_CONTROL_MAGIC);
  log_assert (!ntbtls_check_context (tls));

  /* Get the peer's certs fron ntbtls.  */
  for (idx = 0;
       (cert = ntbtls_x509_get_peer_cert (tls, idx)); idx++)
    {
      if (!idx)
        hostcert = cert;
      else
        {
          /* Quick hack to make verification work by inserting the supplied
           * certs into the cache.  FIXME! */
          cache_cert (cert);
          ksba_cert_release (cert);
        }
    }
  if (!idx)
    {
      err  = gpg_error (GPG_ERR_MISSING_CERT);
      goto leave;
    }

  validate_flags = VALIDATE_FLAG_TLS;

  /* Are we using the standard hkps:// pool use the dedicated
   * root certificate.  */
  hostname = ntbtls_get_hostname (tls);
  if (hostname
      && !ascii_strcasecmp (hostname, "hkps.pool.sks-keyservers.net"))
    {
      validate_flags |= VALIDATE_FLAG_TRUST_HKPSPOOL;
    }
  else /* Use the certificates as requested from the HTTP module.  */
    {
      if ((http_flags & HTTP_FLAG_TRUST_DEF))
        validate_flags |= VALIDATE_FLAG_TRUST_HKP;
      if ((http_flags & HTTP_FLAG_TRUST_SYS))
        validate_flags |= VALIDATE_FLAG_TRUST_SYSTEM;
    }

  if ((http_flags & HTTP_FLAG_NO_CRL))
    validate_flags |= VALIDATE_FLAG_NOCRLCHECK;

  err = validate_cert_chain (ctrl, hostcert, NULL, validate_flags, NULL);

 leave:
  ksba_cert_release (hostcert);
  return err;
}


#else /*!HTTP_USE_NTBTLS*/

/* Dummy function used when not build without ntbtls support.  */
gpg_error_t
gnupg_http_tls_verify_cb (void *opaque,
                          http_t http,
                          http_session_t session,
                          unsigned int flags,
                          void *tls_context)
{
  (void)opaque;
  (void)http;
  (void)session;
  (void)flags;
  (void)tls_context;
  return gpg_error (GPG_ERR_NOT_IMPLEMENTED);
}
#endif /*!HTTP_USE_NTBTLS*/