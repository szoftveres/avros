#include "pm.h"
#include "es.h"

#include "apps.h"

/*
 *
 */

typedef struct session_s {
    char* name;
    char* arg;
    pid_t pid;
} session_t;

/*
 *
 */

static int
launchsession (char** argv) {
    execv(argv[0], argv);
    return 1;
}

/*
 *
 */

int
init (char** argv UNUSED) {
    char* session_arg[3];
    unsigned int i;
    pid_t pid;

    session_t session[] = {
            {"getty", "1/0", NULL},
    };    

    while (1) {
        for (i = 0; i<(sizeof(session)/sizeof(session[0])); i++) {
            if ((!session[i].pid) && (session[i].name)) {
                memset(session_arg, 0, sizeof(session_arg));
                session_arg[0] = session[i].name;
                session_arg[1] = session[i].arg;
                session[i].pid = spawntask(launchsession, DEFAULT_STACK_SIZE, session_arg);
            }
        }
        pid = wait(NULL);
        for (i = 0; i<(sizeof(session)/sizeof(session[0])); i++) {
            if (session[i].pid == pid) {
                session[i].pid = NULL;
                break;
            }
        }
    }
}

/*
 *
 */
