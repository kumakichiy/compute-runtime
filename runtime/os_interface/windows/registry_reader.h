/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/utilities/debug_settings_reader.h"
#include "os_inc.h"
#include <string>
#include <stdint.h>
#include <Windows.h>

namespace OCLRT {
class RegistryReader : public SettingsReader {
  public:
    int32_t getSetting(const char *settingName, int32_t defaultValue) override;
    bool getSetting(const char *settingName, bool defaultValue) override;
    std::string getSetting(const char *settingName, const std::string &value) override;
    RegistryReader(bool userScope) {
        igdrclHkeyType = userScope ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
        setUpProcessName();
    }
    RegistryReader(std::string regKey) {
        registryReadRootKey.append(std::string(1, PATH_SEPARATOR)).append(regKey);
        setUpProcessName();
    }
    const char *appSpecificLocation(const std::string &name) override;

  protected:
    HKEY igdrclHkeyType = HKEY_LOCAL_MACHINE;
    std::string registryReadRootKey = "Software\\Intel\\IGFX\\OCL";
    void setUpProcessName();
    std::string processName;
};
} // namespace OCLRT
