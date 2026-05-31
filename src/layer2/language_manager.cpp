// language_manager.cpp
// Layer 2: 多语言管理器实现
// 翻译表由 tools/i18n_extract.py 从 config/i18n/*.json 生成

#include "language_manager.h"
#include <cstring>
#include <cstdio>

// ============================================================
// 翻译表（由 tools/i18n_extract.py 自动从 zh_CN.json/en_US.json 生成）
// 格式: { "key", "zh_CN 文本", "en_US 文本" }
// ============================================================
const LanguageEntry TRANSLATIONS[] = {
    // --- unit ---
    { "unit.speed",          "km/h",   "km/h"   },
    { "unit.rpm",            "RPM",    "RPM"    },
    { "unit.voltage",        "V",      "V"      },
    { "unit.current",        "A",      "A"      },
    { "unit.temperature",    "°C",     "°C"     },
    { "unit.soc",            "%",      "%"      },
    // --- status ---
    { "status.driving",      "行驶中",     "Driving"  },
    { "status.parked",       "停车",       "Parked"   },
    { "status.standby",     "待机",       "Standby"  },
    { "status.normal",       "正常",       "Normal"   },
    { "status.fault",       "故障",       "Fault"    },
    // --- indicator ---
    { "indicator.left_turn",   "左转向",    "Left Turn"  },
    { "indicator.right_turn",  "右转向",    "Right Turn" },
    { "indicator.high_beam",   "远光灯",    "High Beam"  },
    { "indicator.check_engine","发动机故障","Check Engine"},
    { "indicator.fog_light",   "雾灯",     "Fog Light"  },
    { "indicator.reverse",     "倒车",     "Reverse"    },
    { "indicator.park_brake",  "驻车制动",  "Park Brake" },
    { "indicator.tire_pressure","胎压报警",  "Tire Press."},
    { "indicator.ready",       "READY",    "READY"      },
    { "indicator.high_voltage","高压",     "High Volt."  },
    // --- alarm_text ---
    { "alarm_text.bat_overvolt",   "电池过压！",     "BATTERY OVERVOLTAGE"       },
    { "alarm_text.bat_undervolt",  "电池欠压！",     "BATTERY UNDERVOLTAGE"      },
    { "alarm_text.bat_soc_low",    "电量低，请充电",  "LOW BATTERY"              },
    { "alarm_text.motor_overtemp", "电机温度异常！",  "MOTOR OVERTEMP"            },
    { "alarm_text.overspeed",      "超速！请减速",   "OVERSPEED - SLOW DOWN"     },
    { "alarm_text.brake_fault",    "制动系统故障！", "BRAKE SYSTEM FAULT"        },
    { "alarm_text.door_open",      "车门未关！",     "DOOR OPEN"                 },
    { "alarm_text.tire_pressure_warn", "胎压异常！", "TIRE PRESSURE WARNING"     },
    // --- seatbelt ---
    { "seatbelt.driver",       "主驾",        "DRIVER"     },
    { "seatbelt.passenger",    "副驾",        "PASSENGER"  },
    { "seatbelt.rear_left",    "后左",        "REAR L"     },
    { "seatbelt.rear_center",  "后中",        "REAR C"     },
    { "seatbelt.rear_right",   "后右",        "REAR R"     },
    { "seatbelt.please_buckle","请系安全带",   "buckle up"  },
    { "seatbelt.buckled",      "已系",        "OK"         },
    { "seatbelt.unbuckled",    "未系",        "UNBUCKLED"  },
    { "seatbelt.empty",        "无人",        "EMPTY"      },
};

const int TRANSLATION_COUNT = sizeof(TRANSLATIONS) / sizeof(TRANSLATIONS[0]);

// 元信息：每个语言的 locale 和 font family
static const char* LOCALE_NAMES[LANG_COUNT] = { "zh_CN", "en_US" };
static const char* FONT_FAMILIES[LANG_COUNT] = {
    "Noto Sans CJK SC, Microsoft YaHei, sans-serif",
    "Roboto, Arial, sans-serif"
};

LanguageManager::LanguageManager()
    : m_current(LANG_ZH_CN), m_entries(TRANSLATIONS), m_count(TRANSLATION_COUNT) {}

void LanguageManager::init(const LanguageEntry* entries, int count) {
    m_entries = entries;
    m_count = count;
    m_current = LANG_ZH_CN;
}

void LanguageManager::setLanguage(Language lang) {
    if (lang >= 0 && lang < LANG_COUNT) {
        m_current = lang;
    }
}

const char* LanguageManager::tr(const char* key) const {
    if (!key) return "";
    for (int i = 0; i < m_count; i++) {
        if (strcmp(m_entries[i].key, key) == 0) {
            return (m_current == LANG_ZH_CN) ? m_entries[i].zh : m_entries[i].en;
        }
    }
    return key;  // 未找到返回原 key
}

const char* LanguageManager::currentLocale() const {
    return LOCALE_NAMES[m_current];
}

const char* LanguageManager::currentFontFamily() const {
    return FONT_FAMILIES[m_current];
}
