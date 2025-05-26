#ifndef EZMB_H
#define EZMB_H

#include "e_proxy.h"
#include "e_device.h"

#define EZMB_VERSION "0.1.0"
#define EZMB_DEFAULT_SOUTH_URL "ipc:///tmp/ezmb_south.ipc"
#define EZMB_DEFAULT_NORTH_URL "ipc:///tmp/ezmb_north.ipc"
// #define EZMB_DEFAULT_SOUTH_URL "tcp://127.0.0.1:5555"
// #define EZMB_DEFAULT_NORTH_URL "tcp://127.0.0.1:5556"
#define EZMB_DEFAULT_PROXY_BACKEND_URL EZMB_DEFAULT_SOUTH_URL
#define EZMB_DEFAULT_PROXY_FRONTEND_URL EZMB_DEFAULT_NORTH_URL


#endif // EZMB_H
