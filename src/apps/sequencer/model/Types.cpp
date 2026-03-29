#include "Config.h"
#include "Types.h"

const Types::ConditionInfo Types::conditionInfos[] = {
    [int(Types::Condition::Off)]        = { TXT_LIST_LABEL_CONDITION_OFF,         TXT_LIST_LABEL_CONDITION_OFF_SHORT,    ""  },
    [int(Types::Condition::Fill)]       = { TXT_LIST_LABEL_CONDITION_FILL,        TXT_LIST_LABEL_CONDITION_FILL_SHORT,    ""  },
    [int(Types::Condition::NotFill)]    = { TXT_LIST_LABEL_CONDITION_NOT_FILL,    TXT_LIST_LABEL_CONDITION_NOT_FILL_SHORT,   ""  },
    [int(Types::Condition::Pre)]        = { TXT_LIST_LABEL_CONDITION_PRE,         TXT_LIST_LABEL_CONDITION_PRE_SHORT,    ""  },
    [int(Types::Condition::NotPre)]     = { TXT_LIST_LABEL_CONDITION_NOT_PRE,     TXT_LIST_LABEL_CONDITION_NOT_PRE_SHORT,   ""  },
    [int(Types::Condition::First)]      = { TXT_LIST_LABEL_CONDITION_FIRST,       TXT_LIST_LABEL_CONDITION_FIRST_SHORT,    ""  },
    [int(Types::Condition::NotFirst)]   = { TXT_LIST_LABEL_CONDITION_NOT_FIRST,   TXT_LIST_LABEL_CONDITION_NOT_FIRST_SHORT,   ""  },
};

const Types::VoltageRangeInfo Types::voltageRangeInfos[] = {
    [int(Types::VoltageRange::Unipolar1V)]  = { 0.f, 1.f },
    [int(Types::VoltageRange::Unipolar2V)]  = { 0.f, 2.f },
    [int(Types::VoltageRange::Unipolar3V)]  = { 0.f, 3.f },
    [int(Types::VoltageRange::Unipolar4V)]  = { 0.f, 4.f },
    [int(Types::VoltageRange::Unipolar5V)]  = { 0.f, 5.f },
    [int(Types::VoltageRange::Bipolar1V)]   = { -1.f, 1.f },
    [int(Types::VoltageRange::Bipolar2V)]   = { -2.f, 2.f },
    [int(Types::VoltageRange::Bipolar3V)]   = { -3.f, 3.f },
    [int(Types::VoltageRange::Bipolar4V)]   = { -4.f, 4.f },
    [int(Types::VoltageRange::Bipolar5V)]   = { -5.f, 5.f },
};
