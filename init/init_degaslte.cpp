#include <stdlib.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#include "property_service.h"
#include "vendor_init.h"

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

using android::base::GetProperty;
using android::base::ReadFileToString;
using android::base::Trim;
using android::init::property_set;

#define SERIAL_NUMBER_FILE "/efs/FactoryApp/serial_no"

void property_override(char const prop[], char const value[])
{
    prop_info *pi;

    pi = (prop_info*) __system_property_find(prop);
    if (pi)
        __system_property_update(pi, value, strlen(value));
    else
        __system_property_add(prop, strlen(prop), value, strlen(value));
}

void property_override_dual(char const system_prop[], char const vendor_prop[], char const value[])
{
    property_override(system_prop, value);
    property_override(vendor_prop, value);
}

void vendor_load_properties()
{
    const std::string bootloader = GetProperty("ro.bootloader", "");
    const std::string platform = GetProperty("ro.board.platform", "");

    char const *serial_number_file = SERIAL_NUMBER_FILE;
    std::string serial_number;

    if (platform != ANDROID_TARGET)
        return;

    if (ReadFileToString(serial_number_file, &serial_number)) {
        serial_number = Trim(serial_number);
        property_override("ro.serialno", serial_number.c_str());
    }

    if (bootloader.find("T235Y") == 0) {
        /* degasltezt */
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-T235Y");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "degaslte");
        property_override_dual("ro.product.device", "ro.vendor.product.name", "degasltezt");
        property_override_dual("ro.build.description", "ro.vendor.build.description", "degasltezt-user 4.4.2 KOT49H T235YZTU1AOD1 release-keys");
        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/degasltezt/degaslte:4.4.2/KOT49H/T235YZTU1AOD1:user/release-keys");
        property_override("ro.build.product", "degaslte");
    } else if (bootloader.find("T235") == 0) {
        /* degasltexx */
        property_override_dual("ro.product.model", "ro.vendor.product.model", "SM-T235");
        property_override_dual("ro.product.device", "ro.vendor.product.device", "degaslte");
        property_override_dual("ro.product.device", "ro.vendor.product.name", "degasltexx");
        property_override_dual("ro.build.description", "ro.vendor.build.description", "degasltexx-user 5.1.1 LMY47X T235XXS1BRH3 release-keys");
        property_override_dual("ro.build.fingerprint", "ro.vendor.build.fingerprint", "samsung/degasltexx/degaslte:5.1.1/LMY47X/T235XXS1BRH3:user/release-keys");
        property_override("ro.build.product", "degaslte");
     }

    const std::string device = GetProperty("ro.product.device", "");
    LOG(INFO) << "Found bootloader " << bootloader << ". " << "Setting build properties for " << device << ".\n";
}
