#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <QByteArray>
#include <QString>

class LuaCategoryRuleEngine final
{
public:
    enum class State : std::uint8_t
    {
        Ready,
        Rejected
    };

    enum class Part : std::uint8_t
    {
        Upper,
        Lower,
        Accessory,
        Dress,
        Unknown
    };

    static constexpr std::size_t DefaultMemoryBytes               = 4 * 1024 * 1024;
    static constexpr int         DefaultInstructionsPerExecution  = 100'000;
    static constexpr int         DefaultInstructionsPerValidation = 1'000'000;

    struct Limits
    {
        std::size_t memoryBytes               = DefaultMemoryBytes;
        int         instructionsPerExecution  = DefaultInstructionsPerExecution;
        int         instructionsPerValidation = DefaultInstructionsPerValidation;
    };

    struct Result
    {
        bool    recognized = false;
        QString categoryCode;
        QString level1Code;
        QString level1Name;
        QString level2Code;
        QString level2Name;
        Part    part = Part::Unknown;
        QString error;
    };

    explicit LuaCategoryRuleEngine(const QByteArray &script);
    LuaCategoryRuleEngine(const QByteArray &script, Limits limits);
    ~LuaCategoryRuleEngine();

    LuaCategoryRuleEngine(const LuaCategoryRuleEngine &)            = delete;
    LuaCategoryRuleEngine &operator=(const LuaCategoryRuleEngine &) = delete;
    LuaCategoryRuleEngine(LuaCategoryRuleEngine &&)                 = delete;
    LuaCategoryRuleEngine &operator=(LuaCategoryRuleEngine &&)      = delete;

    [[nodiscard]] State   state() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString ruleId() const;
    [[nodiscard]] QString ruleVersion() const;
    [[nodiscard]] Result  classify(const QString &normalizedStyleId) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
