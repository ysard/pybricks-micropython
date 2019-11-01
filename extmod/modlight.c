// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2019 Laurens Valk

#include <pbio/light.h>
#include <pbio/button.h>

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mpconfig.h"

#include "modlight.h"

#include "modparameters.h"
#include "pberror.h"
#include "pbobj.h"
#include "pbkwarg.h"


STATIC mp_obj_t colorlight_on(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    PB_PARSE_ARGS_FUNCTION(n_args, pos_args, kw_args,
        PB_ARG_REQUIRED(color),
        PB_ARG_DEFAULT_INT(brightness, 100)
    );

    if (color == mp_const_none) {
        color = MP_OBJ_FROM_PTR(&pb_const_black);
    }

    pbio_light_color_t color_id = enum_get_value_maybe(color, &pb_enum_type_Color);

    mp_int_t bright = pb_obj_get_int(brightness);
    bright = bright < 0 ? 0 : bright > 100 ? 100: bright;

    if (bright != 100) {
        pb_assert(PBIO_ERROR_NOT_IMPLEMENTED);
    }

    if (color_id < PBIO_LIGHT_COLOR_NONE || color_id > PBIO_LIGHT_COLOR_PURPLE) {
        pb_assert(PBIO_ERROR_INVALID_ARG);
    }
    if (color_id == PBIO_LIGHT_COLOR_NONE || color_id == PBIO_LIGHT_COLOR_BLACK) {
        pb_assert(pbio_light_off(PBIO_PORT_SELF));
    }
    else {
        pb_assert(pbio_light_on(PBIO_PORT_SELF, color_id));
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(colorlight_on_obj, 0, colorlight_on);

STATIC mp_obj_t colorlight_off() {

    pb_assert(pbio_light_off(PBIO_PORT_SELF));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(colorlight_off_obj, colorlight_off);


STATIC const mp_rom_map_elem_t colorlight_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_colorlight)       },
    { MP_ROM_QSTR(MP_QSTR_on),       MP_ROM_PTR(&colorlight_on_obj)     },
    { MP_ROM_QSTR(MP_QSTR_off),         MP_ROM_PTR(&colorlight_off_obj)       },
};
STATIC MP_DEFINE_CONST_DICT(pb_module_colorlight_globals, colorlight_globals_table);

const mp_obj_module_t pb_module_colorlight = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&pb_module_colorlight_globals,
};

#ifdef PBDRV_CONFIG_HUB_EV3BRICK // FIXME: Don't use hub name here; make compatible with PUPDEVICES

// pybricks.ev3devices.Light class object
typedef struct _ev3devices_Light_obj_t {
    mp_obj_base_t base;
    pbio_lightdev_t dev;
} ev3devices_Light_obj_t;

// pybricks.ev3devices.Light.on
STATIC mp_obj_t ev3devices_Light_on(mp_obj_t self_in) {
    ev3devices_Light_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int16_t unused;
    // TODO: Move to modlight and generalize to deal with any light instance
    pb_assert(ev3device_get_values_at_mode(self->dev.ev3iodev, PBIO_IODEV_MODE_EV3_ULTRASONIC_SENSOR__DIST_CM, &unused));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ev3devices_Light_on_obj, ev3devices_Light_on);

// pybricks.ev3devices.Light.off
STATIC mp_obj_t ev3devices_Light_off(mp_obj_t self_in) {
    ev3devices_Light_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int16_t unused;
    pb_assert(ev3device_get_values_at_mode(self->dev.ev3iodev, PBIO_IODEV_MODE_EV3_ULTRASONIC_SENSOR__SI_CM, &unused));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ev3devices_Light_off_obj, ev3devices_Light_off);

// dir(pybricks.ev3devices.Light)
STATIC const mp_rom_map_elem_t ev3devices_Light_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_on   ), MP_ROM_PTR(&ev3devices_Light_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_off  ), MP_ROM_PTR(&ev3devices_Light_off_obj) },
};
STATIC MP_DEFINE_CONST_DICT(ev3devices_Light_locals_dict, ev3devices_Light_locals_dict_table);

// type(pybricks.ev3devices.Light)
STATIC const mp_obj_type_t ev3devices_Light_type = {
    { &mp_type_type },
    .locals_dict = (mp_obj_dict_t*)&ev3devices_Light_locals_dict,
};

mp_obj_t ev3devices_Light_obj_make_new(pbio_lightdev_t dev) {
    // Create new light instance
    ev3devices_Light_obj_t *light = m_new_obj(ev3devices_Light_obj_t);
    // Set type and iodev
    light->base.type = (mp_obj_type_t*) &ev3devices_Light_type;
    light->dev = dev;
    return light;
}

#endif
