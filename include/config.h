#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#define PROGNAME		"Quantica"
#define INSTANCE_PREFIX	"QUANTZ"
#define VERSION_MAJOR	0
#define VERSION_MINOR	9
#define VERSION_PATCH	41
#define LOGFILE			"quantica.log"

#define CACHE_SLOTS	23
#define DBCACHE_SLOTS	25
#define DBCACHE_DENSITY	75

#ifdef DEBUG
#define DEFAULT_PAGE_SIZE	2 // 16 Kb
#else
#define DEFAULT_PAGE_SIZE	10 // 4 Mb
#endif

#define API_PORT	4017
#define LICENSE		"BSD 3-clause"

#endif // CONFIG_H_INCLUDED
