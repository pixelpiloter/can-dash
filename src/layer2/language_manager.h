// language_manager.h
// Layer 2: 多语言管理器
// 从 JSON 文件加载翻译，QML 通过 Layer 3 访问

#pragma once
#include <stdint.h>
#include <stdbool.h>

// 支持的语言
typedef enum {
    LANG_ZH_CN = 0,
    LANG_EN_US  = 1,
    LANG_COUNT
} Language;

struct LanguageEntry {
    const char* key;   // e.g. "alarm.bat_overvolt"
    const char* zh;
    const char* en;
};

class LanguageManager {
public:
    LanguageManager();

    void init(const LanguageEntry* entries, int count);

    void setLanguage(Language lang);
    Language currentLanguage() const { return m_current; }

    // 按 key 查翻译字符串，key 格式 "section.subsection"
    const char* tr(const char* key) const;

    // 获取当前语言的元信息
    const char* currentLocale() const;
    const char* currentFontFamily() const;

private:
    Language m_current;
    const LanguageEntry* m_entries;
    int m_count;

    static int compareKey(const void* a, const char* key);
};

// 内置翻译表（由 yaml_to_c.py 从 JSON 生成，或手写常量数组）
// 使用 _tr(key) 宏在 C 代码中访问
#define _tr(key) LanguageManager::instance()->tr(key)
