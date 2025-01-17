/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2022 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "genvironmentproxyresolver.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

struct _GEnvironmentProxyResolver {
  GObject parent_instance;

  GProxyResolver *base_resolver;
};

static void g_environment_proxy_resolver_iface_init (GProxyResolverInterface *iface);

#ifdef GENVIRONMENTPROXY_MODULE
static void
g_environment_proxy_resolver_class_finalize (GEnvironmentProxyResolverClass *klass)
{
}

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GEnvironmentProxyResolver,
                                g_environment_proxy_resolver,
                                G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (G_TYPE_PROXY_RESOLVER,
                                                               g_environment_proxy_resolver_iface_init))
#else
G_DEFINE_TYPE_EXTENDED (GEnvironmentProxyResolver,
                        g_environment_proxy_resolver,
                        G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_PROXY_RESOLVER,
                                               g_environment_proxy_resolver_iface_init))
#endif

static gboolean
is_running_libproxy_test (void)
{
  const char *gio_proxy_test_name;
  gio_proxy_test_name = g_getenv ("GIO_PROXY_TEST_NAME");
  if (g_strcmp0 (gio_proxy_test_name, "libproxy") == 0)
    return TRUE;
  return FALSE;
}

static gboolean
g_environment_proxy_resolver_is_supported (GProxyResolver *object)
{
  if (is_running_libproxy_test ())
    return FALSE;

  return (g_getenv ("ftp_proxy") || g_getenv ("FTP_PROXY") ||
          g_getenv ("https_proxy") || g_getenv ("HTTPS_PROXY") ||
          g_getenv ("http_proxy") || g_getenv ("HTTP_PROXY") ||
          g_getenv ("no_proxy") || g_getenv ("NO_PROXY"));
}

static GProxyResolver *
get_base_resolver (GProxyResolver *resolver)
{
  return G_PROXY_RESOLVER (G_ENVIRONMENT_PROXY_RESOLVER (resolver)->base_resolver);
}

static gchar **
g_environment_proxy_resolver_lookup (GProxyResolver  *resolver,
                                     const gchar     *uri,
                                     GCancellable    *cancellable,
                                     GError         **error)
{
  return g_proxy_resolver_lookup (get_base_resolver (resolver), uri, cancellable, error);
}

static void
g_environment_proxy_resolver_lookup_async (GProxyResolver      *resolver,
                                           const gchar         *uri,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_proxy_resolver_lookup_async (get_base_resolver (resolver), uri, cancellable, callback, user_data);
}

static gchar **
g_environment_proxy_resolver_lookup_finish (GProxyResolver     *resolver,
                                            GAsyncResult       *result,
                                            GError            **error)
{
  return g_proxy_resolver_lookup_finish (resolver, result, error);
}

static void
g_environment_proxy_resolver_finalize (GObject *object)
{
  GEnvironmentProxyResolver *resolver = G_ENVIRONMENT_PROXY_RESOLVER (object);
  
  g_object_unref (resolver->base_resolver);

  G_OBJECT_CLASS (g_environment_proxy_resolver_parent_class)->finalize (object);
}

static void
g_environment_proxy_resolver_init (GEnvironmentProxyResolver *resolver)
{
  char **ignore_hosts = NULL;
  const char *default_proxy = NULL;

  if (g_getenv ("no_proxy"))
    ignore_hosts = g_strsplit (g_getenv ("no_proxy"), ",", -1);
  else if (g_getenv ("NO_PROXY"))
    ignore_hosts = g_strsplit (g_getenv ("NO_PROXY"), ",", -1);

  /* The http_proxy/HTTP_PROXY is used for *all* protocols (except FTP or HTTPS,
   * if more specific environment variables are set). It is not just for HTTP.
   * This matches the behavior of libproxy's environment variable module, or
   * GNOME's use-same-proxy setting.
   */
  if (g_getenv ("http_proxy"))
    default_proxy = g_getenv ("http_proxy");
  else if (g_getenv ("HTTP_PROXY"))
    default_proxy = g_getenv ("HTTP_PROXY");

  resolver->base_resolver = g_simple_proxy_resolver_new (default_proxy, ignore_hosts);

  if (g_getenv ("ftp_proxy"))
    g_simple_proxy_resolver_set_uri_proxy (G_SIMPLE_PROXY_RESOLVER (resolver->base_resolver), "ftp", g_getenv ("ftp_proxy"));
  else if (g_getenv ("FTP_PROXY"))
    g_simple_proxy_resolver_set_uri_proxy (G_SIMPLE_PROXY_RESOLVER (resolver->base_resolver), "ftp", g_getenv ("FTP_PROXY"));

  if (g_getenv ("https_proxy"))
    g_simple_proxy_resolver_set_uri_proxy (G_SIMPLE_PROXY_RESOLVER (resolver->base_resolver), "https", g_getenv ("https_proxy"));
  else if (g_getenv ("HTTPS_PROXY"))
    g_simple_proxy_resolver_set_uri_proxy (G_SIMPLE_PROXY_RESOLVER (resolver->base_resolver), "https", g_getenv ("HTTPS_PROXY"));

  g_strfreev (ignore_hosts);
}

static void
g_environment_proxy_resolver_class_init (GEnvironmentProxyResolverClass *resolver_class)
{
  GObjectClass *object_class;
  
  object_class = G_OBJECT_CLASS (resolver_class);
  object_class->finalize = g_environment_proxy_resolver_finalize;
}

static void
g_environment_proxy_resolver_iface_init (GProxyResolverInterface *iface)
{
  iface->is_supported = g_environment_proxy_resolver_is_supported;
  iface->lookup = g_environment_proxy_resolver_lookup;
  iface->lookup_async = g_environment_proxy_resolver_lookup_async;
  iface->lookup_finish = g_environment_proxy_resolver_lookup_finish;
}

#ifdef GENVIRONMENTPROXY_MODULE
void
g_environment_proxy_resolver_register (GIOModule *module)
{
  g_environment_proxy_resolver_register_type (G_TYPE_MODULE (module));
  if (!module)
    g_io_extension_point_register (G_PROXY_RESOLVER_EXTENSION_POINT_NAME);
  g_io_extension_point_implement (G_PROXY_RESOLVER_EXTENSION_POINT_NAME,
                                  g_environment_proxy_resolver_get_type(),
                                  "environment",
                                  100);
}
#endif
