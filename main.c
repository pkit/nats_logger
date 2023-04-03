#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libs/jsmn-stream/jsmn_stream.h"
#include "nats.h"

#define MAX_SERVERS (10)

const char *stream = "MB_LOGS";
const char *subj = "ext.logs";

natsOptions *opts = NULL;
const char *certFile = NULL;
const char *keyFile = NULL;
bool print = false;

struct context {
    char *buf;
    size_t len;
    size_t start;
    size_t depth;
    jsCtx *js;
    natsConnection *nc;
} context_t;

void
start_obj(void *user_arg) {
    struct context *ctx = (struct context *) user_arg;
    ctx->start = ctx->len - 1;
    ctx->depth++;
}

void
end_obj(void *user_arg) {
    struct context *ctx = (struct context *) user_arg;
    // use only top level objects
    if (ctx->depth == 1) {
        js_PublishAsync(ctx->js, subj, (const void *) (ctx->buf + ctx->start), (int) (ctx->len - ctx->start), NULL);

        if (print) {
            ctx->buf[ctx->len] = '\0';
            printf("%s\n", ctx->buf + ctx->start);
        }

        ctx->len = 0;
    }

    ctx->depth--;
}

jsmn_stream_parser parser;

jsmn_stream_callbacks_t cbs = {
        NULL,
        NULL,
        start_obj,
        end_obj,
        NULL,
        NULL,
        NULL
};

natsStatus
jetStream_Init(struct context *ctx) {
    natsStatus s;
    natsConnection *nc = NULL;
    jsCtx *js;
    jsOptions jsOpts;

    s = natsConnection_Connect(&nc, opts);

    if (s == NATS_OK) {
        s = jsOptions_Init(&jsOpts);
    }

    if (s == NATS_OK) {
        jsOpts.PublishAsync.MaxPending = 256;
        s = natsConnection_JetStream(&js, nc, &jsOpts);
    }

    if (s == NATS_OK) {
        jsStreamInfo *si = NULL;
        jsErrCode jerr = 0;

        s = js_GetStreamInfo(&si, js, stream, NULL, &jerr);
        if (s == NATS_NOT_FOUND) {
            jsStreamConfig cfg;

            jsStreamConfig_Init(&cfg);
            cfg.Name = stream;
            cfg.Subjects = (const char *[1]) {subj};
            cfg.SubjectsLen = 1;
            cfg.Storage = js_FileStorage;
            s = js_AddStream(&si, js, &cfg, NULL, &jerr);
        }

        if (s == NATS_OK) {
            jsStreamInfo_Destroy(si);

            ctx->js = js;
            ctx->nc = nc;
        }
    }

    return s;
}

natsStatus
jetStream_Close(struct context *ctx) {
    natsStatus s;
    jsPubOptions jsPubOpts;
    jsPubOptions_Init(&jsPubOpts);
    jsPubOpts.MaxWait = 5000;

    s = js_PublishAsyncComplete(ctx->js, &jsPubOpts);

    if (s == NATS_TIMEOUT) {
        natsMsgList list;

        js_PublishAsyncGetPendingList(&list, ctx->js);
        natsMsgList_Destroy(&list);

        s = NATS_OK;
    }

    jsCtx_Destroy(ctx->js);
    natsConnection_Destroy(ctx->nc);
    natsOptions_Destroy(opts);
    nats_Close();

    return s;
}

static void
printUsageAndExit(const char *progName) {
    printf("\nUsage: %s [options]\n\nThe options are:\n\n"\
"-h             prints the usage\n" \
"-s             server url(s) (comma separated) (default is '%s')\n" \
"-tls           use secure (SSL/TLS) connection\n" \
"-tlscacert     trusted certificates file\n" \
"-tlscert       client certificate (PEM format only)\n" \
"-tlskey        client private key file (PEM format only)\n" \
"-tlsciphers    ciphers suite\n" \
"-tlshost       server certificate's expected hostname\n" \
"-tlsskip       skip server certificate verification\n" \
"-creds         user credentials chained file\n" \
"-stream        stream name (default is 'MB_LOGS')\n" \
"-subj          subject (default is 'ext.logs')\n" \
"-print         print received log lines (default is false)\n",
           progName, NATS_DEFAULT_URL);

    natsOptions_Destroy(opts);
    nats_Close();

    exit(1);
}

static natsStatus
parseUrls(const char *urls, natsOptions *gopts) {
    char *serverUrls[MAX_SERVERS];
    int num = 0;
    natsStatus s = NATS_OK;
    char *urlsCopy = NULL;
    char *commaPos = NULL;
    char *ptr = NULL;

    urlsCopy = strdup(urls);
    if (urlsCopy == NULL)
        return NATS_NO_MEMORY;

    memset(serverUrls, 0, sizeof(serverUrls));

    ptr = urlsCopy;

    do {
        if (num == MAX_SERVERS) {
            s = NATS_INSUFFICIENT_BUFFER;
            break;
        }

        serverUrls[num++] = ptr;
        commaPos = strchr(ptr, ',');
        if (commaPos != NULL) {
            ptr = (char *) (commaPos + 1);
            *(commaPos) = '\0';
        } else {
            ptr = NULL;
        }
    } while (ptr != NULL);

    if (s == NATS_OK)
        s = natsOptions_SetServers(gopts, (const char **) serverUrls, num);

    free(urlsCopy);

    return s;
}

static natsOptions *
parseArgs(int argc, char **argv) {
    natsStatus s = NATS_OK;
    bool urlsSet = false;
    int i;

    if (natsOptions_Create(&opts) != NATS_OK)
        s = NATS_NO_MEMORY;

    // always try to reconnect if needed
    natsOptions_SetAllowReconnect(opts, true);

    for (i = 1; (i < argc) && (s == NATS_OK); i++) {
        if ((strcasecmp(argv[i], "-h") == 0)
            || (strcasecmp(argv[i], "-help") == 0)) {
            printUsageAndExit(argv[0]);
        } else if (strcasecmp(argv[i], "-s") == 0) {
            if (i + 1 == argc)
                printUsageAndExit(argv[0]);

            s = parseUrls(argv[++i], opts);
            if (s == NATS_OK)
                urlsSet = true;
        } else if (strcasecmp(argv[i], "-tls") == 0) {
            s = natsOptions_SetSecure(opts, true);
        } else if (strcasecmp(argv[i], "-tlscacert") == 0) {
            if (i + 1 == argc)
                printUsageAndExit(argv[0]);

            s = natsOptions_LoadCATrustedCertificates(opts, argv[++i]);
        } else if (strcasecmp(argv[i], "-tlscert") == 0) {
            if (i + 1 == argc)
                printUsageAndExit(argv[0]);

            certFile = argv[++i];
        } else if (strcasecmp(argv[i], "-tlskey") == 0) {
            if (i + 1 == argc)
                printUsageAndExit(argv[0]);

            keyFile = argv[++i];
        } else if (strcasecmp(argv[i], "-tlsciphers") == 0) {
            if (i + 1 == argc)
                printUsageAndExit(argv[0]);

            s = natsOptions_SetCiphers(opts, argv[++i]);
        } else if (strcasecmp(argv[i], "-tlshost") == 0) {
            if (i + 1 == argc)
                printUsageAndExit(argv[0]);

            s = natsOptions_SetExpectedHostname(opts, argv[++i]);
        } else if (strcasecmp(argv[i], "-tlsskip") == 0) {
            s = natsOptions_SkipServerVerification(opts, true);
        } else if (strcasecmp(argv[i], "-subj") == 0) {
            if (i + 1 == argc)
                printUsageAndExit(argv[0]);

            subj = argv[++i];
        } else if (strcasecmp(argv[i], "-print") == 0) {
            print = true;
        } else if (strcasecmp(argv[i], "-creds") == 0) {
            if (i + 1 == argc)
                printUsageAndExit(argv[0]);

            s = natsOptions_SetUserCredentialsFromFiles(opts, argv[++i], NULL);
        } else if (strcasecmp(argv[i], "-stream") == 0) {
            if (i + 1 == argc)
                printUsageAndExit(argv[0]);

            stream = argv[++i];
        } else {
            printf("Unknown option: '%s'\n", argv[i]);
            printUsageAndExit(argv[0]);
        }
    }

    if ((s == NATS_OK) && ((certFile != NULL) || (keyFile != NULL)))
        s = natsOptions_LoadCertificatesChain(opts, certFile, keyFile);

    if ((s == NATS_OK) && !urlsSet)
        s = parseUrls(NATS_DEFAULT_URL, opts);

    if (s != NATS_OK) {
        printf("Error parsing arguments: %u - %s\n",
               s, natsStatus_GetText(s));

        nats_PrintLastErrorStack(stderr);

        natsOptions_Destroy(opts);
        nats_Close();
        exit(1);
    }

    return opts;
}

int
main(int argc, char **argv) {
    size_t cap = 4096;
    natsStatus s;
    void * realloc_ptr;

    opts = parseArgs(argc, argv);

    struct context *ctx = (struct context *) malloc(sizeof(context_t));
    ctx->buf = (char *) malloc(cap * sizeof(char));
    ctx->len = 0;
    ctx->start = 0;
    ctx->depth = 0;

    s = jetStream_Init(ctx);

    if (s == NATS_OK) {
        jsmn_stream_init(&parser, &cbs, ctx);

        int ch;
        while ((ch = fgetc(stdin)) != EOF) {
            ctx->buf[ctx->len] = (char) ch;
            if (++(ctx->len) == cap) {
                realloc_ptr = realloc(ctx->buf, (cap *= 2) * sizeof(char));
                if (realloc_ptr == NULL) {
                    printf("Out of memory");
                    exit(1);
                } else {
                    ctx->buf = realloc_ptr;
                }
            }
            jsmn_stream_parse(&parser, (char) ch);
        }

        jetStream_Close(ctx);

        return 0;
    }

    return 1;
}
