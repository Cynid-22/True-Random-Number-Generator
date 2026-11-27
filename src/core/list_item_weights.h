#pragma once

#include <map>
#include <string>
#include <string_view>
#include <windows.h>

class WipedString {
public:
    explicit WipedString(std::string_view text) : m_text(text) {}
    WipedString(const WipedString& other) : m_text(other.m_text) {}
    WipedString& operator=(const WipedString&) = delete;
    ~WipedString() { SecureZeroMemory(m_text.data(), m_text.capacity()); }

    std::string_view View() const { return m_text; }

private:
    std::string m_text;
};

struct WipedStringLess {
    using is_transparent = void;
    bool operator()(const WipedString& a, const WipedString& b) const { return a.View() < b.View(); }
    bool operator()(const WipedString& a, std::string_view b) const { return a.View() < b; }
    bool operator()(std::string_view a, const WipedString& b) const { return a < b.View(); }
};

using ListItemWeights = std::map<WipedString, int, WipedStringLess>;
