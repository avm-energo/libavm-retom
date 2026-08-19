#pragma once

#include <QtCore/qglobal.h>

#if defined(RETOM_EXPORTS)
#define AVM_RETOM_EXPORT Q_DECL_EXPORT
#elif defined(RETOM_IMPORTS)
#define AVM_RETOM_EXPORT Q_DECL_IMPORT
#else
#define AVM_RETOM_EXPORT
#endif
