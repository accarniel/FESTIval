#!/usr/bin/python

import sys
import re

def remove_quote_dot(value):
    return value.replace('"', '').replace('.', '')

process = {
    'POSTGIS_GDAL_VERSION': lambda value: "/* GDAL library version */\n#define FESTIVAL_GDAL_VERSION {}\n".format(value),
    'POSTGIS_GEOS_VERSION': lambda value: "/* GEOS library version */\n#define FESTIVAL_GEOS_VERSION {}\n".format(value),
    'POSTGIS_PGSQL_VERSION': lambda value: "/* PostgreSQL server version */\n#define FESTIVAL_PGSQL_VERSION {}\n".format(value),
    'POSTGIS_LIB_VERSION': lambda value: "/* PostGIS version */\n#define FESTIVAL_POSTGIS_VERSION {}\n".format(remove_quote_dot(value)),
}


def execute(src_file, target_file):
    p = re.compile('#define (\w+) (\S+)', re.IGNORECASE)

    target_file.write("""/* postgis_config.h.  Generated from postgis_config.h.in by python.  */
#ifndef FESTIVAL_CONFIG_H
#define FESTIVAL_CONFIG_H 1

#define HAVE_FLASHDBSIM 1

""")

    for line in src_file.readlines():
        m = p.match(line)
        if m is not None:
            name = m.group(1)
            value = m.group(2)
            if name in process:
                fn = process[name]
                result = fn(value)
                target_file.write(result)
                target_file.write('\n')
        
    target_file.write("""#endif /* FESTIVAL_CONFIG_H */""")
    

if __name__ == '__main__':
    if(len(sys.argv) < 2):
        print("Error Use: {} <ARQUIVO postgis_config.h>".format(sys.argv[0]))
    src_file = open(sys.argv[1], "r")

    target_file = open("festival_config.h", "w")

    execute(src_file, target_file)
