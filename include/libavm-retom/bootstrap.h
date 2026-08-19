#pragma once

#include <QByteArray>
#include <utility>
#include <vector>

// Захваченные байтовые последовательности-"бутстрапы" -- порт src/linretom/_bootstrap.py
// (см. его docstring и docs/RE_LOG.md для обоснования byte-в-byte replay вместо семантики).
// Содержимое сгенерировано из Python-источника, не переписывать вручную.
namespace retom::bootstrap {

extern const std::vector<std::pair<quint16, QByteArray>> kConnectBootstrap;
extern const std::vector<std::pair<quint16, QByteArray>> kEnableBootstrap;
extern const std::vector<std::pair<quint16, QByteArray>> kOutPreamble;

}  // namespace retom::bootstrap
