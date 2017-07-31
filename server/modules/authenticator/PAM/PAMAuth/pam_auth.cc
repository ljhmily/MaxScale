/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2020-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#include"pam_auth.hh"

#include <maxscale/authenticator.h>

#include "pam_instance.hh"
#include "pam_client_session.hh"
#include "../pam_auth_common.hh"

/**
 * Initialize PAM authenticator
 *
 * @param options Listener options
 *
 * @return Authenticator instance, or NULL on error
 */
static void* pam_auth_init(char **options)
{
    return PamInstance::create(options);
}

/**
 * Allocate DCB-specific authenticator data (session)
 *
 * @param instance Authenticator instance the session should be connected to
 *
 * @return Authenticator session
 */
static void* pam_auth_alloc(void *instance)
{
    PamInstance* inst = static_cast<PamInstance*>(instance);
    return PamClientSession::create(*inst);
}

/**
 * Free authenticator session
 *
 * @param data PAM session
 */
static void pam_auth_free(void *data)
{
    delete static_cast<PamClientSession*>(data);
}

/**
 * @brief Extract data from client response
 *
 * @param dcb Client DCB
 * @param read_buffer Buffer containing the client's response
 *
 * @return MXS_AUTH_SUCCEEDED if authentication can continue, MXS_AUTH_FAILED if
 * authentication failed
 */
static int pam_auth_extract(DCB *dcb, GWBUF *read_buffer)
{
    PamClientSession *pses = static_cast<PamClientSession*>(dcb->authenticator_data);
    return pses->extract(dcb, read_buffer);
}

/**
 * @brief Is the client SSL capable
 *
 * @param dcb Client DCB
 *
 * @return True if client supports SSL
 */
static bool pam_auth_connectssl(DCB *dcb)
{
    MySQLProtocol *protocol = (MySQLProtocol*)dcb->protocol;
    return protocol->client_capabilities & GW_MYSQL_CAPABILITIES_SSL;
}

/**
 * @brief Authenticate the client. Should be called after pam_auth_extract().
 *
 * @param dcb Client DCB
 *
 * @return MXS_AUTH_INCOMPLETE if authentication is not yet complete. MXS_AUTH_SUCCEEDED
 * if authentication was successfully completed. MXS_AUTH_FAILED if authentication
 * has failed.
 */
static int pam_auth_authenticate(DCB *dcb)
{
    PamClientSession* pses = static_cast<PamClientSession*>(dcb->authenticator_data);
    return pses->authenticate(dcb);
}

/**
 * Free general authenticator data from a DCB. This is data that is not specific
 * to the client authenticator session and may be used by the backend authenticator
 * session to log onto backends.
 *
 * @param dcb DCB to free data from
 */
static void pam_auth_free_data(DCB *dcb)
{
    if (dcb->data)
    {
        MYSQL_session *ses = (MYSQL_session *)dcb->data;
        MXS_FREE(ses->auth_token);
        MXS_FREE(ses);
        dcb->data = NULL;
    }
}

/**
 * @brief Load database users that use PAM authentication
 *
 * Loading the list of database users that use the 'pam' plugin allows us to
 * give more precise error messages to the clients when authentication fails.
 *
 * @param listener Listener definition
 *
 * @return MXS_AUTH_LOADUSERS_OK on success, MXS_AUTH_LOADUSERS_ERROR on error
 */
static int pam_auth_load_users(SERV_LISTENER *listener)
{
    PamInstance *inst = static_cast<PamInstance*>(listener->auth_instance);
    return inst->load_users(listener->service);
}

MXS_BEGIN_DECLS
/**
 * Module handle entry point
 */
MXS_MODULE* MXS_CREATE_MODULE()
{
    static MXS_AUTHENTICATOR MyObject =
    {
        pam_auth_init,                /* Initialize authenticator */
        pam_auth_alloc,               /* Allocate authenticator data */
        pam_auth_extract,             /* Extract data into structure   */
        pam_auth_connectssl,          /* Check if client supports SSL  */
        pam_auth_authenticate,        /* Authenticate user credentials */
        pam_auth_free_data,           /* Free the client data held in DCB */
        pam_auth_free,                /* Free authenticator data */
        pam_auth_load_users,          /* Load database users */
        users_default_diagnostic,        /* Default user diagnostic */
        users_default_diagnostic_json,   /* Default user diagnostic */
        NULL                             /* No user reauthentication */
    };

    static MXS_MODULE info =
    {
        MXS_MODULE_API_AUTHENTICATOR,
        MXS_MODULE_GA,
        MXS_AUTHENTICATOR_VERSION,
        "PAM authenticator",
        "V1.0.0",
        MXS_NO_MODULE_CAPABILITIES,
        &MyObject,
        NULL, /* Process init. */
        NULL, /* Process finish. */
        NULL, /* Thread init. */
        NULL, /* Thread finish. */
        { { MXS_END_MODULE_PARAMS} }
    };

    return &info;
}
MXS_END_DECLS
