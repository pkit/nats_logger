# Simple logger for `stdin` to NATS jetstream

Reads a JSON formatted object stream from `stdin` and sends it as messages to NATS jetstream.

The reader is byte-by-byte, i.e. infinite streams are properly supported.

```bash
cat test.json
{"time":"2023-03-28 13:48:54","name":"flight","logger":"root","level":"INFO","message":"started"}
{"time":"2023-03-28 13:48:55","name":"flight","logger":"root","level":"INFO","message":"ended"}
```

Any top-level `json` object is considered a log message.

```bash
cat test.json | ./nats_logger
```

Default stream is `MB_LOGS` default subject is `ext.logs`,
can be configured on command line, see [Usage](#usage) below

## To build / install

Prerequisites: [`nats.c`](https://github.com/nats-io/nats.c) library installed in a well known location.

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

```bash
./nats_logger -h

Usage: ./nats_logger [options]

The options are:

-h             prints the usage
-s             server url(s) (comma separated) (default is 'nats://localhost:4222')
-tls           use secure (SSL/TLS) connection
-tlscacert     trusted certificates file
-tlscert       client certificate (PEM format only)
-tlskey        client private key file (PEM format only)
-tlsciphers    ciphers suite
-tlshost       server certificate's expected hostname
-tlsskip       skip server certificate verification
-creds         user credentials chained file
-stream        stream name (default is 'MB_LOGS')
-subj          subject (default is 'ext.logs')
-print         print received log lines (default is false)

```
