/* SPDX-License-Identifier: LGPL-2.1+ */

#include "condition.h"
#include "conf-parser.h"
#include "networkd-util.h"
#include "parse-util.h"
#include "string-table.h"
#include "string-util.h"
#include "util.h"

const char *address_family_boolean_to_string(AddressFamilyBoolean b) {
        if (IN_SET(b, ADDRESS_FAMILY_YES, ADDRESS_FAMILY_NO))
                return yes_no(b == ADDRESS_FAMILY_YES);

        if (b == ADDRESS_FAMILY_IPV4)
                return "ipv4";
        if (b == ADDRESS_FAMILY_IPV6)
                return "ipv6";

        return NULL;
}

AddressFamilyBoolean address_family_boolean_from_string(const char *s) {
        int r;

        /* Make this a true superset of a boolean */

        r = parse_boolean(s);
        if (r > 0)
                return ADDRESS_FAMILY_YES;
        if (r == 0)
                return ADDRESS_FAMILY_NO;

        if (streq(s, "ipv4"))
                return ADDRESS_FAMILY_IPV4;
        if (streq(s, "ipv6"))
                return ADDRESS_FAMILY_IPV6;

        return _ADDRESS_FAMILY_BOOLEAN_INVALID;
}

DEFINE_CONFIG_PARSE_ENUM(config_parse_address_family_boolean, address_family_boolean, AddressFamilyBoolean, "Failed to parse option");

int config_parse_address_family_boolean_with_kernel(
                const char* unit,
                const char *filename,
                unsigned line,
                const char *section,
                unsigned section_line,
                const char *lvalue,
                int ltype,
                const char *rvalue,
                void *data,
                void *userdata) {

        AddressFamilyBoolean *fwd = data, s;

        assert(filename);
        assert(lvalue);
        assert(rvalue);
        assert(data);

        /* This function is mostly obsolete now. It simply redirects
         * "kernel" to "no". In older networkd versions we used to
         * distinguish IPForward=off from IPForward=kernel, where the
         * former would explicitly turn off forwarding while the
         * latter would simply not touch the setting. But that logic
         * is gone, hence silently accept the old setting, but turn it
         * to "no". */

        s = address_family_boolean_from_string(rvalue);
        if (s < 0) {
                if (streq(rvalue, "kernel"))
                        s = ADDRESS_FAMILY_NO;
                else {
                        log_syntax(unit, LOG_ERR, filename, line, 0, "Failed to parse IPForward= option, ignoring: %s", rvalue);
                        return 0;
                }
        }

        *fwd = s;

        return 0;
}

/* Router lifetime can be set with netlink interface since kernel >= 4.5
 * so for the supported kernel we don't need to expire routes in userspace */
int kernel_route_expiration_supported(void) {
        static int cached = -1;
        int r;

        if (cached < 0) {
                Condition c = {
                        .type = CONDITION_KERNEL_VERSION,
                        .parameter = (char *) ">= 4.5"
                };
                r = condition_test(&c);
                if (r < 0)
                        return r;

                cached = r;
        }
        return cached;
}

static void network_config_hash_func(const NetworkConfigSection *c, struct siphash *state) {
        siphash24_compress(c->filename, strlen(c->filename), state);
        siphash24_compress(&c->line, sizeof(c->line), state);
}

static int network_config_compare_func(const NetworkConfigSection *x, const NetworkConfigSection *y) {
        int r;

        r = strcmp(x->filename, y->filename);
        if (r != 0)
                return r;

        return CMP(x->line, y->line);
}

DEFINE_HASH_OPS(network_config_hash_ops, NetworkConfigSection, network_config_hash_func, network_config_compare_func);

int network_config_section_new(const char *filename, unsigned line, NetworkConfigSection **s) {
        NetworkConfigSection *cs;

        cs = malloc0(offsetof(NetworkConfigSection, filename) + strlen(filename) + 1);
        if (!cs)
                return -ENOMEM;

        strcpy(cs->filename, filename);
        cs->line = line;

        *s = TAKE_PTR(cs);

        return 0;
}

void network_config_section_free(NetworkConfigSection *cs) {
        free(cs);
}
