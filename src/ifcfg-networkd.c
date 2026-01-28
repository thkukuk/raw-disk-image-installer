// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "basics.h"

/* Configuration */
#define CMDLINE_PATH "/proc/cmdline"
#define OUTPUT_DIR "/run/systemd/network"
#define FILE_PREFIX "60-ifcfg-"

/* Helper to trim whitespace */
static char *
trim_whitespace(char *str)
{
  char *end;

  while(isspace((unsigned char)*str))
    str++;
  if(*str == 0)
    return str;
  end = str + strlen(str) - 1;

  while(end > str && isspace((unsigned char)*end))
    end--;

  *(end+1) = 0;
  return str;
}

/*
  Sanitizes the interface spec to create a valid filename.
  Replaces '*', ':', etc. with underscores.
*/
static void
sanitize_filename(char *dest, const char *src)
{
  while (*src)
    {
      if (isalnum(*src) || *src == '-' || *src == '.')
	*dest = *src;
      else
	*dest = '_';
      dest++;
      src++;
    }
  *dest = '\0';
}

static int
split_and_print(FILE *fp, const char *key, const char *list)
{
  char *token = NULL, *saveptr = NULL;
  _cleanup_free_ char *copy = NULL;

  if (isempty(list))
    return 0;

  copy = strdup(list);
  if (!copy)
    return -ENOMEM;

  token = strtok_r(copy, " ", &saveptr);
  while (token)
    {
      fprintf(fp, "%s=%s\n", key, token);
      token = strtok_r(NULL, " ", &saveptr);
    }
  return 0;
}


/* Writes the systemd-networkd .network file */
static int
write_network_file(const char *interface, int is_dhcp, int dhcp_v4,
		   int dhcp_v6, int rfc2132, char *ip_list,
		   char *gw_list, char *dns_list, char *domains)
{
  _cleanup_free_ char *filename = NULL;
  _cleanup_free_ char *filepath = NULL;
  _cleanup_fclose_ FILE *fp = NULL;
  int r;

  filename = calloc(1, strlen(interface)+1);
  if (!filename)
    return -ENOMEM;

  sanitize_filename(filename, interface);
  if (asprintf(&filepath, "%s/%s%s.network",
	       OUTPUT_DIR, FILE_PREFIX, filename) < 0)
    return -ENOMEM;

  printf("Creating config: %s for interface '%s'\n", filepath, interface);

  fp = fopen(filepath, "w");
  if (!fp)
    {
      r = -errno;
      fprintf(stderr, "Failed to open network file '%s' for writing: %s",
	      filepath, strerror(-r));
      return r;
    }

  /* [Match] Section: */
  fprintf(fp, "[Match]\n");
  /* Heuristic: If the interface contains ':', assume MAC.
     Otherwise Name (supports globs like eth*). */
  if (strchr(interface, ':'))
    fprintf(fp, "Name=*\nMACAddress=%s\n", interface);
  else
    fprintf(fp, "Name=%s\n", interface);

  /* [Network] Section: */
  fprintf(fp, "\n[Network]\n");

  if (is_dhcp)
    {
      if (dhcp_v4 && dhcp_v6)
	fprintf(fp, "DHCP=yes\n");
      else if (dhcp_v4)
	fprintf(fp, "DHCP=ipv4\n");
      else if (dhcp_v6)
	fprintf(fp, "DHCP=ipv6\n");
    }

  /* Static IPs (space separated) */
  r = split_and_print(fp, "Address", ip_list);
  if (r < 0)
    return r;

  r = split_and_print(fp, "Gateway", gw_list);
  if (r < 0)
    return r;

  r = split_and_print(fp, "DNS", dns_list);
  if (r < 0)
    return r;

  if (!isempty(domains))
    fprintf(fp, "Domains=%s\n", domains);

  /* DHCP Specific Options */
  if (is_dhcp)
    {
      if (dhcp_v4)
	{
	  fprintf(fp, "\n[DHCPv4]\n");
	  fprintf(fp, "UseHostname=false\n");
	  fprintf(fp, "UseDNS=true\n");
	  fprintf(fp, "UseNTP=true\n");

	  if (rfc2132)
	    fprintf(fp, "ClientIdentifier=mac\n");
	}
      if (dhcp_v6)
	{
	  fprintf(fp, "\n[DHCPv6]\n");
	  fprintf(fp, "UseHostname=false\n");
	  fprintf(fp, "UseDNS=true\n");
	  fprintf(fp, "UseNTP=true\n");
	}
    }
  return 0;
}

/* Parses a single ifcfg string */
static int
parse_ifcfg_arg(char *arg)
{
  char *interface = NULL;
  char *config = NULL;
  /* dhcp */
  int is_dhcp = 0;
  int dhcp_v4 = 1;
  int dhcp_v6 = 1;
  int rfc2132 = 0;
  /* static */
  char *ip_list = NULL;
  char *gw_list = NULL;
  char *dns_list = NULL;
  char *domains = NULL;

  // Syntax: <interface>=<config>
  interface = arg;
  config = strchr(arg, '=');
  if (!config)
    {
      fprintf(stderr, "Error: Malformed format. Expected 'ifcfg=<iface>=...'\n");
      return -EINVAL;
    }
  *config++ = '\0';

  if (!interface || !config)
    return -ENOENT;

#define MAX_TOKENS 10
  char *tokens[MAX_TOKENS] = {0};
  int token_count = 0;
  char *cur = config;
  char *comma;

  while (token_count < MAX_TOKENS)
    {
      comma = strchr(cur, ',');
      if (comma)
	{
	  *comma = '\0';
	  tokens[token_count++] = trim_whitespace(cur);
	  cur = comma + 1;
	}
      else
	{
	  tokens[token_count++] = trim_whitespace(cur);
	  break;
	}
    }

    // Check first token for DHCP vs Static detection
    int mode_idx = 0; // Index of the token that determines mode

    if (mode_idx >= token_count)
      return 0; // Empty config

    char *mode_str = tokens[mode_idx];

    if (strneq(mode_str, "dhcp", 4))
      {
        is_dhcp = 1;
        if (streq(mode_str, "dhcp4"))
	  dhcp_v6 = 0;
        else if (strcmp(mode_str, "dhcp6") == 0) dhcp_v4 = 0;
        else if (strcmp(mode_str, "dhcp") == 0) { /* both */ }

        // Scan remaining tokens for options like rfc2132
        for (int i = mode_idx + 1; i < token_count; i++) {
            if (strcmp(tokens[i], "rfc2132") == 0) {
                rfc2132 = 1;
            }
        }
    } else {
        // Static Mode
        // Syntax: IP_LIST,GATEWAY_LIST,NAMESERVER_LIST,DOMAINSEARCH_LIST

        if (mode_idx < token_count) ip_list = tokens[mode_idx];
        if (mode_idx + 1 < token_count) gw_list = tokens[mode_idx + 1];
        if (mode_idx + 2 < token_count) dns_list = tokens[mode_idx + 2];
        if (mode_idx + 3 < token_count) domains = tokens[mode_idx + 3];
    }

    write_network_file(interface, is_dhcp, dhcp_v4, dhcp_v6, rfc2132, ip_list, gw_list, dns_list, domains);

    return 0;
}

/* Reads /proc/cmdline and parses quoted arguments */
int
main(int argc, char *argv[])
{
  _cleanup_free_ char *cmdline = NULL;
  _cleanup_fclose_ FILE *f = NULL;
  struct stat st;
  size_t len = 0;
  int r;

  if (stat(OUTPUT_DIR, &st) == -1)
    {
      // XXX use mkdir_p
      if (mkdir(OUTPUT_DIR, 0755) == -1 && errno != EEXIST)
	{
	  r = errno;
	  fprintf(stderr, "Could not create output directory: %s\n",
		  strerror(r));
	  return r;
	}
    }

  // Allow overriding input for testing: ./app "ifcfg=..."
  if (argc > 1)
    {
      // XXX could be more than one ifcfg= parameter
      cmdline = strdup(argv[1]);
      if (!cmdline)
	{
	  fputs("Out of memory!\n", stderr);
	  return ENOMEM;
	}
    }
  else
    {
      f = fopen(CMDLINE_PATH, "r");
      if (!f)
	{
	  r = errno;
	  fprintf(stderr, "Failed to open %s: %s",
		  CMDLINE_PATH, strerror(r));
	  return r;
	}

      ssize_t read = getline(&cmdline, &len, f);
      if (read == -1)
	{
	  fprintf(stderr, "Failed to read %s: %s",
		  CMDLINE_PATH, strerror(r));
	  return errno;

	  if (read > 0 && cmdline[read-1] == '\n')
	    cmdline[read-1] = '\0';
	}
    }

  // Parse loop handling quotes
  char *p = cmdline;
  char *arg_start = p;
  int in_quote = 0;

  while (*p)
    {
      if (*p == '"')
	in_quote = !in_quote;

      if (p[1] == '\0' || (*p == ' ' && !in_quote))
	{
	  if (*p == ' ')
	    *p = '\0'; // Terminate current arg

	  if (strneq(arg_start, "ifcfg=", 6))
	    {
	      char *val = arg_start + 6;

	      // Strip quotes surround the value part
	      if (val[0] == '"')
		{
		  val++;
		  size_t l = strlen(val);
		  if (l > 0 && val[l-1] == '"')
		    val[l-1] = '\0';
		}
	      parse_ifcfg_arg(val);
	    }
	  arg_start = p + 1;
	}
      p++;
    }

    return 0;
}
