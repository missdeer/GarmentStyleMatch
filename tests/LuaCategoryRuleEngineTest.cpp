#include <array>
#include <cstddef>

#include <QCoreApplication>
#include <QDebug>
#include <QFile>

#include "LuaCategoryRuleEngine.h"

namespace
{
    using Engine = LuaCategoryRuleEngine;

    constexpr int         kTightExecutionInstructions    = 1'000;
    constexpr int         kTightValidationInstructions   = 5'000;
    constexpr std::size_t kRuntimeMemoryBytes            = 256 * 1024;
    constexpr int         kRuntimeExecutionInstructions  = 2'000;
    constexpr int         kRuntimeValidationInstructions = 20'000;

    struct ExpectedCategory
    {
        const char  *code;
        const char  *level1Code;
        const char  *level1Name;
        const char  *level2Code;
        const char  *level2Name;
        Engine::Part part;
    };

    constexpr std::array<ExpectedCategory, 59> kCurrentBrandCategories {{
        {"JD", "2", "外套", "2.5", "羽绒", Engine::Part::Upper},     {"JP", "2", "外套", "2.6", "棉服", Engine::Part::Upper},
        {"JJ", "2", "外套", "2.1", "夹克", Engine::Part::Upper},     {"JE", "2", "外套", "2.8", "牛仔外套", Engine::Part::Upper},
        {"JW", "2", "外套", "2.7", "毛呢", Engine::Part::Upper},     {"JL", "2", "外套", "2.9", "皮衣", Engine::Part::Upper},
        {"JK", "2", "外套", "2.2", "西装", Engine::Part::Upper},     {"JT", "2", "外套", "2.3", "风衣", Engine::Part::Upper},
        {"VW", "2", "外套", "2.4", "背心", Engine::Part::Upper},     {"CK", "1", "上衣", "1.5", "毛针织", Engine::Part::Upper},
        {"KW", "1", "上衣", "1.5", "毛针织", Engine::Part::Upper},   {"KN", "1", "上衣", "1.5", "毛针织", Engine::Part::Upper},
        {"MZ", "1", "上衣", "1.2", "卫衣", Engine::Part::Upper},     {"MW", "1", "上衣", "1.2", "卫衣", Engine::Part::Upper},
        {"MA", "1", "上衣", "1.2", "卫衣", Engine::Part::Upper},     {"MH", "1", "上衣", "1.2", "卫衣", Engine::Part::Upper},
        {"LW", "1", "上衣", "1.1", "T恤", Engine::Part::Upper},      {"LS", "1", "上衣", "1.1", "T恤", Engine::Part::Upper},
        {"LA", "1", "上衣", "1.1", "T恤", Engine::Part::Upper},      {"RW", "1", "上衣", "1.1", "T恤", Engine::Part::Upper},
        {"RS", "1", "上衣", "1.1", "T恤", Engine::Part::Upper},      {"RA", "1", "上衣", "1.1", "T恤", Engine::Part::Upper},
        {"RN", "1", "上衣", "1.1", "T恤", Engine::Part::Upper},      {"RL", "1", "上衣", "1.1", "T恤", Engine::Part::Upper},
        {"HW", "1", "上衣", "1.4", "POLO", Engine::Part::Upper},     {"HS", "1", "上衣", "1.4", "POLO", Engine::Part::Upper},
        {"HA", "1", "上衣", "1.4", "POLO", Engine::Part::Upper},     {"YW", "1", "上衣", "1.3", "衬衫", Engine::Part::Upper},
        {"YS", "1", "上衣", "1.3", "衬衫", Engine::Part::Upper},     {"YC", "1", "上衣", "1.3", "衬衫", Engine::Part::Upper},
        {"YP", "1", "上衣", "1.3", "衬衫", Engine::Part::Upper},     {"YA", "1", "上衣", "1.3", "衬衫", Engine::Part::Upper},
        {"TM", "3", "裤子", "3.1", "卫裤", Engine::Part::Lower},     {"MT", "3", "裤子", "3.1", "卫裤", Engine::Part::Lower},
        {"TC", "3", "裤子", "3.2", "休闲裤", Engine::Part::Lower},   {"TH", "3", "裤子", "3.2", "休闲裤", Engine::Part::Lower},
        {"TG", "3", "裤子", "3.2", "休闲裤", Engine::Part::Lower},   {"TJ", "3", "裤子", "3.3", "牛仔裤", Engine::Part::Lower},
        {"TF", "3", "裤子", "3.3", "牛仔裤", Engine::Part::Lower},   {"TW", "3", "裤子", "3.4", "西装裤", Engine::Part::Lower},
        {"WH", "3", "裤子", "3.5", "半身裙", Engine::Part::Lower},   {"AY", "4", "配件", "4.1", "配饰", Engine::Part::Accessory},
        {"AC", "4", "配件", "4.1", "配饰", Engine::Part::Accessory}, {"AN", "4", "配件", "4.1", "配饰", Engine::Part::Accessory},
        {"AK", "4", "配件", "4.1", "配饰", Engine::Part::Accessory}, {"XP", "4", "配件", "4.1", "配饰", Engine::Part::Accessory},
        {"AW", "4", "配件", "4.1", "配饰", Engine::Part::Accessory}, {"AM", "4", "配件", "4.1", "配饰", Engine::Part::Accessory},
        {"AB", "4", "配件", "4.1", "配饰", Engine::Part::Accessory}, {"AF", "4", "配件", "4.1", "配饰", Engine::Part::Accessory},
        {"AG", "4", "配件", "4.1", "配饰", Engine::Part::Accessory}, {"AP", "4", "配件", "4.1", "配饰", Engine::Part::Accessory},
        {"AS", "4", "配件", "4.1", "配饰", Engine::Part::Accessory}, {"AX", "4", "配件", "4.1", "配饰", Engine::Part::Accessory},
        {"FD", "4", "配件", "4.1", "配饰", Engine::Part::Accessory}, {"FT", "4", "配件", "4.1", "配饰", Engine::Part::Accessory},
        {"MS", "4", "配件", "4.1", "配饰", Engine::Part::Accessory}, {"OA", "4", "配件", "4.1", "配饰", Engine::Part::Accessory},
        {"PP", "4", "配件", "4.1", "配饰", Engine::Part::Accessory},
    }};

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

    int testCurrentBrandRule(const QString &rulePath)
    {
        QFile ruleFile(rulePath);
        if (!check(ruleFile.open(QIODevice::ReadOnly), QStringLiteral("无法读取当前品牌规则：%1").arg(rulePath)))
        {
            return 1;
        }
        const QByteArray script = ruleFile.readAll();
        Engine           engine(script);
        if (!check(engine.state() == Engine::State::Ready && engine.ruleId() == QStringLiteral("current-brand") &&
                       engine.ruleVersion() == QStringLiteral("1"),
                   QStringLiteral("当前品牌规则必须通过全量自验证并暴露稳定身份与修订：%1").arg(engine.errorMessage())))
        {
            return 1;
        }

        for (const ExpectedCategory &expected : kCurrentBrandCategories)
        {
            const QString        code    = QString::fromLatin1(expected.code);
            const QString        styleId = code == QLatin1String("JE") ? QStringLiteral("T0JE26B38A008") : QStringLiteral("T0%126B38A008").arg(code);
            const Engine::Result actual  = engine.classify(styleId);
            if (!check(actual.recognized && actual.error.isEmpty() && actual.categoryCode == code &&
                           actual.level1Code == QString::fromLatin1(expected.level1Code) &&
                           actual.level1Name == QString::fromUtf8(expected.level1Name) &&
                           actual.level2Code == QString::fromLatin1(expected.level2Code) &&
                           actual.level2Name == QString::fromUtf8(expected.level2Name) && actual.part == expected.part,
                       QStringLiteral("当前品牌代码 %1 必须符合已确认的业务层级与粗分类").arg(code)))
            {
                return 1;
            }
        }

        const Engine::Result shortStyleId = engine.classify(QStringLiteral("T0J"));
        const Engine::Result unknownCode  = engine.classify(QStringLiteral("T0ZZ26B38A008"));
        if (!check(!shortStyleId.recognized && shortStyleId.part == Engine::Part::Unknown && shortStyleId.error.isEmpty() &&
                       !unknownCode.recognized && unknownCode.part == Engine::Part::Unknown && unknownCode.error.isEmpty(),
                   QStringLiteral("短款号和未知代码必须作为正常业务未知安全返回")))
        {
            return 1;
        }

        constexpr auto denimMapping = "JE = \"denim-jacket\"";
        if (!check(script.contains(denimMapping), QStringLiteral("规则变更夹具必须能定位 JE 映射")))
        {
            return 1;
        }
        QByteArray incompleteScript = script;
        incompleteScript.replace(denimMapping, "JE = nil");
        Engine incompleteRule(incompleteScript);
        if (!check(incompleteRule.state() == Engine::State::Rejected, QStringLiteral("遗漏任一已支持代码时，全量自验证必须拒绝规则启用")))
        {
            return 1;
        }

        return 0;
    }
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (argc != 2)
    {
        qCritical() << "Usage: LuaCategoryRuleEngineTest <current-brand-rule>";
        return 1;
    }
    int result = 0;
    result |= testValidRuleContract();
    result |= testRejectedRulesAndSandbox();
    result |= testLimitsAndRecovery();
    result |= testCurrentBrandRule(QString::fromLocal8Bit(argv[1]));
    return result;
}
