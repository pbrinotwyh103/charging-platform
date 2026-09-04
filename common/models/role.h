#pragma once

#include <QtGlobal>

namespace Charging {

enum class Role : quint8 {
    Anonymous = 0,
    User = 1,
    Administrator = 2
};

} // namespace Charging
