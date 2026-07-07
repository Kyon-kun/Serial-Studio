/*
 * Serial Studio
 * https://serial-studio.com/
 *
 * Copyright (C) 2020-2025 Alex Spataru
 *
 * This file is dual-licensed:
 *
 * - Under the GNU GPLv3 (or later) for builds that exclude Pro modules.
 * - Under the Serial Studio Commercial License for builds that include
 *   any Pro functionality.
 *
 * You must comply with the terms of one of these licenses, depending
 * on your use case.
 *
 * For GPL terms, see <https://www.gnu.org/licenses/gpl-3.0.html>
 * For commercial terms, see LICENSE_COMMERCIAL.md in the project root.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-SerialStudio-Commercial
 */

#pragma once

#include <QString>
#include <vector>

#include "SerialStudio.h"

namespace DataModel {

/**
 * @brief Builds a folder's "/"-joined path from any folder vector (root -> leaf).
 */
template<typename Folder>
QString folderDisplayPath(const std::vector<Folder>& folders, int folderId)
{
  QString path;
  int cur        = folderId;
  const int kMax = static_cast<int>(folders.size());
  for (int i = 0; i <= kMax && cur != -1; ++i) {
    const Folder* match = nullptr;
    for (const auto& f : folders)
      if (f.folderId == cur) {
        match = &f;
        break;
      }

    if (!match)
      break;

    path = path.isEmpty() ? match->title : (match->title + QLatin1Char('/') + path);
    cur  = match->parentFolderId;
  }

  return path;
}

/**
 * @brief Returns the QML icon path for a SerialStudio::BusType integer.
 */
inline QString busTypeIcon(int busType)
{
  switch (static_cast<SerialStudio::BusType>(busType)) {
    case SerialStudio::BusType::UART:
      return QStringLiteral("qrc:/icons/devices/drivers/uart.svg");
    case SerialStudio::BusType::Network:
      return QStringLiteral("qrc:/icons/devices/drivers/network.svg");
    case SerialStudio::BusType::BluetoothLE:
      return QStringLiteral("qrc:/icons/devices/drivers/bluetooth.svg");
#ifdef BUILD_COMMERCIAL
    case SerialStudio::BusType::Audio:
      return QStringLiteral("qrc:/icons/devices/drivers/audio.svg");
    case SerialStudio::BusType::ModBus:
      return QStringLiteral("qrc:/icons/devices/drivers/modbus.svg");
    case SerialStudio::BusType::CanBus:
      return QStringLiteral("qrc:/icons/devices/drivers/canbus.svg");
    case SerialStudio::BusType::RawUsb:
      return QStringLiteral("qrc:/icons/devices/drivers/usb.svg");
    case SerialStudio::BusType::HidDevice:
      return QStringLiteral("qrc:/icons/devices/drivers/hid.svg");
    case SerialStudio::BusType::Process:
      return QStringLiteral("qrc:/icons/devices/drivers/process.svg");
    case SerialStudio::BusType::Mqtt:
      return QStringLiteral("qrc:/icons/devices/drivers/mqtt.svg");
#endif
    default:
      return QStringLiteral("qrc:/icons/devices/drivers/uart.svg");
  }
}

}  // namespace DataModel
