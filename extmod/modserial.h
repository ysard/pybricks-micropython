// SPDX-License-Identifier: MIT
// Copyright (c) 2019 Laurens Valk
// Copyright (c) 2019 LEGO System A/S

#include <pbio/error.h>

typedef struct _serial_t serial_t;

pbio_error_t serial_get(serial_t **_ser, int tty, int baudrate, int timeout);
