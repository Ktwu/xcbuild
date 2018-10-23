/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#include <xcassets/Slot/Idiom.h>

#include <cstdlib>
#include <string>

using xcassets::Slot::Idiom;
using xcassets::Slot::Idioms;

static const std::string UNIVERSAL = "universal";
static const std::string PHONE = "phone";
static const std::string PAD = "pad";
static const std::string MAC = "mac";
static const std::string TV = "tv";
static const std::string WATCH = "watch";
static const std::string CAR = "car";
static const std::string MARKETING = "marketing";

ext::optional<Idiom> Idioms::
Parse(std::string const &value)
{
    if (value == UNIVERSAL) {
        return Idiom::Universal;
    } else if (value == PHONE) {
        return Idiom::Phone;
    } else if (value == PAD) {
        return Idiom::Pad;
    } else if (value == MAC) {
        return Idiom::Desktop;
    } else if (value == TV) {
        return Idiom::TV;
    } else if (value == WATCH) {
        return Idiom::Watch;
    } else if (value == CAR) {
        return Idiom::Car;
    } else if (value == MARKETING) {
        return Idiom::iOSMarketing;
    } else {
        fprintf(stderr, "warning: unknown idiom %s\n", value.c_str());
        return ext::nullopt;
    }
}

Idiom Idioms::
Parse(car_attribute_identifier_idiom_value value)
{
    switch (value) {
        case car_attribute_identifier_idiom_value_universal:
            return Idiom::Universal;
        case car_attribute_identifier_idiom_value_phone:
            return Idiom::Phone;
        case car_attribute_identifier_idiom_value_pad:
            return Idiom::Pad;
        case car_attribute_identifier_idiom_value_tv:
            return Idiom::TV;
        case car_attribute_identifier_idiom_value_car:
            return Idiom::Car;
        case car_attribute_identifier_idiom_value_watch:
            return Idiom::Watch;
        case car_attribute_identifier_idiom_value_marketing:
            return Idiom::iOSMarketing;
    }

    abort();
}

std::string Idioms::
String(Idiom idiom)
{
    switch (idiom) {
        case Idiom::Universal:
            return UNIVERSAL;
        case Idiom::Phone:
            return PHONE;
        case Idiom::Pad:
            return PAD;
        case Idiom::Desktop:
            return MAC;
        case Idiom::TV:
            return TV;
        case Idiom::Watch:
            return WATCH;
        case Idiom::Car:
            return CAR;
        case Idiom::iOSMarketing:
            return MARKETING;
    }

    abort();
}

std::string Idioms::
String(car_attribute_identifier_idiom_value value)
{
    return Idioms::String(Idioms::Parse(value));
}
