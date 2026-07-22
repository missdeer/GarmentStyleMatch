#include <cstddef>

#include <QCoreApplication>
#include <QDebug>

#include "LuaCategoryRuleEngine.h"

namespace
{
    using Engine = LuaCategoryRuleEngine;

    constexpr int         kTightExecutionInstructions    = 1'000;
    constexpr int         kTightValidationInstructions   = 5'000;
    constexpr std::size_t kRuntimeMemoryBytes            = 256 * 1024;
    constexpr int         kRuntimeExecutionInstructions  = 2'000;
    constexpr int         kRuntimeValidationInstructions = 20'000;

    bool check(bool condition, const QString &message)
    {
        if (!condition)
        {
            qCritical().noquote() << message;
        }
        return condition;
    }

    QByteArray validRule()
    {
        return R"lua(
return {
    ruleId = "test-brand",
    version = "1",
    classify = function(styleId)
        if styleId == "KNOWN" then
            return {
                recognized = true,
                categoryCode = "JE",
                level1Code = "2",
                level1Name = "外套",
                level2Code = "2.8",
                level2Name = "牛仔外套",
                part = "upper"
            }
        end
        return { recognized = false, part = "unknown" }
    end,
    tests = {
        {
            input = "KNOWN",
            expected = {
                recognized = true,
                categoryCode = "JE",
                level1Code = "2",
                level1Name = "外套",
                level2Code = "2.8",
                level2Name = "牛仔外套",
                part = "upper"
            }
        },
        { input = "OTHER", expected = { recognized = false, part = "unknown" } }
    }
}
)lua";
    }

    QByteArray runtimeFailureRule()
    {
        return R"lua(
return {
    ruleId = "runtime-failure",
    version = "1",
    classify = function(styleId)
        if styleId == "LOOP" then
            while true do end
        end
        if styleId == "MEMORY" then
            local value = string.rep("x", 1024 * 1024)
            return value
        end
        if styleId == "INVALID" then
            return { recognized = true, part = "upper" }
        end
        return {
            recognized = true,
            categoryCode = "JE",
            level1Code = "2",
            level1Name = "外套",
            level2Code = "2.8",
            level2Name = "牛仔外套",
            part = "upper"
        }
    end,
    tests = {
        {
            input = "OK",
            expected = {
                recognized = true,
                categoryCode = "JE",
                level1Code = "2",
                level1Name = "外套",
                level2Code = "2.8",
                level2Name = "牛仔外套",
                part = "upper"
            }
        }
    }
}
)lua";
    }

    int testValidRuleContract()
    {
        Engine engine(validRule());
        if (!check(engine.state() == Engine::State::Ready && engine.errorMessage().isEmpty(),
                   QStringLiteral("合法规则必须通过非空自验证进入可用状态")))
        {
            return 1;
        }
        if (!check(engine.ruleId() == QStringLiteral("test-brand") && engine.ruleVersion() == QStringLiteral("1"),
                   QStringLiteral("可用规则必须暴露稳定身份和版本")))
        {
            return 1;
        }

        const Engine::Result known = engine.classify(QStringLiteral("KNOWN"));
        if (!check(known.recognized && known.error.isEmpty() && known.categoryCode == QStringLiteral("JE") &&
                       known.level2Name == QStringLiteral("牛仔外套") && known.part == Engine::Part::Upper,
                   QStringLiteral("合法输入必须返回完整且经过契约校验的分类")))
        {
            return 1;
        }
        const Engine::Result intentionalUnknown = engine.classify(QStringLiteral("OTHER"));
        if (!check(!intentionalUnknown.recognized && intentionalUnknown.part == Engine::Part::Unknown && intentionalUnknown.error.isEmpty(),
                   QStringLiteral("规则主动返回 unknown 必须与执行失败的诊断结果区分")))
        {
            return 1;
        }
        const Engine::Result repeated = engine.classify(QStringLiteral("KNOWN"));
        if (!check(repeated.recognized && repeated.categoryCode == known.categoryCode && repeated.part == known.part,
                   QStringLiteral("相同规则和输入必须得到确定一致的分类")))
        {
            return 1;
        }

        return 0;
    }

    int testRejectedRulesAndSandbox()
    {
        Engine existingRule(validRule());
        Engine emptyRule(QByteArray {});
        Engine syntaxError(QByteArrayLiteral("return {"));
        Engine missingTests(
            QByteArrayLiteral("return { ruleId='x', version='1', classify=function() return {recognized=false, part='unknown'} end }"));
        Engine failingSelfTest(
            QByteArrayLiteral("return { ruleId='x', version='1', classify=function() return {recognized=false, part='unknown'} end,"
                              "tests={{input='x', expected={recognized=true, categoryCode='JE', level1Code='2', level1Name='a',"
                              "level2Code='2.8', level2Name='b', part='upper'}}} }"));
        if (!check(emptyRule.state() == Engine::State::Rejected && syntaxError.state() == Engine::State::Rejected &&
                       missingTests.state() == Engine::State::Rejected && failingSelfTest.state() == Engine::State::Rejected,
                   QStringLiteral("缺失、语法错误、零样例和自验证失败的规则必须被拒绝")))
        {
            return 1;
        }
        if (!check(!emptyRule.classify(QStringLiteral("KNOWN")).error.isEmpty() && existingRule.classify(QStringLiteral("KNOWN")).recognized,
                   QStringLiteral("被拒绝的新规则不得污染既有可用规则")))
        {
            return 1;
        }

        const QByteArray sandboxRule = R"lua(
assert(os == nil and io == nil and package == nil and debug == nil and require == nil)
assert(dofile == nil and loadfile == nil and load == nil and collectgarbage == nil)
assert(pcall == nil and xpcall == nil and setmetatable == nil and controller == nil)
return {
    ruleId = "sandbox",
    version = "1",
    classify = function()
        return { recognized = false, part = "unknown" }
    end,
    tests = {
        { input = "x", expected = { recognized = false, part = "unknown" } }
    }
}
)lua";
        Engine           sandbox(sandboxRule);
        if (!check(sandbox.state() == Engine::State::Ready, QStringLiteral("受限环境必须保留确定性规则所需能力，同时屏蔽系统、动态代码和宿主对象")))
        {
            return 1;
        }

        return 0;
    }

    int testLimitsAndRecovery()
    {
        Engine::Limits tightInstructions;
        tightInstructions.instructionsPerExecution  = kTightExecutionInstructions;
        tightInstructions.instructionsPerValidation = kTightValidationInstructions;
        Engine loadLoop(QByteArrayLiteral("while true do end"), tightInstructions);
        if (!check(loadLoop.state() == Engine::State::Rejected && !loadLoop.errorMessage().isEmpty(),
                   QStringLiteral("加载阶段的无限执行必须在指令上限内拒绝规则")))
        {
            return 1;
        }

        Engine::Limits runtimeLimits;
        runtimeLimits.memoryBytes               = kRuntimeMemoryBytes;
        runtimeLimits.instructionsPerExecution  = kRuntimeExecutionInstructions;
        runtimeLimits.instructionsPerValidation = kRuntimeValidationInstructions;
        Engine runtimeFailures(runtimeFailureRule(), runtimeLimits);
        if (!check(runtimeFailures.state() == Engine::State::Ready,
                   QStringLiteral("整体验证额度必须允许合法规则完成启用验证：%1").arg(runtimeFailures.errorMessage())))
        {
            return 1;
        }

        const Engine::Result loopFailure = runtimeFailures.classify(QStringLiteral("LOOP"));
        if (!check(!loopFailure.recognized && loopFailure.part == Engine::Part::Unknown && !loopFailure.error.isEmpty(),
                   QStringLiteral("运行期无限执行必须只使当前款号安全回退")))
        {
            return 1;
        }
        const Engine::Result afterLoop = runtimeFailures.classify(QStringLiteral("OK"));
        if (!check(afterLoop.recognized && afterLoop.error.isEmpty(), QStringLiteral("指令超限不得阻止同一可用规则处理后续款号")))
        {
            return 1;
        }

        const Engine::Result memoryFailure = runtimeFailures.classify(QStringLiteral("MEMORY"));
        if (!check(!memoryFailure.recognized && memoryFailure.part == Engine::Part::Unknown && !memoryFailure.error.isEmpty(),
                   QStringLiteral("运行期内存超限必须只使当前款号安全回退")))
        {
            return 1;
        }
        const Engine::Result invalid = runtimeFailures.classify(QStringLiteral("INVALID"));
        if (!check(!invalid.recognized && invalid.part == Engine::Part::Unknown && !invalid.error.isEmpty(),
                   QStringLiteral("不完整或矛盾的脚本结果必须回退为带诊断的 unknown")))
        {
            return 1;
        }
        if (!check(runtimeFailures.classify(QStringLiteral("OK")).recognized, QStringLiteral("内存超限和非法结果不得永久拒绝可用规则")))
        {
            return 1;
        }

        return 0;
    }
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    int              result = 0;
    result |= testValidRuleContract();
    result |= testRejectedRulesAndSandbox();
    result |= testLimitsAndRecovery();
    return result;
}
